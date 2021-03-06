#include "libce_guard.h"
#include "gr_window.h"
#include "gr_bitmap.h"
#include "gr_button.h"
#include "gr_label.h"
#include "gr_textbox.h"
#include "gr_progressbar.h"
#include "gr_checkbox.h"
#include "consts.h"
#include "libgr.h"
#include "log.h"

extern "C" natl sem_ini(int val);
extern "C" void sem_signal(natl sem);

gr_window::gr_window(int pos_x, int pos_y, int size_x, int size_y, int z_index, const char * title)
: gr_object(pos_x, pos_y, size_x+BORDER_TICK*2, size_y+BORDER_TICK*2+TOPBAR_HEIGHT, z_index),
	topbar_container(0), topbar_bitmap(0), inner_container(0), background_bitmap(0),
	border_left_bitmap(0), border_right_bitmap(0), border_top_bitmap(0), border_bottom_bitmap(0),
	close_button(0), title_label(0), draggable(true), resizable(true), focused_object(0), clicked_down_object(0), event_head(0), event_tail(0)
{
	constructor(title);
}

gr_window::gr_window(u_basicwindow* u_wind)
: gr_object(u_wind->pos_x, u_wind->pos_y, u_wind->size_x+BORDER_TICK*2, u_wind->size_y+BORDER_TICK*2+TOPBAR_HEIGHT, WINDOWS_PLANE_ZINDEX),
	topbar_container(0), topbar_bitmap(0), inner_container(0), background_bitmap(0),
	border_left_bitmap(0), border_right_bitmap(0), border_top_bitmap(0), border_bottom_bitmap(0),
	close_button(0), title_label(0), draggable(true), resizable(true), focused_object(0), clicked_down_object(0), event_head(0), event_tail(0)
{
	constructor(u_wind->title);

	this->set_resizable(u_wind->resizable);
	this->set_draggable(u_wind->draggable);
	this->set_visibility(u_wind->visible);
}

void gr_window::constructor(const char * title)
{
	// la finestra è composta da tre container: uno che contiene la topbar, uno che contiene gli oggetti della finestra, e uno che
	// contiene entrambi i contenitori (main_container), il quale è aggiunto al doubled_framebuffer

	this->set_search_flag(WINDOW_FLAG);
	this->topbar_container = new gr_object(BORDER_TICK,BORDER_TICK,this->get_size_x()-BORDER_TICK*2,TOPBAR_HEIGHT,0);
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
	this->close_button->set_clicked_color(CLOSEBUTTON_WIN_CLICKEDCOLOR);
	this->close_button->set_text_color(CLOSEBUTTON_WIN_TEXTCOLOR);
	this->close_button->set_text("x");
	this->close_button->render();
	this->topbar_container->add_child(this->close_button);

	// titolo finestra
	this->title_label = new gr_label(TITLELABEL_PADDING_X,TITLELABEL_PADDING_Y,this->close_button->get_pos_x()-TITLELABEL_PADDING_X,
		TOPBAR_HEIGHT-TITLELABEL_PADDING_Y,TITLELABEL_ZINDEX);
	this->title_label->set_text_color(TITLELABEL_TEXTCOLOR);
	this->title_label->set_back_color(TOPBAR_WIN_BACKCOLOR);
	this->title_label->set_text(title);
	this->title_label->render();
	this->topbar_container->add_child(this->title_label);

	// contenitore oggetti finestra + background
	this->inner_container = new gr_object(BORDER_TICK,TOPBAR_HEIGHT+BORDER_TICK,this->size_x-2*BORDER_TICK,this->size_y-TOPBAR_HEIGHT-BORDER_TICK,0);
	this->background_bitmap = new gr_bitmap(0,0,this->inner_container->get_size_x(),this->inner_container->get_size_y(),0);
	this->background_bitmap->paint_uniform(DEFAULT_WIN_BACKCOLOR);
	this->background_bitmap->render();
	this->inner_container->add_child(this->background_bitmap);

	// bordi (destro, sinistro e basso)
	this->border_left_bitmap = new gr_bitmap(0,0, BORDER_TICK, this->size_y, BORDER_ZINDEX);
	this->border_left_bitmap->set_search_flag(BORDER_FLAG);
	this->border_left_bitmap->paint_uniform(BORDER_WIN_COLOR);
	this->border_right_bitmap = new gr_bitmap(this->size_x-BORDER_TICK, 0, BORDER_TICK, this->size_y, BORDER_ZINDEX);
	this->border_right_bitmap->set_search_flag(BORDER_FLAG);
	this->border_right_bitmap->paint_uniform(BORDER_WIN_COLOR);
	this->border_top_bitmap = new gr_bitmap(BORDER_TICK,0, this->size_x-2*BORDER_TICK, BORDER_TICK, BORDER_ZINDEX);
	this->border_top_bitmap->set_search_flag(BORDER_FLAG);
	this->border_top_bitmap->paint_uniform(BORDER_WIN_COLOR);
	this->border_bottom_bitmap = new gr_bitmap(BORDER_TICK, this->size_y-BORDER_TICK, this->size_x-2*BORDER_TICK, BORDER_TICK, BORDER_ZINDEX);
	this->border_bottom_bitmap->set_search_flag(BORDER_FLAG);
	this->border_bottom_bitmap->paint_uniform(BORDER_WIN_COLOR);

	this->add_child(this->border_left_bitmap);
	this->add_child(this->border_right_bitmap);
	this->add_child(this->border_top_bitmap);
	this->add_child(this->border_bottom_bitmap);
	this->border_left_bitmap->render();
	this->border_right_bitmap->render();
	this->border_top_bitmap->render();
	this->border_bottom_bitmap->render();

	this->add_child(this->inner_container);
	this->set_visibility(false);

	this->topbar_container->render();
	this->inner_container->render();

	//inizializzo il semaforo per la sincronizzazione sulla coda degli eventi
	event_sem_sync_notempty = sem_ini(0);
}

