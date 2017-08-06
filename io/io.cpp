// io.cpp
//
#include "costanti.h"
#include "libce_guard.h"
#include "newdelete.h"
#include "windows/cursor.h"
#include "windows/h_resize_cursor.h"
#include "log.h"

//#define BOCHS
////////////////////////////////////////////////////////////////////////////////
//    COSTANTI                                                                //
////////////////////////////////////////////////////////////////////////////////


const natl PRIO = 1000;
const natl LIV = LIV_SISTEMA;


////////////////////////////////////////////////////////////////////////////////
//                        CHIAMATE DI SISTEMA USATE                           //
////////////////////////////////////////////////////////////////////////////////

extern "C" natl activate_p(void f(int), int a, natl prio, natl liv);
extern "C" natl activate_pe(void f(int), int a, natl prio, natl liv, natb type);
extern "C" void terminate_p();
extern "C" void sem_wait(natl sem);
extern "C" void sem_signal(natl sem);
extern "C" natl sem_ini(int val);
extern "C" void wfi();	//
extern "C" void abort_p();
extern "C" void log(log_sev sev, const char* buf, int quanti);
extern "C" addr trasforma(addr ff);
extern "C" void panic(const char *msg);

void *mem_alloc(natl dim);
void mem_free(void *p);

////////////////////////////////////////////////////////////////////////////////
//                    GESTIONE DELLE INTERFACCE SERIALI                 //
////////////////////////////////////////////////////////////////////////////////

enum funz { input_n, input_ln, output_n, output_0 };  //

struct interfse_reg {	//
	ioaddr iRBR, iTHR, iLSR, iIER, iIIR;
};

struct des_se {		//
	interfse_reg indreg;
	natl mutex;
	natl sincr;
	natl cont;
	addr punt;
	funz funzione;
	natb stato;
};

const natl S = 2;
des_se com[S] = {
	{	// com[0]
		{	// indreg
			0x03f8,	// iRBR
			0x03f8,	// iTHR
			0x03fd,	// iLSR
			0x03f9,	// iIER
			0x03fa,	// iIIR
		},
		0,	// mutex
		0,	// sincr
		0,	// cont
		0,	// punt
		(funz)0,// funzione
		0,	// stato
	},
	{	// com[0]
		{	// indreg
			0x02f8,	// iRBR
			0x02f8,	// iTHR
			0x03fd,	// iLSR
			0x02f9,	// iIER
			0x02fa,	// iIIR
		},
		0,	// mutex
		0,	// sincr
		0,	// cont
		0,	// punt
		(funz)0,// funzione
		0,	// stato
	}
};

void input_com(des_se* p_des);	//
void output_com(des_se* p_des);	//
void estern_com(int i) //
{
	natb r;
	des_se *p_des;
	p_des = &com[i];
	for(;;) {
		inputb(p_des->indreg.iIIR, r);
		if ((r&0x06) == 0x04)
			input_com(p_des);
		else if ((r&0x06) == 0x02)
			output_com(p_des);
		wfi();
	}
}

void startse_in(des_se *p_des, natb vetti[], natl quanti, funz op); // [9.2.1]
extern "C" void c_readse_n(natl serial, natb vetti[], natl quanti, natb& errore) // [9.2.1]
{
	des_se *p_des;

	// (* le primitive non devono mai fidarsi dei parametri
	if (serial >= S) {
		flog(LOG_WARN, "readse_n con serial=%d", serial);
		abort_p();
	}
	// *)

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, quanti, input_n);
	sem_wait(p_des->sincr);
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

extern "C" void c_readse_ln(natl serial, natb vetti[], int& quanti, natb& errore)
{
	des_se *p_des;

	// (* le primitive non devono mai fidarsi dei parametri
	if (serial >= S) {
		flog(LOG_WARN, "readse_ln con serial=%d", serial);
		abort_p();
	}
	// *)

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, 80, input_ln);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

extern "C" void go_inputse(ioaddr i_ctr);
void startse_in(des_se *p_des, natb vetti[], natl quanti, funz op) // [9.2.1]
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->funzione = op;
	go_inputse(p_des->indreg.iIER);
}

extern "C" void halt_inputse(ioaddr i_ctr);
void input_com(des_se *p_des) // [9.2.1]
{
	natb c; bool fine;
	fine = false;

	halt_inputse(p_des->indreg.iIER);

	inputb(p_des->indreg.iLSR, c);

	p_des->stato = c & 0x1e;
	if (p_des->stato != 0)
		fine = true;
	else {
		inputb(p_des->indreg.iRBR, c);
		if (p_des->funzione == input_n) {
			*static_cast<natb*>(p_des->punt) = c; // memorizzazione
			p_des->punt = static_cast<natb*>(p_des->punt) + 1;
			p_des->cont--;
			if(p_des->cont == 0)
				fine = true;
		} else {
			if ( (p_des->funzione == input_ln) ) {
				if(c == '\r' || c == '\n') {
					fine = true;
					p_des->cont = 80 - p_des->cont;
				} else {
					*static_cast<natb*>(p_des->punt) = c; // memorizzazione
					p_des->punt = static_cast<natb*>(p_des->punt) + 1;
					p_des->cont--;
					if (p_des->cont == 0) {
						fine = true;
						p_des->cont = 80;
					}
				}
			}
		}
	}

	if(fine == true) {
		*static_cast<natb*>(p_des->punt) = 0;	// carattere nullo
		sem_signal(p_des->sincr);
	} else
		go_inputse(p_des->indreg.iIER);
}

void startse_out(des_se *p_des, natb vetto[], natl quanti, funz op);
extern "C" void c_writese_n(natl serial, natb vetto[], natl quanti)	// [9.2.2]
{
	des_se *p_des;

	// (* le primitive non devono mai fidarsi dei parametri
	if (serial >= S) {
		flog(LOG_WARN, "writese_n con serial=%d", serial);
		abort_p();
	}
	// *)

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, quanti, output_n);
	sem_wait(p_des->sincr);
	sem_signal(p_des->mutex);
}

extern "C" void c_writese_0(natl serial, natb vetto[], natl &quanti)
{
	des_se *p_des;

	// (* le primitive non devono mai fidarsi dei parametri
	if (serial >= S) {
		flog(LOG_WARN, "writese_0 con serial=%d", serial);
		abort_p();
	}
	// *)

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, 0, output_0);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	sem_signal(p_des->mutex);
}

extern "C" void go_outputse(ioaddr i_ctr);
void startse_out(des_se *p_des, natb vetto[], natl quanti, funz op) // [9.2.2]
{
	p_des->cont = quanti;
	p_des->punt = vetto;
	p_des->funzione = op;
	go_outputse(p_des->indreg.iIER);
	output_com(p_des);
}

extern "C" void halt_outputse(ioaddr i_ctr);
void output_com(des_se *p_des)	// [9.2.2]
{
	natb c; bool fine;
        fine = false;

	if (p_des->funzione == output_n) {
		p_des->cont--;
		if(p_des->cont == 0) {
			fine = true;
			halt_outputse(p_des->indreg.iIER);
		}
		c = *static_cast<natb*>(p_des->punt); //prelievo
		outputb(c, p_des->indreg.iTHR);
		p_des->punt = static_cast<natb*>(p_des->punt) + 1;
	} else if (p_des->funzione == output_0) {
		c = *static_cast<natb*>(p_des->punt); //prelievo
		if (c == 0) {
			fine = true;
			halt_outputse(p_des->indreg.iIER);
		} else {
			outputb(c, p_des->indreg.iTHR);
			p_des->cont++;
			p_des->punt = static_cast<natb*>(p_des->punt) + 1;
		}
	}

	if (fine == true)
		sem_signal(p_des->sincr);

}

// ( inizializzazione delle interfacce seriali
extern "C" void com_setup(void);	// vedi "io.S"
// interruzioni hardware delle interfacce seriali
int com_irq[S] = { 4, 3 };

