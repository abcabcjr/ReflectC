#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char* data;
    size_t offset;
    bool copy;
} reader_t;

static uint8_t read_byte(reader_t* reader) {
    reader->offset++;
    return reader->data[reader->offset-1];
}

static size_t read_size_t(reader_t* reader) {
    reader->offset += sizeof(size_t);
    return *((size_t*)(reader->data+reader->offset-sizeof(size_t)));
}

static uint32_t read_uint32_t(reader_t* reader) {
    reader->offset += sizeof(uint32_t);
    return *((uint32_t*)(reader->data+reader->offset-sizeof(uint32_t)));
}

static bool read_bool(reader_t* reader) {
    reader->offset++;
    return *((bool*)reader->data+reader->offset-1);
}

static char* read_c_string(reader_t* reader, const size_t offset) {
    if (reader->copy) {
        const size_t prev_offset = reader->offset;
        reader->offset = offset;

        while (reader->data[reader->offset] != 0)
            reader->offset++;
        reader->offset++;

        // realloc
        char* str = malloc(reader->offset-offset);
        memcpy(str, reader->data+offset, reader->offset-offset);

        reader->offset = prev_offset;
        return str;
    }

    return reader->data+offset;
}

static char* read_string(reader_t* reader) {
    const size_t offset = read_size_t(reader);

    return read_c_string(reader, offset);
}