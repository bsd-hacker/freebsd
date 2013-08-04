#ifndef LIB_CONSOLE_H_
#define LIB_CONSOLE_H_
#include <stdarg.h>
#include <kern/console.h>

#define panic(...) {printf("[panic] "); printf(__VA_ARGS__); while(1);}
int printf(const char *format, ...);
void puts(const char *str);
void putns(const char *str, int n);
void putd(const int num);
void putu(const unsigned num);
void putlu(unsigned long long num);
void putx(const unsigned num);
int getns(char *s, int size);

#endif /* LIB_CONSOLE_H_*/
