#include <iostream>
using namespace std;

#include "gr_object.h"
#include "consts.h"

gr_object::gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer)
	: pos_x(pos_x), pos_y(pos_y), size_x(size_x), size_y(size_y), z_index(z_index),
		child_list(0), child_list_last(0), next_brother(0), previous_brother(0), overlapping_child_list(0), overlapping_next_brother(0)
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
void gr_object::give_focus(gr_object *focuschild)
{
	//lo rimuovo dalla lista
	remove_child(focuschild);

	//lo riaggiungo (viene messo in cima agli altri di pari z-index)
	add_child(focuschild);
}

void gr_object::render()
{
	//renderizza su buffer tutti i figli nella lista child_tree
	for(gr_object *c=child_list; c!=0; c=c->next_brother)
	{
		//controllo bound
		//memcopy...
		cout << "Renderizzo oggetto dalla lista con z-index" << c->z_index << endl;
	}

	for(gr_object *c=child_list_last; c!=0; c=c->previous_brother)
	{
		//controllo bound
		//memcopy...
		cout << "[ inverso ] Renderizzo oggetto dalla lista con z-index" << c->z_index << endl;
	}
}