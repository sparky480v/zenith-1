/* SPDX-License-Identifier: MPL-2.0
 * The few libc pieces the simulator touches, for clang's wasm32 target. */
#include <stddef.h>
#include <stdarg.h>
#include "time.h"
void *memcpy(void *d, const void *s, size_t n) { char *a = d; const char *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, size_t n) { char *a = d; while (n--) *a++ = (char)c; return d; }
void *memmove(void *d, const void *s, size_t n) { char *a = d; const char *b = s; if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; } return d; }
int memcmp(const void *a, const void *b, size_t n) { const unsigned char *x = a, *y = b; while (n--) { if (*x != *y) return *x - *y; x++; y++; } return 0; }
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n) { while (n && *a && *a == *b) { a++; b++; n--; } return n ? (unsigned char)*a - (unsigned char)*b : 0; }
char *strchr(const char *s, int c) { for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return 0; } }
char *strrchr(const char *s, int c) { const char *r = 0; for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; } }
char *strstr(const char *a, const char *b) { size_t n = strlen(b); for (; *a; a++) if (!strncmp(a, b, n)) return (char *)a; return n ? 0 : (char *)a; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
char *strncpy(char *d, const char *s, size_t n) { size_t i = 0; for (; i < n && s[i]; i++) d[i] = s[i]; for (; i < n; i++) d[i] = 0; return d; }
int atoi(const char *s) { int v = 0, g = 1; if (*s == '-') { g = -1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return v * g; }
long strtol(const char *s, char **e, int b) { (void)b; long v = atoi(s); if (e) *e = (char *)s; return v; }
unsigned long strtoul(const char *s, char **e, int b) { return (unsigned long)strtol(s, e, b); }
int snprintf(char *b, size_t n, const char *f, ...) { size_t i = 0; while (f[i] && i + 1 < n) { b[i] = f[i]; i++; } if (n) b[i] = 0; return (int)i; }
static char heap[1 << 16]; static size_t top;
void *malloc(size_t n) { void *p = heap + top; top += (n + 7) & ~(size_t)7; return top <= sizeof heap ? p : 0; }
void free(void *p) { (void)p; if (0) top = 0; }
void exit(int c) { (void)c; __builtin_trap(); }
double log10(double x) { return 0 * x; }
static long now_ns;
int clock_gettime(int c, struct timespec *t) { (void)c; now_ns += 1000; t->tv_sec = now_ns / 1000000000L; t->tv_nsec = now_ns % 1000000000L; return 0; }
double atof(const char *s) { return atoi(s); }
