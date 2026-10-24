/*
 * jsvm_bridge.c —— 方案甲核心：把脚本引擎通过 NAPI 暴露给 ArkTS
 *
 * 作用：ArkTS 侧调用 evalScript(code)，代码在 Native 层的 JS 引擎中解释执行，
 *      结果（返回值 / console 输出 / UI 描述 / 错误）以 JSON 形式返回。
 *
 * 关键点：全程在 .so 内部完成，不 fork、不 exec 任何外部可执行文件，
 *        这是本方案能够在 HarmonyOS 严格的进程策略下工作的根本原因。
 *
 * 构建：
 *   真机   —— 用 OHOS SDK 提供的 clang + sysroot 编译（见 CMakeLists.txt）
 *   沙箱   —— 用 aarch64-linux-musl 工具链做交叉编译验证（见 build_sandbox.sh）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef SANDBOX_VERIFY
#include "napi_stub.h"
#else
#include "napi/native_api.h"
#endif

#include "quickjs.h"

#define LOG_CAP   (256 * 1024)   /* console 输出缓冲区上限 */
#define UI_CAP    (1 << 20)      /* UI 描述 JSON 上限  1MB */
#define VAL_CAP   (64 * 1024)    /* 返回值 JSON 上限   64KB */
#define ERR_CAP   (16 * 1024)

/* 执行超时（毫秒）—— Playground 必须防止用户写出死循环 */
#define EXEC_TIMEOUT_MS   2000
/* 引擎内存上限 64MB，栈上限 1MB */
#define VM_MEMORY_LIMIT   (64 * 1024 * 1024)
#define VM_STACK_LIMIT    (1024 * 1024)

typedef struct {
    JSRuntime *rt;
    JSContext *ctx;
    int        inited;

    char       logs[LOG_CAP];
    size_t     logs_len;

    char       ui[UI_CAP];
    int        ui_valid;

    long       deadline_ms;
    int        timed_out;
} VmState;

static VmState g_vm = {0};

/* ------------------------------------------------------------------ */
/* 工具函数                                                            */
/* ------------------------------------------------------------------ */

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* 追加到日志缓冲区（尽力而为，超容量则丢弃） */
static void log_append(const char *s) {
    size_t len = strlen(s);
    if (g_vm.logs_len + len + 2 >= LOG_CAP) return;
    memcpy(g_vm.logs + g_vm.logs_len, s, len);
    g_vm.logs_len += len;
    g_vm.logs[g_vm.logs_len++] = '\n';
    g_vm.logs[g_vm.logs_len] = '\0';
}

/* ---------------- JSON 字符串序列化（返回用于嵌入 JSON 的合法串） ---------------- */

static char *json_escape(const char *src) {
    if (!src) return strdup("\"\"");
    size_t n = strlen(src);
    /* 最坏情况：每个字符都需要转义 → 6n + 2 + 1 */
    char *out = (char *)malloc(n * 6 + 4);
    if (!out) return strdup("\"\"");
    char *p = out;
    *p++ = '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        switch (c) {
            case '"':  *p++='\\'; *p++='"';  break;
            case '\\': *p++='\\'; *p++='\\'; break;
            case '\n': *p++='\\'; *p++='n';  break;
            case '\r': *p++='\\'; *p++='r';  break;
            case '\t': *p++='\\'; *p++='t';  break;
            case '\b': *p++='\\'; *p++='b';  break;
            case '\f': *p++='\\'; *p++='f';  break;
            default:
                if (c < 0x20) {
                    p += sprintf(p, "\\u%04x", c);
                } else {
                    *p++ = (char)c;
                }
        }
    }
    *p++ = '"';
    *p = '\0';
    return out;
}

/* 把 JSValue 转成 JSON 字符串（错误处理交给调用方） */
static const char *jsvalue_to_json(JSContext *ctx, JSValue val) {
    JSValue json = JS_JSONStringify(ctx, val, JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(json)) return NULL;
    const char *s = JS_ToCString(ctx, json);
    JS_FreeValue(ctx, json);
    return s;   /* 注意：调用方需在使用后 JS_FreeCString */
}

/* ------------------------------------------------------------------ */
/* 注入给 JS 的宿主对象：__host.log / __host.render                     */
/* ------------------------------------------------------------------ */

