#ifndef _LIB_H_
#define _LIB_H_
#include <sys.h>
#include <shlib.h>

char* copy(const char* src, char* dst);
char* convint(int n, char* out);
void *mem_alloc(natl quanti);
void mem_free(void *ptr);

int printf(const char *fmt, ...);
void pause();

#endif
