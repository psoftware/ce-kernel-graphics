#ifndef GRBUTTON_H
#define GRBUTTON_H

#include "consts.h"
#include "gr_object.h"

class gr_button : public gr_object
{
	char * text[BUTTON_MAX_TEXT+1];
	PIXEL_UNIT color;

public:
	gr_button(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT color);

	void render();
	void set_text(char * text);
};

#endif