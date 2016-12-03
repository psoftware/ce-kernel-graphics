// io.cpp
//
#include "costanti.h"
#include "libce.h"
#include "newdelete.h"
#include "windows/font.h"
#include "windows/gui_objects.h"
#include "windows/cursor.h"

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
//                         GESTIONE DELLA CONSOLE                       //
////////////////////////////////////////////////////////////////////////////////

const natl COLS = 80; 	//
const natl ROWS = 25;	//
const natl VIDEO_SIZE = COLS * ROWS;	//

struct interfvid_reg {	//
	ioaddr iIND, iDAT;
};

struct des_vid {	//
	interfvid_reg indreg;
	natw* video;
	natl x, y;
	natw attr;
};

const natl MAX_CODE = 40; //
struct interfkbd_reg {	//
	ioaddr iRBR, iTBR, iCMR, iSTR;
};

struct des_kbd { //
	interfkbd_reg indreg;
	addr punt;
	natl cont;
	bool shift;
	natb tab[MAX_CODE];
	natb tabmin[MAX_CODE];
	natb tabmai[MAX_CODE];
};

struct des_console { //
	natl mutex;
	natl sincr;
	des_kbd kbd;
	des_vid vid;
};

des_console console = {
	0,	// mutex (da inizializzare)
	0,	// sync  (da inizializzare)
	{	// kbd
		{	// indreg
			0x60,	// iRBR
			0x60,	// iTBR
			0x64,	// iCMR
			0x64,	// iSTR
		},
		0,	// punt
		0,	// cont
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
	},
	{	// vid
		{	// indreg
			0x03d4,	// iIND
			0x03d5,	// iDAT
		},
		(natw*)0xb8000,	// video
		0,		// x
		0,		// y
		0x4b00,		// attr
	}
};

extern "C" void cursore(ioaddr iIND, ioaddr iDAT, int x, int y); //

void scroll(des_vid *p_des)	//
{
	for (natl i = 0; i < VIDEO_SIZE - COLS; i++)
		p_des->video[i] = p_des->video[i + COLS];
	for (natl i = 0; i < COLS; i++)
		p_des->video[VIDEO_SIZE - COLS + i] = p_des->attr | ' ';
	p_des->y--;
}

void writeelem(natb c) {	//
	des_vid* p_des = &console.vid;
	switch (c) {
	case 0:
		break;
	case '\r':
		p_des->x = 0;
		break;
	case '\n':
		p_des->x = 0;
		p_des->y++;
		if (p_des->y >= ROWS)
			scroll(p_des);
		break;
	case '\b':
		if (p_des->x > 0 || p_des->y > 0) {
			if (p_des->x == 0) {
				p_des->x = COLS - 1;
				p_des->y--;
			} else
				p_des->x--;
		}
		break;
	default:
		p_des->video[p_des->y * COLS + p_des->x] = p_des->attr | c;
		p_des->x++;
		if (p_des->x >= COLS) {
			p_des->x = 0;
			p_des->y++;
		}
		if (p_des->y >= ROWS)
			scroll(p_des);
		break;
	}
	cursore(p_des->indreg.iIND, p_des->indreg.iDAT,
		p_des->x, p_des->y);
}

void writeseq(cstr seq)	//
{
	const natb* pn = static_cast<const natb*>(seq);
	while (*pn != 0) {
		writeelem(*pn);
		pn++;
	}
}

extern "C" void c_writeconsole(cstr buff) //
{
	des_console *p_des = &console;
	sem_wait(p_des->mutex);
#ifndef AUTOCORR
	writeseq(buff);
	writeelem('\n');
#else /* AUTOCORR */
	flog(LOG_USR, "%s", buff);
#endif /* AUTOCORR */
	sem_signal(p_des->mutex);
}

extern "C" void go_inputkbd(interfkbd_reg* indreg); //
extern "C" void halt_inputkbd(interfkbd_reg* indreg); //

void startkbd_in(des_kbd* p_des, str buff) //
{
	p_des->punt = buff;
	p_des->cont = 80;
	go_inputkbd(&p_des->indreg);
}

extern "C" void c_readconsole(str buff, natl& quanti) //
{
	des_console *p_des;

	p_des = &console;
	sem_wait(p_des->mutex);
	startkbd_in(&p_des->kbd, buff);
	sem_wait(p_des->sincr);
	quanti = p_des->kbd.cont;
	sem_signal(p_des->mutex);
}

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
	des_console *p_des = &console;
	natb a, c;
	bool fine;

	for(;;) {
		halt_inputkbd(&p_des->kbd.indreg);

		inputb(p_des->kbd.indreg.iRBR, c);

		fine = false;
		switch (c) {
		case 0x2a: // left shift make code
			p_des->kbd.shift = true;
			break;
		case 0xaa: // left shift break code
			p_des->kbd.shift = false;
			break;
		default:
			if (c < 0x80) {
				a = converti(&p_des->kbd, c);
				if (a == 0)
					break;
				if (a == '\b') {
					if (p_des->kbd.cont < 80) {
						p_des->kbd.punt = static_cast<natb*>(p_des->kbd.punt) - 1;
						p_des->kbd.cont++;
						writeseq("\b \b");
					}
				} else if (a == '\r' || a == '\n') {
					fine = true;
					p_des->kbd.cont = 80 - p_des->kbd.cont;
					*static_cast<natb*>(p_des->kbd.punt) = 0;
					writeseq("\r\n");
				} else {
					*static_cast<natb*>(p_des->kbd.punt) = a;
					p_des->kbd.punt = static_cast<natb*>(p_des->kbd.punt) + 1;
					p_des->kbd.cont--;
					writeelem(a);
					if (p_des->kbd.cont == 0) {
						fine = true;
						p_des->kbd.cont = 80;
					}
				}
			}
			break;
		}
		if (fine == true)
			sem_signal(p_des->sincr);
		else
			go_inputkbd(&p_des->kbd.indreg);
		wfi();
	}
}