gr_window::~gr_window() {
	//cancelliamo tutti gli eventi
	des_user_event res;
	while(res.type!=NOEVENT)
		this->user_event_pop();


	this->topbar_container->remove_child(this->close_button);
	this->topbar_container->remove_child(this->title_label);
	this->topbar_container->remove_child(this->topbar_bitmap);
	delete close_button;
	delete title_label;
	delete topbar_bitmap;

	this->remove_child(this->topbar_container);
	delete topbar_container;

	this->inner_container->remove_child(this->background_bitmap);
	delete background_bitmap;

	this->remove_child(this->border_left_bitmap);
	this->remove_child(this->border_right_bitmap);
	this->remove_child(this->border_top_bitmap);
	this->remove_child(this->border_bottom_bitmap);
	delete border_left_bitmap;
	delete border_right_bitmap;
	delete border_top_bitmap;
	delete border_bottom_bitmap;

	// vanno, inoltre, eliminati tutti gli oggetti della finestra, cioè tutti i figli dell'inner container
	for(gr_object *child=this->inner_container->get_first_child(); child!=0; child=this->inner_container->get_first_child())
	{
		this->inner_container->remove_child(child);
		delete child;
	}

	this->remove_child(this->inner_container);
	delete inner_container;
}

void gr_window::update_window_from_user(u_basicwindow * u_wind) {
	this->set_pos_x(u_wind->pos_x);
	this->set_pos_y(u_wind->pos_y);
	this->set_content_size_x(u_wind->size_x);
	this->set_content_size_y(u_wind->size_y);
	this->set_title(u_wind->title);
	this->set_resizable(u_wind->resizable);
	this->set_draggable(u_wind->draggable);
	this->set_visibility(u_wind->visible);

	do_resize();
}

