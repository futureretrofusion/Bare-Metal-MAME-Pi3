/*
 * FRF HAL freestanding C memory routines, independent implementation.
 * Copyright (c) 2026 Future Retro Fusion. SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *source, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)source;
    while (count--) *d++ = *s++;
    return dest;
}
void *memset(void *dest, int value, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    while (count--) *d++ = (unsigned char)value;
    return dest;
}
void *memmove(void *dest, const void *source, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)source;
    if (d == s || !count) return dest;
    if ((uintptr_t)d < (uintptr_t)s) {
        while (count--) *d++ = *s++;
    } else {
        d += count; s += count;
        while (count--) *--d = *--s;
    }
    return dest;
}
int memcmp(const void *left, const void *right, size_t count)
{
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    while (count--) {
        if (*a != *b) return (int)*a - (int)*b;
        ++a; ++b;
    }
    return 0;
}
size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}
