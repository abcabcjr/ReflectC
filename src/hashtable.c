#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// TODO: should probably think about this more

typedef struct {
    const char* name;
    size_t id; // ID of typeinfo or field info
} hash_t;

typedef struct HashBucket {
    hash_t data;
    struct HashBucket* next;
} hash_bucket_t;

typedef struct {
    hash_bucket_t* data;
    size_t capacity;
} hashtable_t;

static uint32_t hash_fnv1a(const char *key) {
    uint32_t hash = 2166136261u;
    size_t i = 0;
    while (key[i]) {
        hash ^= (uint8_t)key[i];;
        hash *= 16777619u;
        i++;
    }
    return hash;
}

static hashtable_t hashtable_create(const size_t capacity) {
    const hashtable_t ht = {
        .capacity = capacity,
        .data = calloc(capacity, sizeof(hash_bucket_t)),
    };

    return ht;
}

static size_t hashtable_get(const hashtable_t *ht, const char *key) {
    const uint32_t hash_key = hash_fnv1a(key) % ht->capacity;

    hash_bucket_t* bucket = ht->data + hash_key;

    while (bucket != NULL) {
        if (bucket->data.name == NULL)
            return -1;

        if (strcmp(bucket->data.name, key) == 0)
            return bucket->data.id;

        bucket = bucket->next;
    }

    return -1;
}

static void hashtable_insert(const hashtable_t* ht, const hash_t* value) {
    const uint32_t hash_key = hash_fnv1a(value->name) % ht->capacity;

    if (ht->data[hash_key].data.name == NULL) {
        // new entry
        ht->data[hash_key].data = *value;
        ht->data[hash_key].next = NULL;
    } else {
        hash_bucket_t* entry = &ht->data[hash_key];

        while (entry != NULL) {
            // entry already exists
            if (strcmp(entry->data.name, value->name) == 0) {
                entry->data = *value;
                return;
            }

            // chain new entry
            if (entry->next == NULL) {
                entry->next = malloc(sizeof(hash_bucket_t));
                entry->next->data = *value;
                entry->next->next = NULL;
                return;
            }

            entry = entry->next;
        }

    }
}