#include "libce_guard.h"
#include "libgr.h"
#include "gr_object.h"
#include "consts.h"
#include "log.h"

int gr_object::id_counter = 0;

gr_object::gr_object(int pos_x, int pos_y, int size_x, int size_y, int z_index, PIXEL_UNIT *predefined_buffer)
	: search_flags(0), child_list(0), child_list_last(0), next_brother(0), previous_brother(0), units(0),
		old_pos_x(pos_x), old_pos_y(pos_y), old_size_x(size_x), old_size_y(size_y), old_visible(false), focus_changed(false),
		pos_x(pos_x), pos_y(pos_y), size_x(size_x), size_y(size_y), z_index(z_index), trasparency(false), visible(true)
{
	if(predefined_buffer==0)
		buffer = new PIXEL_UNIT[size_x*size_y];
	else
		buffer=predefined_buffer;

	this->id = id_counter++;
	LOG_DEBUG("Nuovo gr_object (%d) o derivato con size_x %d e this->size_x %d", this->id, size_x, this->size_x);
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

	// se l'oggetto viene eliminato deve essere eliminato anche dal buffer parente,
	// quindi procediamo ad aggiungere una render_unit adatta al parente stesso:
	// l'operazione di redraw avverrà alla chiamata della render() sul parente.
	// facciamo sempre riferimento alle coordinate old perchè mantengono lo stato
	// dell'oggetto all'ultima chiamata della render().
	// ci interessa creare la render_unit solo nel caso in cui l'oggetto è visibile

	if(removechild->old_visible)
		this->push_render_unit(new render_subset_unit(removechild->pos_x, removechild->pos_y, removechild->size_x, removechild->size_y));

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

	// è meglio che lo faccia l'utilizzatore della libreria perchè non è detto che l'istanza stia in memoria dinamica
	//delete removechild;

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

	//forziamo il redraw nella prossima render
	focus_changed = true;
}

bool gr_object::has_child(gr_object *child)
{
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
		if(child == obj)
			return true;

	return false;
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
	while(this->units)
	{
		render_subset_unit * tempnext = this->units->next;
		delete this->units;
		this->units = tempnext;
	}
}

int gr_object::get_pos_x(){
	return this->pos_x;
}
int gr_object::get_pos_y(){
	return this->pos_y;
}
int gr_object::get_size_x(){
	return this->size_x;
}
int gr_object::get_size_y(){
	return this->size_y;
}
void gr_object::set_pos_x(int newval){
	this->pos_x=newval;
}
void gr_object::set_pos_y(int newval){
	this->pos_y=newval;
}
void gr_object::set_size_x(int newval){
	this->size_x=newval;
}
void gr_object::set_size_y(int newval){
	this->size_y=newval;
}
void gr_object::set_trasparency(bool newval){
	this->trasparency=newval;
}
void gr_object::set_visibility(bool newval){
	this->visible=newval;
}

// gestione degli eventi
int gr_object::get_id(){
	return this->id;
}

void gr_object::set_search_flag(natb flag){
	this->search_flags |= flag;
}

bool gr_object::has_flag(natb flag){
	return this->search_flags & flag;
}

gr_object *gr_object::search_child_by_id(int id){
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
		if(obj->get_id()==id)
			return obj;

	return 0;
}

void gr_object::search_tree(int parent_pos_x, int parent_pos_y, const gr_object::search_filter& filter, gr_object::search_result& result)
{
	// scorriamo la lista in modo inverso perchè dobbiamo dare priorità agli oggetti con z-index più alto
	for(gr_object *obj=child_list_last; obj!=0; obj=obj->previous_brother)
		if((parent_pos_x >= (obj->pos_x-filter.padding_x) && parent_pos_x < (obj->pos_x + obj->size_x + filter.padding_x))
		&& (parent_pos_y >= (obj->pos_y-filter.padding_y) && parent_pos_y < (obj->pos_y + obj->size_y + filter.padding_y)) && obj->id!=filter.skip_id)
		{
			search_result child_res;
			obj->search_tree(parent_pos_x - obj->pos_x, parent_pos_y - obj->pos_y, filter, child_res);
			result = child_res;

			if(child_res.target_parent==0 && (this->search_flags & filter.parent_flags) == filter.parent_flags)
				result.target_parent=this;

			return;
		}

	// controlliamo se la foglia possiede i flag impostati nel filtro
	if((this->search_flags & filter.flags) != filter.flags)
	{
		result.target = 0;
		return;
	}

	result.target = this;
	result.target_parent = 0;
	return;
}

