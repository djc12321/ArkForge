#!/usr/bin/env bash
set -euo pipefail
# Build upstream Git's CLI. No command parser or diagnostic replacement.
app="$(cd "$(dirname "$0")/.." && pwd)"
cache="${ARKFORGE_NATIVE_CACHE:-/home/djc/.cache/arkforge}"
build="$cache/git-2.55.0-ohos"
archive="$app/../../../_vendor/git-2.55.0.tar.xz"
mkdir -p "$build"
if [ ! -f "$build/Makefile" ]; then tar -xJf "$archive" -C "$build" --strip-components=1; fi
make -C "$build" -j6 \
  CC="$cache/ohos-sdk-toolchain/bin/clang" \
  AR="$cache/ohos-sdk-toolchain/bin/llvm-ar" \
  CFLAGS='--target=aarch64-linux-ohos -O2 -fPIC' \
  LDFLAGS='--target=aarch64-linux-ohos -shared -Wl,-Bsymbolic,-soname,libgit_exec.so' \
  NO_CURL=YesPlease NO_EXPAT=YesPlease NO_GETTEXT=YesPlease NO_ICONV=YesPlease NO_RUST=YesPlease \
  NO_OPENSSL=YesPlease NO_PERL=YesPlease NO_PYTHON=YesPlease NO_TCLTK=YesPlease NO_REGEX=YesPlease NO_PTHREADS=YesPlease \
  SHELL=/bin/sh SHELL_PATH=/bin/sh git
cp "$build/git" "$app/entry/libs/arm64-v8a/libgit_exec.so"
cp "$build/COPYING" "$app/native/git-COPYING"
