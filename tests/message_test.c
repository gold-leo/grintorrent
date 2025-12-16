#include <stdio.h>
#include <stdlib.h>
#include "../src/message.h"
#include "../src/socket.h"

int main(int argc, char** argv) {
  if (argc != 1 && argc != 3) {
      fprintf(stderr, "Usage: %s [<peer> <port number>]\n", argv[0]);
      exit(1);
  }

  int socket;
  // Create server socket
  unsigned short port = 0;
  int server_fd = server_socket_open(&port);
  if (server_fd == -1) {
      perror("Server socket open failed");
      exit(EXIT_FAILURE);
  }
  printf("Hosting on %hu\n", port);
  fflush(0);

  // Start listening for connections
  if (listen(server_fd, 10)) {
      perror("listen failed");
      exit(EXIT_FAILURE);
  }

  // Connect to peer if given in startup
  if (argc == 3) {
    char* host = argv[1];
    unsigned short target_port = atoi(argv[2]);

    socket = socket_connect(host, target_port);
    if (socket == -1) {
      perror("Failed to connect");
      exit(EXIT_FAILURE);
    }

    message_info_t info = {
      .type = REQUEST_ADDR_SELF,
      .size = 0
    };
    send_message(socket, &info, NULL);

    while (1) {
      message_info_t info;
      if (incoming_message_info(socket, &info) == FAILED) {
        continue;
      }
      if (info.type == ADDR_SELF) {
        printf("ADDR_SELF (%ld)\n", info.size);
        socklen_t server_addr_len = info.size;
        struct sockaddr_in server_addr;
        if (receive_message(socket, &server_addr, server_addr_len)) {
          printf("failed\n");
        }
      } else {
        break;
      }
    }
  } else {
    socklen_t server_addr_len;
    struct sockaddr_in server_addr;
    int socket = server_socket_accept_addr(server_fd, &server_addr, &server_addr_len);
    if (socket == -1) {
        perror("accept failed");
        exit(1);
    }

    while (1) {
      message_info_t info;
      if (incoming_message_info(socket, &info) == FAILED) {
        continue;
      }
      if (info.type == REQUEST_ADDR_SELF) {
        receive_message(socket, NULL, 0);
        message_info_t info = {
          .type = ADDR_SELF,
          .size = server_addr_len
        };
        send_message(socket, &info, &server_addr);
      } else {
        break;
      }
    }

  }

  return 0;
}
