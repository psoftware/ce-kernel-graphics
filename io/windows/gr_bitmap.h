#ifndef GRBITMAP_H
#define GRBITMAP_H

#include "libce_guard.h"
#include "consts.h"
#include "libgr.h"
#include "gr_object.h"

class gr_bitmap : public gr_object
{
public:
	gr_bitmap(int pos_x, int pos_y, int size_x, int size_y, int z_index);
	
	PIXEL_UNIT * get_buffer();
	void render();
};

#endif