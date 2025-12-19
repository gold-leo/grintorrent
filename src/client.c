#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include "socket.h"
#include "file.h"
#include "message.h"
#include "client.h"
#include "ui.h"
#include "ui_adapter.h"

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
    // self_address = get_address_self(host_peer);
  }

  // file was specified and must be added to current list of files
  if (args.file_p)
  {

    tfile_def_t new_tfile;
    generate_tfile(&ht, &new_tfile, args.file_p, args.file_p);

    pthread_t thread;
    send_tfile_t *data = malloc(sizeof(send_tfile_t));
    *data = (send_tfile_t){
        .sender = NO_SENDER_PEER,
        .tfile = new_tfile,
        .peers = &peers};
    pthread_create(&thread, NULL, share_tfile_to_peers, data);
  }

  // Accept conections from peers
  pthread_t thread;
  pthread_create(&thread, NULL, connectWorker, (void *)&server_fd);

  ui_init(ui_input_handler);

  char message[256];

  // Format the message using snprintf
  snprintf(message, sizeof(message), "Running on port %d", network_port);

  // Now call the ui_display function with the formatted message
  ui_display("system", message);

  ui_register_file_list_callback(ui_list_network_files);

  ui_run();
  // display ui running

  // free_args(args);
  ui_exit();
}

/**
 * This function shares all tfiles to a single peer.
 * \param args The struct holding all the necessary information for this worker
 */
void *share_tfiles_to_peer(void *args)
{
  send_tfiles_t data = *((send_tfiles_t *)args);
  pthread_mutex_lock(&data.peers->lock);

  // send all files
  for (int i = 0; i < data.count; i++)
  {

    // create messag info
    message_info_t info = {
        .type = TFILE_DEF,
        .size = sizeof(data.tfile_arr[i])};

    // send message to peer with information on the tfile
    if (send_message(data.peer, &info, (void *)&data.tfile_arr[i]) != 0)
    {
      // peer should be removed
      remove_peer(&peers, data.peer);
    }
  }

  pthread_mutex_unlock(&data.peers->lock);

  return NULL;
}
/**
 * This function shares a tfile to all peers. If the senderof the pfile is specified, it skips them.
 * \param args The struct holding the t file to be sent to all files as the sender of the tfile who should be skipped and should not revceive the tfile again. Set to 0 to skip

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

  // printf("WE ARE SENDING TFILES!!%d\n", data.peers->size);

  for (int i = 0; i < data.peers->size; i++)
  {
    // skip the sender
    if (data.peers->arr[i] == data.sender)
    {
      continue;
    }

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
  while ((opt = getopt(argc, argv, ":p:n:f:u:h")) != -1)
  {
    switch (opt)
    {
    case 'h':
      print_usage(argv);
      exit(EXIT_SUCCESS);
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
    // printf("GOT A NEW PEER\n");

    add_peer(&peers, client_socket_fd);

    // // initialize self from peer
    // if (!isInitialized(self_address))
    //   self_address = get_address_self(client_socket_fd);

    pthread_t thread;

    // send peer all tfiles_describptions
    send_tfiles_t *files_data = malloc(sizeof(send_tfiles_t));
    files_data->count = list_tfiles(&ht, &files_data->tfile_arr);
    files_data->peer = client_socket_fd;
    files_data->peers = &peers;
    pthread_create(&thread, NULL, share_tfiles_to_peer, files_data);

    // watch peer for messages
    int *fd_ptr = malloc(sizeof(int));
    *fd_ptr = client_socket_fd;
    pthread_create(&thread, NULL, readWorker, fd_ptr);
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
  size_t message_size = 100000;
  void *data_read = malloc(message_size);

  while (true)
  {

    // get message data
    message_info_t info;

    if (incoming_message_info(fd_data.server_fd, &info) != 0)
    {
      // ERROR
      remove_peer(&peers, fd_data.client_fd);
      break;
    }

    // Read data from the client
    if (receive_message(fd_data.server_fd, data_read, info.size) != 0)
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

      tfile_def_t new_tfile = *(tfile_def_t *)data_read;

      // add tfile to self definition
      add_htable(&ht, new_tfile);

      // share that file with your pers list
      // run thread to share tfile to all peers
      pthread_t thread;
      send_tfile_t *data = malloc(sizeof(send_tfile_t));
      *data = (send_tfile_t){
          .sender = fd_data.server_fd,
          .tfile = new_tfile,
          .peers = &peers};
      pthread_create(&thread, NULL, share_tfile_to_peers, data);
    }

    else if (info.type == REQUEST_FILE_DATA)
    {
      // LOOK AT FILE CHUNK BEING REQUESTED
      chunk_request_t req = *(chunk_request_t *)data_read;

      verified_chunks_t chunks = verify_tfile(&ht, req.file_hash);

      // i have the chunk and can return the data
      if (is_chunk_verified(chunks, req.chunk_index))
      {

        int out_fd = socket_connect_addr(req.return_addr,
                                         req.return_addr_len);

        if (out_fd != -1)
        {

          send_chunk_message(out_fd, &ht,
                             req.file_hash,
                             req.chunk_index);
        }
      }
      // cannot send this chunk, relay to my peers
      else
      {
        message_info_t fwd = {
            .type = REQUEST_FILE_DATA,
            .size = sizeof(chunk_request_t)};

        pthread_mutex_lock(&peers.lock);
        for (int i = 0; i < peers.size; i++)
        {

          // skip mysef
          if (peers.arr[i] == fd_data.client_fd)
            continue;

          send_message(peers.arr[i], &fwd, &req);
        }
        pthread_mutex_unlock(&peers.lock);
      }
    }
    else if (info.type == FILE_DATA)
    {
      // convert header to readable format
      chunk_payload_t *hdr = (chunk_payload_t *)data_read;

      verified_chunks_t chunks = verify_tfile(&ht, hdr->file_hash);
      // chunk already downloaded
      if (is_chunk_verified(chunks, hdr->chunk_index))
        continue;

      // credit: https://linux.die.net/man/3/ntohl
      uint32_t chunk_size = ntohl(hdr->chunk_size);
      unsigned char *chunk_data =
          (unsigned char *)data_read + sizeof(chunk_payload_t);

      void *dest = NULL;

      // open tfile for write
      off_t expected_size =
          open_tfile(&ht, &dest, hdr->file_hash, hdr->chunk_index);

      // sizes do not match
      if (expected_size != chunk_size)
      {
        perror("An error occured while reading data. Data corrupted.");
      }

      memcpy(dest, chunk_data, chunk_size);
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
  return rc;
}

// Add a peer to peers_t
void add_peer(peers_t *peers, peer_fd_t peer)
{
  if (peers->size >= peers->capacity)
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
      // close(peers->arr[i]);
      peers->arr[i] = peers->arr[--peers->size];
      break;
    }
  }
  pthread_mutex_unlock(&peers->lock);
}

/**
 * Verifies is self address is initialized
 * \param data THe sock data holding information on my curret address
 */
