#include "gr_bitmap.h"
#include "consts.h"

gr_bitmap::gr_bitmap(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index)
{

}

void gr_bitmap::render()
{
	//disegna bottone su buffer
	//cout << "Renderizzo bitmap " << z_index << endl;
}

PIXEL_UNIT * gr_bitmap::get_buffer()
{
	return buffer;
}