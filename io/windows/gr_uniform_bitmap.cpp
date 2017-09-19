#include "gr_uniform_bitmap.h"
#include "consts.h"

gr_uniform_bitmap::gr_uniform_bitmap(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index, false), old_color(0), color(1)
{

}
void gr_uniform_bitmap::set_color(PIXEL_UNIT color) {
	this->color = color;
}

void gr_uniform_bitmap::build_render_areas(render_subset_unit *parent_restriction, gr_object *target, int ancestors_offset_x, int ancestors_offset_y, bool ancestor_modified)
{
	if(this->color != this->old_color)
	{
		render_subset_unit *newunit = new render_subset_unit(0, 0, size_x, size_y);
		push_render_unit(newunit);
		this->old_color = this->color;
	}
}

void gr_uniform_bitmap::render()
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

void gr_uniform_bitmap::draw(PIXEL_UNIT *ancestor_buffer, int ancestor_size_x, int ancestor_size_y, int total_offset_x, int total_offset_y, render_subset_unit *child_restriction)
{
	for(int y=0; y<child_restriction->size_y; y++)
		gr_memset(ancestor_buffer + child_restriction->pos_x + ancestor_size_x*(y+child_restriction->pos_y), this->color, child_restriction->size_x);
}