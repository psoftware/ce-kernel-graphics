#include "libce_guard.h"
#include "gr_label.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_label::gr_label(int pos_x, int pos_y, int size_x, int size_y, int z_index, PIXEL_UNIT back_color)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), back_color(back_color)
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
	this->set_pos_x(u_l.pos_x);
	this->set_pos_y(u_l.pos_y);
	this->set_size_x(u_l.size_x);
	this->set_size_y(u_l.size_y);
	this->z_index = u_l.z_index;
	this->text_color = u_l.text_color;
	this->back_color = u_l.back_color;
	copy(u_l.text, this->text);

	return *this;
}

void gr_label::render()
{
	gr_memset(this->buffer, this->back_color, this->size_x*this->size_y);
	set_fontstring(this->buffer, this->size_x, this->size_y, 0, 0, this->size_x, this->size_y, this->text, this->back_color);

	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_label::set_text(const char * text)
{
	copy(text, this->text);
}