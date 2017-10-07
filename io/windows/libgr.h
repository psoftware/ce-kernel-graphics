#ifndef LIBGR_H
#define LIBGR_H

#include "consts.h"

void inline set_pixel(PIXEL_UNIT * buffer, int x, int y, int MAX_X, int MAX_Y, PIXEL_UNIT col)
{
	if(x<MAX_X && y<MAX_Y && x>=0 && y>=0)
		buffer[MAX_X*y+x] = col;
	else
		flog(LOG_INFO, "libgr: set_pixel() -> buffer overflow detected! at x=%d y=%d", x, y);
}

// solo per 32BPP !!
void inline set_pixel_alpha_blend(PIXEL_UNIT * buffer, int x, int y, int MAX_X, int MAX_Y, PIXEL_UNIT src_pixel)
{
	if(x>=MAX_X || y>=MAX_Y || x<0 || y<0)
	{
		flog(LOG_INFO, "libgr: set_pixel_alpha_blend() -> buffer overflow detected! at x=%d y=%d", x, y);
		return;
	}

	PIXEL_UNIT dst_pixel = buffer[MAX_X*y+x];
	natb alpha = src_pixel >> 24;
	natb src_red = (src_pixel >> 16) & 0xff;
	natb src_green = (src_pixel >> 8) & 0xff;
	natb src_blue = src_pixel & 0xff;
	natb dst_red = (dst_pixel >> 16) & 0xff;
	natb dst_green = (dst_pixel >> 8) & 0xff;
	natb dst_blue = dst_pixel & 0xff;
	dst_red = (alpha*src_red + (255-alpha)*dst_red) / 255;
	dst_green = (alpha*src_green + (255-alpha)*dst_green) / 255;
	dst_blue = (alpha*src_blue + (255-alpha)*dst_blue) / 255;
	buffer[MAX_X*y+x] = (dst_pixel & 0xff000000) | (dst_red << 16) | (dst_green << 8) | dst_blue;
}

void *gr_memcpy(void *__restrict dest, const void *__restrict src, unsigned long int n);
void *gr_memset(void *__restrict dest, const PIXEL_UNIT value, unsigned long int n);

#endif