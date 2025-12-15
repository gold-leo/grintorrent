#include "file.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
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

  // Complete the MD5 Hash.
  MD5_Final(hash, &c);


  return 0;
}

// Create a completely new tfile based off of an existing file on the clients' computer.
// The tfile is added to the hash table.
int new_tfile(htable_t* htable, tfile_def_t* tdef, char* file_path, char name[NAME_LEN]) {
  // Open the file.
  int fd = open(file_path, O_RDONLY); // Consider adding O_LARGEFILE, talk to Charlie about this
  if (fd == -1) {
    perror("Could not open file");
    return -1;
  }
  struct stat buf;
  fstat(fd, &buf);

  if (buf.st_size < MIN_SIZE) {
    perror("File too small");
    return -1;
  }

  md5_hash(fd, 0, buf.st_size, tdef->f_hash);

  tfile_t* ntf = locate_htable(htable, tdef->f_hash);
  if (ntf == NULL) {
    perror("File already has a torrent file");
    return -1;
  }

  // Copy the hash into the tfile.
  memcpy(ntf->f_hash, tdef->f_hash, MD5_DIGEST_LENGTH);

  // Copy the size of the file into the tfile.
  ntf->size = buf.st_size;
  tdef->size = buf.st_size;

  // Set the chunk size. The very last chunk will vary in size.
  off_t c_size = buf.st_size / NUM_CHUNKS;

  // Calculate the hashes for each chunk.
  // We calculate the LAST chunk FIRST because the chunk is not a fixed size.
  off_t offset = c_size * (NUM_CHUNKS-1);                               // The start of the chunk.
  off_t size = buf.st_size - offset;                                   // The size of the chunk.
  md5_hash(fd, offset, size, ntf->c_hashes[NUM_CHUNKS-1]);             // Calculate the hash.
  memcpy(tdef->c_hashes[NUM_CHUNKS-1], ntf->c_hashes[NUM_CHUNKS-1], MD5_DIGEST_LENGTH);

  // Loop for the other chunks.
  for (int i = 0; i < NUM_CHUNKS-1; i++) {
    offset = i * c_size;
    md5_hash(fd, offset, c_size, ntf->c_hashes[i]);
    memcpy(tdef->c_hashes[i], ntf->c_hashes[i], MD5_DIGEST_LENGTH);
  }

  // Close the file.
  close(fd);

  // Copy the name into the tfile
  memcpy(ntf->name, name, NAME_LEN);
  memcpy(tdef->name, name, NAME_LEN);
  // Copy the file path into the tfile.
  // tfile is now the owner of the string.
  // DO NOT free the string.
  int len = strlen(file_path);
  ntf->f_location = malloc(len);
  memcpy(ntf->f_location, file_path, len);

  // Set all chunk locations to NULL
  // (we haven't loaded the file into memory yet)
  ntf->m_location = NULL;

  return 0;
}

