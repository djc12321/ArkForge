#include <napi/native_api.h>
#include "params_run_tool.h"
#include <mutex>
#include <string>
#include <vector>
#include <cstdio>
#include <hilog/log.h>
#include <zip.h>
#include <unzip.h>
#include <fstream>
#include <stdexcept>
#include <set>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/provider.h>

// The reference device adapter uses this optional, independent trace stream.
FILE* g_signTrace = nullptr;

namespace {
std::string StringArgument(napi_env env, napi_value value)
{
    size_t length = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok || length > 4096)
        throw std::runtime_error("Invalid package path");
    std::string text(length + 1, '\0');
    napi_get_value_string_utf8(env, value, text.data(), text.size(), &length);
    text.resize(length);
    return text;
}
void AddZipEntry(zipFile output, const std::string &name, const std::vector<char> &bytes)
{
    zip_fileinfo metadata {};
    if (zipOpenNewFileInZip64(output, name.c_str(), &metadata, nullptr, 0, nullptr, 0,
        nullptr, 0, 0, 0) != ZIP_OK)
        throw std::runtime_error("Cannot create HAP entry: " + name);
    const int written = zipWriteInFileInZip(output, bytes.data(), bytes.size());
    const int closed = zipCloseFileInZip(output);
    if (written != ZIP_OK || closed != ZIP_OK) throw std::runtime_error("Cannot write HAP entry: " + name);
}
napi_value PackHap(napi_env env, napi_callback_info info)
{
    size_t count = 3;
    napi_value arguments[3];
    napi_get_cb_info(env, info, &count, arguments, nullptr, nullptr);
    unzFile input = nullptr;
    zipFile output = nullptr;
    std::string destination;
    try {
        if (count != 3) throw std::runtime_error("packHap requires resource cache, ABC, and output paths");
        const auto cache = StringArgument(env, arguments[0]);
        const auto abcPath = StringArgument(env, arguments[1]);
        destination = StringArgument(env, arguments[2]);
        if (cache == destination || abcPath == destination) throw std::runtime_error("Output must be a new file");
        std::ifstream abcFile(abcPath, std::ios::binary | std::ios::ate);
        const auto size = abcFile.tellg();
        if (!abcFile || size < 16 || size > 64 * 1024 * 1024) throw std::runtime_error("Invalid compiled ABC");
        std::vector<char> abc(static_cast<size_t>(size));
        abcFile.seekg(0); abcFile.read(abc.data(), abc.size());
        if (!abcFile || std::string(abc.data(), 5) != "PANDA") throw std::runtime_error("Invalid ABC header");
        input = unzOpen64(cache.c_str());
        if (!input) throw std::runtime_error("Cannot open resource cache");
        output = zipOpen64(destination.c_str(), APPEND_STATUS_CREATE);
        if (!output) throw std::runtime_error("Cannot create HAP");
        std::set<std::string> names;
        int status = unzGoToFirstFile(input);
        while (status == UNZ_OK) {
            char filename[4096]; unz_file_info64 metadata {};
            if (unzGetCurrentFileInfo64(input, &metadata, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK ||
                metadata.size_filename >= sizeof(filename) || metadata.uncompressed_size > 64 * 1024 * 1024)
                throw std::runtime_error("Invalid resource cache entry");
            std::string name(filename);
            if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos ||
                name.find('\\') != std::string::npos || name.find(".abc") != std::string::npos ||
                !names.insert(name).second || names.size() > 4096)
                throw std::runtime_error("Unsafe or compiled-code entry in resource cache");
            std::vector<char> bytes(static_cast<size_t>(metadata.uncompressed_size));
            if (unzOpenCurrentFile(input) != UNZ_OK) throw std::runtime_error("Cannot read resource cache entry");
            size_t offset = 0; int read = 0;
            while (offset < bytes.size() && (read = unzReadCurrentFile(input, bytes.data() + offset,
                static_cast<unsigned>(bytes.size() - offset))) > 0) offset += read;
            const int crc = unzCloseCurrentFile(input);
            if (offset != bytes.size() || crc != UNZ_OK) throw std::runtime_error("Resource cache CRC failure");
            AddZipEntry(output, name, bytes);
            status = unzGoToNextFile(input);
        }
        if (status != UNZ_END_OF_LIST_OF_FILE || !names.count("module.json") || !names.count("resources.index"))
            throw std::runtime_error("Incomplete resource cache");
        AddZipEntry(output, "ets/modules.abc", abc);
        unzClose(input); input = nullptr;
        const int closed = zipClose(output, nullptr); output = nullptr;
        if (closed != ZIP_OK) throw std::runtime_error("Cannot finalize HAP");
        napi_value result; napi_get_undefined(env, &result); return result;
    } catch (const std::exception &error) {
        if (input) unzClose(input);
        if (output) zipClose(output, nullptr);
        if (!destination.empty()) unlink(destination.c_str());
        napi_throw_error(env, nullptr, error.what()); return nullptr;
    }
}
std::mutex signerMutex;
std::once_flag providerOnce;
struct Work {
    napi_async_work work = nullptr;
    napi_deferred deferred = nullptr;
    std::vector<std::string> args;
    int result = -1;
    std::string error;
};
void Execute(napi_env, void *data)
{
    auto *task = static_cast<Work *>(data);
    std::lock_guard<std::mutex> guard(signerMutex);
    std::vector<char *> args;
    for (auto &arg : task->args) args.push_back(arg.data());
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "ArkForgeNative", "Native signer entered");
    // The official PKCS12 writer uses RC2-40 for certificate bags. OpenSSL 3
    // keeps that format in its statically linked legacy provider. Retain both
    // providers for this process so existing official keystores remain readable.
    std::call_once(providerOnce, [] {
        OSSL_PROVIDER_load(nullptr, "default");
        OSSL_PROVIDER_load(nullptr, "legacy");
    });
    ERR_clear_error();
    task->result = OHOS::SignatureTools::ParamsRunTool::ProcessCmd(args.data(), args.size()) ? 0 : -1;
    if (task->result != 0) {
        while (unsigned long code = ERR_get_error()) {
            const char *reason = ERR_reason_error_string(code);
            if (reason && task->error.size() < 1000) {
                if (!task->error.empty()) task->error += "; ";
                task->error += reason; // Reason only: never return OpenSSL data or command passwords.
            }
        }
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "ArkForgeNative", "Native signer returned %{public}d", task->result);
}
void Complete(napi_env env, napi_status status, void *data)
{
    auto *task = static_cast<Work *>(data);
    napi_value code;
    napi_create_int32(env, status == napi_ok ? task->result : -2, &code);
    if (!task->error.empty()) {
        napi_value message, error;
        napi_create_string_utf8(env, task->error.c_str(), NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &error);
        napi_reject_deferred(env, task->deferred, error);
    } else napi_resolve_deferred(env, task->deferred, code);
    napi_delete_async_work(env, task->work);
    delete task;
}
napi_value Run(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    bool array = false;
    if (argc != 1 || napi_is_array(env, argv[0], &array) != napi_ok || !array) {
        napi_throw_type_error(env, nullptr, "runSigner requires a string array");
        return nullptr;
    }
    uint32_t size = 0;
    napi_get_array_length(env, argv[0], &size);
    if (size == 0 || size > 128) {
        napi_throw_range_error(env, nullptr, "Invalid signer argument count");
        return nullptr;
    }
    auto *task = new Work;
    task->args.emplace_back("hap-sign-tool");
    for (uint32_t i = 0; i < size; i++) {
        napi_value value;
        napi_get_element(env, argv[0], i, &value);
        size_t length = 0;
        if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok || length > 4096) {
            delete task;
            napi_throw_type_error(env, nullptr, "Invalid signer argument");
            return nullptr;
        }
        std::string argument(length + 1, '\0');
        napi_get_value_string_utf8(env, value, argument.data(), argument.size(), &length);
        argument.resize(length);
        task->args.push_back(std::move(argument));
    }
    napi_value promise, name;
    napi_create_promise(env, &task->deferred, &promise);
    napi_create_string_utf8(env, "ArkForgeSigner", NAPI_AUTO_LENGTH, &name);
    if (napi_create_async_work(env, nullptr, name, Execute, Complete, task, &task->work) != napi_ok) {
        napi_throw_error(env, nullptr, "Cannot create signer worker");
        delete task;
        return nullptr;
    }
    if (napi_queue_async_work(env, task->work) != napi_ok) {
        napi_delete_async_work(env, task->work);
        napi_throw_error(env, nullptr, "Cannot queue signer worker");
        delete task;
        return nullptr;
    }
    return promise;
}
napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor properties[] = {
        {"runSigner", nullptr, Run, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"packHap", nullptr, PackHap, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    napi_define_properties(env, exports, 2, properties);
    return exports;
}
}
static napi_module module = {1, 0, nullptr, Init, "signtool", nullptr, {0}};
extern "C" __attribute__((constructor)) void RegisterArkForgeSigner()
{
    napi_module_register(&module);
}
