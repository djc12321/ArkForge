#!/usr/bin/env bash
set -euo pipefail
app="$(cd "$(dirname "$0")/.." && pwd)"
cache="${ARKFORGE_NATIVE_CACHE:-/home/djc/.cache/arkforge}"
build="$cache/git-2.55.0-ohos"
cc="$cache/ohos-sdk-toolchain/bin/clang"
"$cc" --target=aarch64-linux-ohos -O2 -fPIC -c "$app/native/git_exec_bridge.c" -o "$build/arkforge-exec.o"
args=(CC="$cc" AR="$cache/ohos-sdk-toolchain/bin/llvm-ar"
  CFLAGS='--target=aarch64-linux-ohos -O2 -fPIC'
  NO_EXPAT=YesPlease NO_GETTEXT=YesPlease NO_ICONV=YesPlease NO_RUST=YesPlease
  NO_OPENSSL=YesPlease NO_PERL=YesPlease NO_PYTHON=YesPlease NO_TCLTK=YesPlease NO_REGEX=YesPlease NO_PTHREADS=YesPlease
  CURL_CONFIG="$cache/curl-ohos/bin/curl-config"
  CURL_CFLAGS="-I$cache/curl-ohos/include"
  CURL_LIBCURL="$cache/curl-ohos/lib/libcurl.a $cache/openssl-ohos/install/lib/libssl.a $cache/openssl-ohos/install/lib/libcrypto.a -ldl"
  SHELL=/bin/sh SHELL_PATH=/bin/sh)
link="--target=aarch64-linux-ohos -shared -Wl,-Bsymbolic,--wrap=fork,--wrap=execve,--wrap=execvp,--wrap=atexit,--wrap=exit $build/arkforge-exec.o -ldl"
make -C "$build" -j6 "${args[@]}" LDFLAGS="$link -Wl,-soname,libgit_exec.so" git git-remote-http
cp "$build/git" "$app/entry/libs/arm64-v8a/libgit_exec.so"
touch "$build/GIT-LDFLAGS"
make -C "$build" -j6 "${args[@]}" LDFLAGS="$link -Wl,-soname,libgit_remote_http.so" git-remote-http
cp "$build/git-remote-http" "$app/entry/libs/arm64-v8a/libgit_remote_http.so"
cp "$cache/curl-8.23.0/COPYING" "$app/native/curl-COPYING"
