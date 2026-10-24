"""Real loopback HTTP tests for the native Huawei callback (Linux or WSL)."""
import pathlib, subprocess, tempfile, shutil, os, shlex

app = pathlib.Path(__file__).resolve().parents[1]
test = r'''
import ctypes, http.client, json, time, urllib.parse, pathlib
library=ctypes.CDLL(str(pathlib.Path(__file__).parent/'callback.so'))
library.begin.argtypes=[ctypes.c_char_p];library.begin.restype=ctypes.c_int
library.take.restype=ctypes.c_char_p
state='1234567890abcdef1234567890abcdef'
def start():
 port=library.begin(state.encode());assert port>0;return port
def request(port,method,path='/',body='',headers=None):
 connection=http.client.HTTPConnection('127.0.0.1',port,timeout=4)
 connection.request(method,path,body,headers or {})
 response=connection.getresponse();data=response.read();code=response.status;connection.close();return code,data
port=start()
assert request(port,'GET','/')[0]==200
assert request(port,'OPTIONS','/',headers={'Origin':'https://devecostudio.huawei.com','Access-Control-Request-Headers':'content-type,x-requested-with'})[0]==204
assert request(port,'GET','/?code=bad&tempToken=probe')[0]==400
assert json.loads(library.take())=={'pending':True}
assert request(port,'GET','/?code=wrong&tempToken=probe',headers={'Origin':'https://attacker.invalid'})[0]==400
assert json.loads(library.take())=={'pending':True}
for kind in ['query','form','json','multipart']:
 port=start();fields={'code':state,'tempToken':'unit-test-only-token','siteId':'1'}
 if kind=='query':code,_=request(port,'GET','/callback?'+urllib.parse.urlencode(fields))
 elif kind=='form':code,_=request(port,'POST','/',urllib.parse.urlencode(fields),{'Content-Type':'application/x-www-form-urlencoded','Origin':'null','Host':'localhost:'+str(port)})
 elif kind=='json':code,_=request(port,'POST','/',json.dumps({'data':fields}),{'Content-Type':'application/json'})
 else:
  body=''.join('--BOUNDARY\r\nContent-Disposition: form-data; name="'+key+'"\r\n\r\n'+value+'\r\n' for key,value in fields.items())+'--BOUNDARY--\r\n'
  code,_=request(port,'POST','/',body,{'Content-Type':'multipart/form-data; boundary=BOUNDARY'})
 assert code==302,(kind,code)
 assert json.loads(library.take())=={'tempToken':'unit-test-only-token','siteId':'1'}
 assert json.loads(library.take())=={'pending':True},'callback may be consumed only once'
library.cancel();time.sleep(.3)
print('PASS: loopback binding, CORS preflight, CSRF rejection, opaque/null-origin POST compatibility, GET/form/JSON/multipart callbacks, one-time consumption and cancellation')
'''
wrapper = '''#include <string>
extern int ArkForgeStartHuaweiCallback(const char*);
extern std::string ArkForgeTakeHuaweiCallback();
extern void ArkForgeCancelHuaweiCallback();
extern "C" int begin(const char*s){return ArkForgeStartHuaweiCallback(s);}
extern "C" const char* take(){static thread_local std::string result;result=ArkForgeTakeHuaweiCallback();return result.c_str();}
extern "C" void cancel(){ArkForgeCancelHuaweiCallback();}
'''
with tempfile.TemporaryDirectory(prefix='arkforge-huawei-callback-') as directory:
 root=pathlib.Path(directory)
 shutil.copyfile(app/'native/resources/huawei_callback.cpp',root/'callback.cpp')
 vendor=app.parents[2]/'_vendor/cJSON'
 for name in ['cJSON.c','cJSON.h']:shutil.copyfile(vendor/name,root/name)
 (root/'wrapper.cpp').write_text(wrapper)
 (root/'check.py').write_text(test)
 if os.name=='nt':location='/mnt/'+root.drive[0].lower()+root.as_posix()[2:];prefix=['wsl','-d','Ubuntu','--','bash','-c']
 else:location=str(root);prefix=['bash','-c']
 script='cd '+shlex.quote(location)+' && cc -fPIC -c cJSON.c -o json.o && g++ -std=c++17 -shared -fPIC -I. callback.cpp wrapper.cpp json.o -pthread -o callback.so && python3 check.py'
 result=subprocess.run(prefix+[script],capture_output=True)
 print(result.stdout.decode('utf-8',errors='replace'))
 if b'PASS:' not in result.stdout:
  print(result.stderr.decode('utf-8',errors='replace'));raise SystemExit(1)
