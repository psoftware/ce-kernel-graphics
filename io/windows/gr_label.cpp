#include "gr_label.h"
#include "consts.h"

gr_label::gr_label(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index)
{

}

void gr_label::render()
{
	//disegna bottone su buffer
	//cout << "Renderizzo label con z-index " << this->z_index << endl;
}

void gr_label::set_text(char * text)
{
	
}
