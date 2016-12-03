extern "C" void __cxa_pure_virtual()
{
	flog(LOG_INFO, "Pure Virtual function called, aborting.");
}

void * operator new(long unsigned int n)
{
	return mem_alloc(n);
}

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


const int MAX_USER_OBJECTS=20;
class u_window
{
private:
	int w_id;
	int objs_ident[MAX_USER_OBJECTS];
	u_windowObject * objs[MAX_USER_OBJECTS];
	int objs_count;

public:
	u_window(int size_x, int size_y, int pos_x, int pos_y)
	{
		w_id = crea_finestra(size_x, size_y, pos_x, pos_y);
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

	void process_event()
	{
		des_user_event new_event = preleva_evento(w_id);
		if(new_event.type==NOEVENT)
			return;
		flog(LOG_INFO, "process_event: nuovo evento di tipo %d prelevato, rel_x %d rel_y %d delta_z", new_event.type, new_event.rel_x, new_event.rel_y);
		for(int i=0; i<this->objs_count; i++)
			if(new_event.rel_x > objs[i]->pos_x && new_event.rel_x < objs[i]->pos_x + objs[i]->size_x &&
				new_event.rel_y > objs[i]->pos_y && new_event.rel_y < objs[i]->pos_y + objs[i]->size_y)
			{
				objs[i]->process_event(new_event.type);
				update_object(objs[i]);
				flog(LOG_INFO, "process_event: BECCATO %d", LOG_INFO);
			}
	}
};