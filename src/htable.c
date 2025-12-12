#include "file.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// Starting size of the hash table.
// Must be power of 2
#define STARTING_CAPACITY 256

// Initialize hash table.
void create_htable(htable_t* htable) {

  htable->capacity = STARTING_CAPACITY;
  htable->size = 0;
  htable->table = calloc(sizeof(tfile_t), STARTING_CAPACITY);
}

// Destroy hash table.
void free_htable(htable_t* htable) {
  for (int i = 0; i < htable->capacity; i++) {
    free((void*)htable->table[i].f_location);
  }
  free(htable->table);
}

// Add to hash table.
tfile_t* add_htable(htable_t* htable, tfile_t tfile) {
  if (htable->capacity < htable->size * 2) {
    resize_htable(htable);
  }

  // Find the index using the first half of the hash
  uint64_t hash1 = *(uint64_t*)tfile.f_hash;
  uint64_t hash2 = *(((uint64_t*)tfile.f_hash) + 1);
  size_t index = hash1 & (uint64_t)(htable->capacity - 1);

  for (;;) {
    uint64_t test_hash1 = *(uint64_t*)htable->table[index].f_hash;
    uint64_t test_hash2 = *(((uint64_t*)htable->table[index].f_hash) + 1);
    if ((!test_hash1) && (!test_hash2)) {
      htable->table[index] = tfile;
      return &htable->table[index];
    } else if (hash1 == test_hash1 && hash2 == test_hash2) {
      return NULL;
    }

    if (index >= htable->capacity) {
      index = 0;
    } else {
      index++;
    }
  }
}

// Increase size of hash table.
int resize_htable(htable_t* htable) {
  int new_capacity = htable->capacity * 2;
  tfile_t* new_table = calloc(sizeof(tfile_t), new_capacity);
  if (new_table == NULL) {
    return -1;
  }

  for (int i = 0; i < htable->capacity; i++) {
    uint16_t test_hash = *(uint16_t*)htable->table[i].f_hash;
    if (!test_hash) {
      add_htable(htable, htable->table[i]);
    }
  }

  free(htable->table);
  htable->capacity = new_capacity;
  htable->table = new_table;

  return 1;
}

// Search the htable. Return index.
// Return NULL if none is found.
tfile_t* search_htable(htable_t* htable, tfile_t tfile) {
  uint64_t hash1 = *(uint64_t*)tfile.f_hash;
  uint64_t hash2 = *(((uint64_t*)tfile.f_hash) + 1);

  size_t index = hash1 & (uint64_t)(htable->capacity - 1);

  for (;;) {
    uint64_t test_hash1 = *(uint64_t*)htable->table[index].f_hash;
    uint64_t test_hash2 = *(((uint64_t*)htable->table[index].f_hash) + 1);
    if ((!test_hash1) && (!test_hash2)) {
      return NULL;
    } else if (hash1 == test_hash1 && hash2 == test_hash2) {
      return &htable->table[index];
    }

    if (index >= htable->capacity) {
      index = 0;
    } else {
      index++;
    }
  }
}
