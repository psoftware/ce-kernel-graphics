#ifndef GRLABEL_H
#define GRLABEL_H

#include "consts.h"
#include "gr_object.h"
#include "windows/u_obj.h"

class gr_label : public gr_object
{
	char text[LABEL_MAX_TEXT+1];
	bool text_changed;
	PIXEL_UNIT text_color;
	PIXEL_UNIT back_color;

public:
	gr_label(int pos_x, int pos_y, int size_x, int size_y, int z_index);
	gr_label(u_label* u_l);
	gr_label& operator=(const u_label& u_l);

	void render();
	void build_render_areas(render_subset_unit *parent_restriction, gr_object *target, int ancestors_offset_x, int ancestors_offset_y, bool ancestor_modified);
	void draw(PIXEL_UNIT *ancestor_buffer, int ancestor_size_x, int ancestor_size_y, int total_offset_x, int total_offset_y, render_subset_unit *child_restriction);
	void set_text(const char *text);
	void set_back_color(PIXEL_UNIT color);
	void set_text_color(PIXEL_UNIT color);
};

#endif