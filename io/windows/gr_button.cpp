#include "libce.h"
#include "gr_button.h"
#include "consts.h"
#include "libfont.h"

void inline put_pixel(natb * buffer, int x, int y, int MAX_X, int MAX_Y, natb col)
{
	if(x<MAX_X && y<MAX_Y && x>=0 && y>=0)
		buffer[MAX_X*y+x] = col;
}

gr_button::gr_button(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT color)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), background_color(color), border_color(color+1), clicked_color(color), text_color(color+1), clicked(false)
{
	copy("", this->text);
}

void gr_button::render()
{
	PIXEL_UNIT color = (clicked) ? clicked_color : background_color;
	memset(this->buffer, color, size_x*size_y);
	int text_width = get_fontstring_width(this->text);
	set_fontstring(this->buffer, this->size_x, this->size_y, (this->size_x - text_width)/2, (this->size_y - 16)/2, text_width, this->size_y, this->text, this->background_color);
	flog(LOG_INFO, "### text_width=%d x=%d y=%d w=%d h=%d", text_width, (this->size_x - text_width)/2, (this->size_y - 16)/2, text_width, this->size_y);

	memset(this->buffer, border_color, size_x);
	for(natw y=1; y < this->size_y-1; y++)
	{
		put_pixel(this->buffer, 0, y, this->size_x, this->size_y, this->border_color);
		put_pixel(this->buffer, size_x-1, y, this->size_x, this->size_y, this->border_color);
	}
	memset(this->buffer+size_x*(size_y-1), border_color, size_x);
}

void gr_button::set_text(char * text)
{
	copy(text, this->text);
}
