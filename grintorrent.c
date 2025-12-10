#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"

// structure to hold arguments
typedef struct
{
  char *peer_p;
  char *port_p;
  char *file_p;
  char *username_p;

} cmd_args_t;

// FUNCTION DEFINITONS
int send_message(int fd, char *message);
void *readWorker(void *args);
void print_usage(char **argv);
void parse_args(cmd_args_t *args, int argc, char **argv);
void closeConnection(int socket_fd);

// END FUNCTION DEFINITIONS

#define MAX_MESSAGE_LENGTH 2048
#define SOCKET_CLOSED -1

int *clientSockets;
int maxSize = 10;
int currentSize = 0;

// Keep the username in a global so we can access it from the callback
const char *username;

// This function is run whenever the user hits enter after typing a message
void input_callback(const char *message)
{
  if (strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0)
  {
    ui_exit();
  }
  else
  {
    ui_display(username, message);

    // send 2 messages:
    //  first message is the username and the second is the actual message content
    for (int i = 0; i < currentSize; i++)
    {
      if (clientSockets[i] != SOCKET_CLOSED)
      {
        int rc = send_message(clientSockets[i], (char *)username);
        if (rc == -1)
        {
          closeConnection(clientSockets[i]);
        }

        rc = send_message(clientSockets[i], (char *)message);
        if (rc == -1)
        {
          closeConnection(clientSockets[i]);
        }
      }
    }
  }
}

int send_message(int fd, char *message)
{
  // If the message is NULL, set errno to EINVAL and return an error
  if (message == NULL)
  {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the message in a size_t
  size_t len = strlen(message);
  if (write(fd, &len, sizeof(size_t)) != sizeof(size_t))
  {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the message. Loop until the entire message has been written.
  size_t bytes_written = 0;
  while (bytes_written < len)
  {
    // Try to write the entire remaining message
    ssize_t rc = write(fd, message + bytes_written, len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0)
      return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  return 0;
}

// Receive a message from a socket and return the message string (which must be freed later)
char *receive_message(int fd)
{
  // First try to read in the message length
  size_t len;
  if (read(fd, &len, sizeof(size_t)) != sizeof(size_t))
  {
    // Reading failed. Return an error
    return NULL;
  }

  // Now make sure the message length is reasonable
  if (len > MAX_MESSAGE_LENGTH)
  {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the message and a null terminator
  char *result = malloc(len + 1);

  // Try to read the message. Loop until the entire message has been read.
  size_t bytes_read = 0;
  while (bytes_read < len)
  {
    // Try to read the entire remaining message
    ssize_t rc = read(fd, result + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0)
    {
      free(result);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  // Add a null terminator to the message
  result[len] = '\0';

  return result;
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

    pthread_t thread;

    if (currentSize == maxSize)
    {
      clientSockets = realloc(clientSockets, maxSize * 2);
      maxSize += maxSize;
    }
    clientSockets[currentSize] = client_socket_fd;
    currentSize++;

    pthread_create(&thread, NULL, readWorker, (void *)&client_socket_fd);
  }
  return NULL;
}

/**
 * THis function closes a socket and sets its value to SOCKET CLOSED in our bookeeping array
 * \param socket_fd The socket to close
 */
void closeConnection(int socket_fd)
{
  // find the socket in the array and set it to socket closed
  for (int i = 0; i < currentSize; i++)
  {
    if (clientSockets[i] == socket_fd)
    {
      // IF
      clientSockets[i] = SOCKET_CLOSED;
    }
  }
  close(socket_fd);
}
/**
 * This function watches a socket forever until a message is read. That message is then displayed and relayed to all other connected sockets.
 * \param args a void star pointer holding the id of the client to reaad from.
 */
void *readWorker(void *args)
{
  int client_socket_fd = *(int *)args;
  char *message;
  char *username;

  while (true)
  {
    // Read a message from the client
    username = receive_message(client_socket_fd);
    message = receive_message(client_socket_fd);
    if (username == NULL || message == NULL)
    {
      closeConnection(client_socket_fd);
    }
    else
    {
      // print message
      ui_display(username, message);

      // Send a message to the client
      for (int i = 0; i < currentSize; i++)
      {
        // if it is not ourself
        if (clientSockets[i] != client_socket_fd && clientSockets[i] != SOCKET_CLOSED)
        {
          int rc = send_message(clientSockets[i], username);
          if (rc == -1)
          {
            closeConnection(client_socket_fd);
          }
          // send the mesage if not closed
          else
          {
            rc = send_message(clientSockets[i], message);
          }
        }
      }
    }
    // Free the message string
    free(message);
  }

  return NULL;
}

void free_args(cmd_args_t args);

int main(int argc, char **argv)
{

  // parse arguments
  cmd_args_t args = {NULL};
  parse_args(&args, argc, argv);

  // check for extra elements
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

  free_args(args);

  /**
   * MAKE INTERACTIVE FROM TO READ FILES ON NETWORK.
   * IF A FILE IS PRESENT< STORE THE FILE AND SPLIT INTO CHUNKS.
   * SEND AND RECEIVE REQUESTS FORM OTHERS.
   */

  exit(EXIT_SUCCESS);

  clientSockets = malloc(sizeof(int) * maxSize);

  if (clientSockets == NULL)
  {
    perror("Failed to malloc");
    exit(EXIT_FAILURE);
  }

  // Save the username in a global
  username = argv[1];

  // TODO: Set up a server socket to accept incoming connections
  unsigned short port = 0;

  int server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1)
  {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Start listening for connections, with a maximum of one queued connection
  if (listen(server_socket_fd, 1))
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %u\n", port);
  // Did the user specify a peer we should connect to?
  if (argc == 4)
  {
    // Unpack arguments
    char *peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);

    // TODO: Connect to another peer in the chat network
    int socket_fd = socket_connect(peer_hostname, peer_port);
    if (socket_fd == -1)
    {
      perror("Failed to connect");
      exit(EXIT_FAILURE);
    }

    clientSockets[currentSize] = socket_fd;
    currentSize++;

    // read messages using this thread.
    pthread_t thread;
    pthread_create(&thread, NULL, readWorker, (void *)&socket_fd);
  }

  pthread_t thread;
  pthread_create(&thread, NULL, connectWorker, (void *)&server_socket_fd);

  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.
  ui_init(input_callback);

  char buffer[50];
  sprintf(buffer, "%u", port);

  // Once the UI is running, you can use it to display log messages

  ui_display("PORT", buffer);

  // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
  ui_run();
  // free our allocatdd memory
  free(clientSockets);
  return 0;
}

/**
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