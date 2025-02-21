add_library(test_lib OBJECT test_reflect.c)

add_custom_command(
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/reflection.dat.o"
        COMMAND
        python3 "${CMAKE_CURRENT_SOURCE_DIR}/../merge/merge.py" ${CMAKE_CURRENT_BINARY_DIR} ./

        COMMAND
        "${CMAKE_C_COMPILER}" -c
        "reflection.dat.S"
        -o "reflection.dat.o"
        DEPENDS
        $<TARGET_OBJECTS:test_lib>
        "${CMAKE_CURRENT_SOURCE_DIR}/../merge/merge.py"
        WORKING_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT
        "Generating reflection.dat.o via merge.py"
)

add_executable(test_reflect
        reflection.dat.o)
target_link_libraries(test_reflect PRIVATE test_lib reflect)

add_test(NAME ReflectionTests COMMAND test_reflect)