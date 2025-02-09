#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <reflect.h>

typedef enum {
    ENUM_ONE = 1,
    ENUM_TWO = 2
} enum_test_t;

typedef struct {
    int a;
    int b;
    enum_test_t e; // use so it does not get stripped
} struct_test_t;

void test_type_info() {
    const type_info_t* int_type = reflect_type_info_from_name("int");
    assert(int_type != NULL);
    assert(int_type->size == sizeof(int));

    const type_info_t* struct_info = reflect_type_info_from_name("struct_test_t");
    assert(struct_info != NULL);
    assert(struct_info->field_count == 3);

    printf("✅ test_type_info passed!\n");
}

void test_alloc_free() {
    const type_info_t* struct_info = reflect_type_info_from_name("struct_test_t");
    struct_test_t* obj = reflect_alloc(struct_info, NULL, NULL);
    assert(obj != NULL);

    *(int*)reflect_get_field(obj, "a") = 42;
    assert(*(int*)reflect_get_field(obj, "a") == 42);

    reflect_free(obj, NULL, NULL);
    printf("✅ test_alloc_free passed!\n");
}

void test_enum_reflection() {
    const type_info_t* enum_info = reflect_type_info_from_name("enum_test_t");
    assert(enum_info != NULL);

    const size_t* value = reflect_get_enum_value(enum_info, "ENUM_ONE");
    assert(value != NULL);
    assert(*value == 1);

    printf("✅ test_enum_reflection passed!\n");
}

void test_field_info() {
    const type_info_t* struct_info = reflect_type_info_from_name("struct_test_t");
    assert(struct_info != NULL);

    const field_info_t* field_a = reflect_get_field_type(struct_info, "a");
    assert(field_a != NULL);
    assert(field_a->offset == 0);
    assert(field_a->arr_size == 0);
    assert(field_a->ptr_depth == 0);

    printf("✅ test_field_info passed!\n");
}

int main() {
    reflect_load();

    test_type_info();
    test_alloc_free();
    test_enum_reflection();
    test_field_info();

    printf("🎉 All tests passed!\n");
    return 0;
}