# 真机构建：使用 OHOS SDK CMake 工具链，依赖 ace_napi 与 sysroot。
cmake_minimum_required(VERSION 3.5.0)
project(HarmonyPlayground_Native)

set(NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(QJS_DIR    ${NATIVE_DIR}/quickjs)

# ---------------------------------------------------------------------------
# QuickJS 核心编译单元
# 说明：刻意不引入 quickjs-libc.c —— 它承载 std/os/文件 IO 等 POSIX 能力，
#       在 HarmonyOS 应用沙盒中既无必要，也会带来额外的受限接口依赖。
#       这里只用纯引擎内核，宿主能力完全由我们自己的 __host 对象提供。
# ---------------------------------------------------------------------------
set(QJS_SOURCES
    ${QJS_DIR}/quickjs.c
    ${QJS_DIR}/cutils.c
    ${QJS_DIR}/libregexp.c
    ${QJS_DIR}/libunicode.c
    ${QJS_DIR}/dtoa.c
)

add_library(arkforge_script SHARED
    ${NATIVE_DIR}/jsvm_bridge.c
    ${QJS_SOURCES}
)

target_include_directories(arkforge_script PRIVATE
    ${QJS_DIR}
    ${NATIVE_DIR}
)

target_compile_definitions(arkforge_script PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="1.0.0-playground"
    # 注意：若要启用 BigInt 需追加 CONFIG_BIGNUM，并确保 OK 后才能用
)

target_compile_options(arkforge_script PRIVATE
    -fPIC
    -O2
    -fvisibility=hidden
    -Wall
    -Wno-unused-parameter
)

# OHOS：NAPI 由 libace_napi.z.so 提供；日志可选 libhilog_ndk.z.so
target_link_libraries(arkforge_script PUBLIC
    dl
    libace_napi.z.so
    libhilog_ndk.z.so
    m
)

option(ARKFORGE_BUILD_DEVICE_TEST "Build a device-side native smoke test" OFF)
if(ARKFORGE_BUILD_DEVICE_TEST)
    add_executable(arkforge_runtime_test
        ${NATIVE_DIR}/runtime_test.c ${NATIVE_DIR}/jsvm_bridge.c ${QJS_SOURCES})
    target_include_directories(arkforge_runtime_test PRIVATE ${QJS_DIR} ${NATIVE_DIR})
    target_compile_definitions(arkforge_runtime_test PRIVATE
        _GNU_SOURCE SANDBOX_VERIFY CONFIG_VERSION="arkforge-repro")
    target_compile_options(arkforge_runtime_test PRIVATE -O2)
    target_link_libraries(arkforge_runtime_test PRIVATE m)
endif()
