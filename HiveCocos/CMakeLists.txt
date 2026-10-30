cmake_minimum_required(VERSION 3.6)

# HiveCocos: Hive SDK v4 (Windows) bridge for cocos2d-x.
#   HiveCocos         core (no cocos2d-x dependency): bridge, transports, Configuration/AuthV4 facades
#   HiveCocosAdapter  cocos2d-x glue (thread dispatcher)
#   HiveCocosTests    console test, built on demand: cmake --build <dir> --target HiveCocosTests
#
# Expects COCOS2DX_ROOT_PATH to be set by the parent project (rapidjson comes from the engine's external/json).

if(NOT COCOS2DX_ROOT_PATH)
    message(FATAL_ERROR "HiveCocos: set COCOS2DX_ROOT_PATH before add_subdirectory")
endif()

set(HIVECOCOS_CORE_SRC
    src/HiveBridge.cpp
    src/HiveConfiguration.cpp
    src/HiveAuthV4.cpp
    src/DllTransport.cpp
    src/FakeTransport.cpp
)
set(HIVECOCOS_CORE_HEADERS
    include/HiveCocos/HiveTypes.h
    include/HiveCocos/HiveTransport.h
    include/HiveCocos/HiveBridge.h
    include/HiveCocos/HiveConfiguration.h
    include/HiveCocos/HiveAuthV4.h
    include/HiveCocos/DllTransport.h
    include/HiveCocos/FakeTransport.h
)

add_library(HiveCocos STATIC ${HIVECOCOS_CORE_SRC} ${HIVECOCOS_CORE_HEADERS})
target_include_directories(HiveCocos
    PUBLIC include
    PUBLIC ${COCOS2DX_ROOT_PATH}/external
)
set_target_properties(HiveCocos PROPERTIES FOLDER "HiveCocos")

add_library(HiveCocosAdapter STATIC src/CocosDispatcher.cpp include/HiveCocos/CocosDispatcher.h)
target_link_libraries(HiveCocosAdapter PUBLIC HiveCocos cocos2d)
set_target_properties(HiveCocosAdapter PROPERTIES FOLDER "HiveCocos")

add_executable(HiveCocosTests EXCLUDE_FROM_ALL tests/HiveCocosTests.cpp)
target_link_libraries(HiveCocosTests PRIVATE HiveCocos)
set_target_properties(HiveCocosTests PROPERTIES FOLDER "HiveCocos")

# Integration probe against the real SDK DLLs (x64 only; see tests/HiveSdkProbe.cpp). Built on demand.
add_executable(HiveSdkProbe EXCLUDE_FROM_ALL tests/HiveSdkProbe.cpp)
target_link_libraries(HiveSdkProbe PRIVATE HiveCocos)
set_target_properties(HiveSdkProbe PROPERTIES FOLDER "HiveCocos")
