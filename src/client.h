#pragma once
#include <pthread.h>
#include <openssl/md5.h>
#include "socket.h"
#include "file.h"
#include "message.h"

// Socket file descriptor type
typedef int peer_fd_t;

// List of peer socket file descriptors.
typedef struct
{
    pthread_mutex_t lock;
    int capacity;
    int size;
    peer_fd_t *arr;
} peers_t;

// structure to hold arguments
typedef struct
{
    char *peer_p;
    char *port_p;
    char *file_p;
    char *username_p;

} cmd_args_t;

typedef struct
{
    socklen_t server_addr_len;
    struct sockaddr_in server_addr;
} sockdata_t;

typedef struct
{
    tfile_def_t tfile;
    peer_fd_t sender;
    peers_t *peers;
} send_tfile_t;

typedef struct
{
    tfile_def_t *tfile_arr;
    int count;
    peer_fd_t peer;
    peers_t *peers;

} send_tfiles_t;

typedef struct
{
    int server_fd;
    int client_fd;
} client_server_t;

typedef struct
{
    unsigned char file_hash[MD5_DIGEST_LENGTH];
    int chunk_index;
    struct sockaddr_in return_addr;
    socklen_t return_addr_len;
    uint8_t ttl;
} chunk_request_t;

typedef struct
{
    unsigned char file_hash[MD5_DIGEST_LENGTH];
    int chunk_index;
    uint32_t chunk_size;
} chunk_payload_t;

#define NO_SENDER_PEER -1

// FUNCTION DEFINITONS
void print_usage(char **argv);
void parse_args(cmd_args_t *args, int argc, char **argv);
void free_args(cmd_args_t args);
sockdata_t get_address_self(peer_fd_t socket);
void *readWorker(void *args);
void *connectWorker(void *args);
void remove_peers(int peers_to_remove_count, peer_fd_t peers_to_remove[peers_to_remove_count]);
void *share_tfile_to_peers(void *args);
int send_chunk_message(int fd, htable_t *ht,
                       unsigned char file_hash[MD5_DIGEST_LENGTH],
                       int chunk_index);
void remove_peer(peers_t *peers, peer_fd_t peer);
void add_peer(peers_t *peers, peer_fd_t peer);
bool isInitialized(sockdata_t data);
void *download_file(unsigned char file_hash[MD5_DIGEST_LENGTH]);

// END FUNCTION DEFINITIONS