"""Exercise the Unix descriptor/exit protocol with real local processes (Linux/WSL)."""
import pathlib, subprocess, tempfile, shutil
app = pathlib.Path(__file__).resolve().parents[1]
fixture = r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv) {
 if (argc > 1 && !strcmp(argv[1], "nested")) {
  char *args[]={"git", "echo", NULL}; execvp("git",args); return 126;
 }
 char input[128]; if (!fgets(input,sizeof(input),stdin)) return 120;
 printf("%s:%s",getenv("TEST_VALUE"),input); fflush(stdout); return 23;
}
'''
harness = r'''#include "git_process_broker.h"
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <string>
extern "C" int __wrap_execvp(const char *,char *const[]);
static int hostForks;
static void hostChildHook(){if(++hostForks>1)for(int fd=3;fd<64;fd++)close(fd);}
int main(int argc,char **argv) {
 pthread_atfork(nullptr,nullptr,hostChildHook);
 std::string socket="/tmp/arkforge-broker-"+std::to_string(getpid())+".sock";
 pid_t broker=StartGitProcessBroker(argv[1],socket); if(broker<0)return 10;
 int in[2],out[2];pipe(in);pipe(out);
 pid_t child=fork();
 if(!child){setpgid(0,broker);dup2(in[0],0);dup2(out[1],1);close(in[1]);close(out[0]);
  setenv("ARKFORGE_GIT_BROKER",socket.c_str(),1);setenv("TEST_VALUE","clean",1);
  char *args[]={(char*)"git",(char*)"nested",nullptr};__wrap_execvp("git",args);_exit(125);}
 close(in[0]);close(out[1]);write(in[1],"payload\n",8);close(in[1]);
 std::string text;char buf[64];ssize_t n;while((n=read(out[0],buf,sizeof(buf)))>0)text.append(buf,n);close(out[0]);
 int status;waitpid(child,&status,0);kill(-broker,SIGKILL);waitpid(broker,nullptr,0);unlink(socket.c_str());
 if(!WIFEXITED(status)||WEXITSTATUS(status)!=23||text!="clean:payload\n"){fprintf(stderr,"status=%d output=%s\n",status,text.c_str());return 11;}
 puts("PASS: nested exec, stdin/stdout transfer, environment, nonzero exit, inherited host callbacks and cleanup");return 0;
}
'''
def run():
 with tempfile.TemporaryDirectory(prefix="arkforge-git-broker-") as directory:
  root=pathlib.Path(directory)
  for name in ['git_process_broker.cpp','git_process_broker.h','git_process_protocol.h','git_exec_bridge.c']:
   shutil.copyfile(app/'native'/name,root/name)
  (root/'fixture.c').write_text(fixture)
  (root/'harness.cpp').write_text(harness)
  if __import__('os').name=='nt':
   converted='/mnt/'+root.drive[0].lower()+root.as_posix()[2:]
   prefix=['wsl','-d','Ubuntu','--','bash','-c']
  else: converted=str(root);prefix=['bash','-c']
  import shlex
  source='cd '+shlex.quote(converted)+' && cc -fPIC -c git_exec_bridge.c -o bridge.o && cc -shared -fPIC fixture.c bridge.o -Wl,-Bsymbolic,--wrap=execvp,--wrap=execve,--wrap=exit,--wrap=atexit -o libgit_exec.so && g++ harness.cpp git_process_broker.cpp bridge.o -ldl -pthread -o check && ./check '+shlex.quote(converted)+''
  result=subprocess.run(prefix+[source],capture_output=True)
  print('exit',result.returncode, result.stdout.decode('utf-8',errors='replace'))
  if b'PASS:' not in result.stdout:
   print(result.stderr.decode('utf-8',errors='replace'));raise SystemExit(1)
if __name__=='__main__':run()
