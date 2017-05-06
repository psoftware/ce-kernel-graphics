#ifndef FAT16_H
#define FAT16_H

#include "structs.h"
#define MAX_IOPOINTERS_COUNT 32

int open_file(const char * filepath);
int read_file(natb fd, natb *dest, natl bytescount);
int close_file(natb fd);
void fat16_init();

#endif