# IJG libjpeg 9 static library (no upstream CMake). Usage: -DJPEG_SRC=<dir> -DENGINE_JPEG_INC=<engine jpeg/include/win32>
cmake_minimum_required(VERSION 3.10)
project(jpeg C)
set(S ${JPEG_SRC})
set(JPEG_SOURCES
  jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff jcinit jcmainct jcmarker
  jcmaster jcomapi jcparam jcprepct jcsample jctrans jdapimin jdapistd jdarith jdatadst jdatasrc
  jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster jdmerge jdpostct jdsample
  jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst jidctint jmemmgr jmemnobs jquant1
  jquant2 jutils)
set(SRCS "")
foreach(n ${JPEG_SOURCES})
  list(APPEND SRCS ${S}/${n}.c)
endforeach()
add_library(jpeg STATIC ${SRCS})
# engine headers first (jconfig.h / jmorecfg.h / jpeglib.h), then the source tree (jinclude.h, jdct.h ...)
target_include_directories(jpeg PRIVATE ${ENGINE_JPEG_INC} ${S})
target_compile_definitions(jpeg PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(jpeg PROPERTIES OUTPUT_NAME libjpeg)