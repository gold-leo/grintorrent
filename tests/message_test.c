#include <stdio.h>
#include <stdlib.h>
#include "../src/message.h"
#include "../src/socket.h"

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
        exit(1);
    }

    max_sockets = 100;
    sockets = malloc(sizeof(int) * 100);


    // Create server socket
    unsigned short port = 0;
    int server_fd = server_socket_open(&port);
    if (server_fd == -1) {
        perror("Server socket open failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, 10)) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Connect to peer if given in startup
    if (argc == 4) {
        char* host = argv[2];
        unsigned short target_port = atoi(argv[3]);

        int socket_fd = socket_connect(host, target_port);
        if (socket_fd == -1) {
            perror("Failed to connect");
            exit(EXIT_FAILURE);
        }

        // Locking as we update the globals
        pthread_mutex_lock(&sockets_lock);
        sockets[num_of_sockets] = socket_fd;
        num_of_sockets++;
        pthread_mutex_unlock(&sockets_lock);

        // Create read thread
        pthread_t t;
        pthread_create(&t, NULL, readThread, &socket_fd);
        pthread_detach(t);
    }
