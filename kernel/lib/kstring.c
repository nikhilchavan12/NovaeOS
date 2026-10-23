/* =============================================================================
 * NovaeOS - String utilities (kernel/lib/kstring.c)
 * ============================================================================= */

#include "kstring.h"
#include <stdbool.h>
#include <stdint.h>

size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
    return dst;
}

char *kstrcat(char *dst, const char *src) {
    char *d = dst + kstrlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char *kstrchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return c == 0 ? (char *)s : NULL;
}

char *kstrrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char *)last;
}

int katoi(const char *s) {
    int result = 0;
    int sign   = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result * sign;
}

void kitoa(int n, char *buf, int base) {
    const char *digits = "0123456789abcdef";
    char tmp[32];
    int  i = 0;
    bool neg = false;

    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (n < 0 && base == 10) { neg = true; n = -n; }

    unsigned int un = (unsigned int)n;
    while (un > 0) {
        tmp[i++] = digits[un % (unsigned)base];
        un /= (unsigned)base;
    }
    if (neg) tmp[i++] = '-';

    int j = 0;
    for (int k = i - 1; k >= 0; k--) buf[j++] = tmp[k];
    buf[j] = '\0';
}

char *kstrtok(char *str, const char *delim, char **saveptr) {
    char *s = str ? str : *saveptr;
    if (!s) return NULL;

    /* Skip leading delimiters */
    while (*s && kstrchr(delim, *s)) s++;
    if (!*s) { *saveptr = NULL; return NULL; }

    char *start = s;
    while (*s && !kstrchr(delim, *s)) s++;
    if (*s) { *s = '\0'; *saveptr = s + 1; }
    else    { *saveptr = NULL; }

    return start;
}

bool kstarts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) return false;
        s++; prefix++;
    }
    return true;
}
