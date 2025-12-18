#include "ui.h"

#include <form.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// The height of the input field in the user interface
#define INPUT_HEIGHT 3

// The timeout for input
#define INPUT_TIMEOUT_MS 10

// The ncurses forms code is loosely based on the first example at
// http://tldp.org/HOWTO/NCURSES-Programming-HOWTO/forms.html

/*--------------------------UI Objects---------------------------------*/

// The fields array for the display form
FIELD* display_fields[2];

// The form that holds the display field
FORM* display_form;

// The fields array for the input form
FIELD* input_fields[2];

// The form that holds the input field
FORM* input_form;

/*--------------------------Threads---------------------------------*/

// The handle for the UI thread
pthread_t ui_thread;

// A lock to protect the entire UI
pthread_mutexattr_t ui_lock_attr;
pthread_mutex_t ui_lock;

// Global variable indicating whether the UI loop should run
static bool ui_running = false; 

/*--------------------------Callback---------------------------------*/

// The callback function to run on new input
static input_callback_t input_callback = NULL;
// Called when user presses ENTER on empty input
static file_list_callback_t file_list_callback = NULL;

//structure that hold message to be displayed
struct ui_message{
  char* name;
  char* message;
  //pointer to the next message in the list
  struct ui_message* next;
};
// Head of the message list
static struct ui_message* head = NULL;
// Tail of the message list
static struct ui_message* tail = NULL;

// Trims trailing whitespace
static void safe_trim(char* str) {
  // Compute length of the string
  size_t len = strlen(str);
  if (len == 0) return; 
  // Start from the last character
  char* end = str + len - 1; 
  while (end >= str && isspace((unsigned char)*end)) {
    *end = '\0'; 
    end--; 
}
}

// Insert a message in the back of the list to be displayed by the UI thread
static void insert(const char* name, const char* message) {
  // Allocate a new node
  struct ui_message* new_node = malloc(sizeof(*new_node));
  if (!new_node) return;

  // Duplicate name string
  new_node->name = strdup(name);
  // Duplicate message string
  new_node->message = strdup(message); 
  // Initialize next pointer
  new_node->next = NULL; 
  // Lock list for thread-safe access
  pthread_mutex_lock(&ui_lock);

  //if list is  empty
  if (!tail) {
    // New node becomes both head and tail
    head = tail = new_node; 
  } else {
    // Append node to the end of the queue
    tail->next = new_node;
    // Update tail pointer
    tail = new_node; 
  }

  // Unlock list 
  pthread_mutex_unlock(&ui_lock);
}


// Remove the top node from the list; returns NULL if list is empty
static struct ui_message* remove() {
  pthread_mutex_lock(&ui_lock); // Lock queue for safe removal
  // Grab current head
  struct ui_message* top = head;
  //if not empty
  if (top) {
    // Advance head to next element
    head = top->next;
    // If list is now empty, clear tail
    if (!head) tail = NULL;
  }
  //Unlock the list
  pthread_mutex_unlock(&ui_lock);
  return top;
}


// Registers the callback used to fetch a list of network files
// File_list_callback is a pointer to a function that will give us the list of files in the network
void ui_register_file_list_callback(file_list_callback_t cb) {
  // Store callback pointer
  file_list_callback = cb;
}

/**
 * Initialize the user interface and set up a callback function that should be
 * called every time there is a new message to send.
 *
 * \param callback  A function that should run every time there is new input.
 *                  The string passed to the callback should be copied if you
 *                  need to retain it after the callback function returns.
 */
