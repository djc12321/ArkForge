// In-process entry to the unmodified ArkTS-Dyn compiler CLI orchestration.
#include <napi/native_api.h>
#include <mutex>
#include <string>
#include <vector>

int ArkForgeDynamicMain(int argc, const char **argv);
static std::mutex compilerMutex;

static napi_value Compile(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value input;
    napi_get_cb_info(env, info, &argc, &input, nullptr, nullptr);
    bool array = false;
    if (argc != 1 || napi_is_array(env, input, &array) != napi_ok || !array) {
        napi_throw_type_error(env, nullptr, "Compiler arguments must be a string array");
        return nullptr;
    }
    uint32_t count = 0;
    napi_get_array_length(env, input, &count);
    if (count == 0 || count > 128) {
        napi_throw_range_error(env, nullptr, "Invalid compiler argument count");
        return nullptr;
    }
    std::vector<std::string> storage;
    storage.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        napi_value item;
        napi_get_element(env, input, i, &item);
        size_t length = 0;
        if (napi_get_value_string_utf8(env, item, nullptr, 0, &length) != napi_ok || length > 16384) {
            napi_throw_type_error(env, nullptr, "Invalid compiler argument");
            return nullptr;
        }
        std::vector<char> buffer(length + 1);
        napi_get_value_string_utf8(env, item, buffer.data(), buffer.size(), &length);
        storage.emplace_back(buffer.data(), length);
    }
    std::vector<const char *> argv;
    for (const auto &item : storage) argv.push_back(item.c_str());
    int status;
    try {
        std::lock_guard<std::mutex> lock(compilerMutex);
        status = ArkForgeDynamicMain(static_cast<int>(argv.size()), argv.data());
    } catch (const std::exception &error) {
        napi_throw_error(env, nullptr, error.what());
        return nullptr;
    }
    napi_value result;
    napi_create_int32(env, status, &result);
    return result;
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor api[] = {
        {"compile", nullptr, Compile, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    napi_define_properties(env, exports, 2, api);
    return exports;
}
static napi_module module = {1, 0, nullptr, Init, "arkforge_dynamic", nullptr, {0}};
extern "C" __attribute__((constructor)) void RegisterDynamicCompiler()
{
    napi_module_register(&module);
}
