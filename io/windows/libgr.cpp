#include "libce.h"
#include "libgr.h"
#include "consts.h"

void *gr_memcpy(void *__restrict dest, const void *__restrict src, unsigned long int n)
{
	PIXEL_UNIT *s1 = static_cast<PIXEL_UNIT*>(dest);
	const PIXEL_UNIT *s2 = static_cast<const PIXEL_UNIT*>(src);
	for(; 0<n; --n)*s1++ = *s2++;
	return dest;
}

void *gr_memset(void *__restrict dest, const PIXEL_UNIT value, unsigned long int n)
{
	PIXEL_UNIT *s1 = static_cast<PIXEL_UNIT*>(dest);
	for(; 0<n; --n)*s1++ = value;
	return dest;
}