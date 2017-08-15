#ifndef GRPROGRESSBAR_H
#define GRPROGRESSBAR_H

#include "consts.h"
#include "gr_object.h"
#include "windows/u_obj.h"

class gr_progressbar : public gr_object
{
	int progress_perc;

public:
	gr_progressbar(int pos_x, int pos_y, int size_x, int size_y, int z_index);
	gr_progressbar(u_progressbar* u_b);
	gr_progressbar& operator=(const u_progressbar& u_pb);

	void render();
	void set_progress(int progress_perc);
};

#endif