# ReflectC
[![Build](https://github.com/abcabcjr/ReflectC/actions/workflows/default.yml/badge.svg)](https://github.com/abcabcjr/ReflectC/actions/workflows/default.yml)

Pet project for adding reflection in C.

WIP

## About

Implements reflection in C through a Clang plugin that dumps struct, union and enum type data. Type info is merged with a python script into a type data table that is loaded at runtime.

## Building plugin & library

Library has been tested on MacOS and Linux.

### CMake

**Plugin**

```bash
git https://github.com/abcabcjr/ReflectC.git
cd ReflectC/plugin
mkdir build && cd build/
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build .
```

**Library**

```bash
cd ReflectC
mkdir build && cd build/
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build .
./examples/sample/sample
```

## Integrating the library

**Loading the plugin**

```cmake
set(PLUGIN_PATH "${CMAKE_SOURCE_DIR}/plugin/build/libReflectClangPlugin.${PLUGIN_EXT}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -load -Xclang ${PLUGIN_PATH} -Xclang -add-plugin -Xclang reflect-clang-plugin")
```

**Necessary setup**

The simplest build setup involves running the type info merge script post build (see [example](https://github.com/abcabcjr/ReflectC/tree/main/examples/sample)):

```cmake
add_custom_command(
        TARGET sample
        POST_BUILD
        COMMAND python3 "${CMAKE_SOURCE_DIR}/merge/merge.py" ${CMAKE_CURRENT_BINARY_DIR} ./
)
```

Alternatively, the type table can be merged prior to linking and converted to an object file that is embedded directly into the final executable. See example [inlinetest](https://github.com/abcabcjr/ReflectC/tree/main/examples/inlinetest).

## Usage

```c
#include <stdio.h>
#include <reflect.h>

struct reflect_struct {
    int a;
    int b;
    int c;
};

enum reflect_enum {
    REFLECT_A = 1,
    REFLECT_B = 2,
    REFLECT_C = 3
};

int main() {
    // In linked table mode
    reflect_load();

    // Manual load mode (read from reflection.dat file)
    //reflect_load_bytes(bytes_from_file, true);

    // get type info by name
    const type_info_t* struct_type = reflect_type_info_from_name("reflect_struct");

    // allocate a dynamic struct
    struct reflect_struct* instance = reflect_alloc(struct_type, NULL, NULL);

    int a = *(int*)reflect_get_field(instance, "a"); // get the value of a field by name
    int b = *(int*)reflect_get_field_manual(instance, "b", struct_type); // works for structs not allocated using reflect_alloc() too!

    // get info on fields
    field_info_t* field_type = reflect_get_field_type(struct_type, "a")
    printf("%s\n", field_type->type_ptr->name)

    for (const field_info_t* field = reflect_field_info_iter_begin(struct_type); field != reflect_field_info_iter_end(struct_type); field++) {
        // Iterate through the fields of a struct
        // Nested fields are also included (e.g. nested.a.b)
        printf("%s : %s\n", field->name, field->type_ptr->name);
    }

    // access enum values
    const type_info_t* enum_type = reflect_type_info_from_name("reflect_enum");
    const int reflect_a = *reflect_get_enum_value(enum_type, "REFLECT_A");

    reflect_free(instance, NULL, NULL);
    return 0;
}
```

## TODO List

- Flexible arrays
- Better function pointer field support