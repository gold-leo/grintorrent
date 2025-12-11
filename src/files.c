#include <stdint.h>
#include <string.h>
#include "files.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// The size of the blocks read in to a hash.
#define BLOCK_READ_SIZE 8192

// Files.c describes the functions used to manipulate the files that are
// distributed over the network.

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

// Initialize a file description based off of a specified file.
// Before calling, declare a new file_desc_t struct.
int init_file_desc(char* name, char* file_path, file_desc_t* file_desc) {
  // add the details of the file and store within file_desc
  // return 1 if fail, 0 if succeed

  // Copy the name into the file_desc struct.
  memcpy(file_desc->name, name, 20);

  // Open the file.
  int fd = open(file_path, O_RDONLY); // Consider adding O_LARGEFILE, talk to Charlie about this
  struct stat buf;
  fstat(fd, &buf);

  // Calculate the hash.
  unsigned char hash[MD5_DIGEST_LENGTH];
  md5_hash(fd, 0, buf.st_size, hash);

  // Add hash to file_desc and file_status.
  memcpy(file_desc->hash, hash, MD5_DIGEST_LENGTH);

  // Add the size into file_desc.
  file_desc->size = buf.st_size;

  // Set the number of chunks and add into file_desc.
  if (buf.st_size < MAX_CHUNKS_LOWER_BOUND) {
    // Should split up evenly
    file_desc->chunk_num = (buf.st_size / (MAX_CHUNKS_LOWER_BOUND / MAX_CHUNKS)) + 1;
  } else {
    file_desc->chunk_num = MAX_CHUNKS;
  }

  // Set the chunk size. The last chunk will be larger, but we don't have to worry about that
  // because we know the total file size and we will use that as the limit for the last chunk.
  file_desc->chunk_size = buf.st_size / (file_desc->chunk_num);

  // Calculate the hashes for each chunk.
  // We calculate the LAST chunk FIRST because the chunk is not a fixed size.
  off_t offset = (file_desc->chunk_num - 1) * file_desc->chunk_size; // The start of the chunk.
  off_t size = buf.st_size - offset;                                 // The size of the chunk.
  md5_hash(fd, offset, size, hash);                                  // Calculate the hash.
  memcpy(file_desc->chunk_hashes[file_desc->chunk_num - 1], hash, MD5_DIGEST_LENGTH);

  // Loop for the other chunks. If there are none this will skip.
  for (int i = 0; i < file_desc->chunk_num - 1; i++) {
    offset = i * file_desc->chunk_size;
    md5_hash(fd, offset, file_desc->chunk_size, hash);
    memcpy(file_desc->chunk_hashes[i], hash, MD5_DIGEST_LENGTH);
  }

  // Add yourself to the list of available peers.
  // TODO: how to find your own socket and address.
  file_desc->available_peer_chunks.chunk_availability[0] = 0xFF;

  // close file

  return 0;
}

// Update a file status based off of a specified file description and file path.
int update_file_status(char* file_path, file_desc_t* file_desc, file_status_t* file_status) {

  // Open the file.
  int fd = open(file_path, O_RDONLY); // Consider adding O_LARGEFILE, talk to Charlie about this
  struct stat buf;
  fstat(fd, &buf);

  // Declare a hash before moving on to the next section.
  unsigned char hash[MD5_DIGEST_LENGTH];

  // Compare size of file with file_desc size.
  if (file_desc->size < buf.st_size) {
    perror("Your file is larger than the file's description and may be corrupted.");
    return 1;
  } else if (file_desc->size == buf.st_size) {
    // Calculate the hash. Size - 1 because we start at 0.
    md5_hash(fd, 0, buf.st_size, hash);

    // If the hash is equal, all chunks are verified. Add to file_status.
    if (!memcmp(hash, file_desc->hash, MD5_DIGEST_LENGTH)) {
      file_status->verified_chunks = COMPLETE_FILE;
      return 0;
    }
  }

  // Declare the verified chunks char.
  uint8_t verified_chunks = 0;

  // Calculate the hashes for each chunk.
  // We calculate the LAST chunk FIRST because it is not a fixed size.
  off_t offset = (file_desc->chunk_num - 1) * file_desc->chunk_size; // The start of the chunk.
  off_t size = buf.st_size - offset;                                 // The size of the chunk.
  md5_hash(fd, offset, size, hash);                                  // Calculate the hash.
  // Compare the hash with the hash specified in the file_desc. Add a 1 if the hash is equal.
  if (!memcmp(file_desc->chunk_hashes[file_desc->chunk_num - 1], hash, MD5_DIGEST_LENGTH)) {
    // Use a bitmask to add a 1 to the proper place in the char.
    verified_chunks = verified_chunks | 1;
  }

  // Loop for the other chunks. If there are none this will skip.
  for (int i = 0; i < file_desc->chunk_num - 1; i++) {
    offset = i * file_desc->chunk_size;
    md5_hash(fd, offset, file_desc->chunk_size, hash);
    if (!memcmp(file_desc->chunk_hashes[i], hash, MD5_DIGEST_LENGTH)) {
      // Use a bitmask to add a 1 to the proper place in the char.
      verified_chunks = verified_chunks | (1 << (MAX_CHUNKS - i - 1));
    }
  }

  // Add the final result of comparing the hashes to the file_status.
  file_status->verified_chunks = verified_chunks;

  return 0;
}

// Initialize a new file_status.
int init_file_status(char* file_path, file_desc_t* file_desc, file_status_t* file_status) {
  // Copy the hash from the file_desc
  memcpy(file_status->hash, file_desc->hash, MD5_DIGEST_LENGTH);

  // Point location to file_status
  file_status->location = file_path;

  if (update_file_status(file_path, file_desc, file_status)) {
    return 1;
  }

  return 0;
}
