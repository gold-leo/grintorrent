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

  // Next things to make:
  // Return what chunks are available given a hash.
  //  - Check mmap pointers
  //  - Check filepath
  // Add a tfile based off of a tfile_def
  // Open a mem segment for a file and return a pointer and size of a chunk.
  //  - Check mmap pointers
  //  - If none exist, mmap and create them
  //  - Share array of pointers back
  // Close the mmap pointers
  //  - Check if pointers exist
  //  - If they do, unmmap/save
  //  - NULL the pointers
  // Return a pointer to the start of a tfile_def array.
  //  - Index through hash table
  //  - Convert all tfiles to tfile_defs
  //


  return 0;
}
