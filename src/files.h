#if !defined(UI_H)
#define UI_H
#include <openssl/md5.h>
#include <sys/stat.h>

// Describes the number of peers in the peer_chunks_t struct.
#define NUM_OF_PEERS 8
// Changing this REQUIRES changing the size of file_status_t.verified_chunks!
// Please keep this an even number!!!!!!!!!
#define MAX_CHUNKS 8
// The size required for a file to have the max number of chunks.
#define MAX_CHUNKS_LOWER_BOUND 8192

// Describes the status of a file for a client.
// Remains internal to the client. Client shares verified_chunks when asked.
typedef struct {
  // Hash of the file.
  unsigned char hash[MD5_DIGEST_LENGTH];
  // File location.
  char* location;
  // State of the chunks that the client has. 1 = verified, 0 = unverified.
  // char is 8 bits.
  char verified_chunks;
} file_status_t;

// Struct for the availability of a file based off of peers.
// This struct can be outdated if a peer disconnects.
// At the moment, we are limiting the list to eight because eight is the maximum num of chunks.
// This can change if we find that a higher number is more efficient (because peers disconnect).
// This struct should be used to find the peers we want to connect to so we can download a file.
// Each potential peer (0-7) has a chunk_availabilty, peer_address, and peer_socket.
// i.e. indexing 0 on chunk_availability shows the available chunks from the peer of peer_address
typedef struct {
  // Array of bits. 1 = verified, 0 = unverified.
  char chunk_availability[NUM_OF_PEERS];
  // Address of the peer (IP or hostname) so that we can connect to them.
  char peer_address[NUM_OF_PEERS][30];
  // Socket of the peer so we can connect to them.
  unsigned short peer_socket[NUM_OF_PEERS];
} peer_chunks_t;

// File description. Shared with other clients.
typedef struct {
  // Name of the file (max 20 chars)
  char name[20];
  // MD5 hash of the entire file
  unsigned char hash[MD5_DIGEST_LENGTH];
  // Size of the file (in bytes)
  off_t size;
  // Number of chunks that the file has (max is 8)
  int chunk_num;
  // Size of each chunk. The last chunk can be slightly larger if the file is not divisible by the
  // number of chunks. When we download the last chunk, we use the file size as the end limit for
  // the last chunk.
  off_t chunk_size;
  // Hashes for each chunk (max is 8)
  unsigned char chunk_hashes[8][MD5_DIGEST_LENGTH];
  // Chunks reported to be available by peers.
  peer_chunks_t available_peer_chunks;
} file_desc_t;

/* --- Function Declarations --- */
int init_file_desc(char*, char*, file_desc_t*);
int update_file_status(char*, file_desc_t*, file_status_t*);
int init_file_status(char*, file_desc_t*, file_status_t*);

#endif
