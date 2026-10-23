/* =============================================================================
 * NovaeOS - String utilities (kernel/lib/kstring.h)
 * ============================================================================= */

#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdbool.h>

size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrcpy(char *dst, const char *src);
char  *kstrncpy(char *dst, const char *src, size_t n);
char  *kstrcat(char *dst, const char *src);
char  *kstrchr(const char *s, int c);
char  *kstrrchr(const char *s, int c);
int    katoi(const char *s);
void   kitoa(int n, char *buf, int base);
char  *kstrtok(char *str, const char *delim, char **saveptr);
bool   kstarts_with(const char *s, const char *prefix);

#endif /* KSTRING_H */
