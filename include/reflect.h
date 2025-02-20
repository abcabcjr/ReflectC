#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    Base = 1,
    Struct = 2,
    Union = 3,
    Enum = 4
} reflect_obj_type_t;

struct ReflectHashTable;

// Front facing API type data
typedef struct {
    const char* name;
    size_t id;
    size_t size;
    size_t field_count;
    reflect_obj_type_t variant;
} type_info_t;

typedef struct {
    const char* name;
    const type_info_t* type_ptr;
    size_t arr_size; // number of elements if array
    size_t offset;
    uint32_t ptr_depth;
    bool is_const;
} field_info_t;

typedef struct {
    const char* name;
    size_t value;
} enum_field_info_t;

typedef union {
    field_info_t field;
    enum_field_info_t enum_field;
} base_field_info_t;

void reflect_load();
void reflect_load_bytes(char* reflection_metadata, bool copy);

void* reflect_alloc(const type_info_t* type, void* allocator, void*(*alloc)(void*, size_t));
void reflect_free(void* ptr, void* allocator, void (*free_func)(void*, void*));

const type_info_t* reflect_type_info_from_name(const char* name);
const type_info_t* reflect_get_type_info(const void* ptr);

void* reflect_get_field(void* struct_ptr, const char* field_name);
void* reflect_get_field_manual(void* struct_ptr, const char* field_name, const type_info_t* type_info);

const field_info_t* reflect_get_field_type(const type_info_t* type, const char* field_name);

field_info_t* reflect_field_info_iter_begin(const type_info_t* type_info);
field_info_t* reflect_field_info_iter_end(const type_info_t* type_info);

const size_t* reflect_get_enum_value(const type_info_t *enum_type, const char *field_name);
enum_field_info_t* reflect_enum_info_iter_begin(const type_info_t* enum_type);
enum_field_info_t* reflect_enum_info_iter_end(const type_info_t* enum_type);

/* WebAssembly hotreloading by copying state */
void* reflect_hotreload_get_state_ptr();