// (* inizializzazioni
extern "C" void abilita_tastiera(void);
bool vid_init();

extern "C" void c_iniconsole(natb cc)
{
	des_vid *p_des = &console.vid;
	p_des->attr = static_cast<natw>(cc) << 8;
	vid_init();
}

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

extern "C" des_vid vid;

bool vid_init()
{
	des_vid *p_des = &console.vid;
	for (natl i = 0; i < VIDEO_SIZE; i++)
		p_des->video[i] = p_des->attr | ' ';
	cursore(p_des->indreg.iIND, p_des->indreg.iDAT,
		p_des->x, p_des->y);
	flog(LOG_INFO, "vid: video inizializzato");
	return true;
}

bool console_init()
{
	des_console *p_des = &console;

	if ( (p_des->mutex = sem_ini(1)) == 0xFFFFFFFF) {
		flog(LOG_ERR, "kbd: impossibile creare mutex");
		return false;
	}
	if ( (p_des->sincr = sem_ini(0)) == 0xFFFFFFFF) {
		flog(LOG_ERR, "kbd: impossibile creare sincr");
		return false;
	}
	return kbd_init() && vid_init();
}

// *)

////////////////////////////////////////////////////////////////////////////////
//                         GESTIONE FINESTRE                                  //
////////////////////////////////////////////////////////////////////////////////

const int MAX_SCREENX = 1280;
const int MAX_SCREENY = 1024;

natb* framebuffer = (natb*)0xfd000000;
natb doubled_framebuffer[MAX_SCREENX*MAX_SCREENY];	// Secondo buffer (quadruple buffering)
int column_changed_first = 0;
int column_changed_last = 0;
int line_changed_first = 0;				// quando devo copiare il doubled_framebuffer nel framebuffer voglio ottimizzare al massimo l'operazione di copia,
int line_changed_last = 0;			// quindi cerco di copiare solo quello che ho effettivamente modificato.

void inline put_pixel(natb * buffer, int x, int y, int MAX_X, int MAX_Y, natb col)
{
	if(x<MAX_X && y<MAX_Y && x>=0 && y>=0)
		buffer[MAX_X*y+x] = col;
}

int set_fontchar(natb* buff, int x, int y, int nchar, natb backColor)
{
	int row_off = nchar / 16;
	int col_off = nchar % 16;

	natb start = (16 - font_width[nchar])/2;
	natb end = start + font_width[nchar];

	for(int i=0; i<16; i++)
		for(int j=start; j<end; j++)
			if(font_bitmap[(row_off*16 + i)*256 + (j + col_off*16)] == 0x00)
				put_pixel(buff, x+j-start, y+i, MAX_SCREENX, MAX_SCREENY, backColor);
			else
				put_pixel(buff, x+j-start, y+i, MAX_SCREENX, MAX_SCREENY, font_bitmap[(row_off*16 + i)*256 + (j + col_off*16)] & 15);

	return font_width[nchar];
}

void set_fontstring(natb* buff, int x, int y, int bound_x, int bound_y, const char * str, natb backColor)
{
	int slength = (int)strlen(str);
	//int x_eff = ((i*16) % bound_x);
	//int y_eff = (i*16 / bound_x)*16;
	int x_eff = 0;
	int y_eff = 0;
	for(int i=0; i<slength; i++)
	{	
		if(str[i] != '\n')
			x_eff += set_fontchar(buff, x+x_eff, y+(y_eff*16), str[i], backColor);
		if((x_eff + 16) > bound_x || str[i] == '\n')
		{
			x_eff = 0;
			y_eff++;
		}
		
	}
}

// ----- user event
struct des_user_event
{
	user_event_type type;
	union
	{
		mouse_button button;
		int delta_z;
	};
	union
	{
		int rel_x;
	};
	union
	{
		int rel_y;
	};

	des_user_event * next;
};

// l'inserimento evento è fatto in testa
void event_push(des_user_event *& head, des_user_event * elem)
{
	if(elem==0)
		return;
	elem->next=head;
	head=elem;
}

// la rimozione è fatta in coda
void event_pop(des_user_event *& head, des_user_event *& elem)
{
	if(head==0)
		return;

	des_user_event *p=head, *q;
	for(q=head; q->next!=0; q=q->next)
		p=q;

	if(p==head)
		head=0;
	else
		p->next=0;

	elem=q;
}

// ----- des_window
const int MAX_WINDOWS_OBJECTS = 10;
struct des_window
{
	natb id;
	natb p_id;
	natb * render_buff;
	int size_x;
	int size_y;
	int pos_x;
	int pos_y;
	natb backColor;
	natb obj_count;
	windowObject * objects[MAX_WINDOWS_OBJECTS];
	des_user_event * event_list;
};

