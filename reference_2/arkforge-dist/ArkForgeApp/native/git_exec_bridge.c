/* HarmonyOS exec adapter: run bundled upstream Git in clean local processes. */
#define _GNU_SOURCE
#include "git_process_protocol.h"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <signal.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern char **environ;
/* Git/curl are built without threads. Do not run ArkUI's inherited atfork
 * callbacks: their remembered descriptors belong to the host, not this CLI. */
pid_t __wrap_fork(void){return (pid_t)syscall(SYS_clone,SIGCHLD,0,0,0,0);}
pid_t __wrap_vfork(void){return __wrap_fork();}
/* The ArkUI host may close its libc standard FILE objects. A real exec creates
 * new FILE objects; supply the same lifetime for each bundled program image.
 * Symbol aliases avoid conflicting with libc's const stdin declarations. */
FILE *arkforge_stdin __asm__("stdin");
FILE *arkforge_stdout __asm__("stdout");
FILE *arkforge_stderr __asm__("stderr");
__attribute__((constructor)) static void program_stdio(void) {
    arkforge_stdin=fdopen(0,"r");
    arkforge_stdout=fdopen(1,"w");
    arkforge_stderr=fdopen(2,"w");
    if(arkforge_stderr)setvbuf(arkforge_stderr,NULL,_IONBF,0);
}
static void (*exit_handlers[256])(void);
static unsigned exit_count;
int __wrap_atexit(void (*handler)(void)){if(exit_count==256)return -1;exit_handlers[exit_count++]=handler;return 0;}
_Noreturn void __wrap_exit(int status){while(exit_count)exit_handlers[--exit_count]();fflush(NULL);_exit(status);}
static int transfer(int fd,const void *data,size_t bytes){const char *p=data;while(bytes){ssize_t n=write(fd,p,bytes);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;bytes-=n;}return 0;}
static int invoke(const char *file,char *const argv[],char *const envp[]){
    const char *name=strrchr(file,'/');name=name?name+1:file;
    unsigned kind=!strcmp(name,"git")?1:(!strcmp(name,"git-remote-https")||!strcmp(name,"git-remote-http"))?2:3;
    const char *path=getenv("ARKFORGE_GIT_BROKER");if(!path){errno=ENOENT;return -1;}
    char cwd[4096];if(!getcwd(cwd,sizeof(cwd)))return -1;
    struct ArkForgeGitRequest header={ARKFORGE_GIT_MAGIC,kind,0,0,0,3};
    size_t bytes=strlen(cwd)+1;
    while(argv[header.argc]){bytes+=strlen(argv[header.argc])+1;if(++header.argc>256){errno=E2BIG;return -1;}}
    while(envp[header.envc]){bytes+=strlen(envp[header.envc])+1;if(++header.envc>4096){errno=E2BIG;return -1;}}
    if(bytes>ARKFORGE_GIT_MAX_MESSAGE){errno=E2BIG;return -1;}header.bytes=(uint32_t)bytes;
    char *body=malloc(bytes);if(!body)return -1;char *p=body;
    size_t n=strlen(cwd)+1;memcpy(p,cwd,n);p+=n;
    for(unsigned i=0;i<header.argc;i++){n=strlen(argv[i])+1;memcpy(p,argv[i],n);p+=n;}
    for(unsigned i=0;i<header.envc;i++){n=strlen(envp[i])+1;memcpy(p,envp[i],n);p+=n;}
    int descriptor=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(descriptor<0){free(body);return -1;}
    struct sockaddr_un address={0};address.sun_family=AF_UNIX;
    if(strlen(path)>=sizeof(address.sun_path)){free(body);close(descriptor);errno=ENAMETOOLONG;return -1;}
    strcpy(address.sun_path,path);
    if(connect(descriptor,(struct sockaddr *)&address,sizeof(address))<0){free(body);close(descriptor);return -1;}
    int fds[]={-1,-1,-1,-1};if(fcntl(9,F_GETFD)>=0)header.descriptors=4;
    for(unsigned i=0;i<header.descriptors;i++){
        fds[i]=fcntl(i==3?9:(int)i,F_DUPFD_CLOEXEC,64);
        if(fds[i]<0){for(unsigned j=0;j<i;j++)close(fds[j]);free(body);close(descriptor);return -1;}
    }
    int failed=transfer(descriptor,&header,sizeof(header));
    for(unsigned i=0;i<header.descriptors&&!failed;i++){
        char control[CMSG_SPACE(sizeof(int))]={0},marker=(char)i;
        struct iovec io={&marker,1};struct msghdr message={0};
        message.msg_iov=&io;message.msg_iovlen=1;message.msg_control=control;message.msg_controllen=sizeof(control);
        struct cmsghdr *c=CMSG_FIRSTHDR(&message);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(int));memcpy(CMSG_DATA(c),&fds[i],sizeof(int));
        ssize_t sent;do{sent=sendmsg(descriptor,&message,MSG_NOSIGNAL);}while(sent<0&&errno==EINTR);
        if(sent!=1)failed=-1;
    }
    for(unsigned i=0;i<header.descriptors;i++)close(fds[i]);
    if(failed||transfer(descriptor,body,bytes)<0){memset(body,0,bytes);free(body);close(descriptor);return -1;}
    memset(body,0,bytes);free(body);
    // This process is the old program image; only the private result socket survives.
    long limit=sysconf(_SC_OPEN_MAX);if(limit<0||limit>65536)limit=65536;
    for(int fd=0;fd<limit;fd++)if(fd!=descriptor)close(fd);
    int status=127;size_t got=0;while(got<sizeof(status)){ssize_t count=read(descriptor,(char *)&status+got,sizeof(status)-got);if(count<0&&errno==EINTR)continue;if(count<=0)break;got+=count;}
    close(descriptor);_exit(got==sizeof(status)?status:127);
}
int __wrap_execve(const char *file,char *const argv[],char *const envp[]){return invoke(file,argv,envp);}
int __wrap_execvp(const char *file,char *const argv[]){return invoke(file,argv,environ);}
