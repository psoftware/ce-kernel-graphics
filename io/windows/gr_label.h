#ifndef GRLABEL_H
#define GRLABEL_H

#include "consts.h"
#include "gr_object.h"

class gr_label : public gr_object
{
	char text[LABEL_MAX_TEXT+1];
	PIXEL_UNIT text_color;
	PIXEL_UNIT back_color;

public:
	gr_label(int pos_x, int pos_y, int size_x, int size_y, int z_index, PIXEL_UNIT back_color);

	void render();
	void set_text(char *text);
};

#endif