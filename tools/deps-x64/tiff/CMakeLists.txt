# libtiff 4.0.3 static library (no upstream CMake). Usage: -DTIFF_SRC=<tiff root> -DENGINE_TIFF_INC=<engine tiff/include/win32>
cmake_minimum_required(VERSION 3.10)
project(tiff C)
set(S ${TIFF_SRC}/libtiff)
# the engine's tiffconf.h has no JPEG/ZIP/PIXARLOG/LZMA codecs, so those sources are not needed
set(TIFF_SOURCES
  tif_aux tif_close tif_codec tif_color tif_compress tif_dir tif_dirinfo tif_dirread tif_dirwrite
  tif_dumpmode tif_error tif_extension tif_fax3 tif_fax3sm tif_flush tif_getimage tif_luv tif_lzw
  tif_next tif_open tif_packbits tif_predict tif_print tif_read tif_strip tif_swab tif_thunder
  tif_tile tif_version tif_warning tif_write tif_win32)
set(SRCS "")
foreach(n ${TIFF_SOURCES})
  list(APPEND SRCS ${S}/${n}.c)
endforeach()
# tif_config.vc.h defines "snprintf" as _snprintf, which breaks with the modern UCRT; drop that line
file(READ ${S}/tif_config.vc.h _cfg)
string(REPLACE "#define snprintf _snprintf" "" _cfg "${_cfg}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gen/tif_config.h "${_cfg}")
add_library(tiff STATIC ${SRCS})
# engine tiffconf.h/tiff.h/tiffio.h/tiffvers.h first, generated tif_config.h, then the source dir (tif_dir.h ...)
target_include_directories(tiff PRIVATE ${ENGINE_TIFF_INC} ${CMAKE_CURRENT_BINARY_DIR}/gen ${S})
target_compile_definitions(tiff PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
set_target_properties(tiff PROPERTIES OUTPUT_NAME libtiff)