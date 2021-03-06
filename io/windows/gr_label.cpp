#include "libce_guard.h"
#include "gr_label.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_label::gr_label(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), text_color(LABEL_DEFAULT_TEXTCOLOR), back_color(LABEL_DEFAULT_BACKCOLOR)
{
	copy("", this->text);
}

gr_label::gr_label(u_label* u_l)
: gr_object(u_l->pos_x, u_l->pos_y, u_l->size_x, u_l->size_y, u_l->z_index), text_color(u_l->text_color), back_color(u_l->back_color)
{
	copy(u_l->text, this->text);
}

gr_label& gr_label::operator=(const u_label& u_l)
{
	gr_object::operator=(u_l);

	this->text_color = u_l.text_color;
	this->back_color = u_l.back_color;
	copy(u_l.text, this->text);

	return *this;
}

void gr_label::render()
{
	gr_memset(this->buffer, this->back_color, this->size_x*this->size_y);
	set_fontstring(this->buffer, this->size_x, this->size_y, 0, 0, this->size_x, this->size_y, this->text, this->text_color, this->back_color);

	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_label::set_text(const char * text)
{
	copy(text, this->text);
}

void gr_label::set_back_color(PIXEL_UNIT color){
	this->back_color = color;
}

void gr_label::set_text_color(PIXEL_UNIT color){
	this->text_color = color;
}