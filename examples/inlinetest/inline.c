#include <stdio.h>
#include <string.h>
#include <reflect.h>

typedef struct {
    int a;
    int b;
    int d;
} struct_test_t;

int main(void) {
    reflect_load();

    const type_info_t* int_type = reflect_type_info_from_name("int");
    int* dynamic_int_value = reflect_alloc(int_type, NULL, NULL);

    *dynamic_int_value = 1337;

    printf("%d\n", *dynamic_int_value);

    reflect_free(dynamic_int_value, NULL, NULL);

    const type_info_t* hotfix_struct_t = reflect_type_info_from_name("struct_test_t");

    struct_test_t* struct_test = reflect_alloc(hotfix_struct_t, NULL, NULL);

    *(int*)reflect_get_field(struct_test, "a") = 1;
    *(int*)reflect_get_field(struct_test, "b") = 2;
    *(int*)reflect_get_field(struct_test, "d") = 3;

    printf("%d\n", *(int*)reflect_get_field(struct_test, "a"));
    printf("%d %d %d\n", struct_test->a, struct_test->b, struct_test->d);

    reflect_free(struct_test, NULL, NULL);
}