static JSValue host_log(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    if (argc == 0) {
        log_append("");
        return JS_UNDEFINED;
    }

    /*
     * 先把本次调用的所有参数拼成「一行」，再统一追加一次换行。
     * 否则 log_append() 每次都会补 \n，console.log('a','b')
     * 会被拆成三行——正确语义应当是 "a b\n"。
     */
    const char **parts = (const char **)malloc(sizeof(char *) * (size_t)argc);
    if (!parts) return JS_UNDEFINED;

    size_t total = 0;
    for (int i = 0; i < argc; i++) {
        parts[i] = JS_ToCString(ctx, argv[i]);
        if (parts[i]) total += strlen(parts[i]);
        if (i < argc - 1) total += 1;           /* 参数间的空格分隔符 */
    }

    char *line = (char *)malloc(total + 1);
    if (line) {
        char *p = line;
        for (int i = 0; i < argc; i++) {
            if (parts[i]) {
                size_t n = strlen(parts[i]);
                memcpy(p, parts[i], n);
                p += n;
            }
            if (i < argc - 1) *p++ = ' ';
        }
        *p = '\0';
        log_append(line);
        free(line);
    }

    for (int i = 0; i < argc; i++) {
        if (parts[i]) JS_FreeCString(ctx, parts[i]);
    }
    free(parts);
    return JS_UNDEFINED;
}

