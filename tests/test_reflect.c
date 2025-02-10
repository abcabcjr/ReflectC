#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <reflect.h>

typedef enum {
    ENUM_ONE = 1,
    ENUM_TWO = 2,
    ENUM_THREE = 3
} enum_test_t;

typedef struct {
    int a;
    int b;
    enum_test_t e; // use so it does not get stripped
} struct_test_t;

typedef enum {
    NEW_ENUM_A = 10,
    NEW_ENUM_B = 20
} new_enum_t;

typedef struct {
    int x;
    new_enum_t e;
} nested_struct_t;

typedef struct {
    int matrix[2][3];    // 2D array => arr_size = 6
    int** double_ptr;    // pointer-to-pointer => ptr_depth = 2
    nested_struct_t nest;
} struct_2d_t;

typedef union {
    int i;
    float f;
    char c[8];
} union_test_t;

struct ReflectTypedefAliasTest {
    int a;
};

typedef struct ReflectTypedefAliasTest reflect_typedef_alias_test;

/* We reference them in code so the linker won't discard them. */
static struct_test_t    global_test_s;
static struct_2d_t      global_2d_struct;
static union_test_t     global_u;
static reflect_typedef_alias_test reflect_type_alias;

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
    assert(struct_info != NULL);

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
    assert(enum_info->variant == Enum);

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

void test_complex_fields() {
    const type_info_t* t2d_info = reflect_type_info_from_name("struct_2d_t");
    assert(t2d_info != NULL);
    assert(t2d_info->variant == Struct);

    const field_info_t* it  = reflect_field_info_iter_begin(t2d_info);
    const field_info_t* end = reflect_field_info_iter_end(t2d_info);

    size_t found_matrix = 0;
    size_t found_double_ptr = 0;
    size_t found_nest_x = 0;
    size_t found_nest_e = 0;

    for (; it != end; ++it) {
        if (strcmp(it->name, "matrix") == 0) {
            found_matrix = 1;
            assert(it->arr_size == 6); // 2x3 => 6
            assert(it->ptr_depth == 0);
        }
        else if (strcmp(it->name, "double_ptr") == 0) {
            found_double_ptr = 1;
            assert(it->arr_size == 0);
            assert(it->ptr_depth == 2); // pointer-to-pointer
        }
        else if (strcmp(it->name, "nest.x") == 0) {
            found_nest_x = 1;
        }
        else if (strcmp(it->name, "nest.e") == 0) {
            found_nest_e = 1;
        }
    }

    assert(found_matrix);
    assert(found_double_ptr);
    assert(found_nest_x);
    assert(found_nest_e);

    /* Allocate one */
    struct_2d_t* s2d = reflect_alloc(t2d_info, NULL, NULL);
    assert(s2d != NULL);

    int* matrix_ptr = (int*)reflect_get_field(s2d, "matrix");
    for (int i = 0; i < 6; i++) {
        matrix_ptr[i] = i + 10;
    }
    assert(s2d->matrix[0][0] == 10);
    assert(matrix_ptr[5] == 15);

    /* nested.x and nested.e */
    *(int*)reflect_get_field(s2d, "nest.x") = 999;
    assert(*(int*)reflect_get_field(s2d, "nest.x") == 999);

    *(new_enum_t*)reflect_get_field(s2d, "nest.e") = NEW_ENUM_B;
    assert(*(new_enum_t*)reflect_get_field(s2d, "nest.e") == NEW_ENUM_B);

    reflect_free(s2d, NULL, NULL);

    printf("✅ test_complex_fields passed!\n");
}

void test_new_enum_reflection() {
    const type_info_t* enum_info = reflect_type_info_from_name("new_enum_t");
    assert(enum_info != NULL);
    assert(enum_info->variant == Enum);

    const size_t* valA = reflect_get_enum_value(enum_info, "NEW_ENUM_A");
    assert(valA != NULL && *valA == 10);

    const size_t* valB = reflect_get_enum_value(enum_info, "NEW_ENUM_B");
    assert(valB != NULL && *valB == 20);

    printf("✅ test_new_enum_reflection passed!\n");
}

void test_enum_field_iteration() {
    const type_info_t* enum_info = reflect_type_info_from_name("enum_test_t");
    assert(enum_info != NULL);
    assert(enum_info->variant == Enum);

    const enum_field_info_t* it  = reflect_enum_info_iter_begin(enum_info);
    const enum_field_info_t* end = reflect_enum_info_iter_end(enum_info);

    size_t count_found = 0;
    for (; it != end; ++it) {
        printf(" enum_test_t enumerator: %s => %zu\n", it->name, it->value);
        if (strcmp(it->name, "ENUM_ONE") == 0) {
            assert(it->value == 1);
        }
        else if (strcmp(it->name, "ENUM_TWO") == 0) {
            assert(it->value == 2);
        }
        else if (strcmp(it->name, "ENUM_THREE") == 0) {
            assert(it->value == 3);
        }
        count_found++;
    }

    assert(count_found == 3);

    printf("✅ test_enum_field_iteration passed!\n");
}

void test_union_reflection() {
    const type_info_t* union_info = reflect_type_info_from_name("union_test_t");
    assert(union_info != NULL);
    assert(union_info->variant == Union);

    const field_info_t* it  = reflect_field_info_iter_begin(union_info);
    const field_info_t* end = reflect_field_info_iter_end(union_info);

    int found_i = 0;
    int found_f = 0;
    int found_c = 0;
    for (; it != end; ++it) {
        printf(" union_test_t field '%s': offset=%zu arr_size=%zu\n",
               it->name, it->offset, it->arr_size);

        if (strcmp(it->name, "i") == 0) {
            found_i = 1;
        } else if (strcmp(it->name, "f") == 0) {
            found_f = 1;
        } else if (strcmp(it->name, "c") == 0) {
            found_c = 1;
            assert(it->arr_size == 8); // 'char c[8]'
        }

        assert(it->offset == 0);
    }

    assert(found_i && found_f && found_c);

    union_test_t* u = reflect_alloc(union_info, NULL, NULL);
    assert(u != NULL);

    *(int*)reflect_get_field(u, "i") = 1234;
    assert(*(int*)reflect_get_field(u, "i") == 1234);

    *(float*)reflect_get_field(u, "f") = 3.14f;
    assert(*(float*)reflect_get_field(u, "f") == 3.14f);

    char* cval = (char*)reflect_get_field(u, "c");
    strcpy(cval, "abc");
    assert(strcmp(cval, "abc") == 0);

    reflect_free(u, NULL, NULL);

    printf("✅ test_union_reflection passed!\n");
}

void test_aliases() {
    const type_info_t* first = reflect_type_info_from_name("reflect_typedef_alias_test");
    const type_info_t* second = reflect_type_info_from_name("ReflectTypedefAliasTest");

    assert(first != NULL && first == second);
    assert(strcmp(first->name, "ReflectTypedefAliasTest") == 0);

    printf("✅ test_aliases passed!\n");
}

int main() {
    reflect_load();

    test_type_info();
    test_alloc_free();
    test_enum_reflection();
    test_field_info();

    test_complex_fields();
    test_new_enum_reflection();

    test_enum_field_iteration();
    test_union_reflection();

    test_aliases();

    printf("🎉 All tests passed!\n");
    return 0;
}