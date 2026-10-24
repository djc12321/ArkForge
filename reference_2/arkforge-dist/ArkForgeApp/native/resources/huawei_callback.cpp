// Protocol adapted from guantw/HapSign (MIT); see third-party/HapSign-LICENSE.
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <cJSON.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {
std::mutex mutex;
int listener=-1;
unsigned generation=0;
std::string result;
std::string diagnostics="{}";
using Fields=std::map<std::string,std::string>;
std::string Decode(const std::string &value) {
    std::string out;
    auto hex=[](char c)->int {if(c>='0'&&c<='9')return c-'0';c=static_cast<char>(tolower(c));return c>='a'&&c<='f'?c-'a'+10:-1;};
    for(size_t i=0;i<value.size();i++) {
        if(value[i]=='+')out+=' ';
        else if(value[i]=='%'&&i+2<value.size()&&hex(value[i+1])>=0&&hex(value[i+2])>=0){out+=static_cast<char>(hex(value[i+1])*16+hex(value[i+2]));i+=2;}
        else out+=value[i];
    }
    return out;
}
void Form(const std::string &body,Fields &fields) {
    size_t start=0;
    while(start<body.size()) {
        size_t end=body.find('&',start);if(end==std::string::npos)end=body.size();
        size_t equal=body.find('=',start);
        if(equal<end)fields[Decode(body.substr(start,equal-start))]=Decode(body.substr(equal+1,end-equal-1));
        start=end+1;
    }
}
void Body(const std::string &body,const std::string &type,Fields &fields) {
    if(type.find("json")!=std::string::npos) {
        cJSON *json=cJSON_Parse(body.c_str());if(!json)return;
        cJSON *data=cJSON_GetObjectItemCaseSensitive(json,"data");if(!cJSON_IsObject(data))data=json;
        for(const char *key:{"code","tempToken","siteId","quit"}){auto *v=cJSON_GetObjectItemCaseSensitive(data,key);if(cJSON_IsString(v))fields[key]=v->valuestring;else if(!strcmp(key,"siteId")&&cJSON_IsNumber(v))fields[key]=std::to_string(v->valueint);}
        cJSON_Delete(json);
    } else if(body.rfind("--",0)==0) {
        size_t line=body.find("\r\n");if(line==std::string::npos)return;
        std::string boundary=body.substr(0,line);size_t position=0;
        while((position=body.find(boundary,position))!=std::string::npos) {
            size_t headers=body.find("\r\n\r\n",position),end=body.find(boundary,position+boundary.size());
            if(headers==std::string::npos||end==std::string::npos)break;
            size_t name=body.find("name=\"",position),quote=name==std::string::npos?name:body.find('"',name+6);
            if(name<headers&&quote<headers&&end>=headers+6)fields[body.substr(name+6,quote-name-6)]=body.substr(headers+4,end-headers-6);
            position=end;
        }
    } else Form(body,fields);
}
bool Equal(const std::string &a,const std::string &b) {
    if(a.size()!=b.size())return false;unsigned difference=0;
    for(size_t i=0;i<a.size();i++)difference|=static_cast<unsigned char>(a[i]^b[i]);return difference==0;
}
void Reply(int fd,int status,const std::string &origin,const std::string &body,const std::string &requestedHeaders="Content-Type",const std::string &location="") {
    std::string response="HTTP/1.1 "+std::to_string(status)+" Result\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Type: text/html; charset=utf-8\r\n";
    if(!origin.empty())response+="Access-Control-Allow-Origin: "+origin+"\r\nVary: Origin\r\nAccess-Control-Allow-Credentials: true\r\n";
    if(!location.empty())response+="Location: "+location+"\r\n";
    response+="Access-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: "+requestedHeaders+"\r\nAccess-Control-Allow-Private-Network: true\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
    {std::lock_guard<std::mutex> lock(mutex);cJSON *json=cJSON_Parse(diagnostics.c_str());if(!json)json=cJSON_CreateObject();cJSON_AddNumberToObject(json,"status",status);char *encoded=cJSON_PrintUnformatted(json);diagnostics=encoded?encoded:"{}";cJSON_free(encoded);cJSON_Delete(json);}
    size_t sent=0;while(sent<response.size()){ssize_t n=send(fd,response.data()+sent,response.size()-sent,MSG_NOSIGNAL);if(n<=0)break;sent+=n;}
}
bool Serve(int fd,const std::string &state,int port,unsigned id) {
    timeval timeout{3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    std::string request;char bytes[4096];size_t headers=std::string::npos;
    while(request.size()<80*1024&&(headers=request.find("\r\n\r\n"))==std::string::npos){ssize_t n=recv(fd,bytes,sizeof(bytes),0);if(n<=0)return false;request.append(bytes,n);}
    if(headers==std::string::npos){Reply(fd,413,"","Request too large");return false;}
    size_t first=request.find("\r\n");std::string headline=request.substr(0,first);Fields values;
    for(size_t p=first+2;p<headers;){size_t end=request.find("\r\n",p),colon=request.find(':',p);if(colon<end){std::string key=request.substr(p,colon-p);std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return tolower(c);});size_t begin=colon+1;while(begin<end&&request[begin]==' ')begin++;values[key]=request.substr(begin,end-begin);}p=end+2;}
    std::string origin=values["origin"],host=values["host"];
    {std::lock_guard<std::mutex> lock(mutex);cJSON *json=cJSON_CreateObject();cJSON_AddStringToObject(json,"method",headline.substr(0,headline.find(' ')).c_str());cJSON_AddStringToObject(json,"origin",origin.substr(0,160).c_str());cJSON_AddStringToObject(json,"host",host.substr(0,160).c_str());cJSON_AddStringToObject(json,"requestedHeaders",values["access-control-request-headers"].substr(0,160).c_str());char *encoded=cJSON_PrintUnformatted(json);diagnostics=encoded?encoded:"{}";cJSON_free(encoded);cJSON_Delete(json);}
    // Match HapSign's browser protocol: sandboxed OAuth form posts can have a
    // null/opaque Origin. Origin is not an authentication signal here; the
    // loopback Host and unguessable per-session code below are the boundary.
    if(host!="127.0.0.1:"+std::to_string(port)&&host!="localhost:"+std::to_string(port)){Reply(fd,403,"","Invalid callback host");return false;}
    if(headline.rfind("OPTIONS ",0)==0){Reply(fd,204,origin,"",values["access-control-request-headers"].empty()?"Content-Type":values["access-control-request-headers"]);return false;}
    if(headline.rfind("GET ",0)!=0&&headline.rfind("POST ",0)!=0){Reply(fd,405,origin,"Unsupported method");return false;}
    std::string length=values["content-length"];
    if(length.size()>6||(!length.empty()&&length.find_first_not_of("0123456789")!=std::string::npos)){Reply(fd,400,origin,"Invalid length");return false;}
    size_t count=length.empty()?0:static_cast<size_t>(strtoul(length.c_str(),nullptr,10));
    if(count>65536){Reply(fd,413,origin,"Request too large");return false;}
    while(request.size()<headers+4+count){ssize_t n=recv(fd,bytes,sizeof(bytes),0);if(n<=0)return false;request.append(bytes,n);}
    Fields fields;size_t question=headline.find('?'),space=headline.find(' ',question);
    if(question!=std::string::npos)Form(headline.substr(question+1,space-question-1),fields);
    Body(request.substr(headers+4,count),values["content-type"],fields);
    // DevEco probes the listener before posting credentials. HapSign responds
    // successfully to these GET probes, without consuming the login session.
    if(headline.rfind("GET ",0)==0&&!fields.count("code")&&!fields.count("tempToken")){Reply(fd,200,origin,"ArkForge callback server is ready");return false;}
    if(!Equal(fields["code"],state)){Reply(fd,400,origin,"Invalid login session");return false;}
    cJSON *json=cJSON_CreateObject();
    if(fields["tempToken"].empty())cJSON_AddStringToObject(json,"error","Huawei authorization cancelled or missing temporary token");
    else {cJSON_AddStringToObject(json,"tempToken",fields["tempToken"].c_str());cJSON_AddStringToObject(json,"siteId",fields["siteId"].c_str());}
    char *encoded=cJSON_PrintUnformatted(json);cJSON_Delete(json);
    {std::lock_guard<std::mutex> lock(mutex);if(id!=generation){cJSON_free(encoded);return true;}result=encoded?encoded:"{}";}
    cJSON_free(encoded);
    Reply(fd,302,origin,"Huawei authorization received. Return to ArkForge.","Content-Type","https://devecostudio.huawei.com/console/DevEcoIDE/loginSuccess");
    std::fill(request.begin(),request.end(),0);return true;
}
}
void ArkForgeCancelHuaweiCallback() {
    std::lock_guard<std::mutex> lock(mutex);generation++;if(listener>=0)shutdown(listener,SHUT_RDWR);listener=-1;
    std::fill(result.begin(),result.end(),0);result.clear();
}
int ArkForgeStartHuaweiCallback(const char *state) {
    if(!state||strlen(state)!=32)return -1;
    ArkForgeCancelHuaweiCallback();int fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0)return -1;
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(fd,reinterpret_cast<sockaddr *>(&address),sizeof(address))||listen(fd,8)){close(fd);return -1;}
    socklen_t size=sizeof(address);getsockname(fd,reinterpret_cast<sockaddr *>(&address),&size);int port=ntohs(address.sin_port);unsigned id;
    {std::lock_guard<std::mutex> lock(mutex);listener=fd;id=generation;}
    std::thread([fd,port,id,code=std::string(state)]{
        auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
        while(std::chrono::steady_clock::now()<deadline){
            {std::lock_guard<std::mutex> lock(mutex);if(generation!=id)break;}
            pollfd wait{fd,POLLIN,0};if(poll(&wait,1,200)<=0)continue;
            int peer=accept4(fd,nullptr,nullptr,SOCK_CLOEXEC);if(peer<0)break;
            bool done=Serve(peer,code,port,id);close(peer);if(done)break;
        }
        {std::lock_guard<std::mutex> lock(mutex);if(generation==id)listener=-1;}
        close(fd);
    }).detach();return port;
}
std::string ArkForgeTakeHuaweiCallback() {
    std::lock_guard<std::mutex> lock(mutex);if(result.empty())return "{\"pending\":true}";
    std::string copy=result;std::fill(result.begin(),result.end(),0);result.clear();return copy;
}
std::string ArkForgeHuaweiCallbackDiagnostics() {std::lock_guard<std::mutex> lock(mutex);return diagnostics;}