void gr_object::realloc_buffer(){
	delete buffer;
	buffer = new PIXEL_UNIT[this->size_x*this->size_y];
}

void gr_object::reset_status(){
	this->old_pos_x = this->pos_x;
	this->old_pos_y = this->pos_y;
	this->old_size_x = this->size_x;
	this->old_size_y = this->size_y;
	this->old_visible = this->visible;
	this->focus_changed = false;
}

bool gr_object::is_pos_modified(){
	return (this->old_pos_x != this->pos_x) || (this->old_pos_y != this->pos_y) || (this->old_size_x != this->size_x) ||
	(this->old_size_y != this->size_y) || (this->old_visible != this->visible);
}

//renderizza su buffer tutti i figli nella lista child_tree
void gr_object::render()
{
	static natb debug_color = 0x20;
	//flog(LOG_INFO, "## --- inizio render()");

	//per ogni oggetto (obj) del contenitore
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		//flog(LOG_INFO, "   nuovo oggetto x=%d y=%d w=%d h=%d:", obj->pos_x,obj->pos_y, obj->size_x, obj->size_y);
		//render_target *newtarget = new render_target(obj);

		// devo capire se l'oggetto è stato spostato di posizione o cambiato di dimensione: in tal caso devo
		// creare una render_unit che corrisponda esattamente alla dimensione e posizione della finestra in quanto
		// l'oggetto deve essere renderizzato sulla nuova area.
		// è inclusa un'ulteriore ottimizzazione in questo caso: se creo una render_unit grande quanto l'oggetto
		// obj, è inutile scorrere le ulteriori render_unit dell'oggetto in quanto sono tutte contenuta nella
		// render_unit appena creata. Fa esclusione la render_unit "oldareaunit" (vedi dopo) perchè può anche
		// sforare i bound dell'oggetto (unica eccezione ammessa in generale).

		// potrei già inizializzare le render unit qui, ma è meglio fare meno new possibili
		render_subset_unit *newareaunit = 0;
		render_subset_unit *oldareaunit = 0;
		bool modified = obj->is_pos_modified();

		if(modified || (!obj->old_visible && obj->visible) || focus_changed)
		{
			newareaunit = new render_subset_unit(0, 0, obj->size_x, obj->size_y);

			// cancello tutte le render_unit dell'oggetto (OTTIMIZZAZIONE)
			obj->clear_render_units();
		}

		// controllo di presenza della VECCHIA POSIZIONE/DIMENSIONE (AREA PRECEDENTEMENTE OCCUPATA):
		if(modified || (obj->old_visible && !obj->visible)) // questa operazione va fatta solo se la scia era visibile prima del render
		{
			oldareaunit = new render_subset_unit(obj->old_pos_x, obj->old_pos_y, obj->old_size_x, obj->old_size_y);

			// visto che le coordinate old fanno già riferimento al genitore corrente, faccio una offset al contrario, così
			// che venga annullata quella all'interno del successivo ciclo for (è un trucco per risparmiare codice ridondante)
			oldareaunit->offset_position(obj->pos_x*-1, obj->pos_y*-1);
		}

		// resettiamo lo stato delle coordinate old e assegnamogli quelle correnti. anche la visibilità old e focus sono coinvolti
		obj->reset_status();

		// la vecchia area potrebbe intersecarsi con la nuova, quindi magari è meglio farne una che contiene entrambi in tal caso
		if(newareaunit && oldareaunit && newareaunit->intersects(oldareaunit))
		{
			newareaunit->expand(oldareaunit);
			obj->push_render_unit(newareaunit);
			delete oldareaunit;
		}
		else // se, invece, le due aree non si intersecano, allora le tengo separate
		{
			if(oldareaunit)
				obj->push_render_unit(oldareaunit);
			if(newareaunit)
				obj->push_render_unit(newareaunit);
		}

		// itero tutte le subset unit di obj, le tolgo anche dalla lista
		for(render_subset_unit *objunit=obj->pop_render_unit(); objunit!=0; objunit=obj->pop_render_unit())
		{
			// se l'oggetto non è visibile, allora nessuna render unit dovrà essere applicata
			if(!obj->visible)
				continue;

			// dopo aver estratto la render unit, visto che devo aggiungerla alla lista di this, devo sistemare
			// i riferimenti sulla posizione, perchè il genitore cambia. Aggiustare i riferimenti serve anche per intersects.
			objunit->offset_position(obj->pos_x, obj->pos_y);

			// la unit non deve sforare i bound dell'oggetto parente, quindi dopo aver cambiato i riferimenti lo restringo, se necessario
			objunit->apply_bounds(this->size_x, this->size_y);

			// itero tutte le subset unit che ho già creato, cioè quelle del gr_object this,
			// con l'obiettivo di trovare render unit di this che si interesecano con esso e avere
			// una lista di render_unit non intersecate tra di loro.
			render_subset_unit *subsetunit=this->units, *prec=this->units;
			while(subsetunit!=0)
			{
				// mantengo il puntatore a next della subset unit perchè potrei eliminare subsetunit prima della fine del ciclo
				render_subset_unit *tempnext = subsetunit->next;

				// controllo di intersezione con una render_unit già creata
				if(subsetunit->intersects(objunit))
				{
					// aggiorno le coordinate della objunit perchè così posso confrontarla con altre unit che intersecano
					// la objunit originale. In tal modo mi risparmio un ciclo aggiuntivo
					objunit->expand(subsetunit);

					// elimino la render_unit di this che si interseca, perchè ora è contenuta totalmente in "subsetunit"
					if(subsetunit==this->units)	//testa della lista
					{
						this->units=subsetunit->next;
						prec=this->units;
					}
					else
						prec->next=subsetunit->next;

					delete subsetunit;
				}
				else
					prec = subsetunit;
				subsetunit=tempnext;
			}
			//flog(LOG_INFO, "## inserisco objunit %p in this %p", objunit, this);
			this->push_render_unit(objunit);
		}
	}

	// === Inizio fase di stampa ===
	//flog(LOG_INFO, "## stampa su parente %p con x=%d y=%d w=%d h=%d:", this, this->pos_x,this->pos_y, this->size_x, this->size_y);
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		// se l'oggetto non è visibile, questo non deve essere visualizzato. La scia è gestita nel blocco precedente
		if(!obj->visible)
			continue;

		for(render_subset_unit *subsetunit=units; subsetunit!=0; subsetunit=subsetunit->next)
		{
			//flog(LOG_INFO, "# render unit (%p) %d %d %d %d", subsetunit, subsetunit->pos_x, subsetunit->pos_y, subsetunit->size_x, subsetunit->size_y);
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

			//controllo se l'oggetto si interseca con questa render unit
			if(lmax_x<=0 || lmax_y<=0 || lmin_x<0 || lmin_y<0 || lmin_x>lmax_x || lmax_y>lmax_y)
			{
				//flog(LOG_INFO, "# oggetto non intersecante oppure max_x/max_y errate");
				continue;
			}

			if(!obj->trasparency)
				for(int y=0; y<(lmax_y-lmin_y); y++)
					gr_memcpy(this->buffer + lminpos_x + this->size_x*(y+lminpos_y), obj->buffer + lmin_x + (y+lmin_y)*obj->size_x, lmax_x-lmin_x);
					//memset(this->buffer + lminpos_x + this->size_x*(y+lminpos_y), debug_color, lmax_x-lmin_x);
					//memset(this->buffer + subsetunit->pos_x + this->size_x*(y+subsetunit->pos_y), debug_color, lmax_x-lmin_x);
			else
				for(int y=0; y<(lmax_y-lmin_y); y++)
					for(int x=0; x<(lmax_x-lmin_x); x++)
				#if defined(BPP_8)
						if(*(obj->buffer + lmin_x + x + (y+lmin_y)*obj->size_x) != 0x03)
							set_pixel(this->buffer, x+lminpos_x, y+lminpos_y, this->size_x, this->size_y, *(obj->buffer + lmin_x + x + (y+lmin_y)*obj->size_x));
				#elif defined(BPP_32)
						{	// algoritmo per realizzare l'alpha blending
							PIXEL_UNIT src_pixel = *(obj->buffer + lmin_x + x + (y+lmin_y)*obj->size_x);
							PIXEL_UNIT dst_pixel = *(this->buffer + lminpos_x + x + (y+lminpos_y)*this->size_x); //?
							natb alpha = src_pixel >> 24;
							natb src_red = (src_pixel >> 16) & 0xff;
							natb src_green = (src_pixel >> 8) & 0xff;
							natb src_blue = src_pixel & 0xff;
							natb dst_red = (dst_pixel >> 16) & 0xff;
							natb dst_green = (dst_pixel >> 8) & 0xff;
							natb dst_blue = dst_pixel & 0xff;
							dst_red = (alpha*src_red + (255-alpha)*dst_red) / 255;
							dst_green = (alpha*src_green + (255-alpha)*dst_green) / 255;
							dst_blue = (alpha*src_blue + (255-alpha)*dst_blue) / 255;
							*(this->buffer + lminpos_x + x + (y+lminpos_y)*this->size_x) = (dst_pixel & 0xff000000) | (dst_red << 16) | (dst_green << 8) | dst_blue;

							/* //alpha blending debug code
							if(alpha!=0 && alpha!=0xff && false)
							{
								flog(LOG_INFO, "src %p dst %p newdest %p", src_pixel, dst_pixel, *(this->buffer + lminpos_x + x + (y+lminpos_y)*this->size_x));
								flog(LOG_INFO, "alpha %d src_red %d src_green %d src_blue %d", alpha, src_red, src_green, src_blue);
								flog(LOG_INFO, "PRE  dst_red %d dst_green %d dst_blue %d", (dst_pixel >> 16) & 0xff, (dst_pixel >> 8) & 0xff, dst_pixel & 0xff);
								flog(LOG_INFO, "POST dst_red %d dst_green %d dst_blue %d", dst_red, dst_green, dst_blue);
							}*/
						}
				#endif

			debug_color+=3;
		}
	}

	//flog(LOG_INFO, "## --- fine render() sperimentale");
	//flog(LOG_INFO, "#");
	