bool com_init()
{
	des_se *p_des;
	natl id;
	natl i, com_base_prio = PRIO;

	com_setup();

	for(i = 0; i < S; ++i) {
		p_des = &com[i];

		if ( (p_des->mutex = sem_ini(1)) == 0xFFFFFFFF) {
			flog(LOG_ERR, "com: impossibile creare mutex");
			return false;
		}
		if ( (p_des->sincr = sem_ini(0)) == 0xFFFFFFFF) {
			flog(LOG_ERR, "com: impossibile creare sincr");
			return false;
		}

		id = activate_pe(estern_com, i, com_base_prio - i, LIV, com_irq[i]);
		if (id == 0xFFFFFFFF) {
			flog(LOG_ERR, "com: impossibile creare proc. esterno");
			return false;
		}

	}
	flog(LOG_INFO, "com: inizializzate %d seriali", S);
	return true;
}
// )

////////////////////////////////////////////////////////////////////////////////
//                        SCHEDA VIDEO (STDVGA)                               //
////////////////////////////////////////////////////////////////////////////////

#include "windows/consts.h"

struct stdvga {
	PIXEL_UNIT* framebuffer;
	natw * vgareg;
	natw * vbeext;

	//indici per vbeext
	static const natw VBE_DISPI_INDEX_ID = 0x0;
	static const natw VBE_DISPI_INDEX_XRES = 0x1;
	static const natw VBE_DISPI_INDEX_YRES = 0x2;
	static const natw VBE_DISPI_INDEX_BPP = 0x3;
	static const natw VBE_DISPI_INDEX_ENABLE = 0x4;
	static const natw VBE_DISPI_INDEX_BANK = 0x5;
	static const natw VBE_DISPI_INDEX_VIRT_WIDTH = 0x6;
	static const natw VBE_DISPI_INDEX_VIRT_HEIGHT = 0x7;
	static const natw VBE_DISPI_INDEX_X_OFFSET = 0x8;
	static const natw VBE_DISPI_INDEX_Y_OFFSET = 0x9;

	//bit settabili su VBE_DISPI_INDEX_ENABLE
	static const natw VBE_DISPI_DISABLED = 0x00;
	static const natw VBE_DISPI_ENABLED = 0x01;
	static const natw VBE_DISPI_GETCAPS = 0x02;
	static const natw VBE_DISPI_8BIT_DAC = 0x20;
	static const natw VBE_DISPI_LFB_ENABLED = 0x40;
	static const natw VBE_DISPI_NOCLEARMEM =0x80;
};


const int MAX_SCREENX = 1024;
const int MAX_SCREENY = 768;

stdvga main_videocard;

bool bochsvga_init()
{
	natb bus = 0, dev = 0, fun = 0;
	natw deviceID = 0x1234;
	natw vendorID = 0x1111;

	//cerchiamo la scheda video basandoci sul deviceID e vendorID
	if(!pci_find_dev(bus, dev, fun, vendorID, deviceID))
	{
		flog(LOG_WARN, "bochsvga: scheda video non rilevata");
		return false;
	}

	//indirizzo di memoria del framebuffer
	main_videocard.framebuffer = reinterpret_cast<PIXEL_UNIT*>(pci_read_confl(bus, dev, fun, 0x10) & ~0xf);
	//indirizzo dell'area di memoria in cui sono mappati i registri, 1:1, del controller VGA (non usati)
	main_videocard.vgareg = reinterpret_cast<natw*>((pci_read_confl(bus, dev, fun, 0x18) & ~0xf) + 0x0400);
	//indirizzo dell'area di memoria in cui sono mappati i registri, 2:2, delle estensioni BOCHS VBE
	main_videocard.vbeext = reinterpret_cast<natw*>((pci_read_confl(bus, dev, fun, 0x18) & ~0xf) + 0x0500);
	flog(LOG_INFO, "bochsvga: framebuffer su %p e vbext su %p", main_videocard.framebuffer, main_videocard.vbeext);

	//impostiamo risoluzione e profondità di colore usando le api BOCHS VBE
	main_videocard.vbeext[stdvga::VBE_DISPI_INDEX_ENABLE] = stdvga::VBE_DISPI_DISABLED;
	main_videocard.vbeext[stdvga::VBE_DISPI_INDEX_XRES] = MAX_SCREENX;
	main_videocard.vbeext[stdvga::VBE_DISPI_INDEX_YRES] = MAX_SCREENY;
	main_videocard.vbeext[stdvga::VBE_DISPI_INDEX_BPP] = VBE_DISPI_BPP;
	main_videocard.vbeext[stdvga::VBE_DISPI_INDEX_ENABLE] = stdvga::VBE_DISPI_ENABLED | stdvga::VBE_DISPI_LFB_ENABLED;

	return true;
}


////////////////////////////////////////////////////////////////////////////////
//                         GESTIONE FINESTRE                                  //
////////////////////////////////////////////////////////////////////////////////

#include "windows/libgr.h"
#include "windows/libfont.h"
#include "windows/gr_object.h"
#include "windows/gr_bitmap.h"
#include "windows/gr_button.h"
#include "windows/gr_label.h"
#include "windows/gr_window.h"
#include "windows/libtga.h"
#include "windows/resources/resources.h"

// il framebuffer container è collegato al framebuffer
gr_object *framebuffer_container;
// questo oggetto contiene tutti i figli della finestra ed è usato per
// implementare la tecnica del double buffering
gr_object *doubled_framebuffer_container;

// variabili per la gestione della bitmap del cursore
int bitmap_click_offset_x;
int bitmap_click_offset_y;
const void *current_bitmap;
gr_bitmap * mouse_bitmap;

// costanti utilizzate come tipi per i comandi aggiunti nella coda del gestore
// delle finestre
const natb PRIM_SHOW=0;
const natb PRIM_CLOSE_WIND=1;
const natb PRIM_UPDATE_OBJECT=2;
const natb PRIM_TIME_TICK=3;
const natb MOUSE_UPDATE_EVENT=10;
const natb MOUSE_Z_UPDATE_EVENT=11;
const natb MOUSE_MOUSEUP_EVENT=12;
const natb MOUSE_MOUSEDOWN_EVENT=13;
const natb KEYBOARD_KEYPRESS_EVENT=14;

struct des_window_req
{
	gr_window *window;
	natb p_id;

	//Sincronizzazione primitiva se richiesta
	bool to_sync;
	natb if_sync;

	natb act;
	union {
		u_windowObject *obj;	//aggiorna_oggetto
		int delta_x;			//mousebutton
		int delta_z;			//mousez
		mouse_button button;	//mousebutton
		char key;				//tastiera
	};

	union {
		int delta_y;			//mousebuton
	};

	des_window_req()
	{
		to_sync = false;
		if_sync = sem_ini(0);
	}
};

// questa struttura contiene le variabili necessarie al gestore delle finestre
// per funzionare
const natb MAX_REQ_QUEUE=3;
struct des_windows_man
{
	//Elementi per la gestione del focus su finestre e oggetti
	gr_window *focused_window;
	gr_object *dragging_border;

	//Variabili per gestire lo stato di resize o trascinamento
	bool is_dragging;
	bool is_resizing;

	//Coda richieste primitive
	des_window_req req_queue[MAX_REQ_QUEUE];
	natb top;
	natb rear;
	
	//Semafori di sincronizzazione
	natb mutex;
	natb sync_notempty;
	natb sync_notfull;
};

// istanza globale del gestore delle finestre
des_windows_man win_man;

