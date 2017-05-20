#ifndef GRWINDOW_H
#define GRWINDOW_H

#include "consts.h"
#include "gr_object.h"
#include "gr_bitmap.h"
#include "gr_button.h"
#include "gr_label.h"

// gr_window è una specializzazione della classe gr_object ma, a differenza delle altre classi derivate, non 
// ridefinisce la funzione render(), quindi resta ancora un container ma aggiunge nuovi metodi
// e membri alla classe da cui deriva
class gr_window : public gr_object
{
	// flag usati per il membro search_flag dei gr_object (max 8)
public:
	static const char WINDOW_FLAG = 1u << 0;
	static const char BORDER_FLAG = 1u << 1;
	static const char LABEL_FLAG = 1u << 2;
	static const char BUTTON_FLAG = 1u << 3;
	static const char TOPBAR_FLAG = 1u << 4;

	// componenti della finestra
public:
	gr_object * topbar_container;
	gr_bitmap * topbar_bitmap;
	gr_object * inner_container;
	gr_bitmap * background_bitmap;

	gr_bitmap * border_left_bitmap;
	gr_bitmap * border_right_bitmap;
	gr_bitmap * border_bottom_bitmap;

	gr_button * close_button;
	gr_label * title_label;

	gr_window(int pos_x, int pos_y, int size_x, int size_y, int z_index);
	void set_title(const char *str);
};

#endif