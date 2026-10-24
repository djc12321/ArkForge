#include <napi/native_api.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <dlfcn.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <cJSON.h>
#include "git_process_broker.h"
#include "terminal_pty.h"

struct Command {
    napi_async_work work{}; napi_deferred deferred{};
    std::string cwd, home, binary, owner, output, credential;
    std::vector<std::string> args;
    int status = 127;
    int traceStatus = -1, waitStatus = -1;
};
static std::string Base64(const std::string &input) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;uint32_t value=0;int bits=-6;
    for(unsigned char c:input){value=(value<<8)|c;bits+=8;while(bits>=0){output+=alphabet[(value>>bits)&63];bits-=6;}}
    if(bits>-6)output+=alphabet[((value<<8)>>(bits+8))&63];
    while(output.size()%4)output+='=';return output;
}
static std::string String(napi_env env, napi_value value) {
    size_t length=0;
    if(napi_get_value_string_utf8(env,value,nullptr,0,&length)!=napi_ok || length>65536) return {};
    std::string result(length+1,'\0');napi_get_value_string_utf8(env,value,result.data(),result.size(),&length);result.resize(length);return result;
}
static void ReadExitTrace(Command *task,std::string &pending,std::string &session) {
    size_t end;
    while((end=pending.find('\n'))!=std::string::npos){
        std::string line=pending.substr(0,end);pending.erase(0,end+1);
        cJSON *json=cJSON_Parse(line.c_str());if(!json)continue;
        auto *sid=cJSON_GetObjectItemCaseSensitive(json,"sid"),*event=cJSON_GetObjectItemCaseSensitive(json,"event"),*code=cJSON_GetObjectItemCaseSensitive(json,"code");
        if(cJSON_IsString(sid)&&session.empty())session=sid->valuestring;
        if(cJSON_IsString(sid)&&session==sid->valuestring&&cJSON_IsString(event)&&!strcmp(event->valuestring,"exit")&&cJSON_IsNumber(code))task->traceStatus=code->valueint;
        cJSON_Delete(json);
    }
}
static void Execute(napi_env, void *data) {
    auto *task=static_cast<Command *>(data);
    int pipes[2],trace[2];
    if(pipe2(pipes,O_CLOEXEC)!=0){task->output="[ArkForge launcher] pipe: "+std::string(strerror(errno))+"\n";return;}
    if(pipe2(trace,O_CLOEXEC)!=0){close(pipes[0]);close(pipes[1]);task->output="[ArkForge launcher] trace pipe failed\n";return;}
    std::vector<char *> argv;for(auto &arg:task->args)argv.push_back(arg.data());argv.push_back(nullptr);
    std::string directory=task->binary.substr(0,task->binary.find_last_of('/'));
    std::string socketPath=task->home+"/broker-"+std::to_string(getpid())+"-"+std::to_string(reinterpret_cast<uintptr_t>(task))+".sock";
    pid_t broker=StartGitProcessBroker(directory,socketPath);
    if(broker<0){close(pipes[0]);close(pipes[1]);close(trace[0]);close(trace[1]);task->output="[ArkForge launcher] Cannot start local Git process service\n";return;}
    pid_t pid=fork();
    if(pid==0){
        setpgid(0,broker);close(pipes[0]);dup2(pipes[1],STDOUT_FILENO);dup2(pipes[1],STDERR_FILENO);close(pipes[1]);
        close(trace[0]);if(trace[1]!=9){dup2(trace[1],9);close(trace[1]);}
        int null=open("/dev/null",O_RDONLY);if(null>=0 && null!=STDIN_FILENO){dup2(null,STDIN_FILENO);close(null);}
        if(chdir(task->cwd.c_str())!=0){dprintf(2,"[ArkForge launcher] chdir: %s\n",strerror(errno));_exit(127);}
        setenv("HOME",task->home.c_str(),1);
        std::string directory=task->binary.substr(0,task->binary.find_last_of('/'));
        std::string bin=task->home+"/bin";
        mkdir(bin.c_str(),0700);
        for(const char *name:{"git","git-remote-http","git-remote-https"}){
            std::string launcher=bin+"/"+name;
            int fd=open(launcher.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0700);
            if(fd>=0){const char marker[]="# Loaded from the signed native library by ArkForge\n";write(fd,marker,sizeof(marker)-1);close(fd);chmod(launcher.c_str(),0700);}
        }
        setenv("ARKFORGE_GIT_LIBDIR",directory.c_str(),1);
        setenv("ARKFORGE_GIT_DEPTH","0",1);
        setenv("ARKFORGE_GIT_BROKER",socketPath.c_str(),1);
        setenv("GIT_EXEC_PATH",bin.c_str(),1);
        setenv("ARKFORGE_GIT_OWNER",task->owner.c_str(),1);
        if(!task->credential.empty()){
            // Official Git URL-scoped transient configuration, as used by actions/checkout.
            setenv("GIT_CONFIG_COUNT","2",1);
            setenv("GIT_CONFIG_KEY_0","http.https://github.com/.extraHeader",1);
            setenv("GIT_CONFIG_VALUE_0","",1);
            setenv("GIT_CONFIG_KEY_1","http.https://github.com/.extraHeader",1);
            std::string header="AUTHORIZATION: basic "+Base64("x-access-token:"+task->credential);
            setenv("GIT_CONFIG_VALUE_1",header.c_str(),1);
        }
        setenv("GIT_SSL_CAINFO",(task->home+"/cacert.pem").c_str(),1);
        setenv("GIT_TERMINAL_PROMPT","0",1);
        setenv("GIT_PAGER","cat",1);
        setenv("PATH",(bin+":/system/bin:/bin").c_str(),1);
        setenv("LC_ALL","C",1);
        // Git records CLI status before process-wide exit handlers run.
        unsetenv("GIT_TRACE2_PARENT_SID");setenv("GIT_TRACE2_EVENT","9",1);setenv("GIT_TRACE2_EVENT_BRIEF","1",1);
        // HarmonyOS prohibits execve from an app. Load the signed native CLI
        // in this isolated child; Git's own main/parser/messages remain intact.
        void *library=dlopen(task->binary.c_str(),RTLD_NOW|RTLD_LOCAL);
        if(!library){dprintf(2,"[ArkForge launcher] dlopen: %s\n",dlerror());_exit(127);}
        using Main=int (*)(int,char **);
        auto main=reinterpret_cast<Main>(dlsym(library,"main"));
        if(!main){dprintf(2,"[ArkForge launcher] Official Git entry unavailable\n");_exit(127);}
        int result=main(static_cast<int>(task->args.size()),argv.data());
        fflush(stdout);fflush(stderr);_exit(result);
    }
    close(pipes[1]);close(trace[1]);
    if(pid<0){kill(-broker,SIGKILL);waitpid(broker,nullptr,0);unlink(socketPath.c_str());close(pipes[0]);close(trace[0]);task->output="[ArkForge launcher] fork: "+std::string(strerror(errno))+"\n";return;}
    auto started=std::chrono::steady_clock::now();bool truncated=false,outOpen=true,traceOpen=true;
    std::string traceBuffer,session;
    while(outOpen||traceOpen){
        pollfd fds[2]{{outOpen?pipes[0]:-1,POLLIN,0},{traceOpen?trace[0]:-1,POLLIN,0}};int ready=poll(fds,2,250);
        if(ready>0){
            for(int i=0;i<2;i++)if(fds[i].revents&(POLLIN|POLLHUP|POLLERR)){
                char buffer[16384];ssize_t n=read(fds[i].fd,buffer,sizeof(buffer));
                if(n<=0){if(i==0)outOpen=false;else traceOpen=false;continue;}
                if(i==0){if(task->output.size()+n<=16*1024*1024)task->output.append(buffer,n);else truncated=true;}
                else{traceBuffer.append(buffer,n);ReadExitTrace(task,traceBuffer,session);}
            }
        } else if(ready<0&&errno!=EINTR)break;
        if(std::chrono::steady_clock::now()-started>std::chrono::minutes(5)){
            kill(-broker,SIGKILL);task->output+="\n[ArkForge launcher] Command exceeded 5 minutes.\n";break;
        }
    }
    close(pipes[0]);close(trace[0]);int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    kill(-broker,SIGKILL);while(waitpid(broker,nullptr,0)<0&&errno==EINTR){}unlink(socketPath.c_str());
    if(waited==pid)task->waitStatus=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);
    task->status=task->traceStatus>=0?task->traceStatus:task->waitStatus>=0?task->waitStatus:127;
    if(truncated)task->output+="\n[ArkForge launcher] Output exceeds 16 MiB.\n";
    if(!task->credential.empty())for(const auto &secret: {task->credential,Base64("x-access-token:"+task->credential)})
        for(size_t pos=0;(pos=task->output.find(secret,pos))!=std::string::npos;)task->output.replace(pos,secret.size(),"[REDACTED]");
}
static void Complete(napi_env env,napi_status,void *data){
    auto *task=static_cast<Command *>(data);napi_value result,text,code;
    napi_create_object(env,&result);napi_create_string_utf8(env,task->output.data(),task->output.size(),&text);napi_create_int32(env,task->status,&code);
    napi_set_named_property(env,result,"output",text);napi_set_named_property(env,result,"code",code);
    napi_value waited,traced;napi_create_int32(env,task->waitStatus,&waited);napi_create_int32(env,task->traceStatus,&traced);
    napi_set_named_property(env,result,"waitCode",waited);napi_set_named_property(env,result,"traceCode",traced);
    napi_resolve_deferred(env,task->deferred,result);napi_delete_async_work(env,task->work);delete task;
}
static napi_value Run(napi_env env,napi_callback_info info){
    size_t count=4;napi_value args[4];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
    bool array=false;uint32_t length=0;
    if(count<3||napi_is_array(env,args[2],&array)!=napi_ok||!array||napi_get_array_length(env,args[2],&length)!=napi_ok||length<1||length>256){napi_throw_type_error(env,nullptr,"[ArkForge launcher] Invalid argv");return nullptr;}
    auto *task=new Command;task->cwd=String(env,args[0]);task->home=String(env,args[1]);
    if(count==4)task->credential=String(env,args[3]);
    if(task->cwd.empty()||task->cwd[0]!='/'||task->home.empty()){delete task;napi_throw_error(env,nullptr,"[ArkForge launcher] Invalid working directory");return nullptr;}
    for(uint32_t i=0;i<length;i++){napi_value item;napi_get_element(env,args[2],i,&item);task->args.push_back(String(env,item));}
    Dl_info library{};dladdr(reinterpret_cast<void *>(&Run),&library);std::string location=library.dli_fname?library.dli_fname:"";
    task->owner=location;
    task->binary=location.substr(0,location.find_last_of('/'))+"/libgit_exec.so";
    napi_value promise,name;napi_create_promise(env,&task->deferred,&promise);napi_create_string_utf8(env,"Official Git CLI",NAPI_AUTO_LENGTH,&name);
    napi_create_async_work(env,nullptr,name,Execute,Complete,task,&task->work);napi_queue_async_work(env,task->work);return promise;
}
static napi_value Init(napi_env env,napi_value exports){napi_property_descriptor property{"run",nullptr,Run,nullptr,nullptr,nullptr,napi_default,nullptr};napi_define_properties(env,exports,1,&property);RegisterTerminalPty(env,exports);return exports;}
static napi_module module{1,0,nullptr,Init,"arkforge_cli",nullptr,{0}};
extern "C" __attribute__((constructor)) void RegisterOfficialCli(){napi_module_register(&module);}
