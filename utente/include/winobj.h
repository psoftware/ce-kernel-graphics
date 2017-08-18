#ifndef U_WINOBJ_
#define U_WINOBJ_

#include "virtual.h"
#include "log.h"

const int MAX_USER_OBJECTS=100;
class u_window
{
private:
	int w_id;

	// lista oggetti finestra (id e puntatori a struct)
	int objs_ident[MAX_USER_OBJECTS];
	u_windowObject * objs[MAX_USER_OBJECTS];
	int objs_count;

public:
	// handler dell'utente relativi a eventi della finestra
	void(*handler_closing_window)(des_user_event event);
	void(*handler_closed_window)(des_user_event event);

	u_window(int size_x, int size_y, int pos_x, int pos_y, const char *title)
	{
		w_id = crea_finestra(size_x, size_y, pos_x, pos_y, title);
		objs_count=0;
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

	u_windowObject * get_object(int id)
	{
		for(int i=0; i<objs_count; i++)
			if(this->objs[i]->id==id)
				return this->objs[i];
		return 0;
	}

	void next_event()
	{
		des_user_event new_event = preleva_evento(w_id);

		LOG_DEBUG("process_event: nuovo evento di tipo %d prelevato, rel_x %d rel_y %d delta_z", new_event.type, new_event.rel_x, new_event.rel_y);

		if(new_event.type==NOEVENT)
		{
			LOG_ERROR("process_event: nessun evento prelevato, probabile bug");
			return;
		} else if(new_event.type==USER_EVENT_RESIZE)	// è un evento che va diffuso a tutti i figli (resize della finestra)
		{
			for(int i=0; i<objs_count; i++)
			{
				this->objs[i]->process_event(new_event);

				// aggiorniamo l'oggetto
				update_object(this->objs[i]);
			}
			return;	// nient'altro da fare
		}
		else if(new_event.type==USER_EVENT_CLOSE_WINDOW)	// è un evento destinato alla finestra, non dobbiamo diffonderlo ai figli
		{
			// evento pre-chiusura
			if(this->handler_closing_window!=0)
				this->handler_closing_window(new_event);

			// chiudiamo la finestra
			chiudi_finestra(this->w_id);

			// evento post-chiusura (serve al processo per terminare)
			if(this->handler_closed_window!=0)
				this->handler_closed_window(new_event);
			return;
		}

		// l'evento ha l'id dell'oggetto a cui è destinato, scorriamo la lista e vediamo chi lo possiede
		for(int i=0; i<objs_count; i++)
			if(this->objs[i]->id == new_event.obj_id)
			{
				LOG_DEBUG("process_event: trovato oggetto %d", new_event.obj_id);

				// chiamiamo il metodo che si occupa di gestire gli eventi sull'oggetto
				this->objs[i]->process_event(new_event);

				// aggiorniamo l'oggetto
				update_object(this->objs[i]);
				break;
			}
	}
};

#endif
