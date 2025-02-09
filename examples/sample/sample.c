#include <stdio.h>
#include <string.h>
#include <reflect.h>
#include <stdlib.h>

typedef struct {
    int a;
    int b;
    int d;
} struct_test_t;

bool load_reflection() {
    const char *filename = "reflection.dat";
    FILE *file = fopen(filename, "rb");

    if (!file) {
        perror("Error opening file");
        return false;
    }

    fseek(file, 0, SEEK_END);
    const long file_size = ftell(file);
    rewind(file);

    if (file_size <= 0) {
        fprintf(stderr, "File is empty or an error occurred\n");
        fclose(file);
        return false;
    }

    char *buffer = malloc(file_size);
    if (!buffer) {
        perror("Error allocating memory");
        fclose(file);
        return false;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "Error reading file\n");
        free(buffer);
        fclose(file);
        return false;
    }

    fclose(file);
    reflect_load_bytes(buffer, true);

    //free(buffer);

    return true;
}

int main(void) {
    if (!load_reflection())
        return 1;

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