#if !defined(UI_H)
#define UI_H

/**
 * The type of a callback function run by the user interface every time there is
 * a new message provided in the input pane. The parameter points to memory that
 * will be reused after the callback returns, so if you need to retain the
 * string after the callback you must copy it.
 */
typedef void (*input_callback_t)(const char*);

/**
 * Initialize the user interface and set up a callback function that should be
 * called every time there is a new message to send.
 *
 * \param callback  A function that should run every time there is new input.
 *                  The string passed to the callback should be copied if you
 *                  need to retain it after the callback function returns.
 */
void ui_init(input_callback_t callback);

/**
 * Run the main UI loop. This function will only return the UI is exiting.
 */
void ui_run();

/**
 * Add a new message to the user interface's display pane.
 *
 * \param username  The username that should appear before the message. The UI
 *                  code will copy the string out of message, so it is safe to
 *                  reuse the memory pointed to by message after this function.
 *
 * \param message   The string that should be added to the display pane. As with
 *                  the username, the UI code will copy the string passed in.
 */
void ui_display(const char* username, const char* message);

/**
 * Stop the user interface and clean up.
 */
void ui_exit();

#endif
