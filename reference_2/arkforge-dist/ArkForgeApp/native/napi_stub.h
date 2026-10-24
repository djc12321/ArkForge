/*
 * napi_stub.h —— 仅用于沙箱交叉编译验证的 napi 声明桩
 *
 * 说明：
 * 沙箱环境无法获取 OpenHarmony SDK（ci.openharmony.cn 502 / 开发者联盟 403），
 * 因此没有真实的 napi/native_api.h。本文件提供与官方同名的最小声明子集，
 * 用于验证 NAPI 桥接层的代码结构与编译正确性。
 *
 * 【重要】真机构建时必须改用 OHOS SDK 的真实头文件：
 *   #include "napi/native_api.h"
 * 通过 -DSANDBOX_VERIFY 宏区分，请勿在真机产物中链接此桩。
 */
#ifndef NAPI_STUB_H
#define NAPI_STUB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct napi_env__ *napi_env;
typedef struct napi_value__ *napi_value;
typedef struct napi_callback_info__ *napi_callback_info;
typedef void *napi_data;
typedef void (*napi_finalize)(napi_env env, void *data, void *hint);
typedef napi_value (*napi_callback)(napi_env env, napi_callback_info info);

typedef enum {
    napi_ok = 0,
    napi_invalid_arg,
    napi_string_expected,
    napi_generic_failure,
    napi_pending_exception
} napi_status;

typedef enum { napi_default = 0 } napi_property_attributes;

#define NAPI_AUTO_LENGTH SIZE_MAX

typedef struct {
    const char *utf8name;
    napi_value name;
    napi_callback method;
    napi_callback getter;
    napi_callback setter;
    napi_value value;
    napi_property_attributes attributes;
    void *data;
} napi_property_descriptor;

typedef struct napi_module {
    int nm_version;
    unsigned int nm_flags;
    const char *nm_filename;
    napi_status (*nm_register_func)(napi_env env, napi_value exports);
    const char *nm_modname;
    void *nm_priv;
    void *reserved[4];
} napi_module;

#ifdef __cplusplus
extern "C" {
#endif

/* 下列符号在真机上由 libace_napi.z.so 提供；沙箱编译验证时不会真正链接 */
napi_status napi_get_cb_info(napi_env env, napi_callback_info cbinfo,
                             size_t *argc, napi_value *argv,
                             napi_value *this_arg, void **data);
napi_status napi_get_value_string_utf8(napi_env env, napi_value value,
                                       char *buf, size_t bufsize, size_t *result);
napi_status napi_create_string_utf8(napi_env env, const char *str,
                                    size_t length, napi_value *result);
napi_status napi_create_object(napi_env env, napi_value *result);
napi_status napi_define_properties(napi_env env, napi_value object,
                                   size_t property_count,
                                   const napi_property_descriptor *properties);
napi_status napi_throw_error(napi_env env, const char *code, const char *msg);
void napi_module_register(napi_module *mod);

#ifdef __cplusplus
}
#endif

#endif /* NAPI_STUB_H */
