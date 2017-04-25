//funzioni per il rendering del testo

#ifndef FONTLIB_H
#define FONTLIB_H

#include "libgr.h"
#include "consts.h"

int set_fontchar(PIXEL_UNIT *buffer, int x, int y, int MAX_X, int MAX_Y, int nchar, PIXEL_UNIT backColor);
int get_fontstring_width(const char * str);
int get_nextnonspecialchar_index(const char * str, int now_index, int len);
int get_charfont_width(char c);
void set_fontstring(PIXEL_UNIT *buffer, int MAX_X, int MAX_Y, int x, int y, int bound_x, int bound_y, const char * str, PIXEL_UNIT backColor);

#endif