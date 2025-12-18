#include "ui.h"
#include "file.h"
#include "client.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

extern htable_t ht;

/**
 * UI callback: list files available on the network
 * Allocates an array of filenames; UI will free it
 */
int ui_list_network_files(char ***files)
{
    tfile_def_t *tfiles = NULL;
    int count = list_tfiles(&ht, &tfiles);
    if (count <= 0)
    {
        *files = NULL;
        return 0;
    }

    char **names = malloc(sizeof(char *) * count);
    if (!names)
    {
        free(tfiles);
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        names[i] = strdup(tfiles[i].name); // Just store the filename
    }

    free(tfiles);
    *files = names;
    return count;
}

/**
 * Function that handles user input from teh user to downlaod correct data
 * \param input The input string whic his the name of the file to download
 */
void ui_input_handler(const char *input)
{
    // Ignore empty input
    if (!input || strlen(input) == 0)
        return;
    if (strcmp(input, ":quit") == 0 || strcmp(input, ":q") == 0)
    {
        ui_exit();
    }

    tfile_def_t *tfiles = NULL;
    int count = list_tfiles(&ht, &tfiles);

    for (int i = 0; i < count; i++)
    {
        if (strcmp(tfiles[i].name, input) == 0) // compare only the raw filename
        {
            verified_chunks_t status = verify_tfile(&ht, tfiles[i].f_hash);
            if (status == VERIFIED_FILE)
            {
                ui_display("system", "File already downloaded!");
                free(tfiles);
                return;
            }

            ui_display("system", "Starting download...");

            pthread_t dl;
            unsigned char *hash = malloc(MD5_DIGEST_LENGTH);
            memcpy(hash, tfiles[i].f_hash, MD5_DIGEST_LENGTH);

            pthread_create(&dl, NULL,
                           (void *(*)(void *))download_file,
                           hash);

            free(tfiles);
            return;
        }
    }

    ui_display("system", "File not found in network");
    free(tfiles);
}