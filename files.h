#if !defined(UI_H)
#define UI_H

#include <openssl/md5.h>
#include <sys/stat.h>

//structure holding chunk information
typedef struct {
  bool status;
  char hash[MD5_DIGEST_LENGTH];
} chunk_t;

// structure for storing file
typedef struct {
  char *filepath;
  struct stat sb;
  char *mapped_data;
  int no_of_chunks;
  chunk_t *chunks;

} file_desc_t;

// status of a file
typedef struct {

} file_status;

int create_new_file_desc(char*, file_desc_t*);
int verify_file(char*, file_desc_t*, file_status*);

#endif
