cmake_minimum_required(VERSION 3.16)
project(ReflectionInlineTest LANGUAGES C)

# Sample code is built into an object library, this way we can link this and the reflection.dat file
add_library(inline_project OBJECT inline.c)

# inline.c -> inline_project built -> merge.py runs to gen reflection.dat -> reflection.o is compiled
add_custom_command(
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/reflection.dat.o"
        COMMAND
        python3 "${CMAKE_CURRENT_SOURCE_DIR}/../../merge/merge.py" ${CMAKE_CURRENT_BINARY_DIR} ./

        COMMAND
        "${CMAKE_C_COMPILER}" -c
        "reflection.dat.S"
        -o "reflection.dat.o"
        DEPENDS
        $<TARGET_OBJECTS:inline_project>
        "${CMAKE_CURRENT_SOURCE_DIR}/../../merge/merge.py"
        WORKING_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT
        "Generating reflection.dat.o via merge.py"
)

# creates final executable and includes reflection.dat as an object
add_executable(inline_sample
        reflection.dat.o)
target_link_libraries(inline_sample PRIVATE inline_project reflect)