void gr_window::do_resize()
{
	// lasciamo all'implementazione do_resize di gr_object la competenza di riallocare il buffer della finestra
	gr_object::do_resize();

	// se non ci sono modifiche, possiamo evitare tutto il lavoro
	if(!is_size_modified())
		return;

	if(this->size_x<=0 || this->size_y<=0)
		return;

	int delta_size_x = this->size_x - old_size_x;
	int delta_size_y = this->size_y - old_size_y;

	//flog(LOG_INFO, "delta_size_x %d, delta_size_y %d, this_size_x %d, old_size_x %d", delta_size_x, delta_size_y, this->size_x, old_size_x);

	this->inner_container->set_size_x(this->size_x);
	this->inner_container->set_size_y(this->inner_container->get_size_y() + delta_size_y);
	this->inner_container->do_resize();

	this->background_bitmap->set_size_x(this->background_bitmap->get_size_x() + delta_size_x);
	this->background_bitmap->set_size_y(this->background_bitmap->get_size_y() + delta_size_y);
	this->background_bitmap->do_resize();
	this->background_bitmap->paint_uniform(DEFAULT_WIN_BACKCOLOR);

	this->topbar_container->set_size_x(this->topbar_container->get_size_x() + delta_size_x);
	this->topbar_container->do_resize();
	this->topbar_bitmap->set_size_x(this->topbar_container->get_size_x());
	this->topbar_bitmap->do_resize();
	this->topbar_bitmap->paint_uniform(TOPBAR_WIN_BACKCOLOR);

	this->border_left_bitmap->set_size_y(this->border_left_bitmap->get_size_y() + delta_size_y);
	this->border_left_bitmap->do_resize();
	this->border_left_bitmap->paint_uniform(BORDER_WIN_COLOR);

	this->border_right_bitmap->set_pos_x(this->border_right_bitmap->get_pos_x() + delta_size_x);
	this->border_right_bitmap->set_size_y(this->border_right_bitmap->get_size_y() + delta_size_y);
	this->border_right_bitmap->do_resize();
	this->border_right_bitmap->paint_uniform(BORDER_WIN_COLOR);

	this->border_top_bitmap->set_size_x(this->border_top_bitmap->get_size_x() + delta_size_x);
	this->border_top_bitmap->do_resize();
	this->border_top_bitmap->paint_uniform(BORDER_WIN_COLOR);

	this->border_bottom_bitmap->set_pos_y(this->border_bottom_bitmap->get_pos_y() + delta_size_y);
	this->border_bottom_bitmap->set_size_x(this->border_bottom_bitmap->get_size_x() + delta_size_x);
	this->border_bottom_bitmap->do_resize();
	this->border_bottom_bitmap->paint_uniform(BORDER_WIN_COLOR);

	this->close_button->set_pos_x(this->close_button->get_pos_x() + delta_size_x);
	this->close_button->render();


	// ri-renderizziamo tutto
	this->border_left_bitmap->render();
	this->border_right_bitmap->render();
	this->border_top_bitmap->render();
	this->border_bottom_bitmap->render();
	this->topbar_bitmap->render();
	this->topbar_container->render();
	this->background_bitmap->render();
	this->inner_container->render();
}

void gr_window::set_content_size_x(int newval){
	if(newval < BORDER_TICK*2 + CLOSEBUTTON_PADDING_X*2 + CLOSEBUTTON_SIZE)
		return;
	this->size_x=newval + BORDER_TICK*2;
}
void gr_window::set_content_size_y(int newval){
	if(newval < 0)
		return;
	this->size_y=newval + TOPBAR_HEIGHT + BORDER_TICK*2;
}
int gr_window::offset_size_x(int offset){
	if(this->size_x + offset < BORDER_TICK*2 + CLOSEBUTTON_PADDING_X*2 + CLOSEBUTTON_SIZE)
		return 0;
	this->size_x+=offset;
	return offset;
}
int gr_window::offset_size_y(int offset){
	if((this->size_y + offset < TOPBAR_HEIGHT + BORDER_TICK*2))
		return 0;
	this->size_y+=offset;
	return offset;
}

void gr_window::set_title(const char *str)
{
	this->title_label->set_text(str);
}

bool gr_window::get_draggable()
{
	return this->draggable;
}

void gr_window::set_draggable(bool draggable)
{
	this->draggable = draggable;
}

bool gr_window::get_resizable()
{
	return this->resizable;
}

void gr_window::set_resizable(bool resizable)
{
	this->resizable = resizable;
}

