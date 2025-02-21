cmake_minimum_required(VERSION 3.5.2)

project(reflect C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(reflect
    src/reflect.c
)

target_include_directories(reflect INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(PLUGIN_EXT "dylib")
    elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(PLUGIN_EXT "dll")
    else ()
        message(STATUS "Host System Name: ${CMAKE_HOST_SYSTEM_NAME}")
        set(PLUGIN_EXT "so")
    endif ()

    set(PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/plugin/build/libReflectClangPlugin.${PLUGIN_EXT}")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -load -Xclang ${PLUGIN_PATH} -Xclang -add-plugin -Xclang reflect-clang-plugin")
else()
    message(WARNING
        "You are not using Clang as the C compiler, so the reflection plugin will NOT be loaded.")
endif()

add_subdirectory(examples)

enable_testing()
add_subdirectory(tests)