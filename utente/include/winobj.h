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

		// l'evento ha l'id dell'oggetto a cui è destinato, scorriamo la lista e vediamo chi lo possiede
		for(int i=0; i<objs_count; i++)
			if(this->objs[i]->id == new_event.obj_id)
			{
				flog(LOG_INFO, "process_event: trovato oggetto %d", new_event.obj_id);
				this->objs[i]->process_event(new_event);
				update_object(this->objs[i]);
				break;
			}
	}
};

#endif