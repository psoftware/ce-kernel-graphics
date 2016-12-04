#ifndef U_WINOBJ_
#define U_WINOBJ_

#include "virtual.h"

const int MAX_USER_OBJECTS=20;
class u_window
{
private:
	int w_id;
	int objs_ident[MAX_USER_OBJECTS];
	u_windowObject * objs[MAX_USER_OBJECTS];
	int objs_count;

	int internal_focus;

public:
	u_window(int size_x, int size_y, int pos_x, int pos_y)
	{
		w_id = crea_finestra(size_x, size_y, pos_x, pos_y);
		objs_count=0;
		internal_focus=-1;
	}

	void show(bool async=false)
	{
		visualizza_finestra(w_id, true);
	}

	bool add_object(u_windowObject* obj)
	{
		//ho un limite MAX_USER_OBJECTS da rispettare
		if(objs_count>=MAX_USER_OBJECTS)
			return false;

		//controllo che l'oggetto non sia già stato aggiunto
		for(int i=0; i<objs_count; i++)
			if(this->objs[i]==obj)
				return false;

		//mi mantengo solo il puntatore all'oggetto, così che l'utente possa modificarlo senza chiamare funzioni
		this->objs[this->objs_count] = obj;

		//uso la primitiva per creare l'oggetto e mi mantengo l'identificatore per modifiche future
		this->objs_ident[this->objs_count] = crea_oggetto(this->w_id, obj);
		objs_count++;

		return true;
	}

	bool update_object(u_windowObject* obj, bool async=false)
	{
		for(int i=0; i<objs_count; i++)
			if(this->objs[i]==obj)
			{
				aggiorna_oggetto(this->w_id, this->objs_ident[i], this->objs[i], async);
				return true;
			}
		return false;
	}

	void process_event()
	{
		des_user_event new_event = preleva_evento(w_id);
		if(new_event.type==NOEVENT)
			return;
		flog(LOG_INFO, "process_event: nuovo evento di tipo %d prelevato, rel_x %d rel_y %d delta_z", new_event.type, new_event.rel_x, new_event.rel_y);

		//eventi con target x,y
		if(new_event.type==USER_EVENT_MOUSEZ || new_event.type==USER_EVENT_MOUSEUP || new_event.type==USER_EVENT_MOUSEDOWN)
		{
			bool found=false;
			for(int i=0; i<this->objs_count; i++)
				if(new_event.rel_x > objs[i]->pos_x && new_event.rel_x < objs[i]->pos_x + objs[i]->size_x &&
					new_event.rel_y > objs[i]->pos_y && new_event.rel_y < objs[i]->pos_y + objs[i]->size_y)
				{
					internal_focus=i;
					objs[i]->process_event(new_event);
					update_object(objs[i]);
					flog(LOG_INFO, "process_event: new focus on %d", internal_focus);
					found=true;
				}

			//se l'evento con coordinate non ha centrato oggetti, allora devo togliere il focus
			if(!found)
				internal_focus=-1;
		}
		else if(internal_focus!=-1)
		{
			flog(LOG_INFO, "process_event: non-targetting event on %d", internal_focus);
			//eventi senza target x,y, quindi applicati su internal_focus
			objs[internal_focus]->process_event(new_event);
			update_object(objs[internal_focus]);
		}
	}
};

#endif