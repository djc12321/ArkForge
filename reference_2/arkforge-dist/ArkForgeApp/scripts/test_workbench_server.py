"""Real HTTP and immediate process-restart regression for the asset server."""
import os, pathlib, shlex, shutil, subprocess, tempfile

app = pathlib.Path(__file__).resolve().parents[1]
runner = '''#include <cstdio>
#include <unistd.h>
extern "C" int ArkForgeStartWorkbenchServer(const char*);
int main(int argc,char**argv){int port=ArkForgeStartWorkbenchServer(argv[1]);
printf("%d\\n",port);fflush(stdout);if(port<0)return 1;for(;;)pause();}
'''
check = '''import http.client,pathlib,subprocess
root=pathlib.Path(__file__).parent
(root/'assets').mkdir();(root/'assets/index.html').write_text('asset-server-restart')
for i in range(8):
 process=subprocess.Popen([str(root/'server'),str(root/'assets')],stdout=subprocess.PIPE,text=True)
 try:
  port=int(process.stdout.readline());assert port==18712,(i,port)
  connection=http.client.HTTPConnection('127.0.0.1',port,timeout=3)
  connection.request('GET','/index.html');reply=connection.getresponse()
  assert reply.status==200 and reply.read()==b'asset-server-restart';connection.close()
  connection=http.client.HTTPConnection('127.0.0.1',port,timeout=3)
  connection.request('GET','/../outside');reply=connection.getresponse()
  assert reply.status==404;reply.read();connection.close()
 finally:process.kill();process.wait(timeout=3)
print('PASS: real HTTP assets and traversal rejection; eight immediate restarts reuse the stable workbench origin despite TCP TIME_WAIT.')
'''
with tempfile.TemporaryDirectory(prefix='arkforge-workbench-server-') as directory:
    root = pathlib.Path(directory)
    shutil.copyfile(app/'native/resources/workbench_server.cpp', root/'server.cpp')
    (root/'runner.cpp').write_text(runner)
    (root/'check.py').write_text(check)
    location = '/mnt/'+root.drive[0].lower()+root.as_posix()[2:] if os.name == 'nt' else str(root)
    prefix = ['wsl','-d','Ubuntu','--','bash','-c'] if os.name == 'nt' else ['bash','-c']
    result = subprocess.run(prefix+['cd '+shlex.quote(location)+' && g++ -std=c++17 server.cpp runner.cpp -pthread -o server && python3 check.py'],capture_output=True)
    print(result.stdout.decode('utf8',errors='replace'))
    if result.returncode:
        print(result.stderr.decode('utf8',errors='replace'))
        raise SystemExit(result.returncode)
