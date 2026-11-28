#pragma once

#include <stddef.h>

// Copies a UTF-8 string, cutting at a character boundary if it does not fit.
void text_copy(char *dst, size_t size, const char *src);
