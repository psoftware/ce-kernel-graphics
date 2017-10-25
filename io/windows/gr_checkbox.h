#ifndef GRCHECKBOX_H
#define GRCHECKBOX_H

#include "consts.h"
#include "gr_object.h"
#include "windows/u_obj.h"

class gr_checkbox : public gr_object
{
	bool selected;

public:
	gr_checkbox(int pos_x, int pos_y, int z_index);
	gr_checkbox(u_checkbox* u_b);
	gr_checkbox& operator=(u_checkbox& u_c);

	void render();
	void set_selected(bool selected);
};

#endif