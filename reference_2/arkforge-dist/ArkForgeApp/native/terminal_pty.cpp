#define _GNU_SOURCE
#include "terminal_pty.h"
#include "git_process_broker.h"
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <dlfcn.h>
#include <cerrno>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {
struct Terminal {
    std::mutex lock;
    int fd=-1;pid_t child=-1,broker=-1;
    std::string socket,credential,pendingOutput;
};
std::mutex terminalsLock;
std::map<int,std::shared_ptr<Terminal>> terminals;
int nextTerminal=1;
std::string String(napi_env env,napi_value value){
    size_t size=0;if(napi_get_value_string_utf8(env,value,nullptr,0,&size)!=napi_ok||size>1024*1024)return {};
    std::string result(size+1,'\0');napi_get_value_string_utf8(env,value,result.data(),result.size(),&size);result.resize(size);return result;
}
int Number(napi_env env,napi_value value,int fallback=0){int32_t n;return napi_get_value_int32(env,value,&n)==napi_ok?n:fallback;}
void SetString(napi_env env,napi_value object,const char *key,const std::string &text){napi_value value;napi_create_string_utf8(env,text.data(),text.size(),&value);napi_set_named_property(env,object,key,value);}
void SetNumber(napi_env env,napi_value object,const char *key,int number){napi_value value;napi_create_int32(env,number,&value);napi_set_named_property(env,object,key,value);}
std::string Encode64(const std::string &input){
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;unsigned value=0;int bits=-6;for(unsigned char c:input){value=(value<<8)|c;bits+=8;while(bits>=0){out+=alphabet[(value>>bits)&63];bits-=6;}}
    if(bits>-6)out+=alphabet[((value<<8)>>(bits+8))&63];while(out.size()%4)out+='=';return out;
}
std::shared_ptr<Terminal> Find(int id){std::lock_guard<std::mutex> guard(terminalsLock);auto it=terminals.find(id);return it==terminals.end()?nullptr:it->second;}
void Stop(int id){
    std::shared_ptr<Terminal> terminal;
    {std::lock_guard<std::mutex> guard(terminalsLock);auto it=terminals.find(id);if(it==terminals.end())return;terminal=it->second;terminals.erase(it);}
    {std::lock_guard<std::mutex> guard(terminal->lock);if(terminal->fd>=0){pid_t group=0;if(ioctl(terminal->fd,TIOCGPGRP,&group)==0&&group>0)kill(-group,SIGHUP);close(terminal->fd);terminal->fd=-1;}}
    bool running=waitpid(terminal->child,nullptr,WNOHANG)==0;
    if(running){kill(terminal->child,SIGHUP);for(int i=0;i<10;i++){if(waitpid(terminal->child,nullptr,WNOHANG)!=0){running=false;break;}usleep(10000);}}
    if(running){kill(-terminal->child,SIGKILL);kill(terminal->child,SIGKILL);while(waitpid(terminal->child,nullptr,0)<0&&errno==EINTR){}}
    kill(-terminal->broker,SIGKILL);while(waitpid(terminal->broker,nullptr,0)<0&&errno==EINTR){}
    unlink(terminal->socket.c_str());
}
void Marker(const std::string &path){int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0700);if(fd>=0){const char text[]="# ArkForge signed native program entry\n";write(fd,text,sizeof(text)-1);close(fd);chmod(path.c_str(),0700);}}
void Child(const std::string &library,const std::string &cwd,const std::string &home,const std::string &socket,int master,int slave){
    prctl(PR_SET_PDEATHSIG,SIGHUP);prctl(PR_SET_NAME,"arkforge-ash");setsid();ioctl(slave,TIOCSCTTY,0);
    for(int fd=0;fd<3;fd++)dup2(slave,fd);if(slave>2)close(slave);close(master);
    for(int sig:{SIGINT,SIGQUIT,SIGTERM,SIGHUP,SIGCHLD,SIGTSTP,SIGTTIN,SIGTTOU,SIGWINCH})signal(sig,SIG_DFL);
    tcsetpgrp(STDIN_FILENO,getpid());
    long limit=sysconf(_SC_OPEN_MAX);if(limit<0||limit>65536)limit=65536;for(int fd=3;fd<limit;fd++)close(fd);
    if(chdir(cwd.c_str())<0){dprintf(2,"chdir: %s\n",strerror(errno));_exit(127);}
    std::string bin=home+"/bin";mkdir(bin.c_str(),0700);
    for(const char *name:{"git","git-remote-http","git-remote-https","sh","ash","busybox"})Marker(bin+"/"+name);
    setenv("HOME",home.c_str(),1);setenv("PWD",cwd.c_str(),1);setenv("TERM","xterm-256color",1);setenv("COLORTERM","truecolor",1);
    setenv("PATH",(bin+":/system/bin:/bin").c_str(),1);setenv("SHELL",(bin+"/sh").c_str(),1);
    setenv("ARKFORGE_GIT_BROKER",socket.c_str(),1);setenv("ARKFORGE_GIT_LIBDIR",library.substr(0,library.find_last_of('/')).c_str(),1);
    setenv("GIT_EXEC_PATH",bin.c_str(),1);setenv("GIT_SSL_CAINFO",(home+"/cacert.pem").c_str(),1);setenv("GIT_PAGER","cat",1);
    setenv("LANG","C.UTF-8",1);setenv("LC_ALL","C.UTF-8",1);setenv("PS1","ArkForge:\\W $ ",1);
    unsetenv("GIT_CONFIG_COUNT");unsetenv("GIT_TRACE2_EVENT");unsetenv("GIT_TRACE2_PARENT_SID");
    void *image=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);if(!image){dprintf(2,"BusyBox: %s\n",dlerror());_exit(127);}
    using Main=int(*)(int,char**);auto main=reinterpret_cast<Main>(dlsym(image,"main"));if(!main){dprintf(2,"BusyBox: %s\n",dlerror());_exit(127);}
    char *args[]={const_cast<char*>("sh"),const_cast<char*>("-i"),nullptr};int code=main(2,args);fflush(nullptr);_exit(code);
}
napi_value Open(napi_env env,napi_callback_info info){
    napi_value args[5];size_t count=5;napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
    if(count<3){napi_throw_type_error(env,nullptr,"Terminal requires a project directory and home");return nullptr;}
    const std::string cwd=String(env,args[0]),home=String(env,args[1]),credential=String(env,args[2]);
    Dl_info location{};dladdr(reinterpret_cast<void*>(&Open),&location);std::string path=location.dli_fname?location.dli_fname:"";
    const std::string directory=path.substr(0,path.find_last_of('/'));
    auto terminal=std::make_shared<Terminal>();int id;
    {std::lock_guard<std::mutex> guard(terminalsLock);if(terminals.size()>=16){napi_throw_error(env,nullptr,"Close a terminal before opening another");return nullptr;}id=nextTerminal++;}
    int master=posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC|O_NONBLOCK);char name[256]{};
    if(master<0||grantpt(master)<0||unlockpt(master)<0||ptsname_r(master,name,sizeof(name))!=0){if(master>=0)close(master);napi_throw_error(env,nullptr,strerror(errno));return nullptr;}
    int slave=open(name,O_RDWR|O_NOCTTY|O_CLOEXEC);if(slave<0){close(master);napi_throw_error(env,nullptr,strerror(errno));return nullptr;}
    winsize dimensions{};dimensions.ws_col=count>3?Number(env,args[3],80):80;dimensions.ws_row=count>4?Number(env,args[4],24):24;ioctl(master,TIOCSWINSZ,&dimensions);
    terminal->socket=home+"/terminal-"+std::to_string(getpid())+"-"+std::to_string(id)+".sock";
    terminal->broker=StartGitProcessBroker(directory,terminal->socket,credential);
    if(terminal->broker<0){close(slave);close(master);napi_throw_error(env,nullptr,strerror(errno));return nullptr;}
    terminal->child=fork();
    if(terminal->child==0)Child(directory+"/libbusybox_exec.so",cwd,home,terminal->socket,master,slave);
    close(slave);
    if(terminal->child<0){close(master);kill(-terminal->broker,SIGKILL);waitpid(terminal->broker,nullptr,0);unlink(terminal->socket.c_str());napi_throw_error(env,nullptr,strerror(errno));return nullptr;}
    terminal->fd=master;terminal->credential=credential;
    {std::lock_guard<std::mutex> guard(terminalsLock);terminals.emplace(id,terminal);}
    napi_value result;napi_create_object(env,&result);SetNumber(env,result,"id",id);SetNumber(env,result,"pid",terminal->child);return result;
}
struct Task {napi_async_work work{};napi_deferred deferred{};std::shared_ptr<Terminal> terminal;std::string data,output,error;bool write=false,closed=false;int id=0,code=0;};
void Execute(napi_env,void *data){
    auto *task=static_cast<Task*>(data);int fd;
    {std::lock_guard<std::mutex> guard(task->terminal->lock);fd=task->terminal->fd<0?-1:fcntl(task->terminal->fd,F_DUPFD_CLOEXEC,64);}
    if(fd<0){task->closed=true;return;}
    if(task->write){
        if(task->data.find('\3')!=std::string::npos)kill(-task->terminal->broker,SIGINT);
        size_t offset=0;int waits=0;
        while(offset<task->data.size()){
            ssize_t size=write(fd,task->data.data()+offset,task->data.size()-offset);
            if(size>0){offset+=size;continue;}if(size<0&&errno==EINTR)continue;
            if(size<0&&(errno==EAGAIN||errno==EWOULDBLOCK)&&waits++<100){pollfd ready{fd,POLLOUT,0};poll(&ready,1,50);continue;}
            task->error=strerror(errno);break;
        }
    }else{
        pollfd ready{fd,POLLIN,0};int result=poll(&ready,1,100);
        if(result>0){char buffer[16384];while(task->output.size()<65536){ssize_t size=read(fd,buffer,sizeof(buffer));if(size>0){task->output.append(buffer,size);continue;}if(size<0&&errno==EINTR)continue;break;}}
        int status=0;pid_t done=waitpid(task->terminal->child,&status,WNOHANG);
        if(done==task->terminal->child){
            task->closed=true;task->code=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);
            char buffer[16384];for(;;){ssize_t size=read(fd,buffer,sizeof(buffer));if(size>0){task->output.append(buffer,size);continue;}if(size<0&&errno==EINTR)continue;break;}
        }
        if(!task->terminal->credential.empty()){
            std::lock_guard<std::mutex> guard(task->terminal->lock);
            auto &pending=task->terminal->pendingOutput;pending+=task->output;
            std::vector<std::string> secrets{task->terminal->credential,Encode64("x-access-token:"+task->terminal->credential)};
            for(const auto &secret:secrets)for(size_t p=0;(p=pending.find(secret,p))!=std::string::npos;)pending.replace(p,secret.size(),"[REDACTED]");
            size_t hold=0;
            if(!task->closed)for(const auto &secret:secrets)for(size_t n=1;n<secret.size()&&n<=pending.size();n++)
                if(pending.compare(pending.size()-n,n,secret,0,n)==0)hold=std::max(hold,n);
            task->output=pending.substr(0,pending.size()-hold);pending.erase(0,pending.size()-hold);
        }
    }
    close(fd);
}
void Complete(napi_env env,napi_status,void *data){
    auto *task=static_cast<Task*>(data);napi_value result;napi_create_object(env,&result);
    SetString(env,result,"data",Encode64(task->output));SetNumber(env,result,"code",task->code);
    napi_value closed;napi_get_boolean(env,task->closed,&closed);napi_set_named_property(env,result,"closed",closed);
    if(task->closed)Stop(task->id);
    if(task->error.empty())napi_resolve_deferred(env,task->deferred,result);else{napi_value message,error;napi_create_string_utf8(env,task->error.c_str(),NAPI_AUTO_LENGTH,&message);napi_create_error(env,nullptr,message,&error);napi_reject_deferred(env,task->deferred,error);}
    napi_delete_async_work(env,task->work);delete task;
}
napi_value Io(napi_env env,napi_callback_info info,bool write){
    size_t count=2;napi_value args[2];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);int id=count?Number(env,args[0]):-1;
    auto terminal=Find(id);if(!terminal){napi_throw_error(env,nullptr,"Open a terminal to continue");return nullptr;}
    auto *task=new Task;task->terminal=terminal;task->id=id;task->write=write;if(write&&count>1)task->data=String(env,args[1]);
    napi_value promise,name;napi_create_promise(env,&task->deferred,&promise);napi_create_string_utf8(env,"ArkForge PTY",NAPI_AUTO_LENGTH,&name);
    napi_create_async_work(env,nullptr,name,Execute,Complete,task,&task->work);napi_queue_async_work(env,task->work);return promise;
}
napi_value Read(napi_env env,napi_callback_info info){return Io(env,info,false);}
napi_value Write(napi_env env,napi_callback_info info){return Io(env,info,true);}
napi_value Resize(napi_env env,napi_callback_info info){
    size_t count=3;napi_value args[3];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
    if(count==3){auto terminal=Find(Number(env,args[0]));if(terminal){std::lock_guard<std::mutex> guard(terminal->lock);winsize size{};size.ws_col=Number(env,args[1],80);size.ws_row=Number(env,args[2],24);if(terminal->fd>=0)ioctl(terminal->fd,TIOCSWINSZ,&size);}}
    napi_value result;napi_get_undefined(env,&result);return result;
}
napi_value Close(napi_env env,napi_callback_info info){size_t count=1;napi_value arg;napi_get_cb_info(env,info,&count,&arg,nullptr,nullptr);if(count)Stop(Number(env,arg));napi_value result;napi_get_undefined(env,&result);return result;}
}
void RegisterTerminalPty(napi_env env,napi_value exports){
    napi_property_descriptor properties[]={
        {"terminalOpen",nullptr,Open,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"terminalRead",nullptr,Read,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"terminalWrite",nullptr,Write,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"terminalResize",nullptr,Resize,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"terminalClose",nullptr,Close,nullptr,nullptr,nullptr,napi_default,nullptr}};
    napi_define_properties(env,exports,sizeof(properties)/sizeof(properties[0]),properties);
}
