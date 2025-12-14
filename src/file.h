#pragma once
#include <stddef.h>
#include <openssl/md5.h>
#include <stddef.h>
#include <sys/stat.h>

// The number of chunks per file.
#define NUM_CHUNKS 8

// The permissible length of a name.
#define NAME_LEN 32

// The value which defines a vully verified file.
#define VERIFIED_FILE 0xFF
#define UNVERIFIED_FILE 0x00
// Holds information on which chunks are verified (match their respective hash).
typedef uint8_t verified_chunks_t;

// Struct which describes a torrent file (tfile). For INTERNAL use by clients.
typedef struct {
  // Name of the file
  char name[NAME_LEN];
  // Size of the file.
  off_t size;
  // Hash of the entire file.
  unsigned char f_hash[MD5_DIGEST_LENGTH];
  // Hashes of the chunks.
  unsigned char c_hashes[NUM_CHUNKS][MD5_DIGEST_LENGTH];
  // Full file location
  char* f_location;
  // Memory location of the file;
  void* m_location;

  // Potentially add a peer availability list if there's enough time.
} tfile_t;

// Struct which defines a torrent file (tfile). For EXTERNAL use between peers.
typedef struct {
  // Name of the file
  char name[NAME_LEN];
  // Hash of the entire file.
  unsigned char f_hash[MD5_DIGEST_LENGTH];
  // Hashes of the chunks.
  unsigned char c_hashes[NUM_CHUNKS][MD5_DIGEST_LENGTH];
  // Size of the file
  off_t size;
} tfile_def_t;

// Hash table for tfiles.
typedef struct {
  size_t capacity;
  size_t size;
  tfile_t* table;
} htable_t;

/* --- Function Declarations --- */
void init_htable(htable_t*);
int resize_htable(htable_t*);
tfile_t* locate_htable(htable_t*, unsigned char hash[MD5_DIGEST_LENGTH]);
tfile_t* add_htable(htable_t*, tfile_t);
tfile_t* search_htable(htable_t*, unsigned char hash[MD5_DIGEST_LENGTH]);

int file_hash(char*, unsigned char*);
tfile_t* new_tfile(htable_t*, char*, char name[NAME_LEN]);
unsigned char verify_tfile(htable_t*, unsigned char hash[MD5_DIGEST_LENGTH]);
off_t chunk_location(htable_t*, void**, unsigned char hash[MD5_DIGEST_LENGTH], int);
