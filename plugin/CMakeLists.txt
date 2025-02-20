cmake_minimum_required(VERSION 3.20)
project(ReflectClangPlugin)

cmake_policy(SET CMP0144 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

message(STATUS "LLVM_DIR: ${LLVM_DIR}")
message(STATUS "Clang_DIR: ${Clang_DIR}")
message(STATUS "Clang_FOUND: ${Clang_FOUND}")

if(NOT Clang_FOUND)
    message(FATAL_ERROR "ClangConfig.cmake not found! Set Clang_DIR manually.")
endif()

get_property(CLANG_TARGETS DIRECTORY PROPERTY IMPORTED_TARGETS)
set(AUTO_CLANG_LIBS "")

# There HAS to be a better way to do this...
foreach(TGT ${CLANG_TARGETS})
    if(TGT MATCHES "clang[A-Z]")
        list(APPEND AUTO_CLANG_LIBS ${TGT})
    endif()
endforeach()

message(STATUS "Detected Clang Targets: ${AUTO_CLANG_LIBS}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCE_FILES
        ReflectionTypes.cpp
        ReflectionTypes.hpp
        ReflectVisitor.cpp
        ReflectVisitor.hpp
        ReflectionDataSerializer.cpp
        ReflectionDataSerializer.hpp
        ReflectConsumer.cpp
        ReflectConsumer.hpp
        ReflectPluginAction.cpp
        ReflectPluginAction.hpp)

add_library(ReflectClangPlugin SHARED ${SOURCE_FILES})

target_link_libraries(ReflectClangPlugin PRIVATE ${AUTO_CLANG_LIBS})