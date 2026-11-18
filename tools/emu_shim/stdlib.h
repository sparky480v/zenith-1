#pragma once
#include <stddef.h>
void *malloc(size_t n);
void free(void *p);
void exit(int c) __attribute__((noreturn));
int atoi(const char *s);
double atof(const char *s);
long strtol(const char *s, char **e, int b);
unsigned long strtoul(const char *s, char **e, int b);
static inline char *getenv(const char *s) { (void)s; return 0; }
