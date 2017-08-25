#ifndef GRUNIFORMBITMAP_H
#define GRUNIFORMBITMAP_H

#include "libce_guard.h"
#include "consts.h"
#include "libgr.h"
#include "gr_object.h"

class gr_uniform_bitmap : public gr_object
{
	PIXEL_UNIT color;
public:
	gr_uniform_bitmap(int pos_x, int pos_y, int size_x, int size_y, int z_index);

	void set_color(PIXEL_UNIT color);
	void draw(PIXEL_UNIT *ancestor_buffer, int ancestor_size_x, int ancestor_size_y, render_subset_unit *child_restriction, int start_pos_x, int start_pos_y);
	void render();
};

#endif