border_flags gr_window::get_clicked_borders(int rel_pos_x, int rel_pos_y)
{
	// devo capire se mi sono effettivamente mosso su un bordo
	gr_object::search_filter filter;
	gr_object::search_result res;

	memset(&filter, 0, sizeof(filter));
	//filter.skip_id = mouse_bitmap->get_id();
	filter.skip_id = -1;
	filter.padding_x = 5;
	filter.padding_y = 5;
	filter.flags = gr_window::BORDER_FLAG;
	this->search_tree(rel_pos_x, rel_pos_y, filter, res);

	border_flags result = 0;

	// in questo caso il click non è avvenuto su un bordo (considerando anche il padding)
	if(res.target == 0)
		return result; 	// no flags

	if(res.target == this->border_left_bitmap)
		result |= BORDER_LEFT;
	else if(res.target == this->border_right_bitmap)
		result |= BORDER_RIGHT;
	else if(res.target == this->border_top_bitmap)
		result |= BORDER_TOP;
	else if(res.target == this->border_bottom_bitmap)
		result |= BORDER_BOTTOM;

	// valuto se selezionare anche il bordo ortogonale (resize di entrambi le dimensioni x e y)
	if(res.target == this->border_left_bitmap || res.target == this->border_right_bitmap)
	{
		if(rel_pos_y > this->border_left_bitmap->get_size_y() - BORDER_ANGLE_SIZE)
			result |= BORDER_BOTTOM;
		else if(rel_pos_y < BORDER_ANGLE_SIZE)
			result |= BORDER_TOP;
	}
	else if(res.target == this->border_top_bitmap || res.target == this->border_bottom_bitmap)
	{
		if(rel_pos_x > this->border_top_bitmap->get_size_x() - BORDER_ANGLE_SIZE)
			result |= BORDER_RIGHT;
		else if(rel_pos_x < BORDER_ANGLE_SIZE)
			result |= BORDER_LEFT;
	}

	return result;
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
		case W_ID_TEXTBOX:
			newobj = new gr_textbox(static_cast<u_textbox*>(u_obj));
			newobj->set_search_flag(TEXTBOX_FLAG);
			break;
		case W_ID_PROGRESSBAR:
			newobj = new gr_progressbar(static_cast<u_progressbar*>(u_obj));
			newobj->set_search_flag(PROGRESSBAR_FLAG);
			break;
		case W_ID_CHECKBOX:
			newobj = new gr_checkbox(static_cast<u_checkbox*>(u_obj));
			newobj->set_search_flag(CHECKBOX_FLAG);
			break;
		default:
			LOG_ERROR("c_crea_oggetto: tipo oggetto %d errato", u_obj->TYPE);
			return 0;
	}

	LOG_DEBUG("add_user_object: oggetto %d di tipo %d creato su finestra %d", newobj->get_id(), u_obj->TYPE, this->get_id());
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
		case W_ID_TEXTBOX:
			if(!dest_obj->has_flag(gr_window::TEXTBOX_FLAG))
				return false;
			*(static_cast<gr_textbox*>(dest_obj)) = *static_cast<u_textbox*>(u_obj);
			break;
		case W_ID_PROGRESSBAR:
			if(!dest_obj->has_flag(gr_window::PROGRESSBAR_FLAG))
				return false;
			*(static_cast<gr_progressbar*>(dest_obj)) = *static_cast<u_progressbar*>(u_obj);
			break;
		case W_ID_CHECKBOX:
			if(!dest_obj->has_flag(gr_window::CHECKBOX_FLAG))
				return false;
			*(static_cast<gr_checkbox*>(dest_obj)) = *static_cast<u_checkbox*>(u_obj);
			break;
		default:
			LOG_ERROR("update_user_object: tipo oggetto %d errato", u_obj->TYPE);
			return true;
	}

	LOG_DEBUG("update_user_object: oggetto %d di tipo %d modificato su finestra %d", dest_obj->get_id(), u_obj->TYPE, this->get_id());
	dest_obj->render();
	inner_container->render();
	return false;
}

