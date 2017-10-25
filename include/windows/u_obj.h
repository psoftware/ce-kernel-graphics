#ifndef U_OBJ_
#define U_OBJ_

#include "../io/windows/consts.h"
#include "libce_guard.h"
#include "windows/user_event.h"

// costanti per controllare i cast (polimorfismo)
enum u_obj_type {W_ID_LABEL, W_ID_BUTTON, W_ID_TEXTBOX, W_ID_PROGRESSBAR, W_ID_CHECKBOX};

// costanti per la variabile anchor
const natb LEFT_ANCHOR = 1u;
const natb RIGHT_ANCHOR = 1u << 1;
const natb TOP_ANCHOR = 1u << 2;
const natb BOTTOM_ANCHOR = 1u << 3;

class u_windowObject
{
private:
	short anchor;
	short anchor_carry_x;
	short anchor_carry_y;

public:
	const u_obj_type TYPE;	// ci serve per il polimorfismo

	int id;

	short pos_x;
	short pos_y;
	short size_x;
	short size_y;
	short z_index;

	PIXEL_UNIT back_color;

	// handler definibili dall'utente
	void(*handler_mouse_z)(des_user_event event);
	void(*handler_mouse_up)(des_user_event event);
	void(*handler_mouse_down)(des_user_event event);
	void(*handler_keyboard_press)(des_user_event event);

	u_windowObject(u_obj_type TYPE) : anchor(TOP_ANCHOR | LEFT_ANCHOR), TYPE(TYPE) {

	}

	virtual ~u_windowObject() {

	}

	void set_anchor(short flag_anchor)
	{
		anchor = flag_anchor;
	}

	virtual void process_event(des_user_event event){
		// cerchiamo l'handler definito dall'utente che gestisce questo tipo di evento
		void(*handler)(des_user_event) = 0;

		switch(event.type)
		{
			case NOEVENT: break;
			case USER_EVENT_CLOSE_WINDOW: break;	// noi non sappiamo niente della finestra padre, quindi se la deve vedere qualcun'altro
			case USER_EVENT_RESIZE:
				if((anchor & LEFT_ANCHOR) && (anchor & RIGHT_ANCHOR))
					size_x+=event.delta_size_x;
				else if(anchor & RIGHT_ANCHOR)
					pos_x+=event.delta_size_x;
				else if((anchor & LEFT_ANCHOR) == 0)
				{
					pos_x+=(event.delta_size_x + anchor_carry_x)/2;
					anchor_carry_x = (event.delta_size_x + anchor_carry_x) % 2;
				}

				if((anchor & TOP_ANCHOR) && (anchor & BOTTOM_ANCHOR))
					size_y+=event.delta_size_y;
				else if(anchor & BOTTOM_ANCHOR)
					pos_y+=event.delta_size_y;
				else if((anchor & LEFT_ANCHOR) == 0)
				{
					pos_y+=(event.delta_size_y + anchor_carry_y)/2;
					anchor_carry_y = (event.delta_size_y + anchor_carry_y) % 2;
				};
				break;
			case USER_EVENT_MOUSEZ:
				handler = this->handler_mouse_z;
				break;
			case USER_EVENT_MOUSEUP:
				handler = this->handler_mouse_up;
				break;
			case USER_EVENT_MOUSEDOWN:
				handler = this->handler_mouse_down;
				break;
			case USER_EVENT_KEYBOARDPRESS:
				handler = this->handler_keyboard_press;
				break;
			default: break;
		}

		// chiamiamo l'handler definito dall'utente, se definito
		if(handler != 0)
			handler(event);
	}
};

class u_button : public u_windowObject
{
	public:
	char text[100];

	PIXEL_UNIT border_color;
	PIXEL_UNIT clicked_color;
	PIXEL_UNIT text_color;
	bool clicked;
	u_button() : u_windowObject(W_ID_BUTTON), clicked(false)
	{
		border_color=BUTTON_DEFAULT_BORDERCOLOR;
		clicked_color=BUTTON_DEFAULT_CLICKEDCOLOR;
		text_color=BUTTON_DEFAULT_TEXTCOLOR;
		back_color=BUTTON_DEFAULT_BACKCOLOR;
	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);

		if(event.type==USER_EVENT_MOUSEUP)
			clicked=false;
		else if(event.type==USER_EVENT_MOUSEDOWN)
			clicked=true;
	}
};

class u_label : public u_windowObject
{
	public:
	char text[1000];
	PIXEL_UNIT text_color;
	u_label() : u_windowObject(W_ID_LABEL)
	{
		text_color=LABEL_DEFAULT_TEXTCOLOR;
		back_color=LABEL_DEFAULT_BACKCOLOR;
	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);
	}
};

class u_textbox : public u_windowObject
{
	public:
	char text[1000];
	PIXEL_UNIT border_color;
	PIXEL_UNIT text_color;
	u_textbox() : u_windowObject(W_ID_TEXTBOX)
	{
		border_color = TEXTBOX_DEFAULT_BORDERCOLOR;
		text_color = TEXTBOX_DEFAULT_TEXTCOLOR;
	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);

		if(event.type==USER_EVENT_KEYBOARDPRESS)
		{
			int eos = strlen(text);

			if(event.k_char=='\b' && eos !=0)
				text[eos-1]='\0';
			else if(event.k_char!='\b' && eos<100)
			{
				text[eos]=event.k_char;
				text[eos+1]='\0';
			}
		}
	}
};

class u_progressbar : public u_windowObject
{
	public:
	int progress_perc;

	u_progressbar() : u_windowObject(W_ID_PROGRESSBAR)
	{

	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);

		//nessun evento da gestire
	}
};

class u_checkbox : public u_windowObject
{
	public:
	bool selected;

	u_checkbox() : u_windowObject(W_ID_CHECKBOX), selected(false)
	{

	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);

		if(event.type==USER_EVENT_MOUSEUP)
			selected = !selected;
	}
};

#endif
