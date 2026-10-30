# SQLite amalgamation as a shared library. Usage: -DSQLITE_SRC=<amalgamation dir>
cmake_minimum_required(VERSION 3.10)
project(sqlite3 C)
add_library(sqlite3 SHARED ${SQLITE_SRC}/sqlite3.c)
target_include_directories(sqlite3 PRIVATE ${SQLITE_SRC})
target_compile_definitions(sqlite3 PRIVATE "SQLITE_API=__declspec(dllexport)" SQLITE_ENABLE_COLUMN_METADATA _CRT_SECURE_NO_WARNINGS)
set_target_properties(sqlite3 PROPERTIES OUTPUT_NAME sqlite3)