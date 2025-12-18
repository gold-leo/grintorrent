#pragma once
#include <stddef.h>
#include <openssl/md5.h>
#include <stddef.h>
#include <sys/stat.h>
#include <stdbool.h>

// The number of chunks per file.
#define NUM_CHUNKS 8

// The permissible length of a name.
#define NAME_LEN 32

// The value which defines a fully verified file.
#define VERIFIED_FILE 0xFF
#define UNVERIFIED_FILE 0x00
// Holds information on which chunks are verified (match their respective hash).
typedef uint8_t verified_chunks_t;

// Struct which defines a torrent file (tfile). For EXTERNAL use between peers.
typedef struct
{
  // Name of the file
  char name[NAME_LEN];
  // Hash of the entire file
  unsigned char f_hash[MD5_DIGEST_LENGTH];
  // Hashes of the chunks
  unsigned char c_hashes[NUM_CHUNKS][MD5_DIGEST_LENGTH];
  // Size of the file in bytes
  off_t size;
} tfile_def_t;

// Struct which stores the location data of a tfile.
// Used exclusively for the hash table, not to be shared between peers.
typedef struct
{
  // Definition of the tfile.
  tfile_def_t tdef;
  // Location of the file in storage
  char *f_location;
  // Location of the file if it is loaded into memory
  void *m_location;
} tfile_t;

// Hash table for tfiles.
typedef struct
{
  size_t capacity;
  size_t size;
  tfile_t *table;
} htable_t;

/* --- Function Declarations --- */
// htable.c
void init_htable(htable_t *);
int resize_htable(htable_t *);
tfile_t *add_htable(htable_t *, tfile_def_t);
tfile_t *search_htable(htable_t *, unsigned char hash[MD5_DIGEST_LENGTH]);

// file.c

// Generate a completely new tfile based off of an existing file on the clients' computer.
// The tfile is added to the hash table.
int generate_tfile(htable_t *, tfile_def_t *, char *, char name[NAME_LEN]);
// Add an existing tfile (likely from a peer) to the hash table.
tfile_t *add_tfile(htable_t *, tfile_def_t);
// Generate a list of all the tfiles in the hash table.
// Returns the number of tfiles reported.
int list_tfiles(htable_t *, tfile_def_t **);

// Returns the chunks of a tfile which are verified.
verified_chunks_t verify_tfile(htable_t *, unsigned char hash[MD5_DIGEST_LENGTH]);

// Returns if a chunk is verified or not
bool is_chunk_verified(verified_chunks_t chunks, int chunk_index);

// Open a tfile in memory to be read or written to.
// If there is not already a file, the file is created in the working directory.
// Sets <location> to the beginning of the specified <chunk>.
// <chunk> starts at 0!! (e.x. 0-7 assuming 8 chunks)
off_t open_tfile(htable_t *, void **, unsigned char hash[MD5_DIGEST_LENGTH], int);
// Save a tfile to storage and free the memory region.
int save_tfile(htable_t *, unsigned char hash[MD5_DIGEST_LENGTH]);
