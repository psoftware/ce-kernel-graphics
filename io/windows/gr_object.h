#ifndef GROBJECT_H
#define GROBJECT_H

#include <iostream>
using namespace std;

#include "consts.h"

// di default la classe rappresenta un container, differisce da altri oggetti per la funzione render()
// che è virtuale e, quindi, ridefinibile da chi eredita questa classe.
class gr_object {
private:
	gr_object *child_list;
	gr_object *child_list_last;
	gr_object *next_brother;
	gr_object *previous_brother;

	gr_object *overlapping_child_list;
	gr_object *overlapping_next_brother;

protected:
	PIXEL_UNIT *buffer;
	
	unsigned int pos_x;
	unsigned int pos_y;
	unsigned int size_x;
	unsigned int size_y;
	unsigned int z_index;

public:
	gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer=0);

	void add_child(gr_object *child);
	bool remove_child(gr_object *child);
	void focus_child(gr_object *child);


	virtual void render();
};

#endif