const natb PRIM_SHOW=0;
const natb PRIM_UPDATE_OBJECT=2;
const natb MOUSE_UPDATE_EVENT=10;
const natb MOUSE_Z_UPDATE_EVENT=11;
const natb MOUSE_MOUSEUP_EVENT=12;
const natb MOUSE_MOUSEDOWN_EVENT=13;

struct des_window_req
{
	natb w_id;
	natb p_id;

	//Sincronizzazione primitiva se richiesta
	bool to_sync;
	natb if_sync;

	natb act;
	union {
		windowObject * obj;
		int delta_x;
		int delta_z;
		mouse_button button;
	};

	union {
		int delta_y;
	};

	des_window_req()
	{
		to_sync = false;
		if_sync = sem_ini(0);
	}
};

const natb MAX_REQ_QUEUE=3;
struct des_windows_man
{
	//Finestre
	static const int MAX_WINDOWS = 5;
	static const int TOPBAR_HEIGHT = 20;
	natb windows_count;
	des_window windows_arr[MAX_WINDOWS];		//Finestre create
	int focus_wind;								//Finestra selezionata con mouse
	bool is_dragging;

	//Coda richieste primitive
	des_window_req req_queue[MAX_REQ_QUEUE];
	natb top;
	natb rear;
	
	//Semafori di sincronizzazione
	natb mutex;
	natb sync_notempty;
	natb sync_notfull;
};

des_windows_man win_man;

int windows_queue_insert(des_windows_man& win_cont, natb w_id, natb p_id, natb act, bool sync)
{
	//Controllo Coda piena
	if (win_cont.top == ((win_cont.rear - 1 + MAX_REQ_QUEUE) % MAX_REQ_QUEUE))
		return -1;

	natb resindex = win_cont.top;
	win_cont.req_queue[win_cont.top].w_id = w_id;
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

//Primitive
extern "C" int c_crea_finestra(unsigned int size_x, unsigned int size_y, unsigned int pos_x, unsigned int pos_y)
{
	sem_wait(win_man.mutex);

	if(win_man.windows_count >= win_man.MAX_WINDOWS)
	{
		sem_signal(win_man.mutex);
		return -1;
	}
	natb res_id = win_man.windows_count;
	win_man.windows_arr[win_man.windows_count].render_buff = new natb[size_x*size_y];
	win_man.windows_arr[win_man.windows_count].size_x = size_x;
	win_man.windows_arr[win_man.windows_count].size_y = size_y;
	win_man.windows_arr[win_man.windows_count].pos_x = pos_x;
	win_man.windows_arr[win_man.windows_count].pos_y = pos_y;
	win_man.windows_arr[win_man.windows_count].backColor = 0x01;
	win_man.windows_arr[win_man.windows_count].obj_count = 0;
	win_man.windows_arr[win_man.windows_count].event_list = 0;
	win_man.windows_count++;

	sem_signal(win_man.mutex);

	return res_id;
}

const int TOPBAR_HEIGHT = 20;
extern "C" void c_visualizza_finestra(int id, bool sync)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	int new_index;

	if(id >= win_man.MAX_WINDOWS || id<0)
		goto err;
	flog(LOG_INFO, "Inserimento richiesta di renderizzazione finestra");

	new_index = windows_queue_insert(win_man, id, 100, PRIM_SHOW, sync);
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

	des_window * wind;

	if(w_id >= win_man.MAX_WINDOWS || w_id<0)
		goto err;
	
	wind = &win_man.windows_arr[w_id];
	if(wind->obj_count >= MAX_WINDOWS_OBJECTS)
		goto err;

	switch(u_obj->TYPE)
	{
		case W_ID_LABEL:
			wind->objects[wind->obj_count] = new label(static_cast<u_label*>(u_obj));
		break;
		case W_ID_BUTTON:
			wind->objects[wind->obj_count] = new button(static_cast<u_button*>(u_obj));
		break;
		default:
			flog(LOG_INFO, "c_crea_oggetto: tipo oggetto %d errato", u_obj->TYPE);
			goto err;
	}

	flog(LOG_INFO, "c_crea_oggetto: oggetto %d di tipo %d creato su finestra %d", wind->obj_count, u_obj->TYPE, w_id);

	sem_signal(win_man.mutex);
	return wind->obj_count++;

err:	sem_signal(win_man.mutex);
	return -1;
}

extern "C" void c_aggiorna_oggetto(int w_id, int o_id, u_windowObject * u_obj, bool sync)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	int new_index;

	if(w_id >= win_man.MAX_WINDOWS || w_id<0)
		goto err;
	if(o_id >= win_man.windows_arr[w_id].obj_count || o_id<0)
		goto err;
	flog(LOG_INFO, "Inserimento richiesta di aggiornamento oggetto");

	//Copio prima il nuovo contenuto di u_windowObject nell'oggetto windowObject già creato
	switch(u_obj->TYPE)
	{
		case W_ID_LABEL:
			delete win_man.windows_arr[w_id].objects[o_id];
			win_man.windows_arr[w_id].objects[o_id] = new label(static_cast<u_label*>(u_obj));
		break;
		case W_ID_BUTTON:
			delete win_man.windows_arr[w_id].objects[o_id];
			win_man.windows_arr[w_id].objects[o_id] = new button(static_cast<u_button*>(u_obj));
		break;
	}

	new_index = windows_queue_insert(win_man, w_id, 100, PRIM_UPDATE_OBJECT, sync);
	if(new_index == -1)
	{ 	//Questa situazione non può accadere a causa del semaforo not_full, aggiungo codice di gestione
		//errore solo per rendere più robusto il codice
		flog(LOG_INFO, "Inserimento richiesta fallito");
		goto err;
	}

	win_man.req_queue[new_index].obj = win_man.windows_arr[w_id].objects[o_id];

	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notempty);
	if(sync)
		sem_wait(win_man.req_queue[new_index].if_sync);

	return;

