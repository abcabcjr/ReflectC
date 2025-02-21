cmake_minimum_required(VERSION 3.16)
project(ReflectionBenchmarkTest LANGUAGES C)

# Sample code is built into an object library, this way we can link this and the reflection.dat file
add_library(benchmark_project OBJECT benchmarks.c)

# inline.c -> inline_project built -> merge.py runs to gen reflection.dat -> reflection.o is compiled
add_custom_command(
        OUTPUT reflection.dat.o
        COMMAND
        "${CMAKE_C_COMPILER}" -c
        "${CMAKE_SOURCE_DIR}/examples/benchmark/reflectdata/reflection.dat.S"
        -o "reflection.dat.o"
        DEPENDS
        $<TARGET_OBJECTS:benchmark_project>
        "${CMAKE_CURRENT_SOURCE_DIR}/../../merge/merge.py"
        WORKING_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT
        "Generating reflection.dat.o via merge.py"
)

# creates final executable and includes reflection.dat as an object
add_executable(benchmark
        reflection.dat.o)
target_link_libraries(benchmark PRIVATE benchmark_project reflect)