#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t offset;
} reader_t;

uint8_t read_byte(reader_t* reader) {
    reader->offset++;
    return reader->data[reader->offset-1];
}

size_t read_size_t(reader_t* reader) {
    reader->offset += sizeof(size_t);
    return *((size_t*)(reader->data+reader->offset-sizeof(size_t)));
}

uint32_t read_uint32_t(reader_t* reader) {
    reader->offset += sizeof(uint32_t);
    return *((uint32_t*)(reader->data+reader->offset-sizeof(uint32_t)));
}

bool read_bool(reader_t* reader) {
    reader->offset++;
    return *((bool*)reader->data+reader->offset-1);
}

char* read_string(reader_t* reader) {
    size_t offset = reader->offset;

    while (reader->data[reader->offset] != 0)
        reader->offset++;
    reader->offset++;

    // realloc
    char* str = malloc(reader->offset-offset);
    memcpy(str, reader->data+offset, reader->offset-offset);

    return str;
}