bool isInitialized(sockdata_t data)
{
  return data.server_addr_len;
}

/**
 * This function request chunks of data from the network and dowloads the file to the client machine
 *  \param file_hash THe hash of the file whic hshould be downloaded from teh network;
 *
 */
void *download_file(unsigned char file_hash[MD5_DIGEST_LENGTH])
{

  // open temporary port to listen for chunk donwloads.
  unsigned short temp_port = 0;
  int server_fd = server_socket_open(&temp_port);
  if (server_fd == -1)
    return NULL;

  listen(server_fd, 10);

  // Create thread to listen at this prt for incoming connections
  pthread_t recv_thread;
  int *fd_ptr = malloc(sizeof(int));
  *fd_ptr = server_fd;
  pthread_create(&recv_thread, NULL, connectWorker, fd_ptr);

  // The return address of this temporary port so it can be found
  // credit: https://stackoverflow.com/a/13047959
  struct sockaddr_in return_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(temp_port),
      .sin_addr.s_addr = INADDR_ANY};

  while (true)
  {
    // stop if the fie is fully downloaded.
    verified_chunks_t chunks = verify_tfile(&ht, file_hash);
    if (chunks == VERIFIED_FILE)
      break;

    for (int i = 0; i < NUM_CHUNKS; i++)
    {
      // only download incomplete
      if (!is_chunk_verified(chunks, i))
      {
        chunk_request_t req = {
            .chunk_index = i,
            .ttl = 5,
            .return_addr = return_addr,
            .return_addr_len = sizeof(return_addr)};

        memcpy(req.file_hash, file_hash, MD5_DIGEST_LENGTH);

        message_info_t info = {
            .type = REQUEST_FILE_DATA,
            .size = sizeof(chunk_request_t)};

        pthread_mutex_lock(&peers.lock);
        for (int p = 0; p < peers.size; p++)
        {
          send_message(peers.arr[p], &info, &req);
        }
        pthread_mutex_unlock(&peers.lock);
      }
    }

    // allows the response to arrive
    sleep(1);
  }

  // save file to disk
  save_tfile(&ht, file_hash);
  char message[500];

  // Format the message using snprintf
  snprintf(message, sizeof(message), "File '%s' downloaded!", search_htable(&ht, file_hash)->tdef.name);

  // Now call the ui_display function with the formatted message
  ui_display("system", message);

  close(server_fd);
  return NULL;
}
