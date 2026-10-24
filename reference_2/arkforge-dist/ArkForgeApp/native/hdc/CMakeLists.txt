cmake_minimum_required(VERSION 3.20)
project(ArkForgeLocalHdc C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${HDC_SOURCE}/scripts/hdc_hash_gen.py"
    -f hdc_hash_gen.h -i "${HDC_SOURCE}/" -o "${CMAKE_CURRENT_BINARY_DIR}/gen/"
    COMMAND_ERROR_IS_FATAL ANY)
file(GLOB COMMON_SOURCES "${HDC_SOURCE}/src/common/*.cpp")
file(GLOB HOST_SOURCES "${HDC_SOURCE}/src/host/*.cpp")
list(FILTER HOST_SOURCES EXCLUDE REGEX "/(main|host_usb)\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX "/(hdc_huks|password)\\.cpp$")
file(GLOB SECUREC_SOURCES "${SECUREC_ROOT}/src/securec/*.c")
add_library(securec STATIC ${SECUREC_SOURCES})
target_include_directories(securec PUBLIC "${SECUREC_ROOT}/include")
add_library(lz4 STATIC "${VENDOR}/lz4/lib/lz4.c" "${VENDOR}/lz4/lib/lz4hc.c"
    "${VENDOR}/lz4/lib/lz4frame.c" "${VENDOR}/lz4/lib/xxhash.c")
target_include_directories(lz4 PUBLIC "${VENDOR}/lz4/lib")
add_library(hdc_z SHARED ${COMMON_SOURCES} ${HOST_SOURCES}
    "${HDC_BRIDGE}/main.cpp" "${HDC_BRIDGE}/napi.cpp")
target_include_directories(hdc_z PRIVATE "${HDC_SOURCE}/src/common" "${HDC_SOURCE}/src/host"
    "${HDC_BRIDGE}" "${OPENSSL_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/gen")
target_compile_definitions(hdc_z PRIVATE HILOG HOST_OHOS HDC_HOST HARMONY_PROJECT HDC_NO_USB
    HDC_SUPPORT_ENCRYPT_TCP HDC_SUPPORT_UART USE_CONFIG_UV_THREADS SIZE_THREAD_POOL=8)
target_compile_options(hdc_z PRIVATE -Wno-deprecated-declarations -fvisibility=hidden)
target_link_options(hdc_z PRIVATE -Wl,--exclude-libs,ALL -Wl,--no-undefined)
target_link_libraries(hdc_z PRIVATE "${OPENSSL_ROOT}/lib/libssl.a"
    "${OPENSSL_ROOT}/lib/libcrypto.a" securec lz4 uv hilog_ndk.z ace_napi.z)
