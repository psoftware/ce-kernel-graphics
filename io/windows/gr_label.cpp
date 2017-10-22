#include "libce_guard.h"
#include "gr_label.h"
#include "consts.h"
#include "libgr.h"
#include "libfont.h"

gr_label::gr_label(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index), text_changed(true), text_color(LABEL_DEFAULT_TEXTCOLOR), back_color(LABEL_DEFAULT_BACKCOLOR)
{
	copy("", this->text);
}

gr_label::gr_label(u_label* u_l)
: gr_object(u_l->pos_x, u_l->pos_y, u_l->size_x, u_l->size_y, u_l->z_index, false), text_color(u_l->text_color), back_color(u_l->back_color)
{
	copy(u_l->text, this->text);
	text_changed = true;
}

gr_label& gr_label::operator=(const u_label& u_l)
{
	gr_object::operator=(u_l);

	this->text_color = u_l.text_color;
	this->back_color = u_l.back_color;
	copy(u_l.text, this->text);
	text_changed = true;

	return *this;
}

void gr_label::render()
{
	if(!buffered)
		return;

	// fai le modifiche sul buffer
	render_subset_unit objarea(0,0,size_x,size_y);
	draw(this->buffer, size_x, size_y, 0, 0, &objarea);

	// indico l'area modificata
	render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
	push_render_unit(newunit);
}

void gr_label::build_render_areas(render_subset_unit *parent_restriction, gr_object *target, int ancestors_offset_x, int ancestors_offset_y, bool ancestor_modified)
{
	if(text_changed)
	{
		render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
		push_render_unit(newunit);
		text_changed = false;
	}
}

void gr_label::draw(PIXEL_UNIT *ancestor_buffer, int ancestor_size_x, int ancestor_size_y, int total_offset_x, int total_offset_y, render_subset_unit *child_restriction)
{
	for(int y=0; y<child_restriction->size_y; y++)
		gr_memset(ancestor_buffer + child_restriction->pos_x + ancestor_size_x*(y+child_restriction->pos_y), this->back_color, child_restriction->size_x);

	//flog(LOG_INFO, "this (%d) drawing on x/y %d %d %d %d offset/limit %d %d %d %d",this->get_id(),total_offset_x, total_offset_y, this->size_x + total_offset_x, this->size_y + total_offset_y, child_restriction->pos_x - total_offset_x, child_restriction->pos_y - total_offset_y, child_restriction->size_x, child_restriction->size_y);

	set_fontstring(ancestor_buffer, ancestor_size_x, ancestor_size_y,
		total_offset_x, total_offset_y, this->size_x + total_offset_x, this->size_y + total_offset_y,
		child_restriction->pos_x, child_restriction->pos_y, child_restriction->size_x, child_restriction->size_y,
		this->text, this->text_color, this->back_color);
}

void gr_label::set_text(const char * text)
{
	copy(text, this->text);
	text_changed = true;
}

void gr_label::set_back_color(PIXEL_UNIT color){
	this->back_color = color;
}

void gr_label::set_text_color(PIXEL_UNIT color){
	this->text_color = color;
}