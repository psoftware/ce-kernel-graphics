#ifndef GROBJECT_H
#define GROBJECT_H

#include <iostream>
using namespace std;

#include "consts.h"

// di default la classe rappresenta un container, differisce da altri oggetti per la funzione render()
// che è virtuale e, quindi, ridefinibile da chi eredita questa classe.
class gr_object {
	gr_object *child_list;
	gr_object *next_brother;

	unsigned int pos_x;
	unsigned int pos_y;
	unsigned int size_x;
	unsigned int size_y;
	unsigned int z_index;

	PIXEL_UNIT *buffer;

	gr_object *overlapping_child_list;
	gr_object *overlapping_next_brother;

public:
	gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer=0);

	void add_child(gr_object *child);
	bool remove_child(gr_object *child);

	virtual void render();
};

#endif