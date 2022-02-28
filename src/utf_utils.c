
#include "utf_utils.h"
#include <string.h>

// Concatenates a UTF8-encoded character code.
void utf_cat(char *buf, unsigned int code) {
	char *append  = buf + strlen(buf);
	int   n       = 0;
	if (code <= 0x00007f && code) {
		// Insert normally.
		append[0] = code;
		n         = 1;
	} else if (code <= 0x0007ff) {
		// Insert 11-bit.
		append[0] = 0xC0 | ((code >> 6)  & 0x1F);
		append[1] = 0x80 | ( code        & 0x3F);
		n         = 2;
	} else if (code <= 0x00ffff) {
		// Insert 16-bit.
		append[0] = 0xC0 | ((code >> 12) & 0x0F);
		append[1] = 0x80 | ((code >>  6) & 0x3F);
		append[2] = 0x80 | ( code        & 0x3F);
		n         = 3;
	} else if (code <= 0x1fffff) {
		// Insert 21-bit.
		append[0] = 0xE0 | ((code >> 18) & 0x07);
		append[1] = 0x80 | ((code >> 12) & 0x3F);
		append[2] = 0x80 | ((code >>  6) & 0x3F);
		append[3] = 0x80 | ( code        & 0x3F);
		n         = 4;
	}
	append[n] = 0;
}

// Grab a single visible character, by returning the length based on the input.
// Returns the new string pointer.
// Returns zero if end of string.
char *utf_get_char(char *ptr, uint32_t *out) {
	if (*ptr & 0x80) {
		// Long characters.
		size_t bytes;
		char mask;
		if ((*ptr & 0xE0) == 0xC0) {
			bytes = 2;
			mask = 0x1F;
		} else if ((*ptr & 0xF0) == 0xE0) {
			bytes = 3;
			mask = 0x0F;
		} else if ((*ptr & 0xF8) == 0xF0) {
			bytes = 4;
			mask = 0x07;
		} else {
			bytes = 1;
		}
		uint32_t tmp = 0;
		for (size_t i = 0; i < bytes; i++) {
			tmp = (tmp << 6) | (ptr[i] & mask);
			mask = 0x3F;
		}
		*out = tmp;
		return ptr + bytes;
	} else if (*ptr) {
		// Simple characters.
		*out = (uint32_t) *(unsigned char *) ptr;
		return ptr + 1;
	} else {
		// End of the string.
		*out = 0;
		return ptr;
	}
}

// Find an utf8 substring, by returning pointer and length.
void utf_substring(char *str, size_t offset, size_t len, char **str_out, size_t *len_out) {
	// Skip a number of characters.
	while (offset--) {
		uint32_t c;
		str = utf_get_char(str, &c);
		if (!c) {
			// We got nothing.
			*str_out = str;
			*len_out = 0;
			return;
		}
	}
	char *origin = str;
	// Then, count a number of characters.
	while (len--) {
		uint32_t c;
		str = utf_get_char(str, &c);
		if (!c) {
			// We got the end early.
			break;
		}
	}
	*str_out = origin;
	*len_out = str - origin;
	return;
}

// How many UTF8 characters are in str.
size_t utflen(char *str) {
	size_t len = 0;
	while (*str) {
		uint32_t c;
		str = utf_get_char(str, &c);
		if (!c) break;
		len ++;
	}
	return len;
}

// Grab a single visible character, by returning pointer and length.
// Returns the new string pointer.
// Returns a length of zero if end of string.
char *visible_get_char(char *str, char **str_out, size_t *len_out) {
	char *ptr = str;
	while (1) {
		uint32_t c;
		ptr = utf_get_char(ptr, &c);
		if (c == 0x1B) {
			// Let's filter out ANSI control sequences.
			char *ptr1 = utf_get_char(ptr, &c);
			if (c == '[') {
				ptr = ptr1;
				while (1) {
					ptr = utf_get_char(ptr, &c);
					if (c != ';' && (c < '0' || c > '9')) break;
				}
			} else {
				// This is not an ANSI escape sequence.
				break;
			}
		} else {
			// This is a normal character.
			break;
		}
	}
	*str_out = str;
	*len_out = ptr - str;
	return ptr;
}

// Find a visible substring, by returning pointer and length.
void visible_substring(char *str, size_t offset, size_t len, char **str_out, size_t *len_out) {
	// Skip a number of characters.
	while (offset--) {
		char *res;
		size_t res_len;
		str = visible_get_char(str, &res, &res_len);
		if (!res_len) {
			// We got nothing.
			*str_out = str;
			*len_out = 0;
			return;
		}
	}
	char *origin = str;
	// Then, count a number of characters.
	while (len--) {
		char *res;
		size_t res_len;
		str = visible_get_char(str, &res, &res_len);
		if (!res_len) {
			// We got the end early.
			break;
		}
	}
	*str_out = origin;
	*len_out = str - origin;
	return;
}

// Find a visible substring, by returning pointer and length.
size_t visiblelen(char *str) {
	size_t len = 0;
	while (*str) {
		char *c;
		size_t clen;
		str = visible_get_char(str, &c, &clen);
		if (!clen) break;
		len ++;
	}
	return len;
}
