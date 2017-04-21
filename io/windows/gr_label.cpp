#include "libce.h"
#include "gr_label.h"
#include "consts.h"
#include "libfont.h"

gr_label::gr_label(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT back_color)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), back_color(back_color)
{
	copy("", this->text);
}

void gr_label::render()
{
	memset(this->buffer, this->back_color, this->size_x*this->size_y);
	set_fontstring(this->buffer, this->size_x, this->size_y, 0, 0, this->size_x, this->size_y, this->text, this->back_color);

	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_label::set_text(char * text)
{
	copy(text, this->text);
}