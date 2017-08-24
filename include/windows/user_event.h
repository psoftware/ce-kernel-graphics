#ifndef _USER_EVENT_H
#define _USER_EVENT_H

enum user_event_type {NOEVENT, USER_EVENT_MOUSEZ, USER_EVENT_MOUSEUP, USER_EVENT_MOUSEDOWN, USER_EVENT_KEYBOARDPRESS, USER_EVENT_RESIZE, USER_EVENT_CLOSE_WINDOW};
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

#endif