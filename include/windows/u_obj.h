#ifndef U_OBJ_
#define U_OBJ_

enum user_event_type {NOEVENT, USER_EVENT_MOUSEZ, USER_EVENT_MOUSEUP, USER_EVENT_MOUSEDOWN};
enum mouse_button {LEFT,MIDDLE,RIGHT};

struct des_user_event
{
	user_event_type type;
	union
	{
		mouse_button button;
		int delta_z;
	};
	union
	{
		int rel_x;
	};
	union
	{
		int rel_y;
	};

	des_user_event * next;
};

const natb W_ID_LABEL=0;
const natb W_ID_BUTTON=1;

class u_windowObject
{
	public:
	natb TYPE;
	unsigned short size_x;
	unsigned short size_y;
	short pos_x;
	short pos_y;
	short z_index;

	natb backColor;

	virtual void process_event(user_event_type type)=0;
};

class u_button : public u_windowObject
{
	public:
	char text[20];
	natb borderColor;
	bool clicked;
	u_button()
	{
		TYPE=W_ID_BUTTON;
		clicked=false;
	}

	void process_event(user_event_type type)
	{
		if(type==USER_EVENT_MOUSEUP)
			clicked=false;
		else if(type==USER_EVENT_MOUSEDOWN)
			clicked=true;
	}
};

class u_label : public u_windowObject
{
	public:
	char text[100];
	u_label()
	{
		TYPE=W_ID_LABEL;
	}

	void process_event(user_event_type type)
	{

	}
};

#endif