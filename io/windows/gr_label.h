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
	gr_label(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT back_color);

	void render();
	void set_text(char *text);
};

#endif