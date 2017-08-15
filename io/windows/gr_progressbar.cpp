#include "libce_guard.h"
#include "gr_progressbar.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_progressbar::gr_progressbar(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), progress_perc(0)
{

}

gr_progressbar::gr_progressbar(u_progressbar* u_pb)
: gr_object(u_pb->pos_x, u_pb->pos_y, u_pb->size_x, u_pb->size_y, u_pb->z_index), progress_perc(u_pb->progress_perc)
{

}

gr_progressbar& gr_progressbar::operator=(const u_progressbar& u_pb)
{
	gr_object::operator=(u_pb);

	this->progress_perc = u_pb.progress_perc;

	return *this;
}

void gr_progressbar::render()
{
	//bordi e sfondo
	gr_memset(this->buffer, PROGRESSBAR_DEFAULT_BACKCOLOR, size_x*size_y);
	gr_memset(this->buffer, PROGRESSBAR_DEFAULT_BORDERCOLOR, size_x);
	for(natw y=1; y < this->size_y-1; y++)
	{
		set_pixel(this->buffer, 0, y, this->size_x, this->size_y, PROGRESSBAR_DEFAULT_BORDERCOLOR);
		set_pixel(this->buffer, size_x-1, y, this->size_x, this->size_y, PROGRESSBAR_DEFAULT_BORDERCOLOR);
	}
	gr_memset(this->buffer+size_x*(size_y-1), PROGRESSBAR_DEFAULT_BORDERCOLOR, size_x);

	//area riempita
	for(int y=1; y < this->size_y-1; y++)
		gr_memset(this->buffer + y*size_x + 1, PROGRESSBAR_DEFAULT_COLOR, (this->size_x*progress_perc)/100);

	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	this->push_render_unit(newunit);
}

void gr_progressbar::set_progress(int progress_perc) {
	if(progress_perc<0 || progress_perc>100)
		return;

	this->progress_perc=progress_perc;
}