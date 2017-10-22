#include "libce_guard.h"
#include "gr_button.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_button::gr_button(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), back_color(BUTTON_DEFAULT_BACKCOLOR), border_color(BUTTON_DEFAULT_BORDERCOLOR),
	clicked_color(BUTTON_DEFAULT_CLICKEDCOLOR), text_color(BUTTON_DEFAULT_TEXTCOLOR), clicked(false)
{
	copy("", this->text);
}

gr_button::gr_button(u_button* u_b)
: gr_object(u_b->pos_x, u_b->pos_y, u_b->size_x, u_b->size_y, u_b->z_index), back_color(u_b->back_color), border_color(u_b->border_color),
clicked_color(u_b->clicked_color), text_color(u_b->text_color), clicked(false)
{
	copy(u_b->text, this->text);
}

gr_button& gr_button::operator=(const u_button& u_b)
{
	gr_object::operator=(u_b);

	this->back_color = u_b.back_color;
	this->border_color = u_b.border_color;
	this->clicked_color = u_b.clicked_color;
	this->text_color = u_b.text_color;
	this->clicked = u_b.clicked;
	copy(u_b.text, this->text);

	return *this;
}

void gr_button::render()
{
	PIXEL_UNIT color = (clicked) ? clicked_color : back_color;
	gr_memset(this->buffer, color, size_x*size_y);
	int text_width = get_fontstring_width(this->text);
	set_fontstring(this->buffer, this->size_x, this->size_y,
		(this->size_x - text_width)/2, (this->size_y - 16)/2, text_width, this->size_y,
		0, 0, this->size_x, this->size_y,
		this->text, this->text_color, color);

	gr_memset(this->buffer, border_color, size_x);
	for(natw y=1; y < this->size_y-1; y++)
	{
		set_pixel(this->buffer, 0, y, this->size_x, this->size_y, this->border_color);
		set_pixel(this->buffer, size_x-1, y, this->size_x, this->size_y, this->border_color);
	}
	gr_memset(this->buffer+size_x*(size_y-1), border_color, size_x);

	//indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_button::set_text(const char * text)
{
	copy(text, this->text);
}

void gr_button::set_back_color(PIXEL_UNIT color) {
	this->back_color=color;
}
void gr_button::set_border_color(PIXEL_UNIT color) {
	this->border_color=color;
}
void gr_button::set_clicked_color(PIXEL_UNIT color) {
	this->clicked_color=color;
}
void gr_button::set_text_color(PIXEL_UNIT color) {
	this->text_color=color;
}
void gr_button::set_clicked(bool clicked) {
	this->clicked=clicked;
}