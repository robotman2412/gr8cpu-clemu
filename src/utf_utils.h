
#ifndef UTF_UTILS_H
#define UTF_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Concatenates a UTF8-encoded character code.
void utf_cat(char *buf, unsigned int code);

// Grab a single utf8 character, by updating the value of an int.
// Returns the new string pointer.
// Returns zero if end of string.
char *utf_get_char(char *str, uint32_t *out);
// Find an utf8 substring, by returning pointer and length.
void utf_substring(char *str, size_t offset, size_t len, char **str_out, size_t *len_out);
// How many UTF8 characters are in str.
size_t utflen(char *str);

// Grab a single visible character, by returning pointer and length.
// Returns the new string pointer.
// Returns a length of zero if end of string.
char *visible_get_char(char *str, char **str_out, size_t *len_out);
// Find a visible substring, by returning pointer and length.
void visible_substring(char *str, size_t offset, size_t len, char **str_out, size_t *len_out);
// How many visible characters are in str.
size_t visiblelen(char *str);

#endif //UTF_UTILS_H
