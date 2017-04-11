#ifndef GROBJECT_H
#define GROBJECT_H

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
	bool trasparency;
	bool visible;

public:
	gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer=0);

	void add_child(gr_object *child);
	bool remove_child(gr_object *child);
	void focus_child(gr_object *child);

	unsigned int get_pos_x();
	unsigned int get_pos_y();
	unsigned int get_size_x();
	unsigned int get_size_y();
	void set_pos_x(unsigned int newval);
	void set_pos_y(unsigned int newval);
	void set_size_x(unsigned int newval);
	void set_size_y(unsigned int newval);
	void set_trasparency(bool newval);
	void set_visibility(bool newval);

	virtual void render();
};

#endif