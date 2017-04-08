#include "gr_object.h"
#include "consts.h"

void inline put_pixel(natb * buffer, int x, int y, int MAX_X, int MAX_Y, natb col)
{
	if(x<MAX_X && y<MAX_Y && x>=0 && y>=0)
		buffer[MAX_X*y+x] = col;
}

gr_object::gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer)
	: child_list(0), child_list_last(0), next_brother(0), previous_brother(0), overlapping_child_list(0), overlapping_next_brother(0),
		pos_x(pos_x), pos_y(pos_y), size_x(size_x), size_y(size_y), z_index(z_index)
{
	if(predefined_buffer==0)
		buffer = new PIXEL_UNIT[size_x*size_y];
	else
		buffer=predefined_buffer;
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
	for(c=child_list; c!=0 && c->z_index < newchild->z_index; c=c->next_brother)
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

//renderizza su buffer tutti i figli nella lista child_tree
void gr_object::render()
{
	for(gr_object *obj=child_list; obj!=0; obj=obj->next_brother)
	{
		//if(!(obj->is_rendered))
			//continue;

		int max_x = (obj->pos_x + obj->size_x > this->size_x) ? this->size_x - obj->pos_x : obj->size_x;
		int max_y = (obj->pos_y + obj->size_y > this->size_y) ? this->size_y - obj->pos_y : obj->size_y;
		if(max_x<=0 || max_y<=0)
			continue;

		for(int i=0; i<max_x; i++)
			for(int j=0; j<max_y; j++)
				put_pixel(this->buffer, obj->pos_x+i, obj->pos_y+j, this->size_x, this->size_y, obj->buffer[j*obj->size_x+i]);

		//controllo bound
		//memcopy...
		/*for(int y=0; y<size_y; y++)
			for(int x=0; x<size_x; x++)
				this->buffer[(x + c->pos_x) + (y + c->pos_y)*this->size_x] = c->buffer[x + y*c->size_x];*/
		//cout << "Renderizzo oggetto dalla lista con z-index " << obj->z_index << " e x=" << obj->pos_x<< endl;
	}
}