// Takes a hash
// Assigns <location> to the starting point of specified chunk in memory.
// If the file doesn't exist yet, the file is created.
// If the memory-mapped data from the file doesn't exist yet, it is created.
// <chunk> starts at 0!! (e.x. 0-7 assuming 8 chunks)
off_t open_file(htable_t* htable, void** location, unsigned char hash[MD5_DIGEST_LENGTH], int chunk) {
  // Search for the htable that correspons with the hash.
  tfile_t* tf = search_htable(htable, hash);
  if (tf == NULL) {
    return -1;
  }

  // If we dont have a memory-mapped location already, create it.
  if (tf->m_location == NULL) {
    // If we dont have a file already, create it.
    // The name and location are the current working directory and the name of the specified file. (name must include file extension)
    if (tf->f_location == NULL) {
      tf->f_location = tf->name;
    }
    int fd = open(tf->f_location, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    if (fd == -1) {
      perror("Could not open or create file");
      return -1;
    }
    // Set the size to be equal to the tfile specification
    ftruncate(fd, tf->size);
    void* m_location = mmap(NULL, (size_t)tf->size, PROT_WRITE, MAP_SHARED, fd, 0);  // Converting a signed long to an unsigned long might be a bad idea
    if (m_location == MAP_FAILED) {
      perror("Could not create a memory-mapped location for the file");
      return -1;
    }
    tf->m_location = m_location;
    close(fd);
  }

  // Calculate the chunk size.
  off_t chunk_size = tf->size / NUM_CHUNKS;

  // Assign <location> to the starting point of the chunk.
  *location = (chunk * chunk_size) + (char*)tf->m_location;

  // If it's the last chunk, recalculate the chunk size
  if (chunk == NUM_CHUNKS-1) {
    chunk_size = tf->size - (chunk * chunk_size);
  }

  return chunk_size;
}

// Save an open file to storage and close the memory-mapped region.
int save_file(htable_t* ht, unsigned char hash[MD5_DIGEST_LENGTH]) {
  tfile_t* tf = locate_htable(ht, hash);
  if (tf == NULL) {
    return -1;
  }
  if (tf->m_location == NULL) {
    return 0;
  }

  // Synchronize the memory-mapped region immediately, then close the region.
  msync(tf->m_location, tf->size, MS_SYNC);
  if (!munmap(tf->m_location, tf->size)) {
    perror("Could not unmap memory");
    return -1;
  }

  tf->m_location = NULL;

  return 0;
}

// Add an existing tfile (likely from a peer) to the hash table.
tfile_t* add_tfile(htable_t* ht, tfile_def_t tfile_def) {
  tfile_t* tfile = locate_htable(ht, tfile_def.f_hash);
  if (tfile == NULL) {
    return NULL;
  }

  tfile->size = tfile_def.size;
  memcpy(tfile->name, tfile_def.name, NAME_LEN);
  memcpy(tfile->f_hash, tfile_def.f_hash, MD5_DIGEST_LENGTH);
  for (int i = 0; i < NUM_CHUNKS; i++) {
    memcpy(tfile->c_hashes[i], tfile_def.c_hashes[i], MD5_DIGEST_LENGTH);
  }
  tfile->f_location = NULL;
  tfile->m_location = NULL;

  return tfile;
}

// Returns the chunks which are verified.
verified_chunks_t verify_tfile(htable_t* ht, unsigned char hash[MD5_DIGEST_LENGTH]) {
  tfile_t* tf = search_htable(ht, hash);
  if (tf == NULL) {
    return UNVERIFIED_FILE;
  }

  unsigned char c_hashes[NUM_CHUNKS][MD5_DIGEST_LENGTH];

  if (tf->m_location != NULL) {
    // Check the entire file hash first
    unsigned char test_hash[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)tf->m_location, (size_t)tf->size, test_hash);

    // Compare the file hash with the specified tfile hash.
    uint64_t th1 = *(uint64_t*)test_hash;
    uint64_t th2 = *(((uint64_t*)test_hash) + 1);

    uint64_t fh1 = *(uint64_t*)tf->f_hash;
    uint64_t fh2 = *(((uint64_t*)tf->f_hash) + 1);

    // If f is fully verified, we can stop here.
    if (th1 == fh1 && th2 == fh2) {
      return VERIFIED_FILE;
    }

    // Chunk size
    off_t chunk_size = tf->size / NUM_CHUNKS;

    // Calculate the hashes for each chunk.
    // We calculate the LAST chunk FIRST because the chunk is not a fixed size.
    unsigned char* offset = (unsigned char*)tf->m_location + (chunk_size * (NUM_CHUNKS-1));      // The start of the chunk.
    off_t size = tf->size - (chunk_size * (NUM_CHUNKS-1));                                       // The size of the chunk.
    MD5(offset, size, c_hashes[NUM_CHUNKS-1]);                                                 // The hash of the last chunk.

    // Loop for the other chunks.
    for (int i = 0; i < NUM_CHUNKS-1; i++) {
      offset = (unsigned char*)tf->m_location + (chunk_size * i);
      MD5(offset, chunk_size, c_hashes[i]);
    }
  } else if (tf->f_location != NULL) {
    // Open the file.
    int fd = open(tf->f_location, O_RDONLY);
    if (fd == -1) {
      perror("Could not open file for verification");
      return UNVERIFIED_FILE;
    }
    struct stat buf;
    fstat(fd, &buf);

    // Create a hash for the entire file.
    unsigned char test_hash[MD5_DIGEST_LENGTH];
    md5_hash(fd, 0, buf.st_size, test_hash);

    // Compare the file hash with the tfile hash.
    uint64_t th1 = *(uint64_t*)test_hash;
    uint64_t th2 = *(((uint64_t*)test_hash) + 1);

    uint64_t fh1 = *(uint64_t*)tf->f_hash;
    uint64_t fh2 = *(((uint64_t*)tf->f_hash) + 1);

    // File is fully verified if the hashes match. We can stop here.
    if (th1 == fh1 && th2 == fh2) {
      return VERIFIED_FILE;
    }

    // Iterate through the chunks, verfiying them.
    unsigned char verified_chunks = UNVERIFIED_FILE;

    // Calculate the hashes for each chunk.
    // We calculate the LAST chunk FIRST because the chunk is not a fixed size.
    off_t chunk_size = tf->size / NUM_CHUNKS;                               // The chunk size.
    off_t offset = chunk_size * (NUM_CHUNKS-1);                             // The start of the last chunk.
    off_t size = tf->size - offset;                                         // The size of the last chunk.
    md5_hash(fd, offset, size, c_hashes[NUM_CHUNKS-1]);                     // Calculate the hash.

    // Loop for the other chunks.
    for (int i = 0; i < NUM_CHUNKS-1; i++) {
      offset = i * chunk_size;
      md5_hash(fd, offset, chunk_size, c_hashes[i]);
    }
  } else {
    return UNVERIFIED_FILE;
  }

  // Compare the hashes.
  uint64_t t1, t2, c1, c2;
  unsigned char verified_chunks = UNVERIFIED_FILE;

  for (int i = 0; i < NUM_CHUNKS; i++) {
    t1 = *(uint64_t*)c_hashes[i];
    t2 = *(((uint64_t*)c_hashes[i]) + 1);
    c1 = *(uint64_t*)tf->c_hashes[i];
    c2 = *(((uint64_t*)tf->c_hashes[i]) + 1);
    verified_chunks = verified_chunks | ((t1 == c1 && t2 == c2) << (NUM_CHUNKS-1 - i));
  }
  return verified_chunks;
}
