#ifndef GROBJECT_H
#define GROBJECT_H

#include "consts.h"

// di default la classe rappresenta un container, differisce da altri oggetti per la funzione render()
// che è virtuale e, quindi, ridefinibile da chi eredita questa classe.
class gr_object {
private:
	static int id_counter;
	int id;
	char search_flags;

	gr_object *child_list;
	gr_object *child_list_last;
	gr_object *next_brother;
	gr_object *previous_brother;

protected:
	class render_subset_unit;
	render_subset_unit *units;

	PIXEL_UNIT *buffer;
	int old_pos_x;
	int old_pos_y;
	int old_size_x;
	int old_size_y;
	bool old_visible;
	bool focus_changed;

	int pos_x;
	int pos_y;
	int size_x;
	int size_y;
	int z_index;

	//la trasparenza va indicata esplicitamente, per questioni di ottimizzazione del rendering
	bool trasparency;
	bool visible;

public:
	gr_object(int pos_x, int pos_y, int size_x, int size_y, int z_index, PIXEL_UNIT *predefined_buffer=0);

	//metodi per la gestione dei figli del gr_object
	void add_child(gr_object *child);
	bool remove_child(gr_object *child);
	void focus_child(gr_object *child);

	//metodo per la pulizia delle render units (serve per il framebuffer)
	void clear_render_units();

	int get_pos_x();
	int get_pos_y();
	int get_size_x();
	int get_size_y();
	void set_pos_x(int newval);
	void set_pos_y(int newval);
	virtual void set_size_x(int newval);
	virtual void set_size_y(int newval);
	void set_trasparency(bool newval);
	void set_visibility(bool newval);

	//metodo per ricreare il buffer (necessario dopo il cambio delle dimensioni)
	void realloc_buffer();

	virtual void render();

protected:
	// serve per allineare le coordinate old a quelle correnti, resettare focus_changed e old_visible
	void reset_status();

	// mi restituisce vero se le coordinate sono state modificate dall'ultima chiamata a align_old_coords()
	bool is_pos_modified();
	
	//metodi per la gestione della lista delle render_subset_unit (insiemi di render, algoritmo ottimizzato)
	void push_render_unit(render_subset_unit *newunit);
	render_subset_unit *pop_render_unit();

	// classi di utilità per l'algorimo del pittore ottimizzato
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
		int pos_x;
		int pos_y;
		int size_x;
		int size_y;
		bool first_modified_encountered;
		render_target * copy_list;

		render_subset_unit * next;

		render_subset_unit(int pos_x, int pos_y, int size_x, int size_y);
		bool intersects(int pos_x, int pos_y, int size_x, int size_y);
		bool intersects(render_subset_unit *param);
		void expand(int pos_x, int pos_y, int size_x, int size_y);
		void expand(render_subset_unit *param);
		void offset_position(int parent_pos_x, int parent_pos_y);
		void apply_bounds(int size_x, int size_y);
	};

public:
	// struct di utilità per la ricerca avanzata degli elementi nell'albero
	class search_filter
	{
	public:
		int skip_id;
		natb flags;
		natb parent_flags;
		int padding_x;
		int padding_y;
	};

	class search_result
	{
	public:
		gr_object* target;
		gr_object* target_parent;
	};

	//metodi utili per la gestione degli eventi
	int get_id();
	void set_search_flag(natb flag);
	bool has_flag(natb flag);
	gr_object *search_child_by_id(int id);
	void search_tree(int parent_pos_x, int parent_pos_y, const search_filter& filter, search_result& result);
};

#endif