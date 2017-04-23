#ifndef GRBUTTON_H
#define GRBUTTON_H

#include "consts.h"
#include "gr_object.h"

class gr_button : public gr_object
{
	char text[BUTTON_MAX_TEXT+1];
	PIXEL_UNIT background_color;
	PIXEL_UNIT border_color;
	PIXEL_UNIT clicked_color;
	PIXEL_UNIT text_color;

	bool clicked;

public:
	gr_button(int pos_x, int pos_y, int size_x, int size_y, int z_index, PIXEL_UNIT color);

	void render();
	void set_text(char * text);
};

#endif