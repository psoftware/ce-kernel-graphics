#include "libce.h"
#include "gr_window.h"
#include "gr_bitmap.h"
#include "gr_button.h"
#include "gr_label.h"
#include "consts.h"
#include "libgr.h"

gr_window::gr_window(int pos_x, int pos_y, int size_x, int size_y, int z_index)
: gr_object(pos_x, pos_y, size_x, size_y, z_index)
{
	// la finestra è composta da tre container: uno che contiene la topbar, uno che contiene gli oggetti della finestra, e uno che
	// contiene entrambi i contenitori (main_container), il quale è aggiunto al doubled_framebuffer

	// main/topbar container:
	this->topbar_container = new gr_object(0,0,this->size_x,TOPBAR_HEIGHT,0);
	this->topbar_bitmap = new gr_bitmap(0,0,this->size_x,TOPBAR_HEIGHT,0);
	gr_memset(this->topbar_bitmap->get_buffer(), TOPBAR_WIN_BACKCOLOR, this->size_x*TOPBAR_HEIGHT);
	this->topbar_bitmap->render();
	this->topbar_container->add_child(this->topbar_bitmap);
	add_child(this->topbar_container);

	// pulsante chiusura
	this->close_button = new gr_button(this->size_x-TOPBAR_HEIGHT-CLOSEBUTTON_PADDING_X,CLOSEBUTTON_PADDING_Y,18,18,1,CLOSEBUTTON_WIN_BACKCOLOR);
	this->close_button->set_text("x");
	this->close_button->render();
	this->topbar_container->add_child(this->close_button);

	// titolo finestra
	this->title_label = new gr_label(TITLELABEL_PADDING_X,CLOSEBUTTON_PADDING_Y,this->close_button->get_pos_x()-TITLELABEL_PADDING_X,
		TOPBAR_HEIGHT-CLOSEBUTTON_PADDING_Y,1, TOPBAR_WIN_BACKCOLOR);
	this->title_label->set_text("Finestra");
	this->title_label->render();
	this->topbar_container->add_child(this->title_label);

	// contenitore oggetti finestra + background
	this->inner_container = new gr_object(0,TOPBAR_HEIGHT,this->size_x,this->size_y-TOPBAR_HEIGHT,0);
	this->background_bitmap = new gr_bitmap(0,0,this->size_x,this->size_y-TOPBAR_HEIGHT,0);
	gr_memset(this->background_bitmap->get_buffer(), DEFAULT_WIN_BACKCOLOR, this->size_x*(this->size_y-TOPBAR_HEIGHT));
	this->background_bitmap->render();
	this->inner_container->add_child(this->background_bitmap);

	this->topbar_container->render();
	this->inner_container->render();

	add_child(this->inner_container);
	set_visibility(false);
}

void gr_window::set_title(const char *str)
{
	this->title_label->set_text(str);
}