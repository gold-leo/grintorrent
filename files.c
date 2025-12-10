
#include <openssl/md5.h>
#include "files.h"

// Files.c describes the functions used to manipulate the files that are
// distributed over the network.

int create_new_file_desc(char* file_path, file_desc_t* file_desc) {
    // add the details of the file and store within file_desc
    // return -1 if fail, 0 if succeed
}

/*
* Verify a file's status compared to a file_desc. 
* Items compared:
*  - File size
*  - File name
*  - File hash
*  - Num of chunks
*  - Num of 
*/
int verify_file(char* file_path, file_desc_t* file_desc, file_status* file_status) {
    // check the size
    // check the hashes
    // check the name
    // return information on hash success and failure
    // return what chunks are verified
}


