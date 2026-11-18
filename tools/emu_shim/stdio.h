#pragma once
#include <stddef.h>
#include <stdarg.h>
typedef struct { int x; } FILE;
#define stderr ((FILE *)0)
#define stdout ((FILE *)0)
#define stdin ((FILE *)0)
#define EOF (-1)
static inline FILE *fopen(const char *a, const char *b) { (void)a; (void)b; return 0; }
static inline int fclose(FILE *f) { (void)f; return 0; }
static inline size_t fwrite(const void *p, size_t a, size_t b, FILE *f) { (void)p; (void)a; (void)f; return b; }
static inline size_t fread(void *p, size_t a, size_t b, FILE *f) { (void)p; (void)a; (void)b; (void)f; return 0; }
static inline int fflush(FILE *f) { (void)f; return 0; }
static inline int fseek(FILE *f, long a, int b) { (void)f; (void)a; (void)b; return 0; }
#define SEEK_SET 0
#define printf(...) ((void)0)
#define fprintf(...) ((void)0)
#define puts(...) ((void)0)
#define perror(...) ((void)0)
#define putchar(...) ((void)0)
int snprintf(char *b, size_t n, const char *f, ...);
#define sscanf(...) (0)
static inline char *fgets(char *s, int n, FILE *f) { (void)s; (void)n; (void)f; return 0; }
