#include "message.h"
#include <stdlib.h>
#include <unistd.h>

int send_message(int fd, message_info_t *info, void *data)
{
  size_t s = sizeof(message_info_t);
  if (write(fd, info, sizeof(message_info_t)) != sizeof(message_info_t))
  {
    return FAILED;
  }

  if (data != NULL)
  {
    size_t bytes_written = 0;
    while (bytes_written < info->size)
    {
      // Try to write the entire message
      ssize_t rc = write(fd, data + bytes_written, info->size - bytes_written);

      // Did the write fail? If so, return an error
      if (rc <= 0)
        return FAILED;

      // If there was no error, write returned the number of bytes written
      bytes_written += rc;
    }
  }

  return SUCCESS;
}

int receive_message(int fd, void *data, size_t size)
{
  if (data == NULL)
  {
    size = 0;
  }
  message_info_t info;
  if (read(fd, &info, sizeof(message_info_t)) != sizeof(message_info_t))
  {
    // Reading failed. Return an error
    return FAILED;
  }

  // If size is larger than the message, truncate
  if (size > info.size)
  {
    size = info.size;
  }

  // Try to read the message. Loop until the entire message has been read.
  size_t bytes_read = 0;
  while (bytes_read < size)
  {
    // Try to read the entire remaining message
    ssize_t rc = read(fd, data + bytes_read, size - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0)
    {
      return FAILED;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  // If there is extra data to read, discard it.
  ssize_t extra_data = info.size - bytes_read;
  if (extra_data)
  {
    ssize_t bytes_discarded = 0;
    while (bytes_discarded <= extra_data)
    {
      // This flag causes the received bytes of data to be discarded,
      // rather than passed back in a caller-supplied buffer.
      // https://man7.org/linux/man-pages/man7/tcp.7.html
      bytes_discarded += recv(fd, NULL, extra_data - bytes_discarded, MSG_TRUNC);
    }
  }

  return SUCCESS;
}

int incoming_message_info(int fd, message_info_t *info)
{
  size_t s = sizeof(message_info_t);
  if (recv(fd, info, sizeof(message_info_t), MSG_PEEK) != sizeof(message_info_t))
  {
    return FAILED;
  }
  return SUCCESS;
}

