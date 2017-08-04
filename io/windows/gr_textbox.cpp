#include "libce_guard.h"
#include "gr_textbox.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_textbox::gr_textbox(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), text_color(TEXTBOX_DEFAULT_TEXTCOLOR), back_color(TEXTBOX_DEFAULT_BACKCOLOR),
	border_color(TEXTBOX_DEFAULT_BORDERCOLOR)
{
	copy("", this->text);
}

gr_textbox::gr_textbox(u_textbox* u_t)
: gr_object(u_t->pos_x, u_t->pos_y, u_t->size_x, u_t->size_y, u_t->z_index), text_color(u_t->text_color), back_color(u_t->back_color)
{
	copy(u_t->text, this->text);
}

gr_textbox& gr_textbox::operator=(const u_textbox& u_t)
{
	this->set_pos_x(u_t.pos_x);
	this->set_pos_y(u_t.pos_y);
	this->set_size_x(u_t.size_x);
	this->set_size_y(u_t.size_y);
	this->z_index = u_t.z_index;
	this->text_color = u_t.text_color;
	this->back_color = u_t.back_color;
	this->border_color = u_t.border_color;
	copy(u_t.text, this->text);

	return *this;
}

void gr_textbox::render()
{
	// stampo sfondo
	gr_memset(this->buffer, this->back_color, this->size_x*this->size_y);
	// renderizzo il testo
	set_fontstring(this->buffer, this->size_x, this->size_y, 2, 2, this->size_x-2, this->size_y-2, this->text, this->text_color, this->back_color, this->caret_print);

	// stampo i bordi
	gr_memset(this->buffer, this->border_color, this->size_x);
	for(natw y=1; y < this->size_y-1; y++)
	{
		set_pixel(this->buffer, 0, y, this->size_x, this->size_y, this->border_color);
		set_pixel(this->buffer, size_x-1, y, this->size_x, this->size_y, this->border_color);
	}
	gr_memset(this->buffer + this->size_x*(this->size_y-1), this->border_color, this->size_x);

	// indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

bool gr_textbox::get_caret_print() {
	return this->caret_print;
}

void gr_textbox::set_caret_print(bool print) {
	this->caret_print = print;
}

void gr_textbox::set_back_color(PIXEL_UNIT color){
	this->back_color = color;
}

void gr_textbox::set_border_color(PIXEL_UNIT color){
	this->text_color = color;
}

void gr_textbox::set_text_color(PIXEL_UNIT color){
	this->text_color = color;
}