// ======= Funzioni per l'inserimento di comandi nella coda del gestore delle finestre
int windows_queue_insert(des_windows_man& win_cont, gr_window *window, natb p_id, natb act, bool sync)
{
	//Controllo Coda piena
	if (win_cont.top == ((win_cont.rear - 1 + MAX_REQ_QUEUE) % MAX_REQ_QUEUE))
		return -1;

	natb resindex = win_cont.top;
	win_cont.req_queue[win_cont.top].window = window;
	win_cont.req_queue[win_cont.top].p_id = p_id;
	win_cont.req_queue[win_cont.top].act = act;
	win_cont.req_queue[win_cont.top].to_sync = sync;
	win_cont.top = (win_cont.top + 1) % MAX_REQ_QUEUE;

	return resindex; // No errors
}

bool windows_queue_extract(des_windows_man& win_cont, des_window_req& req)
{
	if (win_cont.top == win_cont.rear)
		return false;

	req = win_cont.req_queue[win_cont.rear];
	win_cont.rear = (win_cont.rear + 1) % MAX_REQ_QUEUE;

	return true;
}

// ======= Primitive messe a disposizione dell'utente =======
extern "C" int c_crea_finestra(unsigned int size_x, unsigned int size_y, unsigned int pos_x, unsigned int pos_y)
{
	sem_wait(win_man.mutex);

	gr_window *newwindow = new gr_window(pos_x, pos_y, size_x, size_y, 0);
	doubled_framebuffer_container->add_child(newwindow);
	int newwindow_id = newwindow->get_id();
	sem_signal(win_man.mutex);

	return newwindow_id;
}

extern "C" int c_chiudi_finestra(int w_id, bool sync)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	gr_window *window;
	int new_index;

	gr_object * found_obj = doubled_framebuffer_container->search_child_by_id(w_id);
	if(found_obj==0 || !found_obj->has_flag(gr_window::WINDOW_FLAG))
		goto err;

	window = static_cast<gr_window*>(found_obj);

	LOG_DEBUG("Inserimento richiesta di chiusura finestra");

	new_index = windows_queue_insert(win_man, window, 100, PRIM_CLOSE_WIND, sync);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		goto err;
	}

	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
	if(sync)
		sem_wait(win_man.req_queue[new_index].if_sync);

	return 0;

//Gestione errori (sblocco mutex e sync su array)
err:	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notfull);
	return -1;
}

extern "C" void c_visualizza_finestra(int w_id, bool sync)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	gr_window *window;
	int new_index;

	gr_object * found_obj = doubled_framebuffer_container->search_child_by_id(w_id);
	if(found_obj==0 || !found_obj->has_flag(gr_window::WINDOW_FLAG))
		goto err;

	window = static_cast<gr_window*>(found_obj);

	LOG_DEBUG("Inserimento richiesta di renderizzazione finestra");

	new_index = windows_queue_insert(win_man, window, 100, PRIM_SHOW, sync);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		goto err;
	}

	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
	if(sync)
		sem_wait(win_man.req_queue[new_index].if_sync);

	return;

//Gestione errori (sblocco mutex e sync su array)
err:	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notfull);
}

extern "C" int c_crea_oggetto(int w_id, u_windowObject * u_obj)
{
	sem_wait(win_man.mutex);
	gr_window * window;
	gr_object * newobj;

	gr_object * found_obj = doubled_framebuffer_container->search_child_by_id(w_id);
	if(found_obj==0 || !found_obj->has_flag(gr_window::WINDOW_FLAG))
		goto err;

	window = static_cast<gr_window*>(found_obj);

	newobj = window->add_user_object(u_obj);
	if(newobj == 0)
		goto err;

	window->render();
	sem_signal(win_man.mutex);
	return newobj->get_id();

err:
	flog(LOG_INFO, "c_crea_oggetto: errore generico", u_obj->TYPE);
	sem_signal(win_man.mutex);
	return -1;
}

extern "C" void c_aggiorna_oggetto(int w_id, int o_id, u_windowObject * u_obj, bool sync)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	gr_object * found_window;
	gr_window * window;
	gr_object * found_obj;
	int new_index;

	// cerco la finestra
	found_window = doubled_framebuffer_container->search_child_by_id(w_id);
	if(found_window==0 || !found_window->has_flag(gr_window::WINDOW_FLAG))
		goto err;

	window = static_cast<gr_window*>(found_window);

	// cerco l'oggetto
	found_obj = window->search_user_object(u_obj);
	if(found_obj==0)
		goto err;

	// controllo che l'oggetto utente sia dello stesso tipo dell'oggetto gr
	switch(u_obj->TYPE)
	{
		case W_ID_LABEL:
			if(!found_obj->has_flag(gr_window::LABEL_FLAG))
				goto err;
			break;
		case W_ID_BUTTON:
			if(!found_obj->has_flag(gr_window::BUTTON_FLAG))
				goto err;
			break;
		case W_ID_TEXTBOX:
			if(!found_obj->has_flag(gr_window::TEXTBOX_FLAG))
				goto err;
			break;
		default:
			flog(LOG_INFO, "c_aggiorna_oggetto: tipo oggetto %d errato", u_obj->TYPE);
			goto err;
	}

	LOG_DEBUG("Inserimento richiesta di aggiornamento oggetto");

	new_index = windows_queue_insert(win_man, window, 100, PRIM_UPDATE_OBJECT, sync);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		goto err;
	}

	// nella richiesta ci metto comunque l'oggetto dell'utente perchè il lavoro vero e proprio
	// verrà fatto dalla graphic_aggiorna_oggetto. Qui mi occupo solo di verificare i parametri
	// e di aggiungere la richiesta nella coda
	win_man.req_queue[new_index].obj = u_obj;

	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
	if(sync)
		sem_wait(win_man.req_queue[new_index].if_sync);

	return;

//Gestione errori (sblocco mutex e sync su array)
err:
	flog(LOG_INFO, "c_aggiorna_oggetto: errore generico", u_obj->TYPE);
	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notfull);
}

extern "C" des_user_event c_preleva_evento(int w_id)
{
	sem_wait(win_man.mutex);
	LOG_DEBUG("c_preleva_evento: chiamata su finestra %d", w_id);

	gr_window * window;
	gr_object * found_obj;
	des_user_event popped_event;

	// prelevo l'evento e lo restituisco
	while(true)
	{
		// cerco la finestra dalla quale prelevare l'evento
		// cerco all'interno del ciclo perchè la finestra potrebbe essere stata eliminata!
		found_obj = doubled_framebuffer_container->search_child_by_id(w_id);
		if(found_obj==0 || !found_obj->has_flag(gr_window::WINDOW_FLAG))
			goto err;
		window = static_cast<gr_window*>(found_obj);

		popped_event = window->user_event_pop();
		// se la lista è vuota mi devo bloccare
		if(popped_event.type==NOEVENT)
		{
			// sblocco subito il mutex, altrimenti vado in lock
			sem_signal(win_man.mutex);
			// mi metto in attesa sul semaforo della lista degli elementi
			sem_wait(window->event_sem_sync_notempty);
			// appena vengo sbloccato devo assicurarmi di riottenere la mutua esclusione sul gestore delle finestre
			sem_wait(win_man.mutex);
		}
		else
		{
			// sblocco il mutex e restituisco il risultato
			sem_signal(win_man.mutex);
			return popped_event;
		}
	}

err:
	flog(LOG_INFO, "c_preleva_evento: errore generico");
	sem_signal(win_man.mutex);
	des_user_event error_event;
	error_event.type=NOEVENT;
	return error_event;
}

// ======= Funzione per l'aggiunta dell'evento tick da parte del processo winman_tick =======
void winman_time_tick()
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	//flog(LOG_INFO, "winman_time_tick: chiamata su winman_time_tick");

	int new_index = windows_queue_insert(win_man, 0, 100, PRIM_TIME_TICK, false);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		goto err;
	}

	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
	return;
err:
	flog(LOG_INFO, "winman_time_tick: errore generico");
	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notfull);
}

