#include <iostream>
using namespace std;

#include "gr_object.h"
#include "consts.h"

gr_object::gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer)
: pos_x(pos_x), pos_y(pos_y), size_x(size_x), size_y(size_y), z_index(z_index)
{
	child_list=0;
	next_brother=0;
	overlapping_child_list=0;
	overlapping_next_brother=0;

	if(predefined_buffer==0)
		buffer = new PIXEL_UNIT[size_x*size_y];
	else
		buffer=predefined_buffer;
}

//bisogna farlo per z-index
void gr_object::add_child(gr_object *child)
{
	if(child_list==0)
	{
		child_list=child;
		return;
	}

	gr_object *c;
	for(c=child_list; c!=0 && c->next_brother!=0; c=c->next_brother);

	c->next_brother = child;
}

bool gr_object::remove_child(gr_object *child)
{
	gr_object *c,*p=0;
	for(c=child_list; c!=child; c=c->next_brother)
		p=c;

	if(c==0)
		return false;

	if(p==child_list)
		child_list=0;
	else
	{
		p->next_brother=c->next_brother;
		delete c;
	}
}

void gr_object::render()
{
	//renderizza su buffer tutti i figli nella lista child_tree
	for(gr_object *c=child_list; c!=0; c=c->next_brother)
	{
		//controllo bound
		//memcopy...
		cout << "Renderizzo oggetto dalla lista " << (void*)c << endl;
	}
}