//Gestione errori (sblocco mutex e sync su array)
err:	sem_signal(win_man.mutex);
	sem_signal(win_man.sync_notfull);
}

extern "C" des_user_event c_preleva_evento(int w_id)
{
	sem_wait(win_man.mutex);

	des_user_event event;
	event.type=NOEVENT;

	if(w_id >= win_man.MAX_WINDOWS || w_id<0)
		goto err;
	//flog(LOG_INFO, "c_preleva_evento: chiamata su finestra %d", w_id);

	if(win_man.windows_arr[w_id].event_list==0)
		goto err;

	des_user_event * popped_event;
	event_pop(win_man.windows_arr[w_id].event_list, popped_event);
	event=*popped_event;
	flog(LOG_INFO, "c_preleva_evento: ho trovato un evento di tipo %d", popped_event->type);
	delete popped_event;

err:
	sem_signal(win_man.mutex);
	return event;
}

const natb WIN_BACKGROUND_COLOR = 0x36;
const natb WIN_X_COLOR = 0x28;
const natb WIN_TOPBAR_COLOR = 0x03;

void *gr_memcpy_safe_onvideobuffer(natb * base_buff, void *__restrict dest, const void *__restrict src, unsigned long int n)
{
	if((static_cast<natb*>(dest)+n) > base_buff+(MAX_SCREENX*MAX_SCREENY) || static_cast<natb*>(dest) < base_buff)
	{
		flog(LOG_INFO, "gr_memcpy_safe_framebuffer: anticipato buffer overflow, salto scrittura", dest, src, n);
		return 0;
	}

	char *s1 = static_cast<char*>(dest);
	const char *s2 = static_cast<const char*>(src);
	for(; 0<n; --n)*s1++ = *s2++;
	return dest;
}

void inline update_framebuffer_linechanged(int column_first, int column_last, int line_first, int line_last)
{
	if(line_first < line_changed_first)
		line_changed_first=line_first;
	if(line_last > line_changed_last)
		line_changed_last=line_last;

	if(column_first < column_changed_first)
		column_changed_first=column_first;
	if(column_last > column_changed_last)
		column_changed_last=column_last;
}

void inline update_framebuffer()
{
	// le variabili non devono sforare i margini dello schermo, altrimentri avremmo un pesante buffer overflow
	if(line_changed_first < 0)
		line_changed_first=0;
	if(line_changed_last >= MAX_SCREENY)
		line_changed_last=MAX_SCREENY-1;
	if(column_changed_first < 0)
		column_changed_first=0;
	if(column_changed_last >= MAX_SCREENX)
		column_changed_last=MAX_SCREENX-1;

	flog(LOG_INFO, "update_framebuffer: column_first %d column_last %d line_first %d line_last %d", column_changed_first, column_changed_last, line_changed_first, line_changed_last);

	for(int j=line_changed_first; j<line_changed_last; j++)
		gr_memcpy_safe_onvideobuffer(framebuffer, framebuffer + j*MAX_SCREENX + column_changed_first, doubled_framebuffer + j*MAX_SCREENX + column_changed_first, column_changed_last-column_changed_first);

	line_changed_first=MAX_SCREENY-1;
	line_changed_last=0;
	column_changed_first=MAX_SCREENX-1;
	column_changed_last=0;
}

void inline set_background(natb* buff)
{
	memset(buff, WIN_BACKGROUND_COLOR, MAX_SCREENX*MAX_SCREENY);
	update_framebuffer_linechanged(0, MAX_SCREENX, 0, MAX_SCREENY);
}

void print_palette(natb* buff, int x, int y)
{
	int row=0;
	for(natb i=0; i<0xFF; i++)
	{
		for(int k=0; k<10; k++)
			for(int j=0; j<10; j++)
				put_pixel(buff, x+(i%16)*10+j, y+row*10+k, MAX_SCREENX, MAX_SCREENY, i);
		if(i%16==0 && i!=0)
			row++;
	}

	update_framebuffer_linechanged(x, x+16, y, y+16);
}

void inline clean_window_buffer(des_window * wind)
{
	memset(wind->render_buff, wind->backColor, wind->size_x*wind->size_y);
}

