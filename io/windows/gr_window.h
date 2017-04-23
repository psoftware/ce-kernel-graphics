#ifndef GRWINDOW_H
#define GRWINDOW_H

#include "consts.h"
#include "gr_object.h"
#include "gr_bitmap.h"
#include "gr_button.h"
#include "gr_label.h"

class gr_window : public gr_object
{
	//componenti della finestra
public:
	gr_object * main_container;
	gr_object * topbar_container;
	gr_bitmap * topbar_bitmap;
	gr_object * inner_container;
	gr_bitmap * background_bitmap;

	gr_button * close_button;
	gr_label * title_label;

	gr_window(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index);
	void set_title(char *str);
};

#endif