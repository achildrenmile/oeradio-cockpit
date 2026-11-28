#include "text.h"

#include <string.h>

void text_copy(char *dst, size_t size, const char *src)
{
    if (size == 0) {
        return;
    }
    size_t length = strlen(src);
    if (length >= size) {
        length = size - 1;
        // Do not keep the first bytes of a split multi byte sequence.
        while (length > 0 && ((unsigned char)src[length] & 0xC0) == 0x80) {
            length--;
        }
    }
    memmove(dst, src, length);
    dst[length] = '\0';
}