void inline render_topbar_onvideobuffer(natb* buff, des_window * wind)
{
	//non devo sforare i bound dello schermo
	int max_x=(wind->pos_x+wind->size_x>=MAX_SCREENX) ? MAX_SCREENX-wind->pos_x : wind->size_x;
	int max_y=(wind->pos_y+TOPBAR_HEIGHT>=MAX_SCREENY) ? MAX_SCREENY-wind->pos_y : TOPBAR_HEIGHT;
	if(max_x<=0 || max_y<=0)
		return;

	//renderizzo sfondo TOPBAR
	for(int i=0; i<max_x; i++)
		for(int j=0; j<max_y; j++)
			if(i <= wind->size_x-4 && i>=wind->size_x-20 && j>=2 && j<=17)
				put_pixel(buff, wind->pos_x + i, wind->pos_y + j, MAX_SCREENX, MAX_SCREENY, WIN_X_COLOR);
			else
				put_pixel(buff, wind->pos_x + i, wind->pos_y + j, MAX_SCREENX, MAX_SCREENY, WIN_TOPBAR_COLOR);

	//renderizzo lettera X
	set_fontchar(buff,wind->pos_x + wind->size_x-15, wind->pos_y + 2, 'x', WIN_X_COLOR);

	update_framebuffer_linechanged(wind->pos_x, wind->pos_x+max_x, wind->pos_y, wind->pos_y+max_y);
}

void inline render_window_onvideobuffer(natb* buff, des_window * wind)
{
	// non devo sforare i bound dello schermo
	int max_x=(wind->pos_x+wind->size_x>=MAX_SCREENX) ? MAX_SCREENX-wind->pos_x : wind->size_x;
	int max_y=(wind->pos_y+TOPBAR_HEIGHT+wind->size_y>=MAX_SCREENY) ? MAX_SCREENY-(wind->pos_y+TOPBAR_HEIGHT) : wind->size_y;
	if(max_x<=0 || max_y<=0)
		return;

	// copio il buffer della finestra su quello video
	for(int j=0; j<max_y; j++)
		gr_memcpy_safe_onvideobuffer(buff, buff + wind->pos_x + (j+wind->pos_y+TOPBAR_HEIGHT)*MAX_SCREENX, wind->render_buff + j*wind->size_x, max_x);

	update_framebuffer_linechanged(wind->pos_x, wind->pos_x+max_x, wind->pos_y + TOPBAR_HEIGHT, wind->pos_y + max_y + TOPBAR_HEIGHT);
}

void inline clean_window_onvideobuffer(natb* buff, des_window * wind)
{
	// non devo sforare i bound dello schermo
	int max_x=(wind->pos_x+wind->size_x>=MAX_SCREENX) ? MAX_SCREENX-wind->pos_x : wind->size_x;
	int max_y=(wind->pos_y+TOPBAR_HEIGHT+wind->size_y>=MAX_SCREENY) ? MAX_SCREENY-wind->pos_y : wind->size_y+TOPBAR_HEIGHT;
	if(max_x<=0 || max_y<=0)
		return;

	// pulisco corpo della finestra e topbar
	for(int j=0; j<max_y; j++)
		memset(buff + wind->pos_x + (j+wind->pos_y)*MAX_SCREENX, WIN_BACKGROUND_COLOR, max_x);
	update_framebuffer_linechanged(wind->pos_x, wind->pos_x+max_x, wind->pos_y, wind->pos_y + max_y);
}

void graphic_visualizza_finestra(int id)
{
	des_window * wind = &win_man.windows_arr[id];

	//inizializzo buffer video della finestra
	clean_window_buffer(wind);

	//renderizzo topbar su secondo buffer
	render_topbar_onvideobuffer(doubled_framebuffer, wind);
	render_window_onvideobuffer(doubled_framebuffer, wind);
}

struct des_cursor
{
	int old_x;
	int old_y;
	int x;
	int y;
};

void render_mousecursor_onbuffer(natb* buff, des_cursor* cursor)
{
	int bound_x=(cursor->old_x+32>=MAX_SCREENX) ? MAX_SCREENX-cursor->old_x : 32;
	int bound_y=(cursor->old_y+32>=MAX_SCREENY) ? MAX_SCREENY-cursor->old_y : 32;
	for(int i=0; i<bound_x; i++)
		for(int j=0; j<bound_y; j++)
			put_pixel(buff, cursor->old_x+i, cursor->old_y+j, MAX_SCREENX, MAX_SCREENY, WIN_BACKGROUND_COLOR);
			//buff[(i+cursor->old_y)*MAX_SCREENX+(j+cursor->old_x)]=WIN_BACKGROUND_COLOR;

	update_framebuffer_linechanged(cursor->old_x, cursor->old_x+bound_x, cursor->old_y, cursor->old_y+bound_y);

	//devo rigenerare le finestre sottostanti alla vecchia posizione del cursore
	for(int i=0; i<win_man.windows_count; i++)
	{
		if(i==win_man.focus_wind)
			continue;
		des_window * wind = &win_man.windows_arr[i];
		if((cursor->old_x + 32 > wind->pos_x) && (cursor->old_x < wind->pos_x + wind->size_x) &&
			(cursor->old_y + 32 > wind->pos_y) && (cursor->old_y < wind->pos_y + wind->size_y + TOPBAR_HEIGHT))
		{	// condizione per individuare se il cursore condivide parti di framebuffer con altre finestre
			render_topbar_onvideobuffer(doubled_framebuffer, wind);
			render_window_onvideobuffer(doubled_framebuffer, wind);	//copio nuovamente il bitmap della finestra sul buffer video secondario
		}
	}

	//devo rigenerare sempre per ultima, la finestra con focus
	if(win_man.focus_wind!=-1)
	{
		des_window * wind = &win_man.windows_arr[win_man.focus_wind];
		if((cursor->old_x + 32 > wind->pos_x) && (cursor->old_x < wind->pos_x + wind->size_x) &&
			(cursor->old_y + 32 > wind->pos_y) && (cursor->old_y < wind->pos_y + wind->size_y + TOPBAR_HEIGHT))
		{	//la finestra con focus va renderizzata solo se il cursore si sovrappone ad essa
			render_topbar_onvideobuffer(doubled_framebuffer, wind);
			render_window_onvideobuffer(doubled_framebuffer, wind);
		}
	}

	bound_x=(cursor->x+32>=MAX_SCREENX) ? MAX_SCREENX-cursor->x : 32;
	bound_y=(cursor->y+32>=MAX_SCREENY) ? MAX_SCREENY-cursor->y : 32;
	for(int i=0; i<bound_x; i++)
		for(int j=0; j<bound_y; j++)
			if(main_cursor[j*32+i]!=COLOR_TRASP)
				put_pixel(buff, cursor->x+i, cursor->y+j, MAX_SCREENX, MAX_SCREENY, main_cursor[j*32+i]);
				//buff[(i+cursor->y)*MAX_SCREENX+(j+cursor->x)]=main_cursor[i*32+j];

	update_framebuffer_linechanged(cursor->x, cursor->x+bound_x, cursor->y, cursor->y+bound_y);
	//update_framebuffer_linechanged(0,1000,0,1000);
}

