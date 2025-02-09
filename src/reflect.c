#include "reflect.h"

#include <stddef.h>
#include <stdint.h>

#include "hashtable.c"
#include "reader.c"

#define REFLECT_DYNAMIC_ALLOC_MAGIC 0x75757575
#define REFLECT_TYPE_INFO_INTERNAL_SIZE (sizeof(type_info_internal) - sizeof(type_info_t))

// A type header is added if reflect_alloc(), this is to prevent breakages if
// reflect_get_type_info is called on data not initialized this way
typedef struct {
    uint32_t magic;
    const type_info_t* type_info_ptr;
    size_t size;
    bool is_ptr;
} reflect_type_header_t;

typedef struct {
    union {
        field_info_t* struct_fields;
        enum_field_info_t* enum_fields;
    };
    hashtable_t field_table;
    type_info_t type;
} type_info_internal;

static bool is_init = false;
static type_info_internal* type_table = NULL;
static hashtable_t type_hash_table = {
    .data = NULL,
    .capacity = 0
};

type_info_internal* get_internal_from_type_info(const type_info_t* type_info) {
    return (type_info_internal*)((char*)type_info - REFLECT_TYPE_INFO_INTERNAL_SIZE);
}

type_info_t* add_base_type_info(const char* name, const size_t id, const size_t size) {
    type_table[id].type.id = id;
    type_table[id].type.name = name;
    type_table[id].type.size = size;
    type_table[id].type.variant = Base;
    type_table[id].type.field_count = 0;
    type_table[id].struct_fields = NULL;

    hashtable_insert(&type_hash_table, &(hash_t){
        .name = name,
        .id = id
    });

    return &type_table[id].type;
}

type_info_t* add_struct_type_info(const char* name, const size_t id, const size_t size, uint8_t variant, const size_t field_count) {
    type_table[id].type.id = id;
    type_table[id].type.name = name;
    type_table[id].type.size = size;
    type_table[id].type.variant = variant;
    type_table[id].type.field_count = 0;
    type_table[id].struct_fields = malloc(sizeof(field_info_t) * field_count);
    type_table[id].field_table = hashtable_create(field_count * 2);

    hashtable_insert(&type_hash_table, &(hash_t){
        .name = name,
        .id = id
    });

    return &type_table[id].type;
}

type_info_t* add_enum_type_info(const char* name, const size_t id, const size_t size, const size_t field_count) {
    type_table[id].type.id = id;
    type_table[id].type.name = name;
    type_table[id].type.size = size;
    type_table[id].type.variant = Enum;
    type_table[id].type.field_count = 0;
    type_table[id].enum_fields = malloc(sizeof(enum_field_info_t) * field_count);
    type_table[id].field_table = hashtable_create(field_count * 2);

    hashtable_insert(&type_hash_table, &(hash_t){
        .name = name,
        .id = id
    });

    return &type_table[id].type;
}

void add_struct_field_type(type_info_t* struct_type, const char* field_name, const size_t field_type, const size_t size, const size_t offset, const uint32_t ptr_depth, const bool is_const) {
    const field_info_t field_info = {
        .name = field_name,
        .arr_size = size,
        .offset = offset,
        .type_ptr = (type_info_t*)((char*)(type_table + field_type) + REFLECT_TYPE_INFO_INTERNAL_SIZE),
        .is_const = is_const,
        .ptr_depth = ptr_depth,
    };

    get_internal_from_type_info(struct_type)->struct_fields[struct_type->field_count] = field_info;

    hashtable_insert(&get_internal_from_type_info(struct_type)->field_table, &(hash_t){
        .name = field_name,
        .id = struct_type->field_count++
    });
}

void add_enum_field_type(type_info_t* enum_type, const char* field_name, size_t value) {
    const enum_field_info_t field_info = {
        .name = field_name,
        .value = value
    };

    get_internal_from_type_info(enum_type)->enum_fields[enum_type->field_count] = field_info;

    hashtable_insert(&get_internal_from_type_info(enum_type)->field_table, &(hash_t){
        .name = field_name,
        .id = enum_type->field_count++
    });
}

void reflect_load_bytes(char* reflection_metadata) {
    if (is_init)
        return;

    is_init = true;
    reader_t reader = { .data = reflection_metadata, .offset = 0 };

    const size_t type_count = read_size_t(&reader);

    type_table = malloc((type_count+1) * sizeof(type_info_internal));
    type_hash_table = hashtable_create(type_count * 2);

    for (size_t i = 0; i < type_count; i++) {
        const size_t id = read_size_t(&reader);
        const uint8_t variant = read_byte(&reader);
        const char* name = read_string(&reader);
        const size_t size = read_size_t(&reader);

        if (variant == Base) {
            add_base_type_info(name, id, size);
        } else if (variant == Struct || variant == Union) {
            const size_t field_count = read_size_t(&reader);
            type_info_t* struct_type = add_struct_type_info(name, id, size, variant, field_count);

            for (size_t j = 0; j < field_count; j++) {
                const char* field_name = read_string(&reader);
                const bool is_const = read_bool(&reader);
                const uint32_t ptr_depth = read_uint32_t(&reader);
                const size_t offset = read_size_t(&reader);
                const size_t field_size = read_size_t(&reader);
                const size_t field_type = read_size_t(&reader);

                add_struct_field_type(struct_type, field_name, field_type, field_size, offset, ptr_depth, is_const);
            }
        } else if (variant == Enum) {
            const size_t field_count = read_size_t(&reader);
            type_info_t* enum_type = add_enum_type_info(name, id, size, field_count);

            for (size_t j = 0; j < field_count; j++) {
                const char* field_name = read_string(&reader);
                const size_t value = read_size_t(&reader);

                add_enum_field_type(enum_type, field_name, value);
            }
        }
    }
}

