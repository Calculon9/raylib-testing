/* Small string helper utilities */
#ifndef STR_HELPERS_H
#define STR_HELPERS_H

#include <stddef.h>

// Copy up to dst_size-1 bytes and always NUL-terminate dst (if dst_size>0)
static inline void safe_strncpy(char *dst, const char *src, size_t dst_size)
{
    if (!dst || !src || dst_size == 0)
        return;
    // Use snprintf behaviour to guarantee termination
    size_t i = 0;
    for (; i + 1 < dst_size && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

#endif

