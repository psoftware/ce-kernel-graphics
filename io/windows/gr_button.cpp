#include "libce_guard.h"
#include "gr_button.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_button::gr_button(int pos_x, int pos_y, int size_x, int size_y, int z_index, PIXEL_UNIT color)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), background_color(color), border_color(color+1), clicked_color(color), text_color(color+1), clicked(false)
{
	copy("", this->text);
}

void gr_button::render()
{
	PIXEL_UNIT color = (clicked) ? clicked_color : background_color;
	gr_memset(this->buffer, color, size_x*size_y);
	int text_width = get_fontstring_width(this->text);
	set_fontstring(this->buffer, this->size_x, this->size_y, (this->size_x - text_width)/2, (this->size_y - 16)/2, text_width, this->size_y, this->text, this->background_color);

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
