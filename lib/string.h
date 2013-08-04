#ifndef LIB_STRING_H_
#define LIB_STRING_H_
#include <lib/types.h>

void *memset(void *s, int c, int n);
void *memcpy(void *dest, const void *src, int n);
int strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

#endif /* LIB_STRING_H_*/
