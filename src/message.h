#pragma once

#include <sys/socket.h>
// Unused for now...
#define MAX_MESSAGE_LENGTH 2048

// Types of messages (stored in header data)
#define REQUEST_FILE_DATA 0xA
#define FILE_DATA         0xB
#define TFILE_DEF         0xC
#define ADDR_SELF         0xD
#define REQUEST_ADDR_SELF 0xE
typedef unsigned char message_type_t;

typedef struct {
  message_type_t type;
  size_t size;
} message_info_t;

#define FAILED -1
#define SUCCESS 0

// Send a message over a socket.
// Message type, size, and data are sent in that order.
// Returns FAILED if failed and SUCCESS on success.
int send_message(int fd, message_info_t* info, void* data);

// Recieve a message.
// Return message type or FAILED if failed.
// Data is written to <data>
int receive_message(int fd, void* data, size_t size);

// Get the oncoming message's info.
int incoming_message_info(int fd, message_info_t* info);
