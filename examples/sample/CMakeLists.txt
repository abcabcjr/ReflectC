cmake_minimum_required(VERSION 3.16)
project(ReflectionTest LANGUAGES C)

add_executable(sample sample.c)
target_link_libraries(sample PRIVATE reflect)

add_custom_command(
        TARGET sample
        POST_BUILD
        COMMAND python3 "${CMAKE_SOURCE_DIR}/merge/merge.py" ${CMAKE_BINARY_DIR} ./
)