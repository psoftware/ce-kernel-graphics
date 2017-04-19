#include "libce.h"
#include "libgr.h"
#include "gr_object.h"
#include "consts.h"

gr_object::gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer)
	: child_list(0), child_list_last(0), next_brother(0), previous_brother(0), overlapping_child_list(0), overlapping_next_brother(0),
		modified(true), old_pos_x(pos_x), old_pos_y(pos_y), old_size_x(size_x), old_size_y(size_y),
		pos_x(pos_x), pos_y(pos_y), size_x(size_x), size_y(size_y), z_index(z_index), trasparency(false), visible(true)
{
	if(predefined_buffer==0)
		buffer = new PIXEL_UNIT[size_x*size_y];
	else
		buffer=predefined_buffer;

	flog(LOG_INFO, "Nuovo gr_object o derivato con size_x %d e this->size_x %d", size_x, this->size_x);
}

//O(n)
void gr_object::add_child(gr_object *newchild)
{
	if(child_list==0)
	{
		child_list=newchild;
		child_list_last=newchild;
		return;
	}

	// *c si ferma sull'oggetto con z-index maggiore oppure su NULL, se a fine lista
	// *p è l'elemento precedente
	gr_object *c,*p=child_list;
	for(c=child_list; c!=0 && c->z_index <= newchild->z_index; c=c->next_brother)
		p=c;

	
	if(c==0)
		child_list_last = newchild;
	else
		c->previous_brother = newchild;

	newchild->next_brother = c;
	if(c==child_list)
	{
		child_list = newchild;
		newchild->previous_brother = 0;
	}
	else
	{
		p->next_brother = newchild;
		newchild->previous_brother = p;
	}
}

//O(1)
bool gr_object::remove_child(gr_object *removechild)
{
	if(removechild==0 || child_list==0 || child_list_last==0)
		return false;

	if(removechild==child_list)
	{
		child_list=removechild->next_brother;
		if(child_list==0) //la lista si è svuotata
			child_list_last=0;
		else
			child_list->previous_brother=0;
	}
	else if(removechild==child_list_last)
	{
		child_list_last=removechild->previous_brother;
		if(child_list_last==0) //la lista si è svuotata
			child_list=0;
		else
			child_list_last->next_brother=0;
	}
	else
	{
		removechild->previous_brother->next_brother=removechild->next_brother;
		removechild->next_brother->previous_brother=removechild->previous_brother;
	}

	return true;
}

// serve per dare focus ad un elemento nel suo z-index
// O(1)
void gr_object::focus_child(gr_object *focuschild)
{
	//lo rimuovo dalla lista
	remove_child(focuschild);

	//lo riaggiungo (viene messo in cima agli altri di pari z-index)
	add_child(focuschild);
}

unsigned int gr_object::get_pos_x(){
	return this->pos_x;
}
unsigned int gr_object::get_pos_y(){
	return this->pos_y;
}
unsigned int gr_object::get_size_x(){
	return this->size_x;
}
unsigned int gr_object::get_size_y(){
	return this->size_y;
}
void gr_object::set_pos_x(unsigned int newval){
	this->pos_x=newval;
}
void gr_object::set_pos_y(unsigned int newval){
	this->pos_y=newval;
}
void gr_object::set_size_x(unsigned int newval){
	this->size_x=newval;
}
void gr_object::set_size_y(unsigned int newval){
	this->size_y=newval;
}
void gr_object::set_trasparency(bool newval){
	this->trasparency=newval;
}
void gr_object::set_visibility(bool newval){
	this->visible=newval;
}

//renderizza su buffer tutti i figli nella lista child_tree
void gr_object::render()
{
	/*struct render_target
	{
		gr_object *target;
		render_target *next;

		render_target(gr_object * newobj)
		{
			target=newobj;
		}
	};
	struct render_subset_unit
	{
		unsigned pos_x;
		unsigned pos_y;
		unsigned size_x;
		unsigned size_y;
		bool first_modified_encountered;
		render_target * copy_list;

		render_subset_unit * next;

		render_subset_unit()
		{
			pos_x=0;
			pos_y=0;
			size_x=0;
			size_y=0;
			copy_list=0;
			first_modified_encountered=false;
		}
	};

	render_subset_unit *unit_list=0;

	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		render_target *newtarget = new render_target(obj);

		for(render_subset_unit *subsetunit=unit_list; subsetunit!=0; subsetunit=subsetunit->next)
		{
			if((subsetunit->pos_x + subsetunit->size_x > obj->pos_x) && (subsetunit->pos_x < obj->pos_x + obj->size_x) &&
			(subsetunit->pos_y + subsetunit->size_y > obj->pos_y) && (subsetunit->pos_y < obj->pos_y + obj->size_y))
			{
				//aggiungo l'elemento in testa alla copy_list della subsetunit
				newtarget->next = unit_list->copy_list;
				unit_list->copy_list = newtarget;

				//aggiorno le coordinate del render subset solo se modificato
				if(obj->modified)
				{
					subsetunit->first_modified_encountered=true;
					//subsetunit->...
				}
				
				break;
			}

			//se non ho trovato unità, allora ne creo una nuova e la aggiungo alla lista di unità
			render_subset_unit *newunit = new render_subset_unit;
			newunit->next = unit_list;
			unit_list= newunit;

			//aggiungo l'elemento in testa alla copy_list della nuova subsetunit
			newtarget->next = newunit->copy_list;
			newunit->copy_list = newtarget;
		}	
	}*/
	
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		if(!(obj->visible))
			continue;

		flog(LOG_INFO, "## Renderizzo oggetto dalla lista con z-index %d, main container size_x %d", obj->z_index, this->size_x);
		int max_x = (obj->pos_x + obj->size_x > this->size_x) ? this->size_x - obj->pos_x : obj->size_x;
		int max_y = (obj->pos_y + obj->size_y > this->size_y) ? this->size_y - obj->pos_y : obj->size_y;
		if(max_x<=0 || max_y<=0)
			continue;

		//la trasparenza non è renderizzabile usando memcpy, quindi è molto più lenta. Se non ce n'è bisogno, renderizziamo con memcpy
		if(!obj->trasparency)
			//for(int i=0; i<max_x; i++)
				for(int j=0; j<max_y; j++)
					memcpy(this->buffer + obj->pos_x + (j+obj->pos_y)*this->size_x, obj->buffer + j*obj->size_x, max_x);
					//set_pixel(this->buffer, obj->pos_x+i, obj->pos_y+j, this->size_x, this->size_y, obj->buffer[j*obj->size_x+i]);
		else
			for(int i=0; i<max_x; i++)
				for(int j=0; j<max_y; j++)
					if(obj->buffer[j*obj->size_x+i] != 0x03)
						set_pixel(this->buffer, obj->pos_x+i, obj->pos_y+j, this->size_x, this->size_y, obj->buffer[j*obj->size_x+i]);
		//controllo bound
		//memcopy...
		/*for(int y=0; y<size_y; y++)
			for(int x=0; x<size_x; x++)
				this->buffer[(x + c->pos_x) + (y + c->pos_y)*this->size_x] = c->buffer[x + y*c->size_x];*/

		flog(LOG_INFO, "## Terminata renderizzazione oggetto dalla lista con z-index %d", obj->z_index);
	}
}