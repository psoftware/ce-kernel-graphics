#ifndef FAT16_H
#define FAT16_H

#include "structs.h"
#define MAX_IOPOINTERS_COUNT 32

int open_file(const char * filepath, natb& errno);
int read_file(natb fd, natb *dest, natl bytescount, natb& errno);
int close_file(natb fd, natb& errno);
bool fat16_init();

#endif