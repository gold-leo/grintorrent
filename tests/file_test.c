#include "../src/file.h"
#include <stdio.h>

int main() {
  htable_t ht;
  init_htable(&ht);

  // Creating a new tfile using a new file you already have ownership of.
  // Will return NULL if the hash is already in the table, or if it fails.
  tfile_t* tf = new_tfile(&ht, "./tests/sample.txt", "Test for file setup");
  if (tf == NULL) {
    printf("Failed to create new tfile");
  }

  printf("%s\n", tf->name);
  printf("File size: %ld\n", tf->size);
  printf("Location: %s\n", tf->f_location);
  printf("File Hash: %016lx", *(uint64_t*)tf->f_hash);
  printf("%016lx\n", *(((uint64_t*)tf->f_hash) + 1));
  printf("Chunk Hashes:\n");
  for (int i = 0; i < NUM_CHUNKS; i++) {
    printf("%lx", *(uint64_t*)tf->c_hashes[i]);
    printf("%lx\n", *(((uint64_t*)tf->c_hashes[i]) + 1));
  }

  void* start_location = NULL;
  void* next_location = NULL;
  int next_chunk = 7;
  off_t s_size = chunk_location(&ht, &start_location, tf->f_hash, 0);
  off_t n_size = chunk_location(&ht, &next_location, tf->f_hash, next_chunk);
  if (start_location == NULL | next_location == NULL) {
    printf("Memory mapping failed :(\n");
  } else {
    printf("Starting character -> %c\n Size -> %ld\n", *(char*)start_location, s_size);
    printf("Chunk %d starting character -> %c\n Size -> %ld\n", next_chunk, *(char*)next_location, n_size);
    printf("Distance between: %ld + %ld = %ld\n", next_location-start_location, n_size, next_location - start_location + n_size);
  }

  printf("Verification result: %x\n", verify_tfile(&ht, tf->f_hash));

  return 0;
}
