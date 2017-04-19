#ifndef LIBGR_H
#define LIBGR_H

void inline set_pixel(natb * buffer, int x, int y, int MAX_X, int MAX_Y, natb col)
{
	if(x<MAX_X && y<MAX_Y && x>=0 && y>=0)
		buffer[MAX_X*y+x] = col;
	else
		flog(LOG_INFO, "libgr: set_pixel() -> buffer overflow detected! at x=%d y=%d", x, y);
}

#endif