void renderobject_onwindow(int w_id, windowObject * w_obj, des_cursor* main_cursor)
{
	if(w_id >= win_man.MAX_WINDOWS || w_id<0 || w_obj==0)
		return;

	flog(LOG_INFO, "renderobject_onwindow: renderizzo oggetto...");

	des_window * wind = &win_man.windows_arr[w_id];

	// renderizzo oggetto richiesto
	w_obj->render();

	// pulisco buffer finestra
	clean_window_buffer(wind);

	// ristampo tutti gli oggetti già renderizzati sul buffer della finestra
	for(int i=0; i<wind->obj_count; i++)
	{
		windowObject * obj = wind->objects[i];
		if(!(obj->is_rendered))
			continue;

		int max_x = (obj->pos_x + obj->size_x > wind->size_x) ? wind->size_x - obj->pos_x : obj->size_x;
		int max_y = (obj->pos_y + obj->size_y > wind->size_y) ? wind->size_y - obj->pos_y : obj->size_y;
		if(max_x<=0 || max_y<=0)
			continue;

		for(int i=0; i<max_x; i++)
			for(int j=0; j<max_y; j++)
				put_pixel(wind->render_buff, obj->pos_x+i, obj->pos_y+j, wind->size_x, wind->size_y, obj->render_buff[j*obj->size_x+i]);
	}

	// copio il buffer della finestra su quello video
	render_topbar_onvideobuffer(doubled_framebuffer, wind);
	render_window_onvideobuffer(doubled_framebuffer, wind);

	update_framebuffer_linechanged(wind->pos_x, wind->pos_x+wind->size_x, wind->pos_y, wind->pos_y + wind->size_y + TOPBAR_HEIGHT);

	// renderizzo anche il mouse
	render_mousecursor_onbuffer(doubled_framebuffer, main_cursor);
	
	flog(LOG_INFO, "renderobject_onwindow: renderizzazione completata");
}

int check_topbar_oncoords(int curs_x, int curs_y)
{
	for(int i=0; i<win_man.windows_count; i++)
	{
		des_window * wind = &win_man.windows_arr[i];
		if(curs_x>wind->pos_x && curs_x<wind->pos_x+wind->size_x && curs_y>wind->pos_y && curs_y<wind->pos_y+TOPBAR_HEIGHT)
			return i;
	}
	return -1;
}

int check_window_oncoords(int curs_x, int curs_y)
{
	for(int i=0; i<win_man.windows_count; i++)
	{
		des_window * wind = &win_man.windows_arr[i];
		if(curs_x>wind->pos_x && curs_x<wind->pos_x+wind->size_x && curs_y>wind->pos_y && curs_y<wind->pos_y+wind->size_y+TOPBAR_HEIGHT)
			return i;
	}
	return -1;
}

void move_window(int w_id, int to_x, int to_y)
{
	des_window * wind = &win_man.windows_arr[w_id];

	// devo pulire l'area di framebuffer in cui era presente la finestra
	clean_window_onvideobuffer(doubled_framebuffer, wind);

	// devo renderizzare nuovamente le finestre su cui quella da spostare é sovrapposta
	for(int i=0; i<win_man.windows_count; i++)
	{
		if(w_id==i)
			continue;

		des_window * otherw = &win_man.windows_arr[i];
		// condizione per individuare se la finestra condivide parti di framebuffer con altre finestre
		if((wind->pos_x + wind->size_x > otherw->pos_x) && (wind->pos_x < otherw->pos_x + otherw->size_x) &&
			(wind->pos_y + wind->size_y + TOPBAR_HEIGHT > otherw->pos_y) && (wind->pos_y < otherw->pos_y + otherw->size_y + TOPBAR_HEIGHT))
		{
			render_topbar_onvideobuffer(doubled_framebuffer, otherw);
			render_window_onvideobuffer(doubled_framebuffer, otherw);	//copio nuovamente il bitmap della finestra sul buffer video secondario
		}
	}

	// faccio in modo che la finestra non vada mai oltre i bound dello schermo
	if(to_x<0)
		wind->pos_x=0;
	else if(to_x+wind->size_x>=MAX_SCREENX)
		wind->pos_x=MAX_SCREENX-1-wind->size_x;
	else
		wind->pos_x = to_x;
	if(to_y<0)
		wind->pos_y=0;
	else if(to_y+wind->size_y+TOPBAR_HEIGHT>=MAX_SCREENY)
		wind->pos_y=MAX_SCREENY-1-wind->size_y-TOPBAR_HEIGHT;
	else
		wind->pos_y = to_y;

	render_topbar_onvideobuffer(doubled_framebuffer, wind);
	render_window_onvideobuffer(doubled_framebuffer, wind);
}

