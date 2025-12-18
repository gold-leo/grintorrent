#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include "socket.h"
#include "file.h"
#include "message.h"
#include "socket.h"

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
  int server_fd;
  int client_fd;
} client_server_t;

typedef struct
{
  unsigned char file_hash[MD5_DIGEST_LENGTH];
  int chunk_index;
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
// END FUNCTION DEFINITIONS

// Add a peer to peers_t
void add_peer(peers_t *peers, peer_fd_t peer)
{
  if (peers->capacity >= peers->size)
  {
    pthread_mutex_lock(&peers->lock);
    peers->arr = realloc(peers->arr, peers->capacity * 2 * sizeof(peer_fd_t));
    peers->arr[peers->size++] = peer;
    pthread_mutex_unlock(&peers->lock);
  }
  else
  {
    pthread_mutex_lock(&peers->lock);
    peers->arr[peers->size++] = peer;
    pthread_mutex_unlock(&peers->lock);
  }
}

// Remove a peer from peers_t
void remove_peer(peers_t *peers, peer_fd_t peer)
{
  pthread_mutex_lock(&peers->lock);
  // Find and remove the passed in lock, as order of the array does not matter we just replace the removed socket with the last socket in the array
  // If the socket is already removed, the loop will fail safely.
  for (int i = 0; i < peers->size; i++)
  {
    if (peers->arr[i] == peer)
    {
      close(peers->arr[i]);
      peers->arr[i] = peers->arr[--peers->size];
      break;
    }
  }
  pthread_mutex_unlock(&peers->lock);
}

bool isInitialized(sockdata_t data)
{
  return data.server_addr_len;
}

// GLOBALS
//  HOLDS SELF ADDRESS NAME AND LENGTH
sockdata_t self_address;

// Initialize socket/peer file descriptor array
peers_t peers = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .capacity = 100,
    .size = 0};

// Hash table of the tfiles.
htable_t ht;
// END GLOBALS

// ----Main loop----
int main(int argc, char **argv)
{
  init_htable(&ht);

  // increase peer array
  peers.arr = malloc(peers.capacity * sizeof(peer_fd_t));

  // Create server socket
  unsigned short network_port = 0;
  int server_fd = server_socket_open(&network_port);
  if (server_fd == -1)
  {
    perror("Server socket open failed");
    exit(EXIT_FAILURE);
  }

  // Start listening for connections on server socket
  if (listen(server_fd, 10))
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  // Address of this client.
  // const char *hostname[45];
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(struct sockaddr_in);

  // parse arguments
  cmd_args_t args = {NULL};
  parse_args(&args, argc, argv);

  // check for extra elements that cannot be identified
  if (optind < argc)
  {
    print_usage(argv);
    exit(EXIT_FAILURE);
  }

  // verify username is present
  if (args.username_p == NULL)
  {
    print_usage(argv);
    exit(EXIT_FAILURE);
  }

  // verify both port and peer are specified
  if ((args.peer_p && !args.port_p) || (args.port_p && !args.peer_p))
  {
    print_usage(argv);
    exit(EXIT_FAILURE);
  }

  // Connect to peer if given in startup
  if (args.peer_p)
  {
    char *host = args.peer_p;
    unsigned short target_port = atoi(args.port_p);

    peer_fd_t host_peer = socket_connect(host, target_port);
    if (host_peer == -1)
    {
      perror("Failed to connect");
      exit(EXIT_FAILURE);
    }

    add_peer(&peers, host_peer);

    // read messages using this thread.
    pthread_t thread;
    pthread_create(&thread, NULL, readWorker, (void *)&host_peer);

    // save self information
    self_address = get_address_self(host_peer);
  }

  // file was specified and must be added to current list of files
  if (args.file_p)
  {

    tfile_def_t new_tfile;
    generate_tfile(&ht, &new_tfile, args.file_p, args.file_p);

    // run thread to share tfile to all peers
    send_tfile_t data = {
        .sender = NO_SENDER_PEER,
        .tfile = new_tfile};
    pthread_t thread;
    pthread_create(&thread, NULL, share_tfile_to_peers, (void *)&data);
  }

  // Accept conections from peers
  pthread_t thread;
  pthread_create(&thread, NULL, connectWorker, (void *)&server_fd);

  free_args(args);
  exit(EXIT_SUCCESS);
}

/**
 * This function shares a tfile to all peers. If the senderof the pfile is specified, it skips them.
 * \param args The struct holding the t file to be sent to all files as the sender of the tfile who should be skipped and should not revceive the tfile again. Set to 0 to skip
 * \param new_tfile The tfile to be shared to all peers
 * \param sender The sender of the tfile who should not be sent the message
 */
