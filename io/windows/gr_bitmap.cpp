#include "gr_bitmap.h"
#include "consts.h"

gr_bitmap::gr_bitmap(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index)
{

}

void gr_bitmap::render()
{
	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_bitmap::paint_uniform(PIXEL_UNIT color)
{
	gr_memset(buffer, color, this->size_x*this->size_y);
}

PIXEL_UNIT * gr_bitmap::get_buffer()
{
	return buffer;
}