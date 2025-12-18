#ifndef UI_ADAPTER_H
#define UI_ADAPTER_H

/**
 * Callback function that handles input from the UI.
 *
 * \param input The string entered by the user.
 */
void ui_input_handler(const char *input);

/**
 * Callback function that provides a list of network files.
 *
 * \param files A pointer to an array of strings. The function should allocate
 *              memory for the array and strings, and return the count.
 * \return The number of files returned in the array.
 */
int ui_list_network_files(char ***files);

#endif // UI_ADAPTER_H