static JSValue host_render(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;
    JSValue json = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
    if (!JS_IsException(json)) {
        const char *s = JS_ToCString(ctx, json);
        if (s) {
            snprintf(g_vm.ui, UI_CAP, "%s", s);
            g_vm.ui_valid = 1;
            JS_FreeCString(ctx, s);
        }
    }
    JS_FreeValue(ctx, json);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/* VM 生命周期                                                         */
/* ------------------------------------------------------------------ */

static int interrupt_cb(JSRuntime *rt, void *opaque) {
    (void)rt;
    VmState *vm = (VmState *)opaque;
    if (now_ms() > vm->deadline_ms) {
        vm->timed_out = 1;
        return 1;               /* 非零值会让引擎中止执行 */
    }
    return 0;
}

/*
 * JS 引导代码：提供 console.* 与一个精简的 ui.* 描述 API。
 * 用户写的不是 ArkTS，而是这套 JS 子集；App 侧据此渲染真实 ArkUI 组件。
 */
static const char *BOOTSTRAP_JS =
    "var console = {\n"
    "  log: function(){ __host.log.apply(null, arguments); },\n"
    "  info: function(){ __host.log.apply(null, arguments); },\n"
    "  warn: function(){ __host.log.apply(null, arguments); },\n"
    "  error: function(){ __host.log.apply(null, arguments); }\n"
    "};\n"
    "var ui = {\n"
    "  text: function(v, o){ return __t('Text', v, o); },\n"
    "  button: function(v, o){ return __t('Button', v, o); },\n"
    "  input: function(o){ return __t('Input', null, o); },\n"
    "  image: function(o){ return __t('Image', null, o); },\n"
    "  column: function(children, o){ return {type:'Column', props:o||{}, children:children||[]}; },\n"
    "  row: function(children, o){ return {type:'Row', props:o||{}, children:children||[]}; },\n"
    "  stack: function(children, o){ return {type:'Stack', props:o||{}, children:children||[]}; }\n"
    "};\n"
    "function __t(type, value, props){ return {type:type, value:value, props:props||{}}; }\n"
    "function render(node){ __host.render(node); }\n"
    "Object.freeze(ui);\n";

static void ensure_vm(void) {
    if (g_vm.inited) return;

    g_vm.rt = JS_NewRuntime();
    if (!g_vm.rt) return;
    JS_SetMemoryLimit(g_vm.rt, VM_MEMORY_LIMIT);
    JS_SetMaxStackSize(g_vm.rt, VM_STACK_LIMIT);
    JS_SetInterruptHandler(g_vm.rt, interrupt_cb, &g_vm);

    g_vm.ctx = JS_NewContext(g_vm.rt);
    if (!g_vm.ctx) return;

    /* 注入 __host */
    JSValue global = JS_GetGlobalObject(g_vm.ctx);
    JSValue host   = JS_NewObject(g_vm.ctx);
    JS_SetPropertyStr(g_vm.ctx, host, "log",
                      JS_NewCFunction(g_vm.ctx, host_log, "log", 1));
    JS_SetPropertyStr(g_vm.ctx, host, "render",
                      JS_NewCFunction(g_vm.ctx, host_render, "render", 1));
    JS_SetPropertyStr(g_vm.ctx, global, "__host", host);
    JS_FreeValue(g_vm.ctx, global);

    /*
     * 注意：bootstrap 同样处在中断保护之下，必须先给它一份独立的超时预算。
     * 否则 deadline_ms 仍为初始值 0，判定 now_ms() > 0 恒成立，
     * 引导脚本会被瞬间判为超时（InternalError: interrupted）而无法注入 console/ui。
     */
    g_vm.deadline_ms = now_ms() + EXEC_TIMEOUT_MS;
    JSValue ret = JS_Eval(g_vm.ctx, BOOTSTRAP_JS, strlen(BOOTSTRAP_JS),
                          "<bootstrap>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        JSValue e = JS_GetException(g_vm.ctx);
        const char *s = JS_ToCString(g_vm.ctx, e);
        fprintf(stderr, "[jsvm] bootstrap failed: %s\n", s ? s : "?");
        if (s) JS_FreeCString(g_vm.ctx, s);
        JS_FreeValue(g_vm.ctx, e);
    }
    JS_FreeValue(g_vm.ctx, ret);

    g_vm.inited = 1;
}

static void reset_vm(void) {
    if (!g_vm.inited) return;
    JS_FreeContext(g_vm.ctx);
    JS_FreeRuntime(g_vm.rt);
    memset(&g_vm, 0, sizeof(VmState));
}

/* ------------------------------------------------------------------ */
/* 执行入口                                                            */
/* ------------------------------------------------------------------ */

/*
 * 执行一段 JS，返回 JSON：
 *   { "ok":bool, "value":string, "logs":string, "ui":object|null,
 *     "error":string, "timeout":bool, "timeMs":number }
 */
char *vm_eval(const char *code) {
    ensure_vm();
    if (!g_vm.inited) {
        return strdup("{\"ok\":false,\"error\":\"VM 初始化失败\"}");
    }

    long t0 = now_ms();

    /* 重置本次执行的输出状态 */
    g_vm.logs[0] = '\0';
    g_vm.logs_len = 0;
    g_vm.ui[0] = '\0';
    g_vm.ui_valid = 0;
    g_vm.timed_out = 0;
    g_vm.deadline_ms = t0 + EXEC_TIMEOUT_MS;

    char *value_json  = NULL;
    char *error_json  = NULL;

    JSValue ret = JS_Eval(g_vm.ctx, code, strlen(code), "<playground>",
                          JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(g_vm.ctx);
        const char *s = JS_ToCString(g_vm.ctx, exc);
        error_json = json_escape(s ? s : "未知错误");
        /* 若同时发生了超时，优先提示超时 */
        if (g_vm.timed_out) {
            free(error_json);
            error_json = strdup("\"执行超时：疑似死循环（超过 2 秒），已强制中止\"");
        }
        if (s) JS_FreeCString(g_vm.ctx, s);
        JS_FreeValue(g_vm.ctx, exc);
    } else {
        const char *s = jsvalue_to_json(g_vm.ctx, ret);
        value_json = json_escape(s ? s : "undefined");
        if (s) JS_FreeCString(g_vm.ctx, s);
    }
    JS_FreeValue(g_vm.ctx, ret);

    long cost = now_ms() - t0;

    /* 组装结果 JSON */
    size_t cap = (value_json ? strlen(value_json) : 0)
               + g_vm.logs_len + (g_vm.ui_valid ? strlen(g_vm.ui) : 0)
               + (error_json ? strlen(error_json) : 0) + 1024;
    char *out = (char *)malloc(cap);
    if (!out) {
        free(value_json); free(error_json);
        return strdup("{\"ok\":false,\"error\":\"内存分配失败\"}");
    }

    char *log_esc = json_escape(g_vm.logs);
    snprintf(out, cap,
             "{\"ok\":%s,\"value\":%s,\"logs\":%s,\"ui\":%s,\"error\":%s,"
             "\"timeout\":%s,\"timeMs\":%ld}",
             error_json ? "false" : "true",
             value_json ? value_json : "\"undefined\"",
             log_esc,
             g_vm.ui_valid ? g_vm.ui : "null",
             error_json ? error_json : "\"\"",
             g_vm.timed_out ? "true" : "false",
             cost);

    free(log_esc);
    free(value_json);
    free(error_json);
    return out;     /* 调用方负责 free */
}

void vm_reset(void) { reset_vm(); }

/* ------------------------------------------------------------------ */
/* NAPI 导出层                                                         */
/* ------------------------------------------------------------------ */

static napi_value EvalScript(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {NULL};
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);

    if (argc < 1) {
        napi_throw_error(env, NULL, "evalScript 需要一个字符串参数");
        return NULL;
    }

    /* 读取 JS 源码 */
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], NULL, 0, &len);
    char *code = (char *)malloc(len + 1);
    if (!code) {
        napi_throw_error(env, NULL, "内存不足");
        return NULL;
    }
    napi_get_value_string_utf8(env, args[0], code, len + 1, &len);
    code[len] = '\0';

    char *result = vm_eval(code);
    free(code);

    napi_value out;
    napi_create_string_utf8(env, result, NAPI_AUTO_LENGTH, &out);
    free(result);
    return out;
}