void print_palette(PIXEL_UNIT* buff, int x, int y)
{
	int row=0;
	for(PIXEL_UNIT i=0; i<0xFF; i++)
	{
		for(int k=0; k<10; k++)
			for(int j=0; j<10; j++)
				set_pixel(buff, x+(i%16)*10+j, y+row*10+k, MAX_SCREENX, MAX_SCREENY, i);
		if(i%16==0 && i!=0)
			row++;
	}
}

// ======= Funzioni eseguite dal gestore delle finestre relative ai comandi/primitive in coda =======
void graphic_visualizza_finestra(gr_window *window)
{
	window->set_visibility(true);
	window->render();
	doubled_framebuffer_container->render();
	framebuffer_container->render();
	framebuffer_container->clear_render_units();
}

void graphic_chiudi_finestra(gr_window *window)
{
	doubled_framebuffer_container->remove_child(window);
	doubled_framebuffer_container->render();
	framebuffer_container->render();
	framebuffer_container->clear_render_units();
}

void graphic_aggiorna_oggetto(gr_window *window, u_windowObject* u_obj)
{
	window->update_user_object(u_obj);
	window->render();
	doubled_framebuffer_container->render();
	framebuffer_container->render();
	framebuffer_container->clear_render_units();
}

// ======= Funzioni e strutture per la gestione del cursore =======
struct des_cursor
{
	int old_x;
	int old_y;
	int x;
	int y;
};

void switch_mousecursor_bitmap(const void *newbitmap, int offset_x, int offset_y)
{
	if(current_bitmap==newbitmap)
		return;

	current_bitmap = newbitmap;
	mouse_bitmap->set_pos_x(mouse_bitmap->get_pos_x() + bitmap_click_offset_x - offset_x);
	mouse_bitmap->set_pos_y(mouse_bitmap->get_pos_y() + bitmap_click_offset_y - offset_y);
	bitmap_click_offset_x = offset_x;
	bitmap_click_offset_y = offset_y;
	gr_memcpy(mouse_bitmap->get_buffer(), newbitmap, 32*32);
}

void render_mousecursor_onbuffer(des_cursor* cursor)
{
	mouse_bitmap->set_pos_x(cursor->x - bitmap_click_offset_x);
	mouse_bitmap->set_pos_y(cursor->y - bitmap_click_offset_x);
	mouse_bitmap->render();
}

// ======= Funzioni per inserire nella lista dei comandi eventi =======
void mouse_notify_move(int delta_x, int delta_y)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	LOG_DEBUG("Inserimento richiesta di aggiornamento posizione mouse (x,y)");

	int new_index = windows_queue_insert(win_man, 0, 100, MOUSE_UPDATE_EVENT, false);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		sem_signal(win_man.mutex);
		sem_signal(win_man.sync_notfull);
		return;
	}

	win_man.req_queue[new_index].delta_x = delta_x;
	win_man.req_queue[new_index].delta_y = delta_y;
	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
}

void mouse_notify_z_move(int delta_z)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	LOG_DEBUG("Inserimento richiesta di aggiornamento coordinata z mouse");

	int new_index = windows_queue_insert(win_man, 0, 100, MOUSE_Z_UPDATE_EVENT, false);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		sem_signal(win_man.mutex);
		sem_signal(win_man.sync_notfull);
		return;
	}

	win_man.req_queue[new_index].delta_z = delta_z;
	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
}

void mouse_notify_mousebutton_event(int EVENT, mouse_button who)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	LOG_DEBUG("Inserimento richiesta di aggiornamento stato button mouse");

	int new_index = windows_queue_insert(win_man, 0, 100, EVENT, false);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		sem_signal(win_man.mutex);
		sem_signal(win_man.sync_notfull);
		return;
	}

	win_man.req_queue[new_index].button = who;
	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
}

void keyboard_notify_keypress_event(char key)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	LOG_DEBUG("Inserimento richiesta di inserimento tasto");

	int new_index = windows_queue_insert(win_man, 0, 100, KEYBOARD_KEYPRESS_EVENT, false);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		sem_signal(win_man.mutex);
		sem_signal(win_man.sync_notfull);
		return;
	}

	win_man.req_queue[new_index].key = key;
	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
}

