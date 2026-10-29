# libwebp 0.4.4 static library: decoder + encoder + dsp + utils in ONE lib. Usage: -DWEBP_SRC=<libwebp root>
cmake_minimum_required(VERSION 3.10)
project(webp C)
file(GLOB SRCS ${WEBP_SRC}/src/dec/*.c ${WEBP_SRC}/src/dsp/*.c ${WEBP_SRC}/src/enc/*.c ${WEBP_SRC}/src/utils/*.c)
add_library(webp STATIC ${SRCS})
target_include_directories(webp PRIVATE ${WEBP_SRC}/src)
target_compile_definitions(webp PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(webp PROPERTIES OUTPUT_NAME libwebp)