// For linked reflection.dat use

#ifdef __APPLE__
extern __attribute__((weak)) const char reflection_dat_start[] = "";
#define REFLECTION_DATA_SYMBOL reflection_dat_start
#else
extern __attribute__((weak)) const char _reflection_dat_start[] = "";
#define REFLECTION_DATA_SYMBOL _reflection_dat_start
#endif

void reflect_load() {
    reflect_load_bytes(REFLECTION_DATA_SYMBOL);
}

const type_info_t* reflect_type_info_from_name(const char* name) {
    const size_t result = hashtable_get(&type_hash_table, name);

    if (result == -1)
        return NULL;

    return &type_table[result].type;
}

const type_info_t* reflect_get_type_info(const void* ptr) {
    if (ptr == NULL)
        return NULL;

    const reflect_type_header_t* header = ((const reflect_type_header_t*)ptr) - 1;

    if (header->magic != REFLECT_DYNAMIC_ALLOC_MAGIC)
        return NULL;

    return header->type_info_ptr;
}

void* reflect_alloc(const type_info_t* type, void* allocator, void*(*alloc)(void*, size_t)) {
    if (type == NULL)
        return NULL;

    reflect_type_header_t* allocated = NULL;

    if (alloc == NULL)
        allocated = malloc(sizeof(reflect_type_header_t) + type->size);
    else allocated = alloc(allocator, sizeof(reflect_type_header_t) + type->size);

    if (allocated == NULL)
        return NULL;

    allocated->magic = REFLECT_DYNAMIC_ALLOC_MAGIC;
    allocated->type_info_ptr = type;

    return allocated + 1;
}

void reflect_free(void* ptr, void* allocator, void (*free_func)(void*, void*)) {
    if (ptr == NULL)
        return;

    reflect_type_header_t* header = ((reflect_type_header_t*)ptr) - 1;

    if (free_func == NULL)
        free(header);
    else
        free_func(allocator, header);
}

void* reflect_get_field_manual(void* struct_ptr, const char* field_name, const type_info_t* type_info) {
    if (struct_ptr == NULL || type_info == NULL)
        return NULL;

    const size_t id = hashtable_get(&get_internal_from_type_info(type_info)->field_table, field_name);

    if (id == -1)
        return NULL;

    const field_info_t* field_info = get_internal_from_type_info(type_info)->struct_fields + id;

    return struct_ptr + field_info->offset;
}

void* reflect_get_field(void* struct_ptr, const char* field_name) {
    const type_info_t* struct_type = reflect_get_type_info(struct_ptr);

    if (struct_type == NULL)
        return NULL;

    const size_t id = hashtable_get(&get_internal_from_type_info(struct_type)->field_table, field_name);

    if (id == -1)
        return NULL;

    const field_info_t* field_info = get_internal_from_type_info(struct_type)->struct_fields + id;

    return struct_ptr + field_info->offset;
}

const field_info_t* reflect_get_field_type(const type_info_t* type, const char* field_name) {
    if (type == NULL)
        return NULL;

    const size_t id = hashtable_get(&get_internal_from_type_info(type)->field_table, field_name);

    if (id == -1)
        return NULL;

    return get_internal_from_type_info(type)->struct_fields + id;
}

field_info_t* reflect_field_info_iter_begin(const type_info_t* type_info) {
    if (type_info == NULL)
        return NULL;

    return get_internal_from_type_info(type_info)->struct_fields;
}

field_info_t* reflect_field_info_iter_end(const type_info_t* type_info) {
    if (type_info == NULL)
        return NULL;

    return get_internal_from_type_info(type_info)->struct_fields + type_info->field_count;
}

const size_t *reflect_get_enum_value(const type_info_t *enum_type, const char *field_name) {
    if (enum_type == NULL)
        return NULL;

    if (enum_type->variant != Enum)
        return NULL;

    const size_t id = hashtable_get(&get_internal_from_type_info(enum_type)->field_table, field_name);

    if (id == -1)
        return NULL;

    const enum_field_info_t* result = get_internal_from_type_info(enum_type)->enum_fields + id;

    return &result->value;
}

enum_field_info_t* reflect_enum_info_iter_begin(const type_info_t* enum_type) {
    if (enum_type == NULL || enum_type->variant != Enum) {
        return NULL;
    }

    return get_internal_from_type_info(enum_type)->enum_fields;
}

enum_field_info_t* reflect_enum_info_iter_end(const type_info_t* enum_type) {
    if (enum_type == NULL || enum_type->variant != Enum) {
        return NULL;
    }

    return get_internal_from_type_info(enum_type)->enum_fields + enum_type->field_count;
}