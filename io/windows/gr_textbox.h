#ifndef GRTEXTBOX_H
#define GRTEXTBOX_H

#include "consts.h"
#include "gr_object.h"
#include "windows/u_obj.h"

class gr_textbox : public gr_object
{
	char text[TEXTBOX_MAX_TEXT+1];
	bool caret_print;
	PIXEL_UNIT text_color;
	PIXEL_UNIT back_color;
	PIXEL_UNIT border_color;

public:
	gr_textbox(int pos_x, int pos_y, int size_x, int size_y, int z_index);
	gr_textbox(u_textbox* u_l);
	gr_textbox& operator=(const u_textbox& u_t);

	void render();
	bool get_caret_print();
	void set_caret_print(bool print);
	void set_back_color(PIXEL_UNIT color);
	void set_text_color(PIXEL_UNIT color);
	void set_border_color(PIXEL_UNIT color);
};

#endif