/*
	// ========= ALGORITMO VECCHIO (ALGORITMO DEL PITTORE)
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
*/
}

// distruttore: devo cancellare tutte le render_unit
gr_object::~gr_object(){
	this->clear_render_units();
}

// ==================================================================
// struct di utilità per l'ottimizzazione dell'algoritmo di rendering
// render_target
gr_object::render_target::render_target(gr_object * newobj)
{
	target=newobj;
}

// render_subset_unit
gr_object::render_subset_unit::render_subset_unit(int pos_x, int pos_y, int size_x, int size_y)
{
	this->pos_x = pos_x;
	this->pos_y = pos_y;
	this->size_x = size_x;
	this->size_y = size_y;
	this->copy_list=0;
	this->first_modified_encountered=false;
}

bool gr_object::render_subset_unit::intersects(int pos_x, int pos_y, int size_x, int size_y)
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

void gr_object::render_subset_unit::expand(int pos_x, int pos_y, int size_x, int size_y)
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

void gr_object::render_subset_unit::offset_position(int parent_pos_x, int parent_pos_y)
{
	this->pos_x+=parent_pos_x;
	this->pos_y+=parent_pos_y;
}

void gr_object::render_subset_unit::apply_bounds(int size_x, int size_y)
{
	if(this->pos_x < 0)
	{
		this->pos_x = 0;
		this->size_x -= this->pos_x;
	}
	if(this->pos_x + this->size_x > size_x)
		this->size_x -= this->pos_x + this->size_x - size_x;

	if(this->pos_y < 0)
	{
		this->pos_y = 0;
		this->size_y -= this->pos_y;
	}
	if(this->pos_y + this->size_y > size_y)
		this->size_y -= this->pos_y + this->size_y - size_y;
}