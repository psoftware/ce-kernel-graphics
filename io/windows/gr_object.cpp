#include "libce.h"
#include "libgr.h"
#include "gr_object.h"
#include "consts.h"

gr_object::gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer)
	: child_list(0), child_list_last(0), next_brother(0), previous_brother(0), units(0),
		old_pos_x(pos_x), old_pos_y(pos_y), old_size_x(size_x), old_size_y(size_y),
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
// O(n)
void gr_object::focus_child(gr_object *focuschild)
{
	//lo rimuovo dalla lista
	remove_child(focuschild);

	//lo riaggiungo (viene messo in cima agli altri di pari z-index)
	add_child(focuschild);
}

void gr_object::push_render_unit(render_subset_unit *newunit)
{
	newunit->next = units;
	units=newunit;
}

gr_object::render_subset_unit * gr_object::pop_render_unit()
{
	if(units==0)
		return 0;

	render_subset_unit * temp = units;
	units=units->next;
	return temp;
}

void gr_object::clear_render_units()
{
	units=0; //E' una bestemmia praticamente
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

void gr_object::align_old_coords(){
	this->old_pos_x = this->pos_x;
	this->old_pos_y = this->pos_y;
	this->old_size_x = this->size_x;
	this->old_size_y = this->size_y;
}

//renderizza su buffer tutti i figli nella lista child_tree
void gr_object::render()
{
	static natb debug_color = 0x20;
	//flog(LOG_INFO, "## --- inizio render()");

	//per ogni oggetto (obj) del contenitore
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		//flog(LOG_INFO, "## nuovo oggetto x=%d y=%d w=%d h=%d:", obj->pos_x,obj->pos_y, obj->size_x, obj->size_y);
		//render_target *newtarget = new render_target(obj);

		// controllo di intersezione della VECCHIA POSIZIONE/DIMENSIONE (AREA PRECEDENTEMENTE OCCUPATA):
		// creo una nuova render_unit e faccio finta che faccia parte di quelle di obj.
		// visto che le coordinate old fanno già riferimento al genitore corrente, faccio una offset al contrario, così
		// che venga annullata quella all'interno del successivo ciclo for (è un trucco per risparmiare codice ridondante)
		render_subset_unit *oldareaunit = new render_subset_unit(obj->old_pos_x, obj->old_pos_y, obj->old_size_x, obj->old_size_y);
		oldareaunit->offset_position(obj->pos_x*-1, obj->pos_y*-1);
		// devo, ovviamente, resettare le coordinate old di obj, allineandole a quelle correnti
		obj->align_old_coords();
		obj->push_render_unit(oldareaunit);

		// itero tutte le subset unit di obj, le tolgo anche dalla lista
		for(render_subset_unit *objunit=obj->pop_render_unit(); objunit!=0; objunit=obj->pop_render_unit())
		{
			// dopo aver estratto la render unit, visto che devo aggiungerla alla lista di this, devo sistemare
			// i riferimenti sulla posizione, perchè il genitore cambia. Aggiustare i riferimenti serve anche per intersects.
			objunit->offset_position(obj->pos_x, obj->pos_y);

			// itero tutte le subset unit che ho già creato, cioè quelle del gr_object this,
			// con l'obiettivo di trovare render unit di this che si interesecano con esso e avere
			// una lista di render_unit non intersecate tra di loro.
			render_subset_unit *subsetunit=this->units, *prec=this->units;
			while(subsetunit!=0)
			{
				// controllo di intersezione con una render_unit già creata
				if(subsetunit->intersects(objunit))
				{
					//flog(LOG_INFO, "## trovata render_unit %p", subsetunit);

					//aggiorno le coordinate della objunit perchè così posso confrontarla con altre unit che intersecano
					//la objunit originale. In tal modo mi risparmio un ciclo aggiuntivo
					objunit->expand(subsetunit);
					//INOLTRE DOVREI ELIMINARE LA subsetunit, CHE CULO, STO SCORRENDO LA LISTA QUINDI POSSO FARLA O(1)
					if(subsetunit==units)
						units=subsetunit->next;
					else
						prec->next=subsetunit->next;
				}

				//render_subset_unit *temp = subsetunit;
				prec=subsetunit;
				subsetunit=subsetunit->next;

				//delete temp;
			}
			//flog(LOG_INFO, "## inserisco objunit %p in this %p", objunit, this);
			this->push_render_unit(objunit);
		}
	}

	// ============= TEST
	//flog(LOG_INFO, "## stampa su parente %p con x=%d y=%d w=%d h=%d:", this, this->pos_x,this->pos_y, this->size_x, this->size_y);
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		//questo oggetto mi server solo per sfruttare il metodo intesect e nient'altro
		render_subset_unit objintersect(obj->pos_x, obj->pos_y, obj->size_x, obj->size_y);

		for(render_subset_unit *subsetunit=units; subsetunit!=0; subsetunit=subsetunit->next)
		{
			int lmin_x = (subsetunit->pos_x > obj->pos_x) ? subsetunit->pos_x - obj->pos_x : 0;
			int lmin_y = (subsetunit->pos_y > obj->pos_y) ? subsetunit->pos_y - obj->pos_y : 0;
			int lmax_x = (obj->pos_x + obj->size_x > subsetunit->size_x + subsetunit->pos_x) ? subsetunit->size_x + subsetunit->pos_x - obj->pos_x: obj->size_x;
			int lmax_y = (obj->pos_y + obj->size_y > subsetunit->size_y + subsetunit->pos_y) ? subsetunit->size_y + subsetunit->pos_y - obj->pos_y: obj->size_y;
			//int lminpos_x = (subsetunit->pos_x > obj->pos_x) ? subsetunit->pos_x : obj->pos_x;	//RIDONDANTE
			//int lminpos_y = (subsetunit->pos_y > obj->pos_y) ? subsetunit->pos_y : obj->pos_y;	//RIDONDANTE
			int lminpos_x = obj->pos_x + lmin_x;
			int lminpos_y = obj->pos_y + lmin_y;

			//flog(LOG_INFO, "## (1) debug limiti lmin_x=%d lmin_y=%d lmax_x=%d lmax_y=%d:", lmin_x, lmin_y, lmax_x, lmax_y);
			//flog(LOG_INFO, "## (2) stampo obj %p con x=%d y=%d w=%d h=%d", obj, obj->pos_x,obj->pos_y, obj->size_x, obj->size_y);
			//flog(LOG_INFO, "## (3) stampo render_unit %p con x=%d y=%d w=%d h=%d", subsetunit, subsetunit->pos_x,subsetunit->pos_y, subsetunit->size_x, subsetunit->size_y);

			//if(!subsetunit->intersects(&objintersect))
				//flog(LOG_INFO, "# l'oggetto non interseca nulla");

			if(lmax_x<=0 || lmax_y<=0 || lmin_x<0 || lmin_y<0 || lmin_x>lmax_x || lmax_y>lmax_y)
			{
				flog(LOG_INFO, "# oggetto non intersecante oppure max_x/max_y errate");
				continue;
			}

			//if(!obj->trasparency)
				for(int y=0; y<(lmax_y-lmin_y); y++)
					memcpy(this->buffer + lminpos_x + this->size_x*(y+lminpos_y), obj->buffer + lmin_x + (y+lmin_y)*obj->size_x, lmax_x-lmin_x);
					//memset(this->buffer + lminpos_x + this->size_x*(y+lminpos_y), debug_color, lmax_x-lmin_x);
					//memset(this->buffer + subsetunit->pos_x + this->size_x*(y+subsetunit->pos_y), debug_color, lmax_x-lmin_x);

			debug_color+=3;
		}
	}
	// ==================

	//flog(LOG_INFO, "## --- fine render() sperimentale");
	//flog(LOG_INFO, "#");
	return;

	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		if(!(obj->visible))
			continue;

		//flog(LOG_INFO, "## Renderizzo oggetto dalla lista con z-index %d, main container size_x %d", obj->z_index, this->size_x);
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


		//flog(LOG_INFO, "## Terminata renderizzazione oggetto dalla lista con z-index %d", obj->z_index);
	}
}