static napi_value ResetVM(napi_env env, napi_callback_info info) {
    (void)env; (void)info;
    vm_reset();
    return NULL;
}

#ifndef SANDBOX_VERIFY
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
static int CompilerInterrupt(JSRuntime *runtime, void *opaque) {
    (void)runtime;
    return now_ms() > *(long *)opaque;
}
static JSValue CompilerLog(JSContext *context, JSValueConst self, int count, JSValueConst *args) {
    (void)self;
    for (int i = 0; i < count; i++) {
        const char *text = JS_ToCString(context, args[i]);
        if (text) { printf("%s%s", i ? " " : "[ArkUI] ", text); JS_FreeCString(context, text); }
    }
    printf("\n");
    return JS_UNDEFINED;
}
typedef struct CompilerJob {
    char *code;
    size_t length;
    char *text;
    int failed;
} CompilerJob;
/* Deep ArkUI call chains need a larger native stack than an ArkTS worker.
 * NAPI is used only by the caller; the compiler pthread owns its isolated VM. */
static void *RunCompilerJob(void *opaque) {
    CompilerJob *job = opaque;
    JSRuntime *runtime = JS_NewRuntime();
    if (!runtime) { job->failed = 1; job->text = strdup("Cannot create compiler VM"); return NULL; }
    JS_SetMemoryLimit(runtime, (size_t)2048 * 1024 * 1024);
    JS_SetMaxStackSize(runtime, 8 * 1024 * 1024);
    long deadline = now_ms() + 300000;
    JS_SetInterruptHandler(runtime, CompilerInterrupt, &deadline);
    JSContext *context = JS_NewContext(runtime);
    if (!context) { JS_FreeRuntime(runtime); job->failed = 1; job->text = strdup("Cannot create compiler context"); return NULL; }
    JSValue global = JS_GetGlobalObject(context);
    JSValue console = JS_NewObject(context);
    const char *levels[] = {"log", "info", "warn", "error", "debug", "trace"};
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
        JS_SetPropertyStr(context, console, levels[i], JS_NewCFunction(context, CompilerLog, levels[i], 1));
    JS_SetPropertyStr(context, global, "console", console);
    JS_FreeValue(context, global);
    JSValue evaluated = JS_Eval(context, job->code, job->length, "ui-transform.bundle.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(evaluated)) {
        JSValue exception = JS_GetException(context);
        const char *message = JS_ToCString(context, exception);
        JSValue stack = JS_GetPropertyStr(context, exception, "stack");
        const char *trace = JS_ToCString(context, stack);
        char diagnostic[16384];
        snprintf(diagnostic, sizeof(diagnostic), "%s\n%s", message ? message : "Compiler VM failed", trace ? trace : "");
        job->failed = 1;
        job->text = strdup(diagnostic);
        if (message) JS_FreeCString(context, message);
        if (trace) JS_FreeCString(context, trace);
        JS_FreeValue(context, stack); JS_FreeValue(context, exception);
    } else {
        const char *text = JS_ToCString(context, evaluated);
        if (text) { job->text = strdup(text); JS_FreeCString(context, text); }
        else { job->failed = 1; job->text = strdup("Compiler returned no text"); }
    }
    JS_FreeValue(context, evaluated);
    JS_FreeContext(context); JS_FreeRuntime(runtime);
    return NULL;
}
static napi_value EvalCompiler(napi_env env, napi_callback_info info) {
    size_t count = 1, length = 0;
    napi_value argument, result = NULL;
    napi_get_cb_info(env, info, &count, &argument, NULL, NULL);
    if (count != 1 || napi_get_value_string_utf8(env, argument, NULL, 0, &length) != napi_ok || length > 64 * 1024 * 1024) {
        napi_throw_type_error(env, NULL, "Invalid compiler script"); return NULL;
    }
    CompilerJob job = {0};
    job.code = malloc(length + 1);
    if (!job.code) { napi_throw_error(env, NULL, "Cannot allocate compiler script"); return NULL; }
    napi_get_value_string_utf8(env, argument, job.code, length + 1, &job.length);
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    int status = pthread_attr_setstacksize(&attributes, 32 * 1024 * 1024);
    pthread_t thread;
    if (status == 0) status = pthread_create(&thread, &attributes, RunCompilerJob, &job);
    pthread_attr_destroy(&attributes);
    if (status == 0) pthread_join(thread, NULL);
    free(job.code);
    if (status != 0 || !job.text) napi_throw_error(env, NULL, "Cannot run compiler thread");
    else if (job.failed) napi_throw_error(env, NULL, job.text);
    else napi_create_string_utf8(env, job.text, NAPI_AUTO_LENGTH, &result);
    free(job.text);
    return result;
}
static int saved_tool_stdout = -1;
static int saved_tool_stderr = -1;
static napi_value StartToolLog(napi_env env, napi_callback_info info) {
    size_t argc = 1, length = 0;
    napi_value arg, result;
    char path[4096] = {0};
    napi_get_cb_info(env, info, &argc, &arg, NULL, NULL);
    if (argc != 1 || napi_get_value_string_utf8(env, arg, path, sizeof(path), &length) != napi_ok ||
        length >= sizeof(path) - 1 || path[0] != '/' || strstr(path, "/../") || saved_tool_stdout >= 0) {
        napi_throw_error(env, NULL, "Invalid log path or another native task is active");
        return NULL;
    }
    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0600);
    if (fd < 0) {
        napi_throw_error(env, NULL, "Cannot open native output log");
        return NULL;
    }
    fflush(stdout);
    fflush(stderr);
    saved_tool_stdout = dup(STDOUT_FILENO);
    saved_tool_stderr = dup(STDERR_FILENO);
    if (saved_tool_stdout < 0 || saved_tool_stderr < 0) {
        if (saved_tool_stdout >= 0) close(saved_tool_stdout);
        if (saved_tool_stderr >= 0) close(saved_tool_stderr);
        saved_tool_stdout = saved_tool_stderr = -1;
        close(fd);
        napi_throw_error(env, NULL, "Cannot preserve native output streams");
        return NULL;
    }
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    napi_get_undefined(env, &result);
    return result;
}
static napi_value StopToolLog(napi_env env, napi_callback_info info) {
    (void)info;
    napi_value result;
    fflush(stdout);
    fflush(stderr);
    if (saved_tool_stdout >= 0) {
        dup2(saved_tool_stdout, STDOUT_FILENO);
        close(saved_tool_stdout);
        saved_tool_stdout = -1;
    }
    if (saved_tool_stderr >= 0) {
        dup2(saved_tool_stderr, STDERR_FILENO);
        close(saved_tool_stderr);
        saved_tool_stderr = -1;
    }
    napi_get_undefined(env, &result);
    return result;
}
static napi_value InspectLibrary(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value arg, result;
    char name[80] = {0};
    napi_get_cb_info(env, info, &argc, &arg, NULL, NULL);
    if (argc != 1 || napi_get_value_string_utf8(env, arg, name, sizeof(name), NULL) != napi_ok ||
        (strcmp(name, "libsigntool.so") && strcmp(name, "libhdc_z.so") &&
         strcmp(name, "libes2panda-public.so"))) {
        napi_throw_type_error(env, NULL, "Unknown toolchain library");
        return NULL;
    }
    dlerror();
    void *handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    const char *message = handle ? "loaded" : dlerror();
    napi_create_string_utf8(env, message ? message : "unknown loader error", NAPI_AUTO_LENGTH, &result);
    return result;
}
#endif

#ifndef SANDBOX_VERIFY
/* 真机：注册为可被 ArkTS import 的 NAPI 模块 */
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "evalScript", NULL, EvalScript, NULL, NULL, NULL, napi_default, NULL },
        { "evalCompiler", NULL, EvalCompiler, NULL, NULL, NULL, napi_default, NULL },
        { "resetVM",    NULL, ResetVM,    NULL, NULL, NULL, napi_default, NULL },
        { "inspectLibrary", NULL, InspectLibrary, NULL, NULL, NULL, napi_default, NULL },
        { "startToolLog", NULL, StartToolLog, NULL, NULL, NULL, napi_default, NULL },
        { "stopToolLog", NULL, StopToolLog, NULL, NULL, NULL, napi_default, NULL },
    };
    napi_define_properties(env, exports,
                           sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

static napi_module g_module = {
    .nm_version      = 1,
    .nm_flags        = 0,
    .nm_filename     = NULL,
    .nm_register_func = Init,
    .nm_modname      = "arkforge_script",
    .nm_priv         = ((void*)0),
    .reserved        = { 0 },
};

__attribute__((constructor)) void RegisterJSVMSo(void) {
    napi_module_register(&g_module);
}
#endif /* SANDBOX_VERIFY */
