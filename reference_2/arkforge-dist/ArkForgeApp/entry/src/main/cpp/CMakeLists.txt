cmake_minimum_required(VERSION 3.12)
project(JsvmBridge)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../../native/resources"
                 "${CMAKE_CURRENT_BINARY_DIR}/resources")

# cpp -> main -> src -> entry -> ArkForgeApp
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../../native"
                 "${CMAKE_CURRENT_BINARY_DIR}/jsvm")

get_filename_component(ARKFORGE_APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(ARK_COMPILER_LIBS "${ARKFORGE_APP_ROOT}/entry/libs/arm64-v8a")
add_library(arkforge_cli SHARED "${ARKFORGE_APP_ROOT}/native/official_cli.cpp" "${ARKFORGE_APP_ROOT}/native/git_process_broker.cpp" "${ARKFORGE_APP_ROOT}/native/terminal_pty.cpp")
target_compile_features(arkforge_cli PRIVATE cxx_std_17)
target_link_libraries(arkforge_cli PRIVATE libace_napi.z.so dl arkforge_cjson)
file(GLOB ARK_COMPILER_PUBLIC CONFIGURE_DEPENDS "${ARK_COMPILER_LIBS}/libes2panda-public.so")
if(ARK_COMPILER_PUBLIC)
    add_library(es2abc SHARED "${ARKFORGE_APP_ROOT}/native/es2abc_napi.c")
    target_include_directories(es2abc PRIVATE "${ARKFORGE_APP_ROOT}/native/compiler_include")
    target_link_libraries(es2abc PRIVATE "${ARK_COMPILER_LIBS}/libes2panda-public.so" libace_napi.z.so)
    target_link_options(es2abc PRIVATE "-Wl,-rpath-link,${ARK_COMPILER_LIBS}")
endif()
