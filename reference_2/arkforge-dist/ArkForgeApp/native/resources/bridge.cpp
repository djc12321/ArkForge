#include <napi/native_api.h>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <vector>
#include <cstring>
#include <unzip.h>
#include <set>

static napi_value ValidateArchive(napi_env env, napi_callback_info info)
{
    size_t count = 1, length = 0; napi_value argument, result;
    char path[4096];
    napi_get_cb_info(env, info, &count, &argument, nullptr, nullptr);
    if (count != 1 || napi_get_value_string_utf8(env, argument, path, sizeof(path), &length) != napi_ok || length >= sizeof(path)-1) {
        napi_throw_error(env, nullptr, "Invalid archive path"); return nullptr;
    }
    unzFile archive = unzOpen64(path);
    if (!archive) { napi_throw_error(env, nullptr, "Cannot open ZIP archive"); return nullptr; }
    std::set<std::string> names; uint64_t bytes = 0;
    const char *failure = nullptr;
    int status = unzGoToFirstFile(archive);
    while (status == UNZ_OK) {
        unz_file_info64 metadata {}; char filename[4096];
        if (unzGetCurrentFileInfo64(archive, &metadata, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK || metadata.size_filename >= sizeof(filename)) { failure="Invalid ZIP directory"; break; }
        std::string name(filename, metadata.size_filename);
        if (name.empty() || name.front()=='/' || name.find('\\')!=std::string::npos || name.find(':')!=std::string::npos || name.find('\0')!=std::string::npos ||
            name==".." || name.rfind("../",0)==0 || name.find("/../")!=std::string::npos || (name.size()>=3 && name.compare(name.size()-3,3,"/..")==0) ||
            !names.insert(name).second || ((metadata.external_fa>>16)&0170000)==0120000) { failure="Unsafe or duplicate ZIP path"; break; }
        bytes += metadata.uncompressed_size;
        if (names.size()>50000 || bytes>1024ULL*1024*1024 || metadata.uncompressed_size>128ULL*1024*1024 || metadata.flag&1) { failure="ZIP exceeds project limits or is encrypted"; break; }
        status = unzGoToNextFile(archive);
    }
    if (!failure && status != UNZ_END_OF_LIST_OF_FILE) failure="Invalid ZIP directory";
    unzClose(archive);
    if (failure) { napi_throw_error(env, nullptr, failure); return nullptr; }
    napi_get_undefined(env, &result); return result;
}

static std::mutex resourceMutex;
static napi_value Compile(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argument, result;
    napi_get_cb_info(env, info, &argc, &argument, nullptr, nullptr);
    bool array = false;
    if (argc != 1 || napi_is_array(env, argument, &array) != napi_ok || !array) {
        napi_throw_type_error(env, nullptr, "restool arguments must be an array"); return nullptr;
    }
    uint32_t count = 0;
    napi_get_array_length(env, argument, &count);
    if (count < 2 || count > 128) { napi_throw_range_error(env, nullptr, "Invalid restool argument count"); return nullptr; }
    std::vector<std::string> strings(count);
    for (uint32_t i = 0; i < count; i++) {
        napi_value item; size_t length = 0;
        napi_get_element(env, argument, i, &item);
        if (napi_get_value_string_utf8(env, item, nullptr, 0, &length) != napi_ok || length > 32768) {
            napi_throw_type_error(env, nullptr, "Invalid restool argument"); return nullptr;
        }
        strings[i].resize(length + 1);
        napi_get_value_string_utf8(env, item, strings[i].data(), length + 1, &length);
        strings[i].resize(length);
        // Upstream CLI exits for these informational switches; never expose process exit.
        if (strings[i] == "-v" || strings[i] == "--version" || strings[i] == "-h" || strings[i] == "--help") {
            napi_throw_error(env, nullptr, "Informational CLI switches are unavailable in the embedded compiler"); return nullptr;
        }
    }
    std::vector<char *> argv;
    for (auto &text : strings) argv.push_back(text.data());
    argv.push_back(nullptr);
    std::lock_guard<std::mutex> lock(resourceMutex);
    void *library = dlopen("libarkforge_resource_core.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) { napi_throw_error(env, nullptr, dlerror()); return nullptr; }
    using Run = int (*)(int, char **);
    auto run = reinterpret_cast<Run>(dlsym(library, "ArkForgeResourceRun"));
    if (!run) { dlclose(library); napi_throw_error(env, nullptr, "restool entry unavailable"); return nullptr; }
    int status = run(static_cast<int>(count), argv.data());
    dlclose(library);
    napi_create_int32(env, status, &result);
    return result;
}
extern "C" int ArkForgeStartWorkbenchServer(const char *root);
static napi_value StartWorkbench(napi_env env, napi_callback_info info)
{
    napi_value argument, result; size_t argc = 1, length = 0; char root[4096];
    napi_get_cb_info(env, info, &argc, &argument, nullptr, nullptr);
    if (argc != 1 || napi_get_value_string_utf8(env, argument, root, sizeof(root), &length) != napi_ok || length >= sizeof(root)-1) {
        napi_throw_error(env, nullptr, "Invalid workbench asset directory"); return nullptr;
    }
    int port = ArkForgeStartWorkbenchServer(root);
    if (port < 0) { napi_throw_error(env, nullptr, "Cannot start loopback workbench asset server"); return nullptr; }
    napi_create_int32(env, port, &result); return result;
}
extern int ArkForgeStartHuaweiCallback(const char *state);
extern std::string ArkForgeTakeHuaweiCallback();
extern void ArkForgeCancelHuaweiCallback();
extern std::string ArkForgeHuaweiCallbackDiagnostics();
static napi_value StartHuaweiCallback(napi_env env,napi_callback_info info) {
    size_t count=1,length=0;char state[33]{};napi_value argument,result;
    napi_get_cb_info(env,info,&count,&argument,nullptr,nullptr);
    if(count!=1||napi_get_value_string_utf8(env,argument,state,sizeof(state),&length)!=napi_ok||length!=32){napi_throw_error(env,nullptr,"Invalid login state");return nullptr;}
    int port=ArkForgeStartHuaweiCallback(state);if(port<0){napi_throw_error(env,nullptr,"Cannot start local Huawei callback");return nullptr;}
    napi_create_int32(env,port,&result);return result;
}
static napi_value TakeHuaweiCallback(napi_env env,napi_callback_info) {
    auto text=ArkForgeTakeHuaweiCallback();napi_value result;napi_create_string_utf8(env,text.c_str(),text.size(),&result);return result;
}
static napi_value HuaweiCallbackDiagnostics(napi_env env,napi_callback_info) {
    auto text=ArkForgeHuaweiCallbackDiagnostics();napi_value result;napi_create_string_utf8(env,text.c_str(),text.size(),&result);return result;
}
static napi_value CancelHuaweiCallback(napi_env env,napi_callback_info) {
    ArkForgeCancelHuaweiCallback();napi_value result;napi_get_undefined(env,&result);return result;
}
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor properties[] = {
      {"startHuaweiCallback", nullptr, StartHuaweiCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"takeHuaweiCallback", nullptr, TakeHuaweiCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"huaweiCallbackDiagnostics", nullptr, HuaweiCallbackDiagnostics, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"cancelHuaweiCallback", nullptr, CancelHuaweiCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"startWorkbench", nullptr, StartWorkbench, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"compile", nullptr, Compile, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"validateArchive", nullptr, ValidateArchive, nullptr, nullptr, nullptr, napi_default, nullptr}};
    napi_define_properties(env, exports, sizeof(properties)/sizeof(properties[0]), properties);
    return exports;
}
static napi_module module = {1, 0, nullptr, Init, "arkforge_resources", nullptr, {0}};
extern "C" __attribute__((constructor)) void RegisterResourceCompiler() { napi_module_register(&module); }
