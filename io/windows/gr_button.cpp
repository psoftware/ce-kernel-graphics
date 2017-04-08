#include "gr_button.h"
#include "consts.h"

gr_button::gr_button(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index)
{

}

void gr_button::render()
{
	//disegna bottone su buffer
	//cout << "Renderizzo bottone con z-index " << z_index << endl;
}

void gr_button::set_text(char * text)
{

}
