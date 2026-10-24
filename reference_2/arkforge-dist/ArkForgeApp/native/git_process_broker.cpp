#define _GNU_SOURCE
#include "git_process_broker.h"
#include "git_process_protocol.h"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>
extern char **environ;
struct GitClient { pid_t pid;int socket; };
struct GitMessage { ArkForgeGitRequest header{};std::vector<char> body;std::vector<int> fds; };
static bool ReadAll(int fd,void *buffer,size_t bytes){
    char *out=static_cast<char *>(buffer);while(bytes){ssize_t n=read(fd,out,bytes);if(n<0&&errno==EINTR)continue;if(n<=0)return false;out+=n;bytes-=n;}return true;
}
static bool Receive(int socket,GitMessage &request){
    if(!ReadAll(socket,&request.header,sizeof(request.header)))return false;
    auto &h=request.header;
    if(h.magic!=ARKFORGE_GIT_MAGIC||(h.kind!=1&&h.kind!=2&&h.kind!=3)||h.argc<1||h.argc>256||h.envc>4096||h.bytes>ARKFORGE_GIT_MAX_MESSAGE||(h.descriptors!=3&&h.descriptors!=4))return false;
    for(unsigned i=0;i<h.descriptors;i++){
        char control[CMSG_SPACE(sizeof(int))]{},marker=-1;iovec io{&marker,1};
        msghdr message{};message.msg_iov=&io;message.msg_iovlen=1;message.msg_control=control;message.msg_controllen=sizeof(control);
        ssize_t size;do{size=recvmsg(socket,&message,0);}while(size<0&&errno==EINTR);
        cmsghdr *c=CMSG_FIRSTHDR(&message);
        if(size!=1||!c||c->cmsg_level!=SOL_SOCKET||c->cmsg_type!=SCM_RIGHTS||c->cmsg_len!=CMSG_LEN(sizeof(int)))return false;
        int fd;memcpy(&fd,CMSG_DATA(c),sizeof(fd));request.fds.push_back(fd);
        if(marker!=i||(message.msg_flags&(MSG_CTRUNC|MSG_TRUNC)))return false;
    }
    request.body.resize(h.bytes);return ReadAll(socket,request.body.data(),request.body.size());
}
static std::string Encode64(const std::string &input){
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;unsigned value=0;int bits=-6;
    for(unsigned char c:input){value=(value<<8)|c;bits+=8;while(bits>=0){out+=alphabet[(value>>bits)&63];bits-=6;}}
    if(bits>-6)out+=alphabet[((value<<8)>>(bits+8))&63];while(out.size()%4)out+='=';return out;
}
static void Execute(const std::string &directory,GitMessage &request,const std::string &credential){
    std::vector<char *> values;size_t offset=0;
    while(offset<request.body.size()){
        char *text=request.body.data()+offset;size_t left=request.body.size()-offset;
        size_t size=strnlen(text,left);if(size==left)_exit(127);values.push_back(text);offset+=size+1;
    }
    if(values.size()!=1+request.header.argc+request.header.envc)_exit(127);
    std::vector<char *> argv(values.begin()+1,values.begin()+1+request.header.argc);argv.push_back(nullptr);
    std::vector<char *> env(values.begin()+1+request.header.argc,values.end());env.push_back(nullptr);
    std::vector<int> originals;
    for(int fd:request.fds){int copy=fcntl(fd,F_DUPFD_CLOEXEC,64);if(copy<0)_exit(127);originals.push_back(copy);}
    for(int fd=0;fd<3;fd++)if(dup2(originals[fd],fd)<0)_exit(127);
    if(originals.size()==4&&dup2(originals[3],9)<0)_exit(127);
    long limit=sysconf(_SC_OPEN_MAX);if(limit<0||limit>65536)limit=65536;
    for(int fd=3;fd<limit;fd++)if(fd!=9||originals.size()!=4)close(fd);
    environ=env.data();
    signal(SIGINT,SIG_DFL);
    if(request.header.kind!=3&&!credential.empty()){
        setenv("GIT_CONFIG_COUNT","2",1);setenv("GIT_CONFIG_KEY_0","http.https://github.com/.extraHeader",1);setenv("GIT_CONFIG_VALUE_0","",1);
        setenv("GIT_CONFIG_KEY_1","http.https://github.com/.extraHeader",1);
        setenv("GIT_CONFIG_VALUE_1",("AUTHORIZATION: basic "+Encode64("x-access-token:"+credential)).c_str(),1);
    }
    if(chdir(values[0])<0){dprintf(2,"[ArkForge launcher] Git child working directory unavailable\n");_exit(127);}
    // Broker is forked before any Git image is loaded: every program gets fresh globals.
    std::string path=directory+(request.header.kind==1?"/libgit_exec.so":request.header.kind==2?"/libgit_remote_http.so":"/libbusybox_exec.so");
    void *library=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!library){dprintf(2,"[ArkForge launcher] Git program load failed: %s\n",dlerror());_exit(127);}
    using Main=int (*)(int,char **);auto main=reinterpret_cast<Main>(dlsym(library,"main"));
    if(!main)_exit(127);int result=main(request.header.argc,argv.data());fflush(nullptr);_exit(result);
}
pid_t StartGitProcessBroker(const std::string &directory,const std::string &path,const std::string &credential){
    sockaddr_un address{};address.sun_family=AF_UNIX;
    if(path.size()>=sizeof(address.sun_path)){errno=ENAMETOOLONG;return -1;}
    memcpy(address.sun_path,path.c_str(),path.size()+1);
    int listener=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(listener<0)return -1;
    if(bind(listener,reinterpret_cast<sockaddr *>(&address),sizeof(address))<0||chmod(path.c_str(),0600)<0||listen(listener,32)<0){close(listener);unlink(path.c_str());return -1;}
    pid_t server=fork();
    if(server!=0){close(listener);if(server>0)setpgid(server,server);else unlink(path.c_str());return server;}
    prctl(PR_SET_PDEATHSIG,SIGKILL);setpgid(0,0);signal(SIGCHLD,SIG_DFL);signal(SIGINT,SIG_IGN);std::vector<GitClient> clients;
    // Never retain an application log/pipe in the process service.
    int null=open("/dev/null",O_RDWR);if(null>=0){for(int fd=0;fd<3;fd++)dup2(null,fd);if(null>2)close(null);}
    freopen("/dev/null","r",stdin);freopen("/dev/null","w",stdout);freopen("/dev/null","w",stderr);
    long limit=sysconf(_SC_OPEN_MAX);if(limit<0||limit>65536)limit=65536;
    for(int fd=3;fd<limit;fd++)if(fd!=listener)close(fd);
    for(;;){
        fd_set reads;FD_ZERO(&reads);FD_SET(listener,&reads);timeval timeout{0,50000};
        if(select(listener+1,&reads,nullptr,nullptr,&timeout)>0){
            int connection=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC);
            if(connection>=0){
                ucred peer{};socklen_t length=sizeof(peer);GitMessage request;timeval deadline{5,0};setsockopt(connection,SOL_SOCKET,SO_RCVTIMEO,&deadline,sizeof(deadline));
                bool valid=getsockopt(connection,SOL_SOCKET,SO_PEERCRED,&peer,&length)==0&&peer.uid==getuid()&&clients.size()<64&&Receive(connection,request);
                if(valid){// This server is single threaded; inherited ArkUI atfork callbacks must not run.
                pid_t child=static_cast<pid_t>(syscall(SYS_clone,SIGCHLD,0,0,0,0));if(child==0)Execute(directory,request,credential);if(child>0)clients.push_back({child,connection});else valid=false;}
                for(int fd:request.fds)close(fd);
                std::fill(request.body.begin(),request.body.end(),0);
                if(!valid){int status=127;send(connection,&status,sizeof(status),MSG_NOSIGNAL);close(connection);}
            }
        }
        for(size_t i=0;i<clients.size();){int status=0;pid_t done=waitpid(clients[i].pid,&status,WNOHANG);if(done==clients[i].pid){int code=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);send(clients[i].socket,&code,sizeof(code),MSG_NOSIGNAL);close(clients[i].socket);clients[i]=clients.back();clients.pop_back();}else i++;}
    }
}