gr_object *gr_window::search_user_object(u_windowObject * u_obj)
{
	if(u_obj==0)
		return 0;

	return this->inner_container->search_child_by_id(u_obj->id);
}

bool gr_window::set_focused_child(gr_object *obj)
{
	if(obj == 0)
		return false;

	// se l'oggetto non è cambiato non c'è bisogno di fare altro
	if(obj == this->focused_object)
		return true;

	// controllo che l'oggetto a cui andrò ad assegnare il focus faccia effettivamente parte
	// di questa finestra (dell'inner_container in particolare)
	if(!this->inner_container->has_child(obj))
		return false;

	// devo gestire lo stato del cursore del testo delle textbox:
	// se l'oggetto che aveva il focus era una textbox, allora devo assicurarmi
	// che il cursore del testo venga eliminato
	if(this->focused_object != 0 && this->focused_object->has_flag(TEXTBOX_FLAG))
	{
		gr_textbox *focused_textbox = static_cast<gr_textbox*>(this->focused_object);
		if(focused_textbox->get_caret_print())
		{
			focused_textbox->set_caret_print(false);
			focused_textbox->render();
			inner_container->render();
		}
	}

	this->focused_object = obj;
	return true;
}

void gr_window::clear_focused_child()
{
	this->focused_object = 0;
}

void gr_window::process_tick_event()
{
	if(this->focused_object == 0 || !this->focused_object->has_flag(TEXTBOX_FLAG))
		return;

	gr_textbox *focused_textbox = static_cast<gr_textbox*>(this->focused_object);
	focused_textbox->set_caret_print(!focused_textbox->get_caret_print());
	focused_textbox->render();
	inner_container->render();
}

window_action gr_window::click_on_topbar(gr_object * dest_obj, mouse_button butt, bool mouse_down)
{
	if(dest_obj==0 || butt!=LEFT)
		return NOTHING;

	// non abbiamo niente da fare in questi casi, ma dobbiamo segnalare
	// che va fatto il trascinamento dell'oggetto
	if((dest_obj==this->topbar_bitmap || dest_obj==this->topbar_container || dest_obj==this->title_label) && this->draggable)
		return DRAG;

	// eventi per close_button
	if(dest_obj == this->close_button && mouse_down)
	{
		this->clicked_down_object = dest_obj;
		this->close_button->set_clicked(true);
		this->close_button->render();
	}
	else if(dest_obj == this->close_button && !mouse_down)
	{
		this->clicked_down_object = 0;
		this->close_button->set_clicked(false);
		this->close_button->render();
		this->user_event_add_close_window();
	}

	this->topbar_container->render();

	// il trascinamento non va segnalato se stiamo gestendo eventi
	// su oggetti interni alla topbar (esclusa la bitmap)
	return NOTHING;
}

window_action gr_window::click_event(gr_object *dest_obj, int abs_pos_x, int abs_pos_y, mouse_button butt, bool mouse_down)
{
	// l'evento di mouseup ha sempre come target l'evento su cui è stato precedentemente mandato un evento di mousedown
	if(!mouse_down)
		dest_obj = this->clicked_down_object;

	if(!dest_obj)
		return NOTHING;

	// se il click è stato fatto nella topbar
	if(this->topbar_container->has_child(dest_obj))
	{
		LOG_DEBUG("gr_window: il click è stato fatto sulla topbar della finestra (%d) con mouse_down=%d", this->get_id(), mouse_down);

		// questo metodo processa gli eventi per la topbar e ci comunica se va fatto il trascinamento o meno
		window_action result = this->click_on_topbar(dest_obj, butt, mouse_down);

		// in questo caso è richiesto il rendering della finestra perchè ho processato eventi per gli oggetti della topbar
		// CASO ECCEZIONALE: il rendering della finestra è fatto sempre dal gestore delle finestre, ma in questo caso non possiamo
		// fare altrimenti (per ottimizzare)
		this->render();

		return result;
	}
	else // altrimenti devo generare un evento utente perchè il click è stato fatto dentro la finestra (esclusi bordi e topbar)
	{
		if(mouse_down)
		{
			if(this->set_focused_child(dest_obj))	// devo anche controllare se l'oggetto è valido (cioè se è un oggetto dell'inner_container)
			{
				this->user_event_add_mousebutton((mouse_down) ? USER_EVENT_MOUSEDOWN : USER_EVENT_MOUSEUP, butt, abs_pos_x, abs_pos_y);
				this->clicked_down_object = dest_obj;
			}
		}
		else
		{
			this->user_event_add_mousebutton((mouse_down) ? USER_EVENT_MOUSEDOWN : USER_EVENT_MOUSEUP, butt, abs_pos_x, abs_pos_y);
			this->clicked_down_object = 0;
		}
	}

	return NOTHING;
}