void *share_tfile_to_peers(void *args)
{

  send_tfile_t data = *((send_tfile_t *)args);
  // create messag info
  message_info_t info = {
      .type = TFILE_DEF,
      .size = sizeof(data.tfile)};

  // share file with all peers
  pthread_mutex_lock(&data.peers->lock);

  // store peers to remove and remove them outside of the loop
  peer_fd_t peers_to_remove[data.peers->size];
  int peers_to_remove_count = 0;

  for (int i = 0; i < data.peers->size; i++)
  {
    // skip the sender
    if (data.peers->arr[i] == data.sender)
      continue;

    // send message to peer with information on the tfile
    if (send_message(data.peers->arr[i], &info, (void *)&data.tfile) != 0)
    {
      // remove peer but do it outside of the loop
      peers_to_remove[peers_to_remove_count++] = i;
    }
  }

  remove_peers(peers_to_remove_count, peers_to_remove);

  pthread_mutex_unlock(&data.peers->lock);

  return NULL;
}

/**
 * Removes peers specified in teh list of peers to remove
 * \param peers_to_remove_count The nubmer of peers to remove
 * \param peers_to_remove Array containing the peers to remove
 */
void remove_peers(int peers_to_remove_count, peer_fd_t peers_to_remove[peers_to_remove_count])
{
  for (int i = 0; i < peers_to_remove_count; i++)
  {
    remove_peer(&peers, peers_to_remove[i]);
  }
}

/**
 * This function requests self solcket data from peers
 * \param socket The peer socket which would be used to get the self hostname
 */
sockdata_t get_address_self(peer_fd_t socket)
{
  message_info_t info = {
      .type = REQUEST_ADDR_SELF,
      .size = 0};
  send_message(socket, &info, NULL);

  socklen_t server_addr_len;
  struct sockaddr_in server_addr;

  while (true)
  {
    message_info_t info;
    if (incoming_message_info(socket, &info) == FAILED)
    {
      continue;
    }
    if (info.type == ADDR_SELF)
    {
      printf("ADDR_SELF (%ld)\n", info.size);
      server_addr_len = info.size;

      if (receive_message(socket, &server_addr, server_addr_len))
      {
        printf("failed\n");
      }
    }
    else
    {
      break;
    }
  }

  sockdata_t return_data;
  return_data.server_addr = server_addr;
  return_data.server_addr_len = server_addr_len;

  return return_data;
} /**
   * Frees all arguments which were allocated
   * \param args Strcutre holding arguments data
   */
void free_args(cmd_args_t args)
{
  if (args.username_p)
  {
    printf("username: %s\n", args.username_p);
    free(args.username_p);
  }
  if (args.peer_p)
  {
    printf("peer: %s\n", args.peer_p);
    free(args.peer_p);
  }
  if (args.port_p)
  {
    printf("port: %s\n", args.port_p);
    free(args.port_p);
  }
  if (args.file_p)
  {
    printf("filename: %s\n", args.file_p);
    free(args.file_p);
  }
}

/**
 * Prints a usage message to be displayed to the user
 * \param argv The argv buffer that is passed in main
 */
void print_usage(char **argv)
{
  fprintf(stderr, "Usage: %s <-u username> [-p <peer> -n <port number>] [-f file]\n", argv[0]);
}

/**
 * Parses the command line arguments passed to it
 * \param args The arguments structure to hold the information
 * \param argc The argc value from main
 * \param argv The argv value from main
 */
void parse_args(cmd_args_t *args, int argc, char **argv)
{
  /**
   * User can type the following commands
   * grinntorrent <-u username> [-p <peer> -n <port number>] [-f file]
   */
  int opt;
  while ((opt = getopt(argc, argv, ":p:n:f:u:")) != -1)
  {
    switch (opt)
    {
    case 'p':
      args->peer_p = strdup(optarg);
      if (args->peer_p == NULL)
      {
        perror("Memory Allocation error");
        exit(EXIT_FAILURE);
      }
      break;
    case 'n':
      args->port_p = strdup(optarg);
      if (args->port_p == NULL)
      {
        perror("Memory Allocation error");
        exit(EXIT_FAILURE);
      }
      break;
    case 'f':
      args->file_p = strdup(optarg);
      if (args->file_p == NULL)
      {
        perror("Memory Allocation error");
        exit(EXIT_FAILURE);
      }
      break;
    case 'u':
      args->username_p = strdup(optarg);
      if (args->username_p == NULL)
      {
        perror("Memory Allocation error");
        exit(EXIT_FAILURE);
      }
      break;
    case ':':
      printf("IAM HERE!\n");
      print_usage(argv);
      exit(EXIT_FAILURE);
      break;
    case '?':
      printf("Unknown option: %c\n", optopt);
      print_usage(argv);
      exit(EXIT_FAILURE);
      break;
    }
  }
}

/**
 * This function is the function called by threads to wait for an incoming connection and creates a new thread to watch it for messages
 * \param args a void star pointer containing the socket of the client
 */
