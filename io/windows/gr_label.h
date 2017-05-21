#ifndef GRLABEL_H
#define GRLABEL_H

#include "consts.h"
#include "gr_object.h"
#include "windows/u_obj.h"

class gr_label : public gr_object
{
	char text[LABEL_MAX_TEXT+1];
	PIXEL_UNIT text_color;
	PIXEL_UNIT back_color;

public:
	gr_label(int pos_x, int pos_y, int size_x, int size_y, int z_index);
	gr_label(u_label* u_l);
	gr_label& operator=(const u_label& u_l);

	void render();
	void set_text(const char *text);
	void set_back_color(PIXEL_UNIT color);
	void set_text_color(PIXEL_UNIT color);
};

#endif