// gestione degli eventi per utente
void gr_window::user_event_push(des_user_event * event)
{
	if(event==0)
		return;

	event->next = 0;

	// facciamo un inserimento in coda
	if(event_tail == 0)	//lista vuota
		event_head = event;
	else
		event_tail->next = event;

	event_tail = event;

	sem_signal(event_sem_sync_notempty);
}

des_user_event gr_window::user_event_pop()
{
	// restituiamo un evento con tipo NOEVENT se la lista è vuota
	if(event_head==0)
	{
		des_user_event error_event;
		error_event.type=NOEVENT;
		return error_event;
	}

	//ci occupiamo qui dell'eliminazione dell'oggetto, che restituiremo solo per valore
	des_user_event *target = event_head;
	des_user_event result = *target;

	//estraiamo dalla testa
	event_head = event_head->next;
	delete target;

	if(event_head==0)
		event_tail=0;

	return result;
}

void gr_window::user_event_add_mousemovez(int delta_z, int abs_x, int abs_y)
{
	des_user_event * event = new des_user_event();
	event->type=USER_EVENT_MOUSEZ;
	event->delta_z=delta_z;
	user_event_push_on_focused(abs_x, abs_y, event);
}

void gr_window::user_event_add_mousebutton(user_event_type event_type, mouse_button butt, int abs_x, int abs_y)
{
	des_user_event * event = new des_user_event();
	event->type=event_type;
	event->button=butt;
	user_event_push_on_focused(abs_x, abs_y, event);
}

void gr_window::user_event_add_keypress(char key)
{
	des_user_event * event = new des_user_event();
	event->type=USER_EVENT_KEYBOARDPRESS;
	event->k_char=key;
	user_event_push_on_focused(0, 0, event);
}

void gr_window::user_event_add_resize(int delta_pos_x, int delta_pos_y, int delta_size_x, int delta_size_y)
{
	// questo metodo gestisce l'inserimento dell'evento resize in modo particolare:
	// se l'ultima operazione di resize non è stata ancora completata quando una nuova ne viene inserita,
	// è meglio andare ad aggiornare l'ultima richiesta e non inserine una nuova. In tal modo evitiamo
	// di sovraccaricare il processo utente con troppe richieste, anche perchè se ne abbiamo trovata una ancora
	// in coda, significa che il processo utente non riesce a processare velocemente le richieste.
	// Se il processo è abbastanza veloce a processare tutte le richieste, non troveremo in coda eventi resize
	// e il processo farà comunque un resize fluido. Lo scopo è bilanciare prestazioni e qualità del resize.
	if(event_tail != 0 && event_tail->type == USER_EVENT_RESIZE)
	{
		event_tail->delta_pos_x+=delta_pos_x;
		event_tail->delta_pos_y+=delta_pos_y;
		event_tail->delta_size_x+=delta_size_x;
		event_tail->delta_size_y+=delta_size_y;
		return;
	}

	des_user_event * event = new des_user_event();
	event->type=USER_EVENT_RESIZE;
	event->delta_pos_x=delta_pos_x;
	event->delta_pos_y=delta_pos_y;
	event->delta_size_x=delta_size_x;
	event->delta_size_y=delta_size_y;
	user_event_push(event);
}

void gr_window::user_event_add_close_window()
{
	des_user_event * event = new des_user_event();
	event->type=USER_EVENT_CLOSE_WINDOW;
	user_event_push(event);
}