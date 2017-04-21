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

protected:
	class render_subset_unit;
	render_subset_unit *units;

	PIXEL_UNIT *buffer;
	unsigned int old_pos_x;
	unsigned int old_pos_y;
	unsigned int old_size_x;
	unsigned int old_size_y;

	unsigned int pos_x;
	unsigned int pos_y;
	unsigned int size_x;
	unsigned int size_y;
	unsigned int z_index;

	//la trasparenza va indicata esplicitamene, per questioni di ottimizzazione del rendering
	bool trasparency;
	bool visible;

public:
	gr_object(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y, unsigned int z_index, PIXEL_UNIT *predefined_buffer=0);

	//metodi per la gestione dei figli del gr_object
	void add_child(gr_object *child);
	bool remove_child(gr_object *child);
	void focus_child(gr_object *child);

	//metodo per la pulizia delle render units (serve per il framebuffer)
	void clear_render_units();

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

	// struct di utilità per l'algorimo del pittore ottimizzato
protected:
	// serve per allineare le coordinate old a quelle correnti
	void align_old_coords();
	
	//metodi per la gestione della lista delle render_subset_unit (insiemi di render, algoritmo ottimizzato)
	void push_render_unit(render_subset_unit *newunit);
	render_subset_unit *pop_render_unit();

	class render_target
	{
	public:
		gr_object *target;
		render_target *next;

		render_target(gr_object * newobj);
	};

	class render_subset_unit
	{
	public:
		unsigned pos_x;
		unsigned pos_y;
		unsigned size_x;
		unsigned size_y;
		bool first_modified_encountered;
		render_target * copy_list;

		render_subset_unit * next;

		render_subset_unit(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y);
		bool intersects(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y);
		bool intersects(render_subset_unit *param);
		void expand(unsigned int pos_x, unsigned int pos_y, unsigned int size_x, unsigned int size_y);
		void expand(render_subset_unit *param);
		void offset_position(unsigned int parent_pos_x, unsigned int parent_pos_y);
	};
};

#endif