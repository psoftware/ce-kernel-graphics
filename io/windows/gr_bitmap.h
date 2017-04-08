#ifndef GRBITMAP_H
#define GRBITMAP_H

#include "consts.h"
#include "gr_object.h"

class gr_bitmap : public gr_object
{
public:
	gr_bitmap(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index);
	
	PIXEL_UNIT * get_buffer();
	void render();
};

#endif