// ==================================================================
// struct di utilità per l'ottimizzazione dell'algoritmo di rendering
// render_target
gr_object::render_target::render_target(gr_object * newobj)
{
	target=newobj;
}

// render_subset_unit
gr_object::render_subset_unit::render_subset_unit(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y)
{
	this->pos_x = pos_x;
	this->pos_y = pos_y;
	this->size_x = size_x;
	this->size_y = size_y;
	this->copy_list=0;
	this->first_modified_encountered=false;
}

bool gr_object::render_subset_unit::intersects(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y)
{
	if((this->pos_x + this->size_x > pos_x) && (this->pos_x < pos_x + size_x) &&
				(this->pos_y + this->size_y > pos_y) && (this->pos_y < pos_y + size_y))
		return true;

	return false;
}

bool gr_object::render_subset_unit::intersects(render_subset_unit *param)
{
	return intersects(param->pos_x, param->pos_y, param->size_x, param->size_y);
}

void gr_object::render_subset_unit::expand(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y)
{
	//flog(LOG_INFO, "### nuove coordinate/dimensioni this: x=%d, y=%d, w=%d, h=%d", this->pos_x, this->pos_y, this->size_x, this->size_y);
	//flog(LOG_INFO, "### nuove coordinate/dimensioni param: x=%d, y=%d, w=%d, h=%d", pos_x, pos_y, size_x, size_y);
	if(this->pos_x + this->size_x < pos_x + size_x)
			this->size_x += pos_x + size_x - (this->pos_x + this->size_x);
	if(this->pos_x > pos_x)
	{
		this->size_x += this->pos_x - pos_x;
		this->pos_x = pos_x;
	}

	if(this->pos_y + this->size_y < pos_y + size_y)
		this->size_y += pos_y + size_y - (this->pos_y + this->size_y);
	if(this->pos_y > pos_y)
	{
		this->size_y += this->pos_y - pos_y;
		this->pos_y = pos_y;
	}

	//flog(LOG_INFO, "### nuove coordinate/dimensioni risultato render_unit: x=%d, y=%d, w=%d, h=%d", this->pos_x, this->pos_y, this->size_x, this->size_y);
}

void gr_object::render_subset_unit::expand(render_subset_unit *param)
{
	expand(param->pos_x, param->pos_y, param->size_x, param->size_y);
}

void gr_object::render_subset_unit::offset_position(unsigned int parent_pos_x, unsigned int parent_pos_y)
{
	this->pos_x+=parent_pos_x;
	this->pos_y+=parent_pos_y;
}