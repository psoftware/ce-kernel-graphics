#ifndef U_OBJ_
#define U_OBJ_

enum user_event_type {NOEVENT, USER_EVENT_MOUSEZ, USER_EVENT_MOUSEUP, USER_EVENT_MOUSEDOWN, USER_EVENT_KEYBOARDPRESS};
enum mouse_button {LEFT,MIDDLE,RIGHT};

struct des_user_event
{
	user_event_type type;
	union
	{
		mouse_button button;	//mousebutton
		int delta_z;			//mousez
		natb k_char;			//tastiera
	};
	union
	{
		int rel_x;				//mousebutton
		natb k_flag;			//tastiera
	};
	union
	{
		int rel_y;				//mousebutton
	};

	des_user_event * next;
};

const natb W_ID_LABEL=0;
const natb W_ID_BUTTON=1;
const natb W_ID_TEXTBOX=2;

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

	natb back_color;

	virtual void process_event(des_user_event event)=0;
};

class u_button : public u_windowObject
{
	public:
	char text[20];

	natb border_color;
	natb clicked_color;
	natb text_color;
	bool clicked;
	u_button()
	{
		TYPE=W_ID_BUTTON;
		clicked=false;
	}

	void process_event(des_user_event event)
	{
		if(event.type==USER_EVENT_MOUSEUP)
			clicked=false;
		else if(event.type==USER_EVENT_MOUSEDOWN)
			clicked=true;
	}
};

class u_label : public u_windowObject
{
	public:
	char text[100];
	natb text_color;
	u_label()
	{
		TYPE=W_ID_LABEL;
	}

	void process_event(des_user_event event)
	{

	}
};

class u_textbox : public u_windowObject
{
	public:
	char text[100];
	u_textbox()
	{
		TYPE=W_ID_TEXTBOX;
	}

	void process_event(des_user_event event)
	{
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

#endif