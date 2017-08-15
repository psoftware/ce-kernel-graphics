#ifndef U_OBJ_
#define U_OBJ_

#include "../io/windows/consts.h"

enum user_event_type {NOEVENT, USER_EVENT_MOUSEZ, USER_EVENT_MOUSEUP, USER_EVENT_MOUSEDOWN, USER_EVENT_KEYBOARDPRESS, USER_EVENT_RESIZE};
enum mouse_button {LEFT,MIDDLE,RIGHT};

struct des_user_event
{
	int obj_id;
	user_event_type type;
	union
	{
		mouse_button button;	//mousebutton
		int delta_z;			//mousez
		natb k_char;			//tastiera
		int delta_pos_x;		//resize
	};
	union
	{
		int rel_x;				//mousebutton
		natb k_flag;			//tastiera
		int delta_pos_y;		//resize
	};
	union
	{
		int rel_y;				//mousebutton
		int delta_size_x;		//resize
	};
	union
	{
		int delta_size_y;		//resize
	};

	des_user_event * next;
};

// costanti per controllare i cast
const natb W_ID_LABEL=0;
const natb W_ID_BUTTON=1;
const natb W_ID_TEXTBOX=2;
const natb W_ID_PROGRESSBAR=3;

// costanti per la variabile anchor
const natb LEFT_ANCHOR = 1u;
const natb RIGHT_ANCHOR = 1u << 1;
const natb TOP_ANCHOR = 1u << 2;
const natb BOTTOM_ANCHOR = 1u << 3;

class u_windowObject
{
public:
	int id;
	natb TYPE;
	short size_x;
	short size_y;
	short pos_x;
	short pos_y;
	short z_index;

	char anchor;
	short anchor_carry_x;
	short anchor_carry_y;

	PIXEL_UNIT back_color;

	void(*handler_mouse_z)(des_user_event event);
	void(*handler_mouse_up)(des_user_event event);
	void(*handler_mouse_down)(des_user_event event);
	void(*handler_keyboard_press)(des_user_event event);

	u_windowObject() : anchor(TOP_ANCHOR | LEFT_ANCHOR) {

	}

	void set_anchor(short flag_anchor)
	{
		anchor = flag_anchor;
	}

	virtual void process_event(des_user_event event){
		switch(event.type)
		{
			case NOEVENT: break;
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
				//flog(LOG_INFO, "pos dx %d dy. size dx %d dy %d", event.delta_pos_x, event.delta_pos_y, event.delta_size_x, event.delta_size_y);
			break;
			default: break;
		}
	}
};

class u_button : public u_windowObject
{
	public:
	char text[20];

	PIXEL_UNIT border_color;
	PIXEL_UNIT clicked_color;
	PIXEL_UNIT text_color;
	bool clicked;
	u_button()
	{
		TYPE=W_ID_BUTTON;
		clicked=false;
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
	char text[120];
	PIXEL_UNIT text_color;
	u_label()
	{
		TYPE=W_ID_LABEL;
	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);
	}
};

class u_textbox : public u_windowObject
{
	public:
	char text[100];
	PIXEL_UNIT border_color;
	PIXEL_UNIT text_color;
	u_textbox()
	{
		TYPE=W_ID_TEXTBOX;
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

	u_progressbar()
	{
		TYPE=W_ID_PROGRESSBAR;
	}

	void process_event(des_user_event event)
	{
		u_windowObject::process_event(event);

		//nessun evento da gestire
	}
};

#endif