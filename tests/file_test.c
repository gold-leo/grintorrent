#include "../src/file.h"
#include <stdio.h>

int main() {
  htable_t ht;
  create_htable(&ht);

  tfile_t* tf = new_tfile(&ht, "./tests/sample.txt", "Test for file setup");
  if (tf == NULL) {
    printf("Failed to create new tfile");
  }

  printf("%s\n", tf->name);
  printf("Location: %s\n", tf->f_location);
  printf("File Hash: %lx", *(uint64_t*)tf->f_hash);
  printf("%lx\n", *(((uint64_t*)tf->f_hash) + 1));
  printf("Minimum chunk size: %ld\n", tf->c_minsize) ;
  printf("Chunk Hashes:\n");
  for (int i = 0; i < NUM_CHUNKS-1; i++) {
    printf("%lx", *(uint64_t*)tf->c_hashes[i]);
    printf("%lx\n", *(((uint64_t*)tf->c_hashes[i]) +1));
  }

  return 0;
}
