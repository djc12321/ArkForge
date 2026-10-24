/*
 * es2abc_napi.c —— ArkTS 静态编译器的 NAPI 桥接层
 *
 * 重要：这个文件基于「实际编译并运行成功」的官方公开 C API 编写，
 *       取代早期按旧版头文件臆测的 es2abc_napi.cpp。
 *
 * Linux x86_64 C API 实测结论（2026-10-21；不代表真机 NAPI 已通过）：
 *   libes2panda-public.so 2.8MB + libes2panda-lib.so 14MB
 *   调用 es2panda_GetImpl(1) -> MemInitialize -> CreateConfig(argc,argv)
 *        -> CreateContextFromFile -> ProceedToState(BIN_GENERATED)
 *   最终状态 6 (ES2PANDA_STATE_BIN_GENERATED)，产出含 PANDA 魔数的 .abc
 *   全程无 fork、无 exec。
 *
 * 为什么用 C API 而不是 C++ API：
 *   - 纯 C（extern "C"），无 C++ ABI / 异常跨库问题，NAPI 最稳
 *   - 自带 MemInitialize / MemFinalize，不用自己管 Arena 内存池
 *   - CreateConfig 直接吃 es2abc 的命令行参数，迁移成本最低
 */

#include <napi/native_api.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <es2panda_lib.h>

static const struct es2panda_Impl *g_impl = NULL;

static bool StringArgs(napi_env env, size_t count, napi_value *args)
{
    for (size_t i = 0; i < count; ++i) {
        napi_valuetype type;
        if (napi_typeof(env, args[i], &type) != napi_ok || type != napi_string) {
            return false;
        }
    }
    return true;
}

/* ---- 结果对象的小工具（C 里不能用 lambda，写成静态函数） ---- */
static void ResSetStr(napi_env env, napi_value obj, const char *key, const char *val)
{
    napi_value s = NULL;
    napi_create_string_utf8(env, val != NULL ? val : "", NAPI_AUTO_LENGTH, &s);
    napi_set_named_property(env, obj, key, s);
}

static void ResSetBool(napi_env env, napi_value obj, const char *key, bool val)
{
    napi_value b = NULL;
    napi_get_boolean(env, val, &b);
    napi_set_named_property(env, obj, key, b);
}

static void ResSetInt(napi_env env, napi_value obj, const char *key, int32_t val)
{
    napi_value n = NULL;
    napi_create_int32(env, val, &n);
    napi_set_named_property(env, obj, key, n);
}

/* ------------------------------------------------------------------ *
 * 惰性初始化：第一次调用时才加载实现并初始化内存子系统
 * ------------------------------------------------------------------ */
static const struct es2panda_Impl *EnsureImpl(void)
{
    if (g_impl == NULL) {
        g_impl = es2panda_GetImpl(ES2PANDA_LIB_VERSION);
        if (g_impl != NULL) {
            g_impl->MemInitialize();
        }
    }
    return g_impl;
}

/* ================================================================== *
 * compileArkTS(inputPath, outputPath, execPath) -> result
 *
 * execPath 用于让引擎定位 arktsconfig.json 与 etsstdlib 等内置资源，
 * 必须指向真实的 es2abc 所在目录，不能随便填。
 * ================================================================== */
static napi_value CompileArkTS(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value argv[3] = {NULL, NULL, NULL};
    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    napi_value result = NULL;
    napi_create_object(env, &result);

#define SetStr(k, v) ResSetStr(env, result, (k), (v))
#define SetBool(k, v) ResSetBool(env, result, (k), (v))
#define SetInt(k, v) ResSetInt(env, result, (k), (v))

    if (argc != 3 || !StringArgs(env, 3, argv)) {
        SetBool("ok", false);
        SetStr("error", "compileArkTS requires inputPath, outputPath, execPath strings");
        return result;
    }

    const struct es2panda_Impl *impl = EnsureImpl();
    if (impl == NULL) {
        SetBool("ok", false);
        SetStr("error", "无法获取 es2panda 实现");
        return result;
    }

    char inputPath[1024] = {0};
    char outputPath[1024] = {0};
    char execPath[1024] = {0};
    size_t len = 0;
    if (argc > 0 && argv[0] != NULL) {
        napi_get_value_string_utf8(env, argv[0], inputPath, sizeof(inputPath), &len);
    }
    if (argc > 1 && argv[1] != NULL) {
        napi_get_value_string_utf8(env, argv[1], outputPath, sizeof(outputPath), &len);
    }
    if (argc > 2 && argv[2] != NULL) {
        napi_get_value_string_utf8(env, argv[2], execPath, sizeof(execPath), &len);
    }

    unlink(outputPath);
    const char *cliArgs[] = {execPath, inputPath, "--output", outputPath};
    es2panda_Config *config = impl->CreateConfig(4, cliArgs);
    if (config == NULL) {
        SetBool("ok", false);
        SetStr("error", "CreateConfig 失败（检查 execPath 与参数）");
        return result;
    }

    es2panda_Context *context = impl->CreateContextFromFile(config, inputPath);
    if (context == NULL) {
        impl->DestroyConfig(config);
        SetBool("ok", false);
        SetStr("error", "CreateContextFromFile 失败");
        return result;
    }

    /* 一路推进到「二进制已生成」。注意：返回的 context 会替换掉旧的（旧的被消费） */
    context = impl->ProceedToState(context, ES2PANDA_STATE_BIN_GENERATED);

    if (context == NULL) {
        impl->DestroyConfig(config);
        SetBool("ok", false);
        SetStr("error", "ProceedToState returned NULL");
        return result;
    }
    es2panda_ContextState state = impl->ContextState(context);
    bool failed = impl->IsAnyError(context) || state != ES2PANDA_STATE_BIN_GENERATED;

    SetBool("ok", !failed);
    SetInt("state", (int32_t)state);
    SetStr("output", outputPath);

    if (failed) {
        const char *msg = impl->ContextErrorMessage(context);
        SetStr("error", msg != NULL ? msg : "编译失败（无详细信息）");
    }

    impl->DestroyContext(context);
    impl->DestroyConfig(config);
    if (failed) unlink(outputPath);
    return result;
}

