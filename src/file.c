#include "file.h"
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
tfile_t* new_tfile(htable_t* htable, char* file_path, char name[NAME_LEN]) {
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

  unsigned char hash[MD5_DIGEST_LENGTH];
  md5_hash(fd, 0, buf.st_size, hash);

  tfile_t* ntf = locate_htable(htable, hash);
  if (ntf == NULL) {
    perror("File already has a torrent file");
    return NULL;
  }

  // Copy the hash into the tfile.
  memcpy(ntf->f_hash, hash, MD5_DIGEST_LENGTH);

  // Copy the size of the file into the tfile.
  ntf->size = buf.st_size;

  // Set the chunk size. The very last chunk will vary in size.
  off_t c_size = buf.st_size / NUM_CHUNKS;

  // Calculate the hashes for each chunk.
  // We calculate the LAST chunk FIRST because the chunk is not a fixed size.
  off_t offset = c_size * NUM_CHUNKS-1;                               // The start of the chunk.
  off_t size = buf.st_size - offset;                                  // The size of the chunk.
  md5_hash(fd, offset, size, ntf->c_hashes[NUM_CHUNKS-1]);             // Calculate the hash.

  // Loop for the other chunks.
  for (int i = 0; i < NUM_CHUNKS-1; i++) {
    offset = i * c_size;
    md5_hash(fd, offset, c_size, ntf->c_hashes[i]);
  }

  // Copy the name into the tfile
  memcpy(ntf->name, name, NAME_LEN);
  // Copy the file path into the tfile.
  // tfile is now the owner of the string.
  // DO NOT free the string.
  ntf->f_location = file_path;

  // Set all chunk locations to NULL
  // (we haven't loaded the file into memory yet)
  ntf->m_location = NULL;

  // Close the file.
  close(fd);

  return ntf;
}

// Takes a hash
// Assigns <location> to the starting point of specified chunk in memory.
// If the file doesn't exist yet, the file is created.
// If the memory-mapped data from the file doesn't exist yet, it is created.
// <chunk> starts at 0!!
off_t chunk_location(htable_t* htable, void** location, unsigned char hash[MD5_DIGEST_LENGTH], int chunk) {
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
    int fd = open(tf->f_location, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
    if (fd == -1) {
      perror("Cound not open or create file");
      return -1;
    }
    // Set the size to be equal to the tfile specification
    ftruncate(fd, tf->size);
    void* m_location = mmap(NULL, (size_t)tf->size, PROT_WRITE, MAP_FILE, fd, 0);  // Converting a signed long to an unsigned long might be a bad idea
    if (m_location == MAP_FAILED) {
      perror("Could not create a memory-mapped location for the file");
      return -1;
    }
    tf->m_location = m_location;
    close(fd);
  }

  // Calculate the chunk size.
  off_t chunk_size = tf->size / NUM_CHUNKS;
  *location = (chunk * chunk_size) + (char*)tf->m_location;

  // If it's the last chunk, recalculate the chunk size
  if (chunk == NUM_CHUNKS-1) {
    chunk_size = tf->size - (NUM_CHUNKS-1 * chunk_size);
  }

  return chunk_size;
}

// Add an existing tfile (likely from a peer) to the hash table.
int add_tfile(htable_t* ht, tfile_def_t tfile_def) {
  tfile_t* tfile = locate_htable(ht, tfile_def.f_hash);
  if (tfile == NULL) {
    return -1;
  }

  tfile->size = tfile_def.size;
  memcpy(tfile->name, tfile_def.name, NAME_LEN);
  memcpy(tfile->f_hash, tfile_def.f_hash, MD5_DIGEST_LENGTH);
  for (int i = 0; i < NUM_CHUNKS-1; i++) {
    memcpy(tfile->c_hashes[i], tfile_def.c_hashes[i], MD5_DIGEST_LENGTH);
  }
  tfile->f_location = NULL;
  tfile->m_location = NULL;

  return 0;
}

// Initializes a tfile_def from a
int init_tfile_def() {
  return 0;
}
