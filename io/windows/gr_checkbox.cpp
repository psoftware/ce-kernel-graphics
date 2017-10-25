#include "libce_guard.h"
#include "gr_checkbox.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_checkbox::gr_checkbox(int pos_x, int pos_y, int z_index)
: gr_object(pos_x, pos_y, CHECKBOX_SIZEX, CHECKBOX_SIZEY, z_index), selected(false)
{

}

gr_checkbox::gr_checkbox(u_checkbox* u_c)
: gr_object(u_c->pos_x, u_c->pos_y, CHECKBOX_SIZEX, CHECKBOX_SIZEY, u_c->z_index), selected(u_c->selected)
{

}

gr_checkbox& gr_checkbox::operator=(u_checkbox& u_c)
{
	// non devo permettere all'utente di cambiare la size di quest'oggetto
	u_c.size_x = CHECKBOX_SIZEX;
	u_c.size_y = CHECKBOX_SIZEY;

	gr_object::operator=(u_c);

	this->selected = u_c.selected;

	return *this;
}

void gr_checkbox::render()
{
	//stampo lo sfondo
	gr_memset(this->buffer, CHECKBOX_DEFAULT_BACKCOLOR, size_x*size_y);

	//stampo i bordi
	gr_memset(this->buffer, CHECKBOX_DEFAULT_BORDERCOLOR, size_x);
	for(natw y=1; y < this->size_y-1; y++)
	{
		set_pixel(this->buffer, 0, y, this->size_x, this->size_y, CHECKBOX_DEFAULT_BORDERCOLOR);
		set_pixel(this->buffer, size_x-1, y, this->size_x, this->size_y, CHECKBOX_DEFAULT_BORDERCOLOR);
	}
	gr_memset(this->buffer+size_x*(size_y-1), CHECKBOX_DEFAULT_BORDERCOLOR, size_x);

	//stampo il quadrato colorato centrale se l'oggetto è stato selezionato
	if(this->selected)
		for(int y=CHECKBOX_TICKPADDING; y<this->size_y - CHECKBOX_TICKPADDING; y++)
			gr_memset(buffer + y*this->size_x + CHECKBOX_TICKPADDING, CHECKBOX_DEFAULT_TICKCOLOR, this->size_x - CHECKBOX_TICKPADDING*2);

	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_checkbox::set_selected(bool selected) {
	this->selected=selected;
}