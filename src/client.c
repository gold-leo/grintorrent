// This is where all the client code will be stored.
// TODO:
// -
// Address to a peer
// typedef struct {
//   char host[45]; // Length of a host address is 45 chars for ip, but the length of mathLAN computer should be checked.
//   int socket;
// } address_t;
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "socket.h"
#include "file.h"

// Socket file descriptor type
typedef int peer_fd_t;

// List of peer socket file descriptors.
typedef struct {
  pthread_mutex_t lock;
  int capacity;
  int size;
  peer_fd_t* arr;
} peers_t;

// Add a peer to peers_t
void add_peer(peers_t* peers, peer_fd_t peer) {
  if (peers->capacity >= peers->size) {
    pthread_mutex_lock(&peers->lock);
    peers->arr = realloc(peers->arr, peers->capacity * 2 * sizeof(peer_fd_t));
    peers->arr[peers->size++] = peer;
    pthread_mutex_unlock(&peers->lock);
  } else {
    pthread_mutex_lock(&peers->lock);
    peers->arr[peers->size++] = peer;
    pthread_mutex_unlock(&peers->lock);
  }
}

// Remove a peer from peers_t
void remove_peer(peers_t* peers, peer_fd_t peer) {
    pthread_mutex_lock(&peers->lock);
    // Find and remove the passed in lock, as order of the array does not matter we just replace the removed socket with the last socket in the array
    // If the socket is already removed, the loop will fail safely.
    for (int i = 0; i < peers->size; i++) {
        if (peers->arr[i] == peer) {
            close(peers->arr[i]);
            peers->arr[i] = peers->arr[--peers->size];
            break;
        }
    }
    pthread_mutex_unlock(&peers->lock);
}

// ----Main loop----
int main(int argc, char** argv) {
  if (argc != 2 && argc != 4) {
      fprintf(stderr, "Usage:\n%s [<peer> <port number>] <your address>\n%s host <your address>\n", argv[0], argv[0]);
      exit(1);
  }

  // Initialize socket/peer file descriptor array
  peers_t peers = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .capacity = 100,
    .size = 0
  };
  peers.arr = malloc(peers.capacity * sizeof(peer_fd_t));

  // Create server socket
  unsigned short network_port = 0;
  int server_fd = server_socket_open(&network_port);
  if (server_fd == -1) {
      perror("Server socket open failed");
      exit(EXIT_FAILURE);
  }

  // Start listening for connections on server socket
  if (listen(server_fd, 10)) {
      perror("listen failed");
      exit(EXIT_FAILURE);
  }

  // Address of this client.
  const char* hostname[45];
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(struct sockaddr_in);
  

  // Hash table of the tfiles.
  htable_t ht;
  init_htable(&ht);

  // Connect to peer if given in startup
  if (argc == 3) {
      char* host = argv[1];
      unsigned short target_port = atoi(argv[2]);

      peer_fd_t host_peer = socket_connect(host, target_port);
      if (host_peer == -1) {
          perror("Failed to connect");
          exit(EXIT_FAILURE);
      }

      add_peer(&peers, host_peer);

      // Recieve from peer the hostname of this client
      // For now, just memcpy the hostname.
      memcpy(hostname, , unsigned long)

      // Create thread for peer
      // pthread_t t;
      // pthread_create(&t, NULL, <thread>, &socket_fd);
      // pthread_detach(t);
  } else {
    // Set hostname of this client
    memcpy(hostname, argv[1], 45);
  }



}