/* ================================================================== *
 * compileArkTSSource(source, fileName, outputPath, execPath) -> result
 *
 * 直接吃内存里的源码字符串 —— 这是 Agent 场景最有价值的形态：
 * 模型吐出代码 -> 不落盘直接编译 -> 拿结果。
 * ================================================================== */
static napi_value CompileArkTSSource(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value argv[4] = {NULL, NULL, NULL, NULL};
    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    if (argc != 4 || !StringArgs(env, 4, argv)) {
        napi_throw_type_error(env, NULL, "compileArkTSSource requires four string arguments");
        return NULL;
    }

    napi_value result = NULL;
    napi_create_object(env, &result);

#define SetStr(k, v) ResSetStr(env, result, (k), (v))
#define SetBool(k, v) ResSetBool(env, result, (k), (v))

    const struct es2panda_Impl *impl = EnsureImpl();
    if (impl == NULL) {
        SetBool("ok", false);
        SetStr("error", "无法获取 es2panda 实现");
        return result;
    }

    size_t srcLen = 0;
    napi_get_value_string_utf8(env, argv[0], NULL, 0, &srcLen);
    char *source = (char *)malloc(srcLen + 1);
    if (source == NULL) {
        SetBool("ok", false);
        SetStr("error", "Out of memory");
        return result;
    }
    napi_get_value_string_utf8(env, argv[0], source, srcLen + 1, &srcLen);

    size_t len = 0;
    char fileName[512] = {0};
    char outputPath[1024] = {0};
    char execPath[1024] = {0};
    napi_get_value_string_utf8(env, argv[1], fileName, sizeof(fileName), &len);
    napi_get_value_string_utf8(env, argv[2], outputPath, sizeof(outputPath), &len);
    napi_get_value_string_utf8(env, argv[3], execPath, sizeof(execPath), &len);

    unlink(outputPath);
    const char *cliArgs[] = {execPath, fileName, "--output", outputPath};
    es2panda_Config *config = impl->CreateConfig(4, cliArgs);
    if (config == NULL) {
        free(source);
        SetBool("ok", false);
        SetStr("error", "CreateConfig 失败");
        return result;
    }

    es2panda_Context *context = impl->CreateContextFromString(config, source, fileName);
    free(source);

    if (context == NULL) {
        impl->DestroyConfig(config);
        SetBool("ok", false);
        SetStr("error", "CreateContextFromString 失败");
        return result;
    }

    context = impl->ProceedToState(context, ES2PANDA_STATE_BIN_GENERATED);

    if (context == NULL) {
        impl->DestroyConfig(config);
        SetBool("ok", false);
        SetStr("error", "ProceedToState returned NULL");
        return result;
    }
    es2panda_ContextState state = impl->ContextState(context);
    bool failed = impl->IsAnyError(context) || state != ES2PANDA_STATE_BIN_GENERATED;

    SetBool("ok", !failed);
    SetStr("output", outputPath);
    if (failed) {
        const char *msg = impl->ContextErrorMessage(context);
        SetStr("error", msg != NULL ? msg : "编译失败");
    }

    impl->DestroyContext(context);
    impl->DestroyConfig(config);
    return result;
}

/* ================================================================== *
 * 模块注册
 * ================================================================== */
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"compileArkTS", NULL, CompileArkTS, NULL, NULL, NULL, napi_default, NULL},
        {"compileArkTSSource", NULL, CompileArkTSSource, NULL, NULL, NULL, napi_default, NULL},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

static napi_module g_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = NULL,
    .nm_register_func = Init,
    .nm_modname = "es2abc",
    .nm_priv = NULL,
    .reserved = {NULL},
};

__attribute__((constructor)) void RegisterEs2abcModule(void)
{
    napi_module_register(&g_module);
}
