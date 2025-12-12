#include "file.h"
#include <stdio.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// The size of the blocks read into a hash.
#define BLOCK_READ_SIZE 8192

// The size required for a file in bytes.
#define MIN_SIZE 512

// Complete a hash of a section of a file.
// hash must be size of MD5_HASH_DIGEST
// Returns 1 if failed.
int md5_hash(int fd, off_t offset, off_t size, unsigned char* hash) {
  // Set offset to desired offset
  lseek(fd, offset, SEEK_SET);

  // Initialize a MD5
  MD5_CTX c;
  MD5_Init(&c);

  // Read data in block by block.
  // Can probably make this code a bit better
  char data_block[BLOCK_READ_SIZE];
  ssize_t size_read;
  int num_of_iterations = size / BLOCK_READ_SIZE;
  // Skips loop if num_of_iterations = 0.
  for (int i = 0; i < size / BLOCK_READ_SIZE; i++) {
    size_read = read(fd, data_block, BLOCK_READ_SIZE);
    if (size_read != BLOCK_READ_SIZE) {
      perror("Ran into end of file.");
      return 1;
    }
    MD5_Update(&c, &data_block, BLOCK_READ_SIZE);
  }
  // Read the final data
  size_read = read(fd, data_block, size % BLOCK_READ_SIZE);
  if (size_read != size % BLOCK_READ_SIZE) {
    perror("Ran into end of file.");
    return 1;
  }
  MD5_Update(&c, data_block, size % BLOCK_READ_SIZE);

  // Complete the MD5 Hash and place in file_desc.
  MD5_Final(hash, &c);


  return 0;
}

// Find the hash of a file.
int file_hash(char* file_path, unsigned char* hash) {
  // Open the file.
  int fd = open(file_path, O_RDONLY); // Consider adding O_LARGEFILE, talk to Charlie about this
  if (fd == -1) {
    perror("Could not open file");
    return -1;
  }
  struct stat buf;
  fstat(fd, &buf);

  // Calculate the hash.
  md5_hash(fd, 0, buf.st_size, hash);

  // Close the file.
  close(fd);
  return 0;
}

// Create a completely new tfile and add to the hash table.
// Returns the location of the tfile or NULL if failed.
tfile_t* new_tfile(htable_t* htable, char* file_path, char name[32]) {
  // Open the file.
  int fd = open(file_path, O_RDONLY); // Consider adding O_LARGEFILE, talk to Charlie about this
  if (fd == -1) {
    perror("Could not open file");
    return NULL;
  }
  struct stat buf;
  fstat(fd, &buf);

  if (buf.st_size < MIN_SIZE) {
    perror("File too small");
    return NULL;
  }

  // Create new tfile
  tfile_t ntf;

  // Find the file's hash and copy into the tfile.
  md5_hash(fd, 0, buf.st_size, ntf.f_hash);

  // Make sure we don't already have this file's hash in the hash table.
  if (!search_htable(htable, ntf)) {
    perror("File already has a tfile");
    return NULL;
  }

  // Set the chunk size. The very last chunk will vary in size.
  ntf.c_minsize = buf.st_size / NUM_CHUNKS;

  // Calculate the hashes for each chunk.
  // We calculate the LAST chunk FIRST because the chunk is not a fixed size.
  off_t offset = ntf.c_minsize * NUM_CHUNKS-1;                        // The start of the chunk.
  off_t size = buf.st_size - offset;                                  // The size of the chunk.
  md5_hash(fd, offset, size, ntf.c_hashes[NUM_CHUNKS-1]);             // Calculate the hash.

  // Loop for the other chunks.
  for (int i = 0; i < NUM_CHUNKS-1; i++) {
    offset = i * ntf.c_minsize;
    md5_hash(fd, offset, ntf.c_minsize, ntf.c_hashes[i]);
  }

  // Copy the name into the tfile
  memcpy(ntf.name, name, 32);
  // Copy the file path into the tfile.
  // tfile is now the owner of the string.
  // DO NOT free the string.
  ntf.f_location = file_path;

  // Set all chunk locations to NULL
  // (we have the full file)
  for (int i = 0; i < NUM_CHUNKS; i++) {
    ntf.c_locations[i] = NULL;
  }

  // Close the file.
  close(fd);

  // Add to the hash table.
  return add_htable(htable, ntf);
}

// Add an existing tfile (likely from a peer) to the hash table.
int add_tfile(tfile_def_t tfile_def) {

  return 0;
}

// Initializes a tfile_def from a
int init_tfile_def() {
  return 0;
}
