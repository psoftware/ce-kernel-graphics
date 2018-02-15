#ifndef GRGAUSSBLUR_H
#define GRGAUSSBLUR_H

#include "libce_guard.h"
#include "consts.h"
#include "libgr.h"
#include "gr_object.h"

class gr_gaussblur : public gr_object
{
public:
	gr_gaussblur(int pos_x, int pos_y, int size_x, int size_y, int z_index);

	void build_render_areas(render_subset_unit *parent_restriction, gr_object *target, int, int, bool);
	void draw(PIXEL_UNIT *ancestor_buffer, int ancestor_size_x, int ancestor_size_y, int total_offset_x, int total_offset_y, render_subset_unit *child_restriction);
	void render();
};

#endif