void ui_init(input_callback_t callback) {
  // Initialize curses
  initscr();
  // Disable line buffering
  cbreak();
  // Do not echo typed characters
  noecho();
  // Hide curser
  curs_set(0);
  // make getch() non-blocking
  timeout(INPUT_TIMEOUT_MS);
  // Enable function and arrow keys
  keypad(stdscr, TRUE);

  // Get the number of rows and columns in the terminal display
  int rows;
  int cols;
  // This uses a macro to modify rows and cols
  getmaxyx(stdscr, rows, cols);

  // Calculate the height of the display field
  int display_height = rows - INPUT_HEIGHT - 1;

  // Create the larger message display window
  // height, width, start row, start col, overflow buffer lines, buffers
  display_fields[0] = new_field(display_height, cols, 0, 0, 0, 0);
  display_fields[1] = NULL;

  // Create the input field
  input_fields[0] = new_field(INPUT_HEIGHT, cols, display_height + 1, 0, 0, 0);
  input_fields[1] = NULL;

  // Grow the display field buffer as needed
  field_opts_off(display_fields[0], O_STATIC);

  // Don't advance to the next field automatically when using the input field
  field_opts_off(input_fields[0], O_AUTOSKIP);

  // Turn off word wrap (nice, but causes other problems)
  field_opts_off(input_fields[0], O_WRAP);
  field_opts_off(display_fields[0], O_WRAP);

  // Create the forms
  display_form = new_form(display_fields);
  input_form = new_form(input_fields);

  // Display the forms
  post_form(display_form);
  post_form(input_form);
  refresh();

  // Draw a horizontal split
  for (int i = 0; i < cols; i++) {
    mvprintw(display_height, i, "-");
  }

  // Update the display
  refresh();

  // Save the callback function
  input_callback = callback;

  // Initialize the UI lock
  pthread_mutexattr_init(&ui_lock_attr);
  pthread_mutexattr_settype(&ui_lock_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&ui_lock, &ui_lock_attr);

  // Running
  ui_running = true;
}

/**
 * Run the main UI loop. This function will only return the UI is exiting.
 */
void ui_run() {
  // Loop as long as the UI is running
  while (ui_running) {
    // Get a character
    int ch = getch();

    // If there was no character, try again
    if (ch == -1) continue;
    //Process messages from other threads
    struct ui_message* message;
    while((message = remove()) != NULL){
      //moving to a new line on a display field
      form_driver(display_form, REQ_NEW_LINE);
      // Render name character by character
      for(const char* c = message->name; *c; c++){
        form_driver(display_form, *c);
      }

      form_driver(display_form, ':');
      form_driver(display_form, ' ');
      // Render message text
      for (const char* c = message->message; *c; c++){
        form_driver(display_form, *c);
      }

      free(message->name); // Free duplicated username string
      free(message->message); // Free duplicated message string
      free(message); // Free queue node
    }

    //Handle backspace and delete hey
    if(ch == KEY_BACKSPACE || ch == 127){
      //delete previous charachter
      form_driver(input_form, REQ_DEL_PREV);
    } else if(ch == '\n' || ch == KEY_ENTER){
      form_driver(input_form, REQ_NEXT_FIELD);
      // Obtain ncurses-managed buffer
      char* raw = field_buffer(input_fields[0], 0);
      // Independent copy
      char* copy = strdup(raw); 
      // Trim trailing whitespace
      safe_trim(copy);
      // Empty input: request file list
      if (strlen(copy) == 0) {
        // Ensure callback is registered
        if (file_list_callback) {
          // Pointer to array of filenames
          char** files = NULL;
          int count = file_list_callback(&files);
          // Creating spacing before header
          form_driver(display_form, REQ_NEW_LINE);
          form_driver(display_form, REQ_NEW_LINE);
          // Header label
          const char* header = "[ Network Files ]";
          // Render header
          for (const char* c = header; *c; c++){
            form_driver(display_form, *c);
          }
        
          // Iterate through returned file list
          for (int i = 0; i < count; i++) {
            form_driver(display_form, REQ_NEW_LINE);// New line per file
            // Render filename
            for (char* c = files[i]; *c; c++){
              form_driver(display_form, *c);
            }
            // Free individual filename 
            free(files[i]);
          }
          // Free filename array
          free(files); 
        }
      }else{
        if(input_callback){
          input_callback(copy);
        }
      }
      //free duplicate
      free(copy);
      //if still running clear the field
      if(ui_running){
        form_driver(input_form, REQ_CLR_FIELD);
      }
    } else{
    form_driver(input_form, ch);
    }
    refresh();
  }
}

//Displaying a 
void ui_display(const char* username, const char* message) {
  insert(username, message); // Queue message for UI thread
}

/**
 * Stop the user interface and clean up.
 */
void ui_exit() {
  // Block access to the UI
  pthread_mutex_lock(&ui_lock);

  // The UI is not running
  ui_running = false;

  // Clean up
  unpost_form(display_form);
  unpost_form(input_form);
  free_form(display_form);
  free_form(input_form);
  free_field(display_fields[0]);
  free_field(input_fields[0]);
  endwin();

  // Unlock the UI
  pthread_mutex_unlock(&ui_lock);
}
