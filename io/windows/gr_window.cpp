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
		CLOSEBUTTON_SIZE,CLOSEBUTTON_SIZE,CLOSEBUTTON_ZINDEX);
	this->close_button->set_back_color(CLOSEBUTTON_WIN_BACKCOLOR);
	this->close_button->set_border_color(CLOSEBUTTON_WIN_BORDERCOLOR);
	this->close_button->set_text_color(CLOSEBUTTON_WIN_TEXTCOLOR);
	this->close_button->set_text("x");
	this->close_button->render();
	this->topbar_container->add_child(this->close_button);

	// titolo finestra
	this->title_label = new gr_label(TITLELABEL_PADDING_X,TITLELABEL_PADDING_Y,this->close_button->get_pos_x()-TITLELABEL_PADDING_X,
		TOPBAR_HEIGHT-TITLELABEL_PADDING_Y,TITLELABEL_ZINDEX);
	this->title_label->set_back_color(TOPBAR_WIN_BACKCOLOR);
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

gr_object *gr_window::add_user_object(u_windowObject * u_obj)
{
	gr_object* newobj;
	switch(u_obj->TYPE)
	{
		case W_ID_LABEL:
			newobj = new gr_label(static_cast<u_label*>(u_obj));
			newobj->set_search_flag(LABEL_FLAG);
			break;
		case W_ID_BUTTON:
			newobj = new gr_button(static_cast<u_button*>(u_obj));
			newobj->set_search_flag(BUTTON_FLAG);
			break;
		/*case W_ID_TEXTBOX:
			wind->objects[wind->obj_count] = new textbox(static_cast<u_textbox*>(u_obj));
		break;*/
		default:
			flog(LOG_INFO, "c_crea_oggetto: tipo oggetto %d errato", u_obj->TYPE);
			return 0;
	}

	flog(LOG_INFO, "add_user_object: oggetto %d di tipo %d creato su finestra %d", newobj->get_id(), u_obj->TYPE, this->get_id());
	u_obj->id = newobj->get_id();
	newobj->render();

	this->inner_container->add_child(newobj);
	inner_container->render();
	return newobj;
}

bool gr_window::update_user_object(u_windowObject * u_obj)
{
	gr_object * dest_obj = this->inner_container->search_child_by_id(u_obj->id);
	if(dest_obj==0)
		return false;

	switch(u_obj->TYPE)
	{
		case W_ID_LABEL:
			if(!dest_obj->has_flag(gr_window::LABEL_FLAG))
				return false;
			*(static_cast<gr_label*>(dest_obj)) = *static_cast<u_label*>(u_obj);
			break;
		case W_ID_BUTTON:
			if(!dest_obj->has_flag(gr_window::BUTTON_FLAG))
				return false;
			*(static_cast<gr_button*>(dest_obj)) = *static_cast<u_button*>(u_obj);
			break;
		/*case W_ID_TEXTBOX:
			wind->objects[wind->obj_count] = new textbox(static_cast<u_textbox*>(u_obj));
		break;*/
		default:
			flog(LOG_INFO, "update_user_object: tipo oggetto %d errato", u_obj->TYPE);
			return true;
	}

	flog(LOG_INFO, "update_user_object: oggetto %d di tipo %d modificato su finestra %d", dest_obj->get_id(), u_obj->TYPE, this->get_id());
	dest_obj->render();
	inner_container->render();
	return false;
}

gr_object *gr_window::search_user_object(u_windowObject * u_obj)
{
	if(u_obj==0)
		return 0;
flog(LOG_INFO, "searching u_obj->id %d", u_obj->id);
	return this->inner_container->search_child_by_id(u_obj->id);
}