# Chipmunk2D 7.0.1 static library built as plain C (upstream CMake forces C++ on MSVC, but the sources
# need implicit void* conversions, so only C works... except for one empty "= {}" initializer, patched below).
# cpHastySpace.c (Chipmunk Pro / pthread / sysctl only) is excluded. Usage: -DCHIPMUNK_SRC=<Chipmunk2D root>
cmake_minimum_required(VERSION 3.10)
project(chipmunk C)
file(GLOB SRCS ${CHIPMUNK_SRC}/src/*.c)
list(FILTER SRCS EXCLUDE REGEX "cpHastySpace\\.c$")
list(FILTER SRCS EXCLUDE REGEX "cpPolyline\\.c$")
file(READ ${CHIPMUNK_SRC}/src/cpPolyline.c _poly)
string(REPLACE "struct Notch notch = {};" "struct Notch notch = {0};" _poly "${_poly}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gen/cpPolyline.c "${_poly}")
list(APPEND SRCS ${CMAKE_CURRENT_BINARY_DIR}/gen/cpPolyline.c)
add_library(chipmunk STATIC ${SRCS})
target_include_directories(chipmunk PRIVATE ${CHIPMUNK_SRC}/include)
target_compile_definitions(chipmunk PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(chipmunk PROPERTIES OUTPUT_NAME libchipmunk)