// funzione main del processo windows_manager
void main_windows_manager(int n)
{
	des_cursor main_cursor = {0,0,0,0};
	win_man.is_dragging=false;

	while(true)
	{
		sem_wait(win_man.sync_notempty);
		sem_wait(win_man.mutex);

		LOG_DEBUG("main_windows_manager: risvegliato");
		des_window_req newreq;
		if(!windows_queue_extract(win_man, newreq))
		{
			LOG_ERROR("main_windows_manager: Errore nell'estrazione delle richieste.");
			abort_p();
		}

		switch(newreq.act)
		{
			case PRIM_SHOW:
				LOG_DEBUG("act(%d): Processo richiesta di renderizzazione finestra per finestra %d", newreq.act, newreq.window->get_id());
				graphic_visualizza_finestra(newreq.window);
				if(newreq.to_sync)
					sem_signal(newreq.if_sync);
			break;
			case PRIM_CLOSE_WIND:
				LOG_DEBUG("act(%d): Processo richiesta di chiusura finestra per finestra %d", newreq.act, newreq.window->get_id());
				graphic_chiudi_finestra(newreq.window);
				if(newreq.to_sync)
					sem_signal(newreq.if_sync);
			break;
			case PRIM_UPDATE_OBJECT:
				LOG_DEBUG("act(%d): Processo richiesta di aggiornamento oggetto per finestra %d", newreq.act, newreq.window->get_id());
				graphic_aggiorna_oggetto(newreq.window, newreq.obj);
				//renderobject_onwindow(newreq.w_id, newreq.obj, &main_cursor);
				if(newreq.to_sync)
					sem_signal(newreq.if_sync);
			break;
			case PRIM_TIME_TICK:
			{
				if(win_man.focused_window==0)
					break;
				LOG_DEBUG("act(%d): Processo richiesta di evento tick per finestra %d", newreq.act, win_man.focused_window->get_id());
				win_man.focused_window->process_tick_event();
				win_man.focused_window->render();
				doubled_framebuffer_container->render();
				framebuffer_container->render();
				framebuffer_container->clear_render_units();
			}
			break;
			case MOUSE_UPDATE_EVENT:
			{
					//flog(LOG_INFO, "act(%d): Processo richiesta di aggiornamento dati mouse %d", newreq.act, newreq.w_id);
					main_cursor.old_x=main_cursor.x;
					main_cursor.old_y=main_cursor.y;
					main_cursor.x+=newreq.delta_x;
					main_cursor.y+=newreq.delta_y;
	
					//se si è già verificato il mouse_up, significa che ora devo trascinare la finestra
					if(win_man.is_dragging && win_man.focused_window!=0)
					{
						win_man.focused_window->set_pos_x(win_man.focused_window->get_pos_x() + newreq.delta_x);
						win_man.focused_window->set_pos_y(win_man.focused_window->get_pos_y() + newreq.delta_y);
					}

					if(win_man.is_resizing && win_man.focused_window!=0)
					{
						if(win_man.dragging_border == win_man.focused_window->border_left_bitmap)
						{
							int effective_delta = win_man.focused_window->offset_size_x(newreq.delta_x*-1);
							win_man.focused_window->set_pos_x(win_man.focused_window->get_pos_x() + effective_delta*-1);
						}
						else if(win_man.dragging_border == win_man.focused_window->border_right_bitmap)
							win_man.focused_window->offset_size_x(newreq.delta_x);
						else if(win_man.dragging_border == win_man.focused_window->border_bottom_bitmap)
							win_man.focused_window->offset_size_y(newreq.delta_y);

						win_man.focused_window->resize();
						win_man.focused_window->render();
					}

					//sposto il cursore sulla posizione nuova
					render_mousecursor_onbuffer(&main_cursor);

					//copio il buffer secondario sulla memoria video
					doubled_framebuffer_container->render();
					framebuffer_container->render();
					framebuffer_container->clear_render_units();
			}
			break;
			case MOUSE_Z_UPDATE_EVENT:
				if(win_man.focused_window!=0)
					win_man.focused_window->user_event_add_mousemovez(newreq.delta_z, main_cursor.x, main_cursor.y);
			break;
			case MOUSE_MOUSEDOWN_EVENT:
				if(newreq.button==LEFT)
				{
					//cerco di capire su quale oggetto ho cliccato e se fa parte di una finestra
					gr_object::search_filter filter;
					gr_object::search_result res;
					memset(&filter, 0, sizeof(filter));
					filter.skip_id=mouse_bitmap->get_id();
					filter.padding_x = 2;
					filter.padding_y = 2;
					filter.parent_flags = gr_window::WINDOW_FLAG;
					doubled_framebuffer_container->search_tree(main_cursor.x, main_cursor.y, filter, res);

					gr_object *clicked_object = res.target;

					// se ho cliccato su un oggetto di una finestra
					if(res.target_parent!=0)
					{
						// non c'è bisogno di un dynamic_cast perchè la ricerca restituisce solo oggetti gr_windows (se tutto va bene)
						gr_window *window_of_clicked_object = static_cast<gr_window*>(res.target_parent);

						// imposto il focus sulla finestra
						win_man.focused_window = window_of_clicked_object;
						win_man.focused_window->render();
						doubled_framebuffer_container->focus_child(win_man.focused_window);

						// devo capire su che punto della finestra ho cliccato, provo a vedere se sui bordi
						memset(&filter, 0, sizeof(filter));
						filter.skip_id = mouse_bitmap->get_id();
						filter.padding_x = 5;
						filter.padding_y = 5;
						filter.flags = gr_window::BORDER_FLAG;
						window_of_clicked_object->search_tree(main_cursor.x-window_of_clicked_object->get_pos_x(), main_cursor.y-window_of_clicked_object->get_pos_y(), filter, res);
						if(res.target != 0)
						{
							LOG_DEBUG("winman: il click è stato fatto sui bordi della finestra %d", window_of_clicked_object->get_id());
							win_man.dragging_border = res.target;
							win_man.is_resizing = true;
							switch_mousecursor_bitmap(h_resize_cursor, h_resize_cursor_click_x, h_resize_cursor_click_y);
						}
						else
						{
							// se, invece, ho cliccato su una topbar, allora significa che devo iniziare a trascinare
							memset(&filter, 0, sizeof(filter));
							filter.skip_id = mouse_bitmap->get_id();
							filter.parent_flags = gr_window::TOPBAR_FLAG;
							window_of_clicked_object->search_tree(main_cursor.x-window_of_clicked_object->get_pos_x(), main_cursor.y-window_of_clicked_object->get_pos_y(), filter, res);
							if(res.target_parent != 0)
							{
								flog(LOG_INFO, "winman: il click è stato fatto sulla topbar della finestra %d", window_of_clicked_object->get_id());
								win_man.is_dragging = true;
							}
							else // altrimenti devo generare un evento utente perchè il click è stato fatto dentro la finestra (esclusi bordi e topbar)
							{
								win_man.focused_window->set_focused_child(clicked_object);
								win_man.focused_window->user_event_add_mousebutton(USER_EVENT_MOUSEDOWN, newreq.button, main_cursor.x, main_cursor.y);
							}
						}

						doubled_framebuffer_container->render();
						framebuffer_container->render();
					}
					else // se il click non è stato fatto su una finestra dobbiamo togliere il focus alla finestra con focus
						win_man.focused_window = 0;
				}
			break;
			case MOUSE_MOUSEUP_EVENT:
			{
				if(newreq.button==LEFT)
				{
					win_man.is_dragging=false;
					win_man.is_resizing=false;

					if(win_man.focused_window!=0)
						win_man.focused_window->user_event_add_mousebutton(USER_EVENT_MOUSEUP, newreq.button, main_cursor.x, main_cursor.y);

					switch_mousecursor_bitmap(main_cursor_bitmap, main_cursor_click_x, main_cursor_click_y);
					doubled_framebuffer_container->render();
					framebuffer_container->render();
				}
			}
			break;
			case KEYBOARD_KEYPRESS_EVENT:
			{
				if(win_man.focused_window!=0)
					win_man.focused_window->user_event_add_keypress(newreq.button);
			}
			break;
		}

		sem_signal(win_man.mutex);
		sem_signal(win_man.sync_notfull);
	}
		
}

// main del processo per la generazione degli eventi tick
void main_winman_tick(int n)
{
	while(true)
	{
		winman_time_tick();
		delay(20);
	}
}

