cmake_minimum_required(VERSION 3.15)
project(restool)
set(CMAKE_CXX_STANDARD 17)
if (WIN32)
set(CMAKE_EXE_LINKER_FLAGS "-static -s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fstack-protector -DLIBXML_STATIC -Wno-attributes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector -Wno-attributes")
elseif (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-fPIE -pie -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fstack-protector -O2 -g -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector")
else()
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,relro,-z,now -fPIE -pie -Wl,-z,noexecstack -s -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fstack-protector -O2 -g -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector")
endif()

set(jsoncpp_dir ${CMAKE_SOURCE_DIR}/../../third_party/jsoncpp)
set(png_dir ${CMAKE_SOURCE_DIR}/../../third_party/libpng)
set(zlib_dir ${CMAKE_SOURCE_DIR}/../../third_party/zlib)
set(bound_checking_function_dir ${CMAKE_SOURCE_DIR}/../../third_party/bounds_checking_function)

include_directories(include)
include_directories(${jsoncpp_dir}/include)
include_directories(${png_dir}/)
include_directories(${bound_checking_function_dir}/include)

aux_source_directory(src restool_source)

aux_source_directory(${bound_checking_function_dir}/src lib_sec_source)
add_library(securec STATIC ${lib_sec_source})

aux_source_directory(${jsoncpp_dir}/src/lib_json json_source)
add_library(jsoncpp STATIC ${json_source})

aux_source_directory(${zlib_dir}/ zlib_source)
add_library(zlib STATIC ${zlib_source})

aux_source_directory(${png_dir}/ png_source)
add_library(png STATIC ${png_source})

add_executable(restool ${restool_source})
target_link_libraries(restool jsoncpp securec zlib png)