void *connectWorker(void *args)
{
  while (true)
  {
    // Wait for a client to connect
    int client_socket_fd = server_socket_accept(*((int *)args));
    if (client_socket_fd == -1)
    {
      // remove that peer
      remove_peer(&peers, client_socket_fd);
      continue;
    }

    add_peer(&peers, client_socket_fd);

    // initialize self from peer
    if (!isInitialized(self_address))
      self_address = get_address_self(client_socket_fd);

    // watch peer for meesages
    pthread_t thread;
    pthread_create(&thread, NULL, readWorker, (void *)&client_socket_fd);
  }
  return NULL;
}

/**
 * This function watches a socket forever until a message is read. That message is then displayed and relayed to all other connected sockets.
 * \param args a void star pointer holding the struct with the client and server fd
 */
void *readWorker(void *args)
{

  client_server_t fd_data = *(client_server_t *)args;
  void *data_read;

  while (true)
  {
    // get message data
    message_info_t info;

    if (incoming_message_info(fd_data.client_fd, &info) != 0)
    {
      // ERROR
      remove_peer(&peers, fd_data.client_fd);
      break;
    }

    // Read data from the client
    if (receive_message(fd_data.client_fd, &data_read, info.size) != 0)
    {
      // ERROR
      remove_peer(&peers, fd_data.client_fd);
      break;
    }

    // Check type of message and handle
    if (info.type == REQUEST_ADDR_SELF)
    {
      socklen_t server_addr_len;
      struct sockaddr_in server_addr;
      int socket = server_socket_accept_addr(fd_data.server_fd, &server_addr, &server_addr_len);
      if (socket == -1)
      {
        perror("accept failed");
        remove_peer(&peers, fd_data.client_fd);
        break;
      }

      // return address self
      message_info_t info = {
          .type = ADDR_SELF,
          .size = server_addr_len};
      send_message(socket, &info, &server_addr);
    }
    else if (info.type == TFILE_DEF)
    {
      tfile_def_t new_tfile = *((tfile_def_t *)data_read);

      // add tfile to self definition
      add_htable(&ht, new_tfile);

      // share that file with your pers list
      // run thread to share tfile to all peers
      send_tfile_t share_data = {
          .sender = fd_data.client_fd,
          .tfile = new_tfile};

      pthread_t thread;
      pthread_create(&thread, NULL, share_tfile_to_peers, (void *)&share_data);
    }

    else if (info.type == REQUEST_FILE_DATA)
    {
      // LOOK AT FILE CHUNK BEING REQUESTED
      chunk_request_t data = *(chunk_request_t *)args;

      verified_chunks_t chunks = verify_tfile(&ht, data.file_hash);

      // i have the chunk and can return the data
      if (is_chunk_verified(chunks, data.chunk_index))
      {
        if (send_chunk_message(fd_data.client_fd, &ht, data.file_hash, data.chunk_index) == FAILED)
        {
          remove_peer(&peers, fd_data.client_fd);
          break;
        }
      }
      // cannot send this chunk, return nothing
      else
      {
        // send response without data
        message_info_t info = {
            .type = FILE_DATA,
            .size = 0};
        send_message(fd_data.client_fd, &info, NULL);
      }
    }
    else if (info.type == FILE_DATA)
    {
      // add file data to where it needs to go
    }
  }

  return NULL;
}

/**
 * This fucntion sends a chunk of data over to the peer
 * \param fd the file descriptor of the person to send to,
 * \param ht the hash table
 * \param file_hash the hash of the file
 * \param chunk_index the index of the chunk which must be sent
 */
int send_chunk_message(int fd, htable_t *ht,
                       unsigned char file_hash[MD5_DIGEST_LENGTH],
                       int chunk_index)
{
  void *chunk_ptr = NULL;
  off_t chunk_size = open_tfile(ht, &chunk_ptr, file_hash, chunk_index);
  if (chunk_size <= 0 || chunk_ptr == NULL)
  {
    return FAILED;
  }

  // Allocate payload buffer
  size_t payload_size = sizeof(chunk_payload_t) + chunk_size;
  unsigned char *payload = malloc(payload_size);
  if (!payload)
    return FAILED;

  // Fill payload header
  chunk_payload_t *hdr = (chunk_payload_t *)payload;
  memcpy(hdr->file_hash, file_hash, MD5_DIGEST_LENGTH);
  hdr->chunk_index = chunk_index;
  hdr->chunk_size = htonl((uint32_t)chunk_size);

  // Copy chunk bytes
  memcpy(payload + sizeof(chunk_payload_t),
         chunk_ptr,
         chunk_size);

  // Fill message info
  message_info_t info = {
      .type = FILE_DATA,
      .size = payload_size};

  int rc = send_message(fd, &info, payload);
  free(payload);
  return rc;
}