bool windows_init()
{
	//framebuffer e doubledbuffer
	framebuffer_container = new gr_object(0,0, MAX_SCREENX, MAX_SCREENY,0, main_videocard.framebuffer);
	doubled_framebuffer_container = new gr_object(0,0, MAX_SCREENX, MAX_SCREENY,0);
	framebuffer_container->add_child(doubled_framebuffer_container);

	//sfondo
	gr_bitmap * bitmap = new gr_bitmap(0,0,MAX_SCREENX,MAX_SCREENY,0);
	LOG_DEBUG("bitmap buffer address %p", bitmap->get_buffer());
	//gr_memset(bitmap->get_buffer(), WIN_BACKGROUND_COLOR, MAX_SCREENX*MAX_SCREENY);
	TgaParser tga_c(tga_wallpaper);
	tga_c.to_bitmap(bitmap->get_buffer());
	//print_palette(bitmap->get_buffer(), 0,0);
	bitmap->render();
	doubled_framebuffer_container->add_child(bitmap);

	//cursore
	mouse_bitmap = new gr_bitmap(0,0,32,32,777);
	switch_mousecursor_bitmap(main_cursor_bitmap, main_cursor_click_x, main_cursor_click_y);
	mouse_bitmap->set_trasparency(true);
	mouse_bitmap->render();
	doubled_framebuffer_container->add_child(mouse_bitmap);

	//barra
	gr_bitmap * screen_bar = new gr_bitmap(0,0, MAX_SCREENX, MAINBAR_SIZE, MAINBAR_ZINDEX);
	#ifdef BPP_8
		gr_memset(screen_bar->get_buffer(), MAINBAR_COLOR, MAX_SCREENX*MAINBAR_SIZE);
	#elif defined BPP_32
		for(natl y=0; y<MAINBAR_SIZE; y++)
			gr_memset(screen_bar->get_buffer() + y*MAX_SCREENX, MAINBAR_COLOR+(((MAINBAR_SIZE-y)*4)<<24), MAX_SCREENX);
	#endif

	screen_bar->set_trasparency(true);
	screen_bar->render();
	doubled_framebuffer_container->add_child(screen_bar);

	doubled_framebuffer_container->render();
	framebuffer_container->render();
	framebuffer_container->clear_render_units();

	//creare un processo che si occupi della stampa delle finestre
	win_man.mutex = sem_ini(1);
	win_man.sync_notfull = sem_ini(MAX_REQ_QUEUE - 1);
	win_man.sync_notempty = sem_ini(0);

	win_man.top = 0;
	win_man.rear = 0;
	
	if(win_man.sync_notfull==-1 || win_man.sync_notempty==-1)
	{
		return false;
	}

	flog(LOG_INFO, "attivo gestore delle finestre...");
	activate_p(main_windows_manager, 0, PRIO_WINMAN, LIV_SISTEMA);
	activate_p(main_winman_tick, 0, PRIO_WINTICK, LIV_SISTEMA);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
//                         GESTIONE DELLA TASTIERA                            //
////////////////////////////////////////////////////////////////////////////////

const natl MAX_CODE = 40; //
struct interfkbd_reg {	//
	ioaddr iRBR, iTBR, iCMR, iSTR;
};

struct des_kbd { //
	interfkbd_reg indreg;
	bool shift;
	natb tab[MAX_CODE];
	natb tabmin[MAX_CODE];
	natb tabmai[MAX_CODE];
};

des_kbd kdb = {
	{	// indreg
		0x60,	// iRBR
		0x60,	// iTBR
		0x64,	// iCMR
		0x64,	// iSTR
	},
	0,	// shift
	{	// tab
		0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
		0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
		0x26, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
		0x39, 0x1C, 0x0e, 0x01
	},
	{	// tamin
		'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
		'a', 's', 'd', 'f', 'g', 'h', 'j', 'k',
		'l', 'z', 'x', 'c', 'v', 'b', 'n', 'm',
		' ', '\n', '\b', 0x1B
	},
	{	// tabmai
		'!', '"', '@', '$', '%', '&', '/', '(', ')', '=',
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
		'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K',
		'L', 'Z', 'X', 'C', 'V', 'B', 'N', 'M',
		' ', '\r', '\b', 0x1B
	}
};

extern "C" void go_inputkbd(interfkbd_reg* indreg); //
extern "C" void halt_inputkbd(interfkbd_reg* indreg); //

natb converti(des_kbd* p_des, natb c) { //
	natb cc;
	natl pos = 0;
	while (pos < MAX_CODE && p_des->tab[pos] != c)
		pos++;
	if (pos == MAX_CODE)
		return 0;
	if (p_des->shift)
		cc = p_des->tabmai[pos];
	else
		cc = p_des->tabmin[pos];
	return cc;
}

void estern_kbd(int h) //
{
	des_kbd *k_des = &kdb;
	natb a, c;

	for(;;) {
		//halt_inputkbd(&k_des->indreg);

		inputb(k_des->indreg.iRBR, c);

		switch (c) {
		case 0x2a: // left shift make code
			k_des->shift = true;
			break;
		case 0xaa: // left shift break code
			k_des->shift = false;
			break;
		default:
			if (c < 0x80) {
				a = converti(k_des, c);
				if (a == 0)
					break;
				if (a == '\b') {
					//write "\b \b"
				} else if (a == '\r' || a == '\n') {
					//write "\r\n"
				} else {

				}
				keyboard_notify_keypress_event(a);
			}
			break;
		}

		/*else
			go_inputkbd(&k_des->indreg);*/
		wfi();
	}
}

// (* inizializzazioni
extern "C" void abilita_tastiera(void);

// Interruzione hardware della tastiera
const int KBD_IRQ = 1;

bool kbd_init()
{
	if (activate_pe(estern_kbd, 0, PRIO, LIV, KBD_IRQ) == 0xFFFFFFFF) {
		flog(LOG_ERR, "kbd: impossibile creare estern_kbd");
		return false;
	}
	return true;
}


////////////////////////////////////////////////////////////////////////////////
//                             GESTIONE MOUSE                                 //
////////////////////////////////////////////////////////////////////////////////

struct des_mouse
{
	natl iRBR;
	natl iTBR;
	natl iCMR;
	natl iSTR;

	int x;
	int y;
	int z;
	int old_x;
	int old_y;
	int old_z;

	bool left_click;
	bool middle_click;
	bool right_click;

	bool is_intellimouse;
};

des_mouse ps2_mouse = {0x60,0x60,0x64,0x64, 0,0,0,0,0,0, false};

void inline wait_kdb_read()
{
	natb res;
	do
	{
		inputb(ps2_mouse.iSTR, res);
	}
	while(!(res & 0x01)); //attendo di poter leggere iCMR
}

void inline wait_kdb_write()
{
	natb res;
	do
	{
		inputb(ps2_mouse.iSTR, res);
	}
	while(res & 0x02); //attendo di poter leggere iCMR
}

void inline mouse_send(natb data)
{
	natb mouseresp;
	wait_kdb_write();
	outputb(0xD4, ps2_mouse.iCMR);
	wait_kdb_write();
	outputb(data, ps2_mouse.iTBR);
	wait_kdb_read();
	inputb(ps2_mouse.iRBR, mouseresp);
}

int inline bytetosignedshort(natb a, bool s)
{
	return ( (s ) ?0xFFFFFF00:0x00000000) | a;
}

int inline bytetosignedshort_z(natb a)
{
	return (a>127)? a-256 : a;
}

const int MOUSE_DELTAX_DIVIDER=1;
const int MOUSE_DELTAY_DIVIDER=1;

void mouse_handler(int i)
{
	int mouse_count=0;
	bool discard_one_packet=false;
	natb mouse_bytes[4];

	while(true)
	{
		//flog(LOG_INFO, "mouse_count %d",mouse_count);
		//scarto un pacchetto se è settata questa variabile booleana. Mi serve per gestire i disallineamenti
		if(discard_one_packet)
		{
			natb discarded;
			inputb(ps2_mouse.iRBR, discarded);
			discard_one_packet=false;
			wfi();
		}

		inputb(ps2_mouse.iRBR, mouse_bytes[mouse_count++]);

		if ((mouse_count == 4 && ps2_mouse.is_intellimouse) || (mouse_count==3 && !ps2_mouse.is_intellimouse))
		{
			int x,y, new_x,new_y,new_z;
			if(!(mouse_bytes[0] & 0x08)) //Il bit 3 deve sempre essere ad 1 per byte dei flag
			{
				LOG_ERROR("mouse_driver: invalid packets alignment");
				discard_one_packet=true;
				goto fine;
			}

			// condizione di overflow (cestino il dato)
			if ((mouse_bytes[0] & 0x80) || (mouse_bytes[0] & 0x40))
			{
				LOG_ERROR("mouse_driver: Mouse Overflow!");
				goto fine;
			}

			x = bytetosignedshort(mouse_bytes[1], (mouse_bytes[0] & 0x10) >> 4);
			y = bytetosignedshort(mouse_bytes[2], (mouse_bytes[0] & 0x20) >> 5);

			// Calcolo le nuove coordinate e mi assicuro che non sforino i margini dello schermo
			ps2_mouse.old_x=ps2_mouse.x;
			ps2_mouse.old_y=ps2_mouse.y;
			
			new_x=ps2_mouse.x+(x/MOUSE_DELTAX_DIVIDER);
			new_y=ps2_mouse.y-(y/MOUSE_DELTAY_DIVIDER);
			
			if(new_x<0)
				ps2_mouse.x=0;
			else if(new_x>=MAX_SCREENX)
				ps2_mouse.x=MAX_SCREENX-1;
			else
				ps2_mouse.x=new_x;

			if(new_y<0)
				ps2_mouse.y=0;
			else if(new_y>=MAX_SCREENY)
				ps2_mouse.y=MAX_SCREENY-1;
			else
				ps2_mouse.y=new_y;

			//della z mi tengo da parte solo il nuovo e il vecchio delta
			new_z=bytetosignedshort_z(mouse_bytes[3]);
			ps2_mouse.old_z=ps2_mouse.z;
			ps2_mouse.z=new_z;


			if (mouse_bytes[0] & 0x4 && !ps2_mouse.middle_click)
			{
				LOG_DEBUG("mouse_driver: Middle button is pressed!");
				ps2_mouse.middle_click=true;
				mouse_notify_mousebutton_event(MOUSE_MOUSEDOWN_EVENT, MIDDLE);
			}
			else if (!(mouse_bytes[0] & 0x4) && ps2_mouse.middle_click)
			{
				LOG_DEBUG("mouse_driver: Middle button is released!");
				ps2_mouse.middle_click=false;
				mouse_notify_mousebutton_event(MOUSE_MOUSEUP_EVENT, MIDDLE);
			}

			if (mouse_bytes[0] & 0x2 && !ps2_mouse.right_click)
			{
				LOG_DEBUG("mouse_driver: Right button is pressed!");
				ps2_mouse.right_click=true;
				mouse_notify_mousebutton_event(MOUSE_MOUSEDOWN_EVENT, RIGHT);
			}
			else if (!(mouse_bytes[0] & 0x2) && ps2_mouse.right_click)
			{
				LOG_DEBUG("mouse_driver: Right button is released!");
				ps2_mouse.right_click=false;
				mouse_notify_mousebutton_event(MOUSE_MOUSEUP_EVENT, RIGHT);
			}

			if (mouse_bytes[0] & 0x1 && !ps2_mouse.left_click)
			{
				LOG_DEBUG("mouse_driver: Left button is pressed!");
				ps2_mouse.left_click=true;
				mouse_notify_mousebutton_event(MOUSE_MOUSEDOWN_EVENT, LEFT);
			}
			else if (!(mouse_bytes[0] & 0x1) && ps2_mouse.left_click)
			{
				LOG_DEBUG("mouse_driver: Left button is released!");
				ps2_mouse.left_click=false;
				mouse_notify_mousebutton_event(MOUSE_MOUSEUP_EVENT, LEFT);
			}
			
			// notifico al windows manager che il mouse ha mandato nuove coordinate
			if(ps2_mouse.x!=ps2_mouse.old_x || ps2_mouse.y!=ps2_mouse.old_y)
				mouse_notify_move(ps2_mouse.x-ps2_mouse.old_x, ps2_mouse.y-ps2_mouse.old_y);

			if(ps2_mouse.z!=ps2_mouse.old_z)
				mouse_notify_z_move(ps2_mouse.z);

			//flog(LOG_INFO, "mouse_driver: x: %d y: %d z: %d dx: %d dy: %d old_x %d old_y %d", ps2_mouse.x, ps2_mouse.y, ps2_mouse.z,x, y, ps2_mouse.old_x, ps2_mouse.old_y);

			fine: mouse_count = 0; // reset the counter
		}

		wfi();
	}
}

bool mouse_init()
{
	//Abilito seconda porta ps/2
	outputb(0xA8, ps2_mouse.iCMR);

	//Leggo registro di controllo
	//wait_kdb_write();
	outputb(0x20, ps2_mouse.iCMR);
	wait_kdb_read();
	natb resp;
	inputb(ps2_mouse.iRBR, resp);

	//Abilito interruzioni su registro di controllo
	wait_kdb_write();
	outputb(0x60, ps2_mouse.iCMR);
	wait_kdb_write();
	outputb(resp | 0x02, ps2_mouse.iTBR);

	//Abilito mouse
	mouse_send(0xF6);
	mouse_send(0xF4);

	//Abilito IntelliMouse
	ps2_mouse.is_intellimouse=true;
	mouse_send(0xF3);
	mouse_send(200);
	mouse_send(0xF3);
	mouse_send(100);
	mouse_send(0xF3);
	mouse_send(80);
	mouse_send(0xF2);
	wait_kdb_read();
	natb mouseresp;
	inputb(ps2_mouse.iRBR, mouseresp);
	flog(LOG_INFO, "Risposta mouse: %d", mouseresp);

	//Setto handler
	activate_pe(mouse_handler, 0, PRIO_MOUSE, LIV_SISTEMA, 12);

	flog(LOG_INFO, "mouse inizializzato");

	return true;
}
// inerfacce ATA

enum hd_cmd { WRITE_SECT = 0x30, READ_SECT = 0x20, WRITE_DMA = 0xCA, READ_DMA = 0xC8 };
struct interfata_reg {
	ioaddr iBR;
	ioaddr iCNL, iCNH, iSNR, iHND, iSCR, iERR,
	       iCMD, iSTS, iDCR, iASR;
};
struct pci_ata
{
	ioaddr iBMCMD, iBMSTR, iBMDTPR;
};
struct des_ata {
	interfata_reg indreg;
	pci_ata bus_master;
	natl prd[2];
	hd_cmd comando;
	natb errore;
	natl mutex;
	natl sincr;
	natb cont;
	addr punt;
};
des_ata hd = {
	{
		0x0170,	// iBR
		0x0174, // iCNL
		0x0175, // iCNH
		0x0173, // iSNR
		0x0176, // iHND
		0x0172, // iSCR
		0x0171, // iERR
		0x0177, // iCMD
		0x0177, // iSTS
		0x0376, // iDCR
		0x0376  // iASR
	}
	// il resto e' inizializzato a zero
};


const natb HD_IRQ = 15;

extern "C" void hd_write_address(des_ata* p_des, natl primo);
// scrive primo nei registri di indirizzo CNL, CNH, SNR e HND
extern "C" void hd_write_command(hd_cmd cmd, ioaddr iCMD);
// scrive cmd nel registro iCMD e aspetta che BSY torni a 0

extern "C" bool hd_wait_data(ioaddr iSTS) // [9.3.1]
{
	natb stato;

	do {
		inputb(iSTS, stato);
	} while (stato & 0x80);

	return ( (stato & 0x08) && !(stato & 0x01) );
}
// attende che BSY passi a zero e DRQ a 1. Restituisce false se ERR vale 1
extern "C" void hd_go_inout(ioaddr iSTS);
// abilita l'interfaccia a generare interruzioni
extern "C" void hd_halt_inout(ioaddr iSTS);
// disabilita l'interfaccia a generare interruzioni

void hd_componi_prd(des_ata* p_dmades, addr iff, natw quanti)
{
	p_dmades->prd[0] = reinterpret_cast<natq>(iff);
	p_dmades->prd[1] = 0x80000000 | quanti;					// EOT posto a 1
}
extern "C" void hd_select_device(short ms, ioaddr iHND);
void hd_sel_drv(des_ata* p_des) //
{
	natb stato;

	hd_select_device(0, p_des->indreg.iHND);

	do {
		inputb(p_des->indreg.iSTS, stato);
	} while ( (stato & 0x80) || (stato & 0x08) );
}
void starthd_in(des_ata *p_des, natw vetti[], natl primo, natb quanti);
extern "C" void c_readhd_n(natw vetti[], natl primo,
		natb quanti, natb &errore)
{
	des_ata *p_des;
	p_des = &hd;
	sem_wait(p_des->mutex);
	starthd_in(p_des, vetti, primo, quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}
void starthd_out(des_ata *p_des, natw vetto[], natl primo, natb quanti);
extern "C" void c_writehd_n(natw vetto[], natl primo,
		natb quanti, natb &errore)
{
	des_ata *p_des;
	p_des = &hd;
	sem_wait(p_des->mutex);
	starthd_out(p_des, vetto, primo, quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}
void starthd_in(des_ata *p_des, natw vetti[], natl primo, natb quanti)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->comando = READ_SECT;
	hd_sel_drv(p_des);
	hd_write_address(p_des, primo);
	outputb(quanti, p_des->indreg.iSCR);
	hd_go_inout(p_des->indreg.iDCR);
	hd_write_command(READ_SECT, p_des->indreg.iCMD);
}
void starthd_out(des_ata *p_des, natw vetto[], natl primo, natb quanti)
{
	p_des->cont = quanti;
	p_des->punt = vetto + DIM_BLOCK / 2;
	p_des->comando = WRITE_SECT;
	hd_sel_drv(p_des);
	hd_write_address(p_des, primo);
	outputb(quanti, p_des->indreg.iSCR);
	hd_go_inout(p_des->indreg.iDCR);
	hd_write_command(WRITE_SECT, p_des->indreg.iCMD);
	hd_wait_data(p_des->indreg.iSTS);
	outputbw(vetto, DIM_BLOCK/2, p_des->indreg.iBR);
}
void dmastarthd_in(des_ata *p_des, natw vetti[], natl primo, natb quanti);
extern "C" void c_dmareadhd_n(natw vetti[], natl primo, natb quanti,
		natb &errore)
{
	des_ata *p_des;
	p_des = &hd;
	sem_wait(p_des->mutex);
	dmastarthd_in(p_des, vetti, primo, quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}
void dmastarthd_out(des_ata *p_des, natw vetto[], natl primo, natb quanti);
extern "C" void c_dmawritehd_n(natw vetto[], natl primo, natb quanti,
		natb& errore)
{
	des_ata *p_des;
	p_des = &hd;
	sem_wait(p_des->mutex);
	dmastarthd_out(p_des, vetto, primo, quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}
void dmastarthd_in(des_ata *p_des, natw vetti[], natl primo, natb quanti)
{
	// la scrittura ini iBMDTPR di &prd[0] avviene in fase di inizializzazione
	natb work; addr iff;
	p_des->comando = READ_DMA;
	p_des->cont = 1;					// informazione per il driver
	iff = trasforma(vetti);
	hd_componi_prd(p_des, iff, quanti * DIM_BLOCK);
	inputb(p_des->bus_master.iBMCMD, work);
	work |= 0x08;							// lettura da PCI
	outputb(work, p_des->bus_master.iBMCMD);
	inputb(p_des->bus_master.iBMSTR, work);
	work &= 0xF9;						// bit interruzione ed errore a 0
	outputb(work, p_des->bus_master.iBMSTR);
	hd_sel_drv(p_des);
	hd_write_address(p_des, primo);
	outputb(quanti, p_des->indreg.iSCR);
	hd_go_inout(p_des->indreg.iDCR);
	hd_write_command(READ_DMA, p_des->indreg.iCMD);
	inputb(p_des->bus_master.iBMCMD, work);
	work |= 0x01; 							// avvio dell’operazione
	outputb(work, p_des->bus_master.iBMCMD);
}
void dmastarthd_out(des_ata *p_des, natw vetto[], natl primo, natb quanti)
{
	// la scrittura in iBMDTPR di &prd[0] avviene in fase di inizializzazione
	natb work; addr iff;
	p_des->comando = WRITE_DMA;
	p_des->cont = 1; 				// informazione per il driver
	iff = trasforma(vetto);
	hd_componi_prd(p_des, iff, quanti * DIM_BLOCK);
	inputb(p_des->bus_master.iBMCMD, work);
	work &= 0xF7; 					// scrittura verso PCI
	outputb(work, p_des->bus_master.iBMCMD);
	inputb(p_des->bus_master.iBMSTR, work);
	work &= 0xF9;					// bit interruzione ed errore a 0
	outputb(work, p_des->bus_master.iBMSTR);
	hd_sel_drv(p_des);
	hd_write_address(p_des, primo);
	outputb(quanti, p_des->indreg.iSCR);
	hd_go_inout(p_des->indreg.iDCR);
	hd_write_command(WRITE_DMA, p_des->indreg.iCMD);
	inputb(p_des->bus_master.iBMCMD, work);
	work |= 1;								// avvio dell’operazione
	outputb(work, p_des->bus_master.iBMCMD);
}

void esternAta(int h)			// codice commune ai 2 processi esterni ATA
{
	des_ata* p_des = &hd;
	natb stato, work;
	for(;;) {
		p_des->cont--;
		if (p_des->cont == 0)
			hd_halt_inout(p_des->indreg.iDCR);
		p_des->errore = 0;
		inputb(p_des->indreg.iSTS, stato); 				// ack dell'interrupt
		switch (p_des->comando) {
		case READ_SECT:
			if (!hd_wait_data(p_des->indreg.iSTS))
				inputb(p_des->indreg.iERR, p_des->errore);
			else
				inputbw(p_des->indreg.iBR, static_cast<natw*>(p_des->punt),
						DIM_BLOCK / 2);
			p_des->punt = static_cast<natw*>(p_des->punt) + DIM_BLOCK / 2;
			break;
		case WRITE_SECT:
			if (p_des->cont != 0) {
				if (!hd_wait_data(p_des->indreg.iSTS))
					inputb(p_des->indreg.iERR, p_des->errore);
				else
					outputbw(static_cast<natw*>(p_des->punt),
							DIM_BLOCK / 2, p_des->indreg.iBR);
				p_des->punt = static_cast<natw*>(p_des->punt) +
					DIM_BLOCK / 2;
			}
			break;
		case READ_DMA:
		case WRITE_DMA:
			inputb(p_des->bus_master.iBMCMD, work);
			work &= 0xFE;				// azzeramento del bit n. 0 (start/stop)
			outputb(work, p_des->bus_master.iBMCMD);
			inputb(p_des->bus_master.iBMSTR, stato);	// ack  interrupt in DMA
			if ((stato & 0x05) == 0)
				inputb(p_des->indreg.iERR, p_des->errore);
			break;
		}
		if (p_des->cont == 0)
			sem_signal(p_des->sincr);
		wfi();
	}
}

bool hd_init()
{
	natl id;
	natb bus = 0, dev = 0, fun = 0;
	natb code[] = { 0xff, 0x01, 0x01 };
	des_ata* p_des;

	p_des = &hd;

	if ( (p_des->mutex = sem_ini(1)) == 0xFFFFFFFF) {
		flog(LOG_ERR, "hd: impossibile creare mutex");
		return false;
	}
	if ( (p_des->sincr = sem_ini(0)) == 0xFFFFFFFF) {
		flog(LOG_ERR, "hd: impossibile creare sincr");
		return false;
	}

	if (!pci_find_class(bus, dev, fun, code)) {
		flog(LOG_WARN, "hd: bus master non trovato");
	} else {
		natb prog_if = pci_read_confb(bus, dev, fun, 0x9);
		if (prog_if & 0x80) {
			natl base = pci_read_confl(bus, dev, fun, 0x20);
			base &= ~0x1;
			hd.bus_master.iBMCMD  = (ioaddr)(base + 0x08);
			hd.bus_master.iBMSTR  = (ioaddr)(base + 0x0a);
			hd.bus_master.iBMDTPR = (ioaddr)(base + 0x0c);
			addr iff = trasforma(&hd.prd[0]);
			outputl(reinterpret_cast<natq>(iff),
					hd.bus_master.iBMDTPR);
			natw cmd = pci_read_confw(bus, dev, fun, 4);
			pci_write_confw(bus, dev, fun, 4, cmd | 0x5);
		}
	}

	id = activate_pe(esternAta, 0, PRIO, LIV, HD_IRQ);
	if (id == 0xFFFFFFFF) {
		flog(LOG_ERR, "com: impossibile creare proc. esterno");
		return false;
	}
	return true;
}


extern "C" void do_log(log_sev sev, const char* buf, natl l)
{
	log(sev, buf, l);
}

natl mem_mutex;

void* mem_alloc(natl dim)
{
	void *p;

	sem_wait(mem_mutex);
	p = alloca(dim);
	sem_signal(mem_mutex);

	return p;
}


void mem_free(void* p)
{
	sem_wait(mem_mutex);
	dealloca(p);
	sem_signal(mem_mutex);
}

////////////////////////////////////////////////////////////////////////////////
//                 INIZIALIZZAZIONE DEL SOTTOSISTEMA DI I/O                   //
////////////////////////////////////////////////////////////////////////////////

// inizializza i gate usati per le chiamate di IO
//
extern "C" void fill_io_gates(void);

extern "C" natl end;
// eseguita in fase di inizializzazione
//

extern "C" void cmain(int sem_io)
{

	fill_io_gates();
	mem_mutex = sem_ini(1);
	if (mem_mutex == 0xFFFFFFFF) {
		flog(LOG_ERR, "impossible creare semaforo mem_mutex");
		abort_p();
	}
	heap_init(&end, DIM_IO_HEAP);

	if(!bochsvga_init())
		abort_p();
	if(!windows_init())
		abort_p();
	if (!kbd_init())
		abort_p();
	if(!mouse_init())
		abort_p();
	if (!com_init())
		abort_p();
	if (!hd_init())
		abort_p();
	sem_signal(sem_io);
	terminate_p();
}
