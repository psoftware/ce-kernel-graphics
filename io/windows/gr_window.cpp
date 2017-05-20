#include "libce_guard.h"
#include "gr_window.h"
#include "gr_bitmap.h"
#include "gr_button.h"
#include "gr_label.h"
#include "consts.h"
#include "libgr.h"

gr_window::gr_window(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x+BORDER_TICK*2, size_y+BORDER_TICK+TOPBAR_HEIGHT, z_index)
{
	// la finestra è composta da tre container: uno che contiene la topbar, uno che contiene gli oggetti della finestra, e uno che
	// contiene entrambi i contenitori (main_container), il quale è aggiunto al doubled_framebuffer

	this->set_search_flag(WINDOW_FLAG);
	this->topbar_container = new gr_object(0,0,this->get_size_x(),TOPBAR_HEIGHT,0);
	this->topbar_container->set_search_flag(TOPBAR_FLAG);
	this->topbar_bitmap = new gr_bitmap(0,0,this->topbar_container->get_size_x(),this->topbar_container->get_size_y(),0);
	this->topbar_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);
	this->topbar_bitmap->render();
	this->topbar_container->add_child(this->topbar_bitmap);
	this->add_child(this->topbar_container);

	// pulsante chiusura
	this->close_button = new gr_button(this->topbar_container->get_size_x()-CLOSEBUTTON_PADDING_X-CLOSEBUTTON_SIZE,CLOSEBUTTON_PADDING_Y,
		CLOSEBUTTON_SIZE,CLOSEBUTTON_SIZE,1,CLOSEBUTTON_WIN_BACKCOLOR);
	this->close_button->set_text("x");
	this->close_button->render();
	this->topbar_container->add_child(this->close_button);

	// titolo finestra
	this->title_label = new gr_label(TITLELABEL_PADDING_X,TITLELABEL_PADDING_Y,this->close_button->get_pos_x()-TITLELABEL_PADDING_X,
		TOPBAR_HEIGHT-TITLELABEL_PADDING_Y,1, TOPBAR_WIN_BACKCOLOR);
	this->title_label->set_text("Titolo Finestra");
	this->title_label->render();
	this->topbar_container->add_child(this->title_label);

	// contenitore oggetti finestra + background
	this->inner_container = new gr_object(BORDER_TICK,TOPBAR_HEIGHT,this->size_x-2*BORDER_TICK,this->size_y-TOPBAR_HEIGHT-BORDER_TICK,0);
	this->background_bitmap = new gr_bitmap(0,0,this->inner_container->get_size_x(),this->inner_container->get_size_y(),0);
	this->background_bitmap->paint_uniform(DEFAULT_WIN_BACKCOLOR);
	this->background_bitmap->render();
	this->inner_container->add_child(this->background_bitmap);

	// bordi (destro, sinistro e basso)
	this->border_left_bitmap = new gr_bitmap(0,TOPBAR_HEIGHT, BORDER_TICK, this->size_y, BORDER_ZINDEX);
	this->border_left_bitmap->set_search_flag(BORDER_FLAG);
	this->border_left_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);
	this->border_right_bitmap = new gr_bitmap(this->size_x-BORDER_TICK, TOPBAR_HEIGHT, BORDER_TICK, this->size_y-TOPBAR_HEIGHT, BORDER_ZINDEX);
	this->border_right_bitmap->set_search_flag(BORDER_FLAG);
	this->border_right_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);
	this->border_bottom_bitmap = new gr_bitmap(BORDER_TICK, this->size_y-BORDER_TICK, this->size_x-2*BORDER_TICK, BORDER_TICK, BORDER_ZINDEX);
	this->border_bottom_bitmap->set_search_flag(BORDER_FLAG);
	this->border_bottom_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);

	this->add_child(this->border_left_bitmap);
	this->add_child(this->border_right_bitmap);
	this->add_child(this->border_bottom_bitmap);
	this->border_left_bitmap->render();
	this->border_right_bitmap->render();
	this->border_bottom_bitmap->render();

	this->add_child(this->inner_container);
	this->set_visibility(false);

	this->topbar_container->render();
	this->inner_container->render();
}

void gr_window::resize()
{
	if(!is_pos_modified())
		return;

	if(this->size_x<=0 || this->size_y<=0)
		return;

	int delta_size_x = this->size_x - old_size_x;
	int delta_size_y = this->size_y - old_size_y;

	//flog(LOG_INFO, "delta_size_x %d, delta_size_y %d, this_size_x %d, old_size_x %d", delta_size_x, delta_size_y, this->size_x, old_size_x);

	this->inner_container->set_size_x(this->size_x);
	this->inner_container->set_size_y(this->inner_container->get_size_y() + delta_size_y);
	this->inner_container->realloc_buffer();

	this->background_bitmap->set_size_x(this->background_bitmap->get_size_x() + delta_size_x);
	this->background_bitmap->set_size_y(this->background_bitmap->get_size_y() + delta_size_y);
	this->background_bitmap->realloc_buffer();
	this->background_bitmap->paint_uniform(DEFAULT_WIN_BACKCOLOR);

	this->topbar_container->set_size_x(this->topbar_container->get_size_x() + delta_size_x);
	this->topbar_container->realloc_buffer();
	this->topbar_bitmap->set_size_x(this->topbar_container->get_size_x());
	this->topbar_bitmap->realloc_buffer();
	this->topbar_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);

	this->border_left_bitmap->set_size_y(this->border_left_bitmap->get_size_y() + delta_size_y);
	this->border_left_bitmap->realloc_buffer();
	this->border_left_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);

	this->border_right_bitmap->set_pos_x(this->border_right_bitmap->get_pos_x() + delta_size_x);
	this->border_right_bitmap->set_size_y(this->border_right_bitmap->get_size_y() + delta_size_y);
	this->border_right_bitmap->realloc_buffer();
	this->border_right_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);

	this->border_bottom_bitmap->set_pos_y(this->border_bottom_bitmap->get_pos_y() + delta_size_y);
	this->border_bottom_bitmap->set_size_x(this->border_bottom_bitmap->get_size_x() + delta_size_x);
	this->border_bottom_bitmap->realloc_buffer();
	this->border_bottom_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);

	this->close_button->set_pos_x(this->close_button->get_pos_x() + delta_size_x);
	this->close_button->render();

	this->realloc_buffer();

	this->border_left_bitmap->render();
	this->border_right_bitmap->render();
	this->border_bottom_bitmap->render();
	this->topbar_bitmap->render();
	this->topbar_container->render();
	this->background_bitmap->render();
	this->inner_container->render();
}

void gr_window::set_size_x(int newval){
	if(newval < BORDER_TICK*2 + CLOSEBUTTON_PADDING_X*2 + CLOSEBUTTON_SIZE)
		return;
	this->size_x=newval + BORDER_TICK*2;
}
void gr_window::set_size_y(int newval){
	if(newval < 0)
		return;
	this->size_y=newval + TOPBAR_HEIGHT + BORDER_TICK;
}
int gr_window::offset_size_x(int offset){
	if(this->size_x + offset < BORDER_TICK*2 + CLOSEBUTTON_PADDING_X*2 + CLOSEBUTTON_SIZE)
		return 0;
	this->size_x+=offset;
	return offset;
}
int gr_window::offset_size_y(int offset){
	if((this->size_y + offset < TOPBAR_HEIGHT + BORDER_TICK))
		return 0;
	this->size_y+=offset;
	return offset;
}

void gr_window::set_title(const char *str)
{
	this->title_label->set_text(str);
}