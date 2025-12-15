#include "../src/file.h"
#include <openssl/md5.h>
#include <stdio.h>
void print_hash(unsigned char hash[MD5_DIGEST_LENGTH]) {
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    printf("%02x", hash[i]);
  }
}


int main() {
  htable_t ht;
  init_htable(&ht);

  // Creating a new tfile using a new file you already have ownership of.
  // Will return NULL if the hash is already in the table, or if it fails.
  tfile_def_t tf;
  if (!new_tfile(&ht, &tf, "./tests/sample.txt", "Test for file setup")) {
    printf("generating tfile failed");
  }

  // Print the information generated about the tfile.
  printf("%s\n", tf.name);
  printf("File size: %ld\n", tf.size);
  printf("File Hash: ");
  print_hash(tf.f_hash);
  printf("\n");
  printf("Chunk Hashes:\n");
  for (int i = 0; i < NUM_CHUNKS; i++) {
    print_hash(tf.c_hashes[i]);
    printf("\n");
  }

  void* start_location = NULL;
  void* next_location = NULL;
  int next_chunk = 5;
  off_t s_size = open_file(&ht, &start_location, tf.f_hash, 0);
  off_t n_size = open_file(&ht, &next_location, tf.f_hash, next_chunk);
  if (start_location == NULL | next_location == NULL) {
    printf("Memory mapping failed :(\n");
  } else {
    printf("Starting character -> %c\n Size -> %ld\n", *(char*)start_location, s_size);
    printf("Chunk %d starting character -> %c\n Size -> %ld\n", next_chunk, *(char*)next_location, n_size);
    printf("Distance between: %ld + %ld = %ld\n", next_location-start_location, n_size, next_location - start_location + n_size);
  }

  char c = *(char*)next_location;
  printf("Verification result: %x\n", verify_tfile(&ht, tf.f_hash));
  *(char*)next_location = 'A';
  printf("Changed starting character of chunk %d to %c\n", next_chunk, *(char*)next_location);
  printf("Verification result after edit: %x\n", verify_tfile(&ht, tf.f_hash));
  *(char*)next_location = c;


  // Now test adding a tfile_def to the hash table.
  printf("\n");

  tfile_def_t tdef = {
    .name = "Empty file test",
    .f_hash = "0123456789abcde",
    .c_hashes = {"qwertyuiopasdfg", "qwertyuiopasdfg", "qwertyuiopasdfg", "qwertyuiopasdfg", "qwertyuiopasdfg", "qwertyuiopasdfg", "qwertyuiopasdfg", "qwertyuiopasdfg"},
    .size = 1024
  };

  unsigned char hash[MD5_DIGEST_LENGTH] = "0123456789abcde";
  tfile_t* tfd = add_tfile(&ht, tdef);
  printf("%s\n", tfd->name);
  printf("File size: %ld\n", tfd->size);
  printf("Location: %s\n", tfd->f_location);
  printf("File Hash: ");
  print_hash(tfd->f_hash);
  printf("\n");
  printf("Chunk Hashes:\n");
  for (int i = 0; i < NUM_CHUNKS; i++) {
    print_hash(tfd->c_hashes[i]);
    printf("\n");
  }
  printf("Verification result: %02x\n", verify_tfile(&ht, hash));



  return 0;
}
