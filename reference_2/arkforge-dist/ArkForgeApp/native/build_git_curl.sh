#!/usr/bin/env bash
set -euo pipefail
app="$(cd "$(dirname "$0")/.." && pwd)"
cache="${ARKFORGE_NATIVE_CACHE:-/home/djc/.cache/arkforge}"
source="$cache/curl-8.23.0"
mkdir -p "$source"
if [ ! -f "$source/CMakeLists.txt" ]; then
  tar -xJf "$app/../../../_vendor/curl-8.23.0.tar.xz" -C "$source" --strip-components=1
fi
cmake -S "$source" -B "$source/out-ohos" \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER="$cache/ohos-sdk-toolchain/bin/clang" \
  -DCMAKE_C_FLAGS='--target=aarch64-linux-ohos -O2 -fPIC' \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_INSTALL_PREFIX="$cache/curl-ohos" \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF \
  -DCURL_USE_OPENSSL=ON -DOPENSSL_ROOT_DIR="$cache/openssl-ohos/install" \
  -DOPENSSL_INCLUDE_DIR="$cache/openssl-ohos/install/include" \
  -DOPENSSL_SSL_LIBRARY="$cache/openssl-ohos/install/lib/libssl.a" \
  -DOPENSSL_CRYPTO_LIBRARY="$cache/openssl-ohos/install/lib/libcrypto.a" \
  -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_ZLIB=OFF \
  -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF \
  -DENABLE_THREADED_RESOLVER=OFF -DENABLE_ARES=OFF \
  -DHTTP_ONLY=ON -DHAVE_LINUX_TCP_H=OFF
cmake --build "$source/out-ohos" -j6
cmake --install "$source/out-ohos"
