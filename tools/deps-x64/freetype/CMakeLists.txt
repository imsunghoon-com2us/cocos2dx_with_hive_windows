# FreeType 2.5.5 static library (no upstream CMake). Usage: -DFT_SRC=<freetype root> -DENGINE_FT_INC=<engine freetype2/include/win32/freetype2>
# The engine's own config/ftoption.h + ftmodule.h are used (first on the include path) so module set and struct layout match.
cmake_minimum_required(VERSION 3.10)
project(freetype C)
set(S ${FT_SRC}/src)
set(SRCS
  ${S}/base/ftsystem.c ${FT_SRC}/builds/windows/ftdebug.c
  ${S}/base/ftinit.c ${S}/base/ftbase.c ${S}/base/ftbbox.c ${S}/base/ftbdf.c ${S}/base/ftbitmap.c
  ${S}/base/ftcid.c ${S}/base/ftfstype.c ${S}/base/ftgasp.c ${S}/base/ftglyph.c ${S}/base/ftgxval.c
  ${S}/base/ftlcdfil.c ${S}/base/ftmm.c ${S}/base/ftotval.c ${S}/base/ftpatent.c ${S}/base/ftpfr.c
  ${S}/base/ftstroke.c ${S}/base/ftsynth.c ${S}/base/fttype1.c ${S}/base/ftwinfnt.c
  ${S}/autofit/autofit.c ${S}/bdf/bdf.c ${S}/cff/cff.c ${S}/cid/type1cid.c ${S}/pcf/pcf.c
  ${S}/pfr/pfr.c ${S}/psaux/psaux.c ${S}/pshinter/pshinter.c ${S}/psnames/psmodule.c
  ${S}/raster/raster.c ${S}/sfnt/sfnt.c ${S}/smooth/smooth.c ${S}/truetype/truetype.c
  ${S}/type1/type1.c ${S}/type42/type42.c ${S}/winfonts/winfnt.c
  ${S}/gzip/ftgzip.c ${S}/lzw/ftlzw.c ${S}/cache/ftcache.c)
add_library(freetype STATIC ${SRCS})
target_include_directories(freetype PRIVATE ${ENGINE_FT_INC} ${FT_SRC}/include)
target_compile_definitions(freetype PRIVATE FT2_BUILD_LIBRARY _CRT_SECURE_NO_WARNINGS)
set_target_properties(freetype PROPERTIES OUTPUT_NAME freetype)