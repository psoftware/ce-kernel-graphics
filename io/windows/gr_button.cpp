#include "gr_button.h"
#include "consts.h"

void inline put_pixel(natb * buffer, int x, int y, int MAX_X, int MAX_Y, natb col)
{
	if(x<MAX_X && y<MAX_Y && x>=0 && y>=0)
		buffer[MAX_X*y+x] = col;
}

gr_button::gr_button(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT color)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), color(color)
{

}

void gr_button::render()
{
	//disegna bottone su buffer
	for(int i=0; i<this->size_x; i++)
			for(int j=0; j<this->size_y; j++)
				put_pixel(this->buffer, i, j, this->size_x, this->size_y, this->color);
	//cout << "Renderizzo bottone con z-index " << z_index << endl;
}

void gr_button::set_text(char * text)
{

}