void mouse_notify_move(int delta_x, int delta_y)
{
	sem_wait(win_man.sync_notfull);
	sem_wait(win_man.mutex);

	flog(LOG_INFO, "Inserimento richiesta di aggiornamento posizione mouse (x,y)");

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

	flog(LOG_INFO, "Inserimento richiesta di aggiornamento coordinata z mouse");

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

	flog(LOG_INFO, "Inserimento richiesta di aggiornamento stato button mouse");

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

inline bool coords_on_window(des_window *wind, int abs_x, int abs_y)
{
	if(abs_x > wind->pos_x && abs_x < wind->pos_x + wind->size_x &&
			abs_y > wind->pos_y + TOPBAR_HEIGHT && abs_y < wind->pos_y + wind->size_y + TOPBAR_HEIGHT)
		return true;
	return false;
}

void user_add_mousemovez_event_onfocused(int delta_z, int abs_x, int abs_y)
{
	if(win_man.focus_wind==-1)
	{
		flog(LOG_INFO, "user_add_mousemovez_event_onfocused: nessuna finestra in focus");
		return;
	}

	//metto l'evento nella coda degli eventi della finestra a cui è rivolto
	des_window * wind_ev = &win_man.windows_arr[win_man.focus_wind];
	if(!coords_on_window(wind_ev, abs_x, abs_y))
	{
		flog(LOG_INFO, "user_add_mousemovez_event_onfocused: coordinata fuori da finestra");
		return;
	}
	des_user_event * event = new des_user_event();
	event->type=USER_EVENT_MOUSEZ;
	event->delta_z=delta_z;
	event->rel_x = abs_x - wind_ev->pos_x;
	event->rel_y = abs_y - wind_ev->pos_y - TOPBAR_HEIGHT;
	event_push(wind_ev->event_list, event);
}

void user_add_mousebutton_event_onfocused(user_event_type event_type, mouse_button butt, int abs_x, int abs_y)
{
	if(win_man.focus_wind==-1)
		return;

	//metto l'evento nella coda degli eventi della finestra a cui è rivolto
	des_window * wind_ev = &win_man.windows_arr[win_man.focus_wind];
	if(!coords_on_window(wind_ev, abs_x, abs_y))
		return;
	des_user_event * event = new des_user_event();
	event->type=event_type;
	event->button=butt;
	event->rel_x = abs_x - wind_ev->pos_x;
	event->rel_y = abs_y - wind_ev->pos_y - TOPBAR_HEIGHT;
	event_push(wind_ev->event_list, event);
}

void main_windows_manager(int n)
{
	set_background(doubled_framebuffer);
	print_palette(doubled_framebuffer, 900,450);
	update_framebuffer();

	//patina di debug
	memset(framebuffer, 0x80, MAX_SCREENX*MAX_SCREENY);

	des_cursor main_cursor = {0,0,0,0};
	win_man.focus_wind=-1;
	win_man.is_dragging=false;

	while(true)
	{
		sem_wait(win_man.sync_notempty);
		sem_wait(win_man.mutex);

		flog(LOG_INFO, "main_windows_manager: risvegliato");
		des_window_req newreq;
		if(!windows_queue_extract(win_man, newreq))
		{
			flog(LOG_ERR, "main_windows_manager: Errore nell'estrazione delle richieste.");
			abort_p();
		}

		switch(newreq.act)
		{
			case PRIM_SHOW:
				flog(LOG_INFO, "act(%d): Processo richiesta di renderizzazione finestra per finestra %d", newreq.act, newreq.w_id);
				graphic_visualizza_finestra(newreq.w_id);
				if(newreq.to_sync)
					sem_signal(newreq.if_sync);
			break;
			case PRIM_UPDATE_OBJECT:
				flog(LOG_INFO, "act(%d): Processo richiesta di aggiornamento oggetto per finestra %d", newreq.act, newreq.w_id);
				renderobject_onwindow(newreq.w_id, newreq.obj, &main_cursor);
				if(newreq.to_sync)
					sem_signal(newreq.if_sync);
			break;
			case MOUSE_UPDATE_EVENT:
			{
					flog(LOG_INFO, "act(%d): Processo richiesta di aggiornamento dati mouse %d", newreq.act, newreq.w_id);
					main_cursor.old_x=main_cursor.x;
					main_cursor.old_y=main_cursor.y;
					main_cursor.x+=newreq.delta_x;
					main_cursor.y+=newreq.delta_y;
	
					//se si è già verificato il mouse_up, significa che ora devo trascinare la finestra
					if(win_man.is_dragging && win_man.focus_wind!=-1)
					{
						des_window * f_wind = &win_man.windows_arr[win_man.focus_wind];
						move_window(win_man.focus_wind, f_wind->pos_x + newreq.delta_x, f_wind->pos_y + newreq.delta_y);
					}
					//sposto il cursore sulla posizione nuova
					render_mousecursor_onbuffer(doubled_framebuffer, &main_cursor);
			}
			break;
			case MOUSE_Z_UPDATE_EVENT:
				user_add_mousemovez_event_onfocused(newreq.delta_z, main_cursor.x, main_cursor.y);
			break;
			case MOUSE_MOUSEDOWN_EVENT:
				if(newreq.button==LEFT)
				{	
					//se ho cliccato su una topbar, allora significa che devo iniziare a trascinare
					win_man.focus_wind = check_topbar_oncoords(main_cursor.x, main_cursor.y);
					if(win_man.focus_wind!=-1)
						win_man.is_dragging=true;
					else //altrimenti controllo se ho cliccato sul corpo di una finestra e aggiorno il focus
						win_man.focus_wind = check_window_oncoords(main_cursor.x, main_cursor.y);
				}
				user_add_mousebutton_event_onfocused(USER_EVENT_MOUSEDOWN, newreq.button, main_cursor.x, main_cursor.y);
			break;
			case MOUSE_MOUSEUP_EVENT:
			{
				if(newreq.button==LEFT)
					win_man.is_dragging=false;

				user_add_mousebutton_event_onfocused(USER_EVENT_MOUSEUP, newreq.button, main_cursor.x, main_cursor.y);
			}
			break;
		}

		//copio il buffer secondario sulla memoria video
		update_framebuffer();

		sem_signal(win_man.mutex);
		sem_signal(win_man.sync_notfull);
	}
		
}

bool windows_init()
{
	//creare un processo che si occupi della stampa delle finestre
	win_man.mutex = sem_ini(1);
	win_man.sync_notfull = sem_ini(MAX_REQ_QUEUE - 1);
	win_man.sync_notempty = sem_ini(0);

	win_man.windows_count = 0;
	win_man.top = 0;
	win_man.rear = 0;
	
	if(win_man.sync_notfull==-1 || win_man.sync_notempty==-1)
	{
		flog(LOG_ERR, "attivazione del gestore delle finestre fallita.");
		return false;
	}

	flog(LOG_INFO, "attivo gestore delle finestre...");
	activate_p(main_windows_manager, 0, 200, LIV_SISTEMA);

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

const int MOUSE_DELTAX_DIVIDER=2;
const int MOUSE_DELTAY_DIVIDER=2;

void mouse_handler(int i)
{
	int mouse_count=0;
	bool discard_one_packet=false;
	natb mouse_bytes[4];

	while(true)
	{
		flog(LOG_INFO, "mouse_count %d",mouse_count);
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
				flog(LOG_INFO, "mouse_driver: invalid packets alignment");
				discard_one_packet=true;
				goto fine;
			}

			// condizione di overflow (cestino il dato)
			if ((mouse_bytes[0] & 0x80) || (mouse_bytes[0] & 0x40))
			{
				flog(LOG_INFO, "mouse_driver: Mouse Overflow!");
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
				flog(LOG_INFO, "mouse_driver: Middle button is pressed!");
				ps2_mouse.middle_click=true;
				mouse_notify_mousebutton_event(MOUSE_MOUSEDOWN_EVENT, MIDDLE);
			}
			else if (!(mouse_bytes[0] & 0x4) && ps2_mouse.middle_click)
			{
				flog(LOG_INFO, "mouse_driver: Middle button is released!");
				ps2_mouse.middle_click=false;
				mouse_notify_mousebutton_event(MOUSE_MOUSEUP_EVENT, MIDDLE);
			}

			if (mouse_bytes[0] & 0x2 && !ps2_mouse.right_click)
			{
				flog(LOG_INFO, "mouse_driver: Right button is pressed!");
				ps2_mouse.right_click=true;
				mouse_notify_mousebutton_event(MOUSE_MOUSEDOWN_EVENT, RIGHT);
			}
			else if (!(mouse_bytes[0] & 0x2) && ps2_mouse.right_click)
			{
				flog(LOG_INFO, "mouse_driver: Right button is released!");
				ps2_mouse.right_click=false;
				mouse_notify_mousebutton_event(MOUSE_MOUSEUP_EVENT, RIGHT);
			}

			if (mouse_bytes[0] & 0x1 && !ps2_mouse.left_click)
			{
				flog(LOG_INFO, "mouse_driver: Left button is pressed!");
				ps2_mouse.left_click=true;
				mouse_notify_mousebutton_event(MOUSE_MOUSEDOWN_EVENT, LEFT);
			}
			else if (!(mouse_bytes[0] & 0x1) && ps2_mouse.left_click)
			{
				flog(LOG_INFO, "mouse_driver: Left button is released!");
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
	activate_pe(mouse_handler, 0, 2, 0, 12);

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

	/*if (!console_init())
		abort_p();*/
	if(!windows_init())
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
