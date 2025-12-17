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

// FUNCTION DEFINITONS
void print_usage(char **argv);
void parse_args(cmd_args_t *args, int argc, char **argv);
void free_args(cmd_args_t args);
sockdata_t get_address_self(peer_fd_t socket);
void *readWorker(void *args);
void *connectWorker(void *args);
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
// END GLOBALS

// ----Main loop----
int main(int argc, char **argv)
{

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

  // Hash table of the tfiles.
  htable_t ht;
  init_htable(&ht);

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
  // else
  // {
  //   // Set hostname of this client
  //   memcpy(hostname, argv[1], 45);
  // }

  /**
   * MAKE INTERACTIVE FROM TO READ FILES ON NETWORK.
   * IF A FILE IS PRESENT< STORE THE FILE AND SPLIT INTO CHUNKS.
   * SEND AND RECEIVE REQUESTS FORM OTHERS.
   */

  // file was specified and must be added to current list of files
  if (args.file_p)
  {
    tfile_def_t new_tfile;
    generate_tfile(&ht, &new_tfile, args.file_p, args.file_p);
  }

  // Accept conections from peers
  pthread_t thread;
  pthread_create(&thread, NULL, connectWorker, (void *)&server_fd);

  free_args(args);
  exit(EXIT_SUCCESS);
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

  while (1)
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
      perror("accept failed");
      exit(EXIT_FAILURE);
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
 * \param args a void star pointer holding the id of the client to reaad from.
 */
void *readWorker(void *args)
{
  int client_socket_fd = *(int *)args;
  void *data_read;

  while (true)
  {
    // get message data
    message_info_t info;

    if (incoming_message_info(client_socket_fd, &info) != 0)
    {
      // ERROR
      // TODO CLOSE CONNECTION
      break;
    }

    // Read data from the client
    if (receive_message(client_socket_fd, &data_read, info.size) != 0)
    {
      // ERROR
      // TODO CLOSE CONNECTION
      break;
    }

    // Check type of message and handle
    if (info.type == REQUEST_ADDR_SELF)
    {
      // return address self
    }
    else if (info.type == TFILE_DEF)
    {
      // add tfile to self definition
    }

    else if (info.type == FILE_DATA)
    {
      // add file data to where it needs to go
    }
  }

  return NULL;
}