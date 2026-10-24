#!/usr/bin/env bash
# ==============================================================================
# 构建鸿蒙 aarch64 可用的方舟编译器动态库
#
# 关键事实（已实证）：
#   ets2panda/CMakeLists.txt:767
#       panda_frontend_add_library(es2panda-lib ${PANDA_DEFAULT_LIB_TYPE} ...)
#   static_core/cmake/Definitions.cmake:224
#       set(PANDA_DEFAULT_LIB_TYPE "SHARED")
#   => 编译器核心「默认就编成 .so」，无需改造上游
#
#   public/es2panda_lib.h 是官方为嵌入场景准备的纯 C API（es2panda_GetImpl），
#   这才是给 NAPI 用的正确形态，对应构建目标为 es2panda-public。
#
# 产物：
#   libes2panda-public.so  —— C 嵌入 API（NAPI 直接用这个）
#   libes2panda-lib.so     —— 编译器核心（被上面那个依赖）
#   etsstdlib.abc          —— ArkTS 标准库字节码，运行时必需
# ==============================================================================
set -euo pipefail

SRC_ROOT="${SRC_ROOT:-/opt/arkforge/src}"
BUILD_DIR="${BUILD_DIR:-$SRC_ROOT/build_ohos}"
SYSROOT="${SYSROOT:-}"                       # ← 必填：OHOS SDK 的 sysroot
TOOLCHAIN_CLANG_ROOT="${TOOLCHAIN_CLANG_ROOT:-}" # Linux-host OHOS LLVM directory
JOBS="${JOBS:-6}"                            # ← 别开满，32 并发会被 OOM 杀掉
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-cross-ohos-musl-aarch64.cmake}"

log() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }

if [ -z "$SYSROOT" ]; then
    echo "错误：请先 export SYSROOT=/你的/OHOS/sysroot" >&2
    echo "      还需 export TOOLCHAIN_CLANG_ROOT=/Linux版/OHOS/native/llvm" >&2
    exit 1
fi
if [ ! -x "$TOOLCHAIN_CLANG_ROOT/bin/clang" ] || [ ! -d "$SYSROOT/usr/include" ]; then
    echo "错误：需要在 Linux 上可执行的 OHOS clang 及有效 sysroot；Windows clang.exe 不适用于此脚本" >&2
    exit 1
fi
export NPROC_PER_JOB="$JOBS"

# ------------------------------------------------------------------------------
log "1/4 获取源码（两个仓，必须按官方方式摆放）"
# ------------------------------------------------------------------------------
mkdir -p "$SRC_ROOT"
cd "$SRC_ROOT"

[ -d runtime_core ] || \
    { echo "请先运行 scripts/setup_env.sh 准备锁定版本的源码" >&2; exit 1; }
[ -d ets_frontend ] || \
    { echo "请先运行 scripts/setup_env.sh 准备锁定版本的源码" >&2; exit 1; }

# 官方要求的目录关联，缺了就找不到 es2panda
if [ ! -e runtime_core/static_core/tools/es2panda ]; then
    ln -s ../../../ets_frontend/ets2panda runtime_core/static_core/tools/es2panda
fi

# 前置依赖见 scripts/setup_env.sh：ruby、jinja2 CLI、libdwarf、git 身份
command -v ruby   >/dev/null || echo "[警告] 缺 ruby，代码生成会失败"
command -v jinja2 >/dev/null || echo "[警告] 缺 jinja2，标准库生成会失败"

# ------------------------------------------------------------------------------
log "2/4 配置 CMake"
# ------------------------------------------------------------------------------
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# -DCMAKE_CXX_FLAGS="-Wno-error" 很重要：
#   官方 CMakeLists.txt:77 硬编码了 -Werror，新版编译器会把一批误报当错误
#   （clobbered / stringop-overflow / maybe-uninitialized），构建必中断。
#   由于该变量在 -Werror 之后才拼接用户传入值，传 -Wno-error 可以覆盖。
cmake "$SRC_ROOT/runtime_core/static_core" \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-Wno-error" \
    -DCMAKE_TOOLCHAIN_FILE="$SRC_ROOT/runtime_core/static_core/cmake/toolchain/$TOOLCHAIN_FILE" \
    -DTOOLCHAIN_SYSROOT="$SYSROOT" \
    -DTOOLCHAIN_CLANG_ROOT="$TOOLCHAIN_CLANG_ROOT" \
    -DPANDA_TARGET_ARM64=ON \
    -DPANDA_TARGET_UNIX=ON \
    -DPANDA_WITH_COMPILER=OFF \
    -DPANDA_WITH_TESTS=OFF \
    -DPANDA_LLVM_BACKEND=OFF

# 说明：
#   PANDA_WITH_COMPILER=OFF  跳过 asmjit / zydis / LLVM 后端（我们只要前端：源码→字节码）
#   PANDA_WITH_TESTS=OFF     跳过 googletest / benchmark / rapidcheck
#   PANDA_LLVM_BACKEND=OFF   跳过 LLVM（数 GB 的 third_party_llvm-project）

# ------------------------------------------------------------------------------
log "3/4 构建目标"
# ------------------------------------------------------------------------------
ninja -j"$JOBS" es2panda-lib
ninja -j"$JOBS" es2panda-public
ninja -j"$JOBS" plugins/ets/etsstdlib       # ArkTS 标准库字节码，运行时必需

# ------------------------------------------------------------------------------
log "4/4 产物核验"
# ------------------------------------------------------------------------------
PUB="$(find "$BUILD_DIR" -name 'libes2panda-public.so' | head -1)"
CORE="$(find "$BUILD_DIR" -name 'libes2panda-lib.so' | head -1)"
STD="$(find "$BUILD_DIR" -name 'etsstdlib.abc' | head -1)"

if [ -z "$PUB" ]; then
    echo "未找到产物，请检查上面构建日志" >&2
    exit 1
fi

echo "C API 库 : $PUB  ($(du -h "$PUB" | cut -f1))"
echo "编译器核 : $CORE ($(du -h "$CORE" | cut -f1))"
echo "标准库   : $STD  ($(du -h "$STD" | cut -f1))"
echo
file "$PUB"

echo
echo "架构核验（应为 ARM aarch64）:"
if command -v aarch64-linux-gnu-readelf >/dev/null 2>&1; then
    aarch64-linux-gnu-readelf -A "$PUB" | grep -i tag_cpu | head -2
    echo "动态依赖:"
    aarch64-linux-gnu-readelf -d "$PUB" | grep NEEDED | head -10
fi

echo
echo "=== 完成 ==="
echo "下一步："
echo "  1. 打包所有传递依赖 .so，而非仅两个编译器库（以 readelf -d 的 NEEDED 为准）"
echo "     .abc 与匹配版本的 arktsconfig/声明文件另放 rawfile"
echo "  2. 用 native/es2abc_napi.c 作为 NAPI 桥接层"
echo "  3. 注意 argv[0]（execPath）必须指向能找到 arktsconfig.json 的真实路径，"
echo "     否则 CreateConfig 会失败 —— 这是实测踩到的坑"
echo "  4. 该目标为静态 ArkTS 编译器；传统 ArkUI 页面还需匹配的 UI 转换与动态编译链路"
