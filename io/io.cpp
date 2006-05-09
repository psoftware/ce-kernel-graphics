// io.cpp
//
#include "costanti.h"
#define BOCHS
////////////////////////////////////////////////////////////////////////////////
//COSTANTI                                   //
////////////////////////////////////////////////////////////////////////////////


// Interruzione hardware a priorita' minore 
//
const int IRQ_MAX = 15;

// Priorita' base dei processi esterni
const int PRIO_ESTERN_BASE = 0x400;


typedef char* ind_b;		// indirizzo di un byte (una porta)
typedef short* ind_w;		// indirizzo di una parola
typedef int* ind_l;		// indirizzo di una parola lunga

////////////////////////////////////////////////////////////////////////////////
//                        CHIAMATE DI SISTEMA USATE                           //
////////////////////////////////////////////////////////////////////////////////

extern "C" void terminate_p(void);
extern "C" int sem_ini(int val);
extern "C" void sem_wait(int sem);
extern "C" void sem_signal(int sem);
extern "C" short activate_p(void f(int), int a, int prio, char liv);

////////////////////////////////////////////////////////////////////////////////
//               INTERFACCIA OFFERTA DAL NUCLEO AL MODULO DI IO               //
////////////////////////////////////////////////////////////////////////////////

extern "C" short activate_pe(void f(int), int a, int prio, char liv, estern_id interf);

enum controllore { master=0, slave=1 };
extern "C" void nwfi(controllore c);

extern "C" bool verifica_area(void *area, unsigned int dim, bool write);
extern "C" void fill_gate(int gate, void (*f)(void), int tipo, int dpl);
extern "C" void abort_p();

extern "C" void writevid_n(int off, const unsigned char* vett, int quanti);
extern "C" void attrvid_n(int off, int quanti, unsigned char bg, unsigned char fg, bool blink);

////////////////////////////////////////////////////////////////////////////////
//                      FUNZIONI GENERICHE DI SUPPORTO                        //
////////////////////////////////////////////////////////////////////////////////

// ingresso di un byte da una porta di IO
extern "C" void inputb(ind_b reg, char &a);

// uscita di un byte su una porta di IO
extern "C" void outputb(char a, ind_b reg);

// ingresso di una word da una porta di IO
extern "C" void inputw(ind_b reg, short &a);

// uscita di una word su una porta di IO
extern "C" void outputw(short a, ind_b reg);

// ingresso di una stringa di word da un buffer di IO
extern "C" void inputbuffw(ind_b reg, short *a,short n);

// uscita di una stringa di word su un buffer di IO
extern "C" void outputbuffw(short *a, ind_b reg,short n);

////////////////////////////////////////////////////////////////////////////////
//                    GESTIONE DELLE INTERFACCE SERIALI                       //
////////////////////////////////////////////////////////////////////////////////

const int S = 2;

extern "C" void go_inputse(ind_b i_ctr);
extern "C" void halt_inputse(ind_b i_ctr);
extern "C" void go_outputse(ind_b i_ctr);
extern "C" void halt_outputse(ind_b i_ctr);

enum funz { input_n, input_ln, output_n, output_0 };

struct interfse_reg {
	ind_b iRBR;
	ind_b iTHR;
	ind_b iLSR;
	ind_b iIER;
	ind_b iIIR;
};

struct des_se {
	interfse_reg indreg;
	int mutex;
	int sincr;
	int cont;
	ind_b punt;
	funz funzione;
	char stato;
};

extern "C" des_se com[S];

void startse_in(des_se *p_des, char vetti[], int quanti, funz op)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->funzione = op;
	go_inputse(p_des->indreg.iIER);
}

extern "C" void c_readse_n(int serial, char vetti[], int quanti, char &errore)
{
	des_se *p_des;

	if(serial < 0 || serial >= S || !verifica_area(vetti, quanti, true) ||
			!verifica_area(&errore, sizeof(char), true))
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, quanti, input_n);
	sem_wait(p_des->sincr);
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

extern "C" void c_readse_ln(int serial, char vetti[], int &quanti, char &errore)
{
	des_se *p_des;

	if(serial < 0 || serial >= S ||
			!verifica_area(&quanti, sizeof(int), true) ||
			!verifica_area(vetti, quanti, true) ||
 			!verifica_area(&errore, sizeof(char), true))
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, 80, input_ln);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

void input_com(int h)
{
	char c, s;
	bool fine;
	des_se *p_des;

	p_des = &com[h];

	for(;;) {
		fine = false;
		halt_inputse(p_des->indreg.iIER);
		inputb(p_des->indreg.iRBR, c);
		inputb(p_des->indreg.iLSR, s);

		p_des->stato = s&0x1e;
		if(p_des->stato != 0)
			fine = true;
		else {
			if(p_des->funzione == input_n) {
				*p_des->punt = c;
				p_des->punt++;
				p_des->cont--;
				if(p_des->cont == 0)
					fine = true;
			} else if(p_des->funzione == input_ln)
				if(c == '\r' || c == '\n') {
					fine = true;
					p_des->cont = 80 - p_des->cont;
				} else {
					*p_des->punt = c;
					p_des->punt++;
					p_des->cont--;
					if(p_des->cont == 0) {
						fine = true;
						p_des->cont = 80;
					}
				}
		}

		if(fine == true) {
			*p_des->punt = 0;	// manca *
			sem_signal(p_des->sincr);
		} else
			go_inputse(p_des->indreg.iIER);

		nwfi(master); // sia com1 che com2 sono sul master
	}
}

// forza l' output del primo byte, sembra che l' interfaccia emulata da
//  Bochs presenti la richiesta di interruzione solo quando THR si svuota
//  dopo l' invio del primo byte
//
void kickse_out(des_se *p_des)
{
	char c = *p_des->punt;
	bool fine = false;

	p_des->punt++;

	if(p_des->funzione == output_n) {
		p_des->cont--;
		if(p_des->cont == 0) {
			halt_outputse(p_des->indreg.iIER);
			fine = true;
		}

		outputb(c, p_des->indreg.iTHR);
	} else if(p_des->funzione == output_0) {
		if(c == 0) {
			fine = true;
			halt_outputse(p_des->indreg.iIER);
		} else {
			p_des->cont++;
			outputb(c, p_des->indreg.iTHR);
		}
	}

	if(fine == true)
		sem_signal(p_des->sincr);
}

void startse_out(des_se *p_des, char vetto[], int quanti, funz op)
{
	p_des->cont = quanti;
	p_des->punt = vetto;
	p_des->funzione = op;
	go_outputse(p_des->indreg.iIER);
#ifdef BOCHS
	kickse_out(p_des);
#endif
}

extern "C" void c_writese_n(int serial, char vetto[], int quanti)
{
	des_se *p_des;

	if(serial < 0 || serial >= S || !verifica_area(vetto, quanti, false))
		return;

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, quanti, output_n);
	sem_wait(p_des->sincr);
	sem_signal(p_des->mutex);
}

extern "C" void c_writese_0(int serial, char vetto[], int &quanti)
{
	des_se *p_des;

	if(serial < 0 || serial >= S ||
			!verifica_area(&quanti, sizeof(int), true) ||
			!verifica_area(vetto, quanti, false))
		return;

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, 0, output_0);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	sem_signal(p_des->mutex);
}

void output_com(int h)
{
	char c;
	bool fine;
	des_se *p_des;

	p_des = &com[h];	// como

	for(;;) {
		fine = false;
		if(p_des->funzione == output_n) {
			p_des->cont--;
			if(p_des->cont == 0) {
				fine = true;
				halt_outputse(p_des->indreg.iIER);
			}

			c = *p_des->punt;
			outputb(c, p_des->indreg.iTHR);
			p_des->punt++;
		} else if(p_des->funzione == output_0) {
			c = *p_des->punt;
			if(c == 0) {
				fine = true;
				halt_outputse(p_des->indreg.iIER);
			} else {
				outputb(c, p_des->indreg.iTHR);
				p_des->cont++;
				p_des->punt++;
			}
		}

		if(fine == true)
			sem_signal(p_des->sincr);

		nwfi(master); // sia com1 che com2 sono sul master
	}
}

extern "C" void com_setup(void);

const estern_id com_id[2][2] = {
	{ com1_in, com1_out },
	{ com2_in, com2_out }
};

// Interruzione hardware della seconda interfaccia seriale
//
const int COM2_IRQ = 4;

int com_init()
{
	des_se *p_des;
	short id;
	int i, com_base_prio = PRIO_ESTERN_BASE + IRQ_MAX - COM2_IRQ;

	com_setup();

	for(i = 0; i < S; ++i) {
		p_des = &com[i];

		if ( (p_des->mutex = sem_ini(1)) == 0)
			return -201;
		if ( (p_des->sincr = sem_ini(0)) == 0)
			return -202;

		id = activate_pe(input_com, i, com_base_prio - i, LIV_SISTEMA, com_id[i][0]);
		if (id == 0)
			return -203;

		id = activate_pe(output_com, i, com_base_prio - i, LIV_SISTEMA, com_id[i][1]);
		if (id == 0)
			return -204;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//                         GESTIONE DELLA TASTIERA                            //
////////////////////////////////////////////////////////////////////////////////

#define KBD_MOD_NR	2		// numero di modificatori usati
#define KBD_COD_NR	127		// numero di codici

// modificatori
enum kbd_mod {
	KBD_MOD_NONE = 0,
	KBD_MOD_SHIFT = 1,
	KBD_MOD_ALT = 2,
	KBD_MOD_ALTGR = 3,
	KBD_MOD_ALTSH = 4,
	KBD_MOD_CTRL = 5
};

#define KBD_SHIFT	(KBD_MOD_SHIFT << 8)
#define KBD_ALT		(KBD_MOD_ALT << 8)
#define KBD_ALTGR	(KBD_MOD_ALTGR << 8)
#define KBD_ALTSH	(KBD_MOD_ALTSH << 8)
#define KBD_CTRL	(KBD_MOD_CTRL << 8)

#define KBD_MOD_MASK	0x0f00

#define KBD_IS_MOD(c)	((c&KBD_MOD_MASK) != 0)
#define KBD_MOD(c)	((c&KBD_MOD_MASK) >> 8)

// tasti funzione
#define KBD_F1		0x1000
#define KBD_F2		0x2000
#define KBD_F3		0x3000
#define KBD_F4		0x4000
#define KBD_F5		0x5000
#define KBD_F6		0x6000
#define KBD_F7		0x7000
#define KBD_F8		0x8000
#define KBD_F9		0x9000
#define KBD_F10		0xa000
#define KBD_F11		0xb000
#define KBD_F12		0xc000

#define KBD_FNUM(code)	(((code) - KBD_F1) >> 12)

#define KBD_CANC	0xd000

// tabella di corrispondenza tra codici e caratteri ascii (tastiera italiana)
static unsigned short kbd_keymap[KBD_MOD_NR][KBD_COD_NR] = {
	{ 0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
	'9', '0', '\'', 141, '\b', '\t', 'q', 'w', 'e', 'r',
	't', 'y', 'u', 'i', 'o', 'p', 138, '+', '\n', KBD_CTRL,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 149,
	133, '\\', KBD_SHIFT, 151, 'z', 'x', 'c', 'v', 'b', 'n',
	'm', ',', '.', '-', KBD_SHIFT, '*', KBD_ALT, ' ', 0, KBD_F1,
	KBD_F2, KBD_F3, KBD_F4, KBD_F5, KBD_F6, KBD_F7, KBD_F8, KBD_F9,
		KBD_F10, 0,
	0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
	0, 0, 0, KBD_CANC, '\n', 0, 0, KBD_F11, KBD_F12, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0 },

	{ 0, 27, '!', '"', 156, '$', '%', '&', '/', '(',
	')', '=', '?', '^', 0, 0, 'Q', 'W', 'E', 'R',
	'T', 'Y', 'U', 'I', 'O', 'P', 130, '*', 0, KBD_CTRL,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 135,
	248, '|', KBD_SHIFT, 21, 'Z', 'X', 'C', 'V', 'B', 'N',
	'M', ';', ':', '_', KBD_SHIFT, '*', KBD_ALT, ' ', 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, '>', 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0 }
};

// maschere per il campo flags
enum {
	KBD_FLAG_NONE = 0,
	KBD_FLAG_SHIFT = 0x100,
	KBD_FLAG_ALT = 0x200,
	KBD_FLAG_CTRL = 0x400
};


// flag in corrispondenza dei modificatori
const int kbd_modflag_tab[6] = {
	KBD_FLAG_NONE,
	KBD_FLAG_SHIFT,
	KBD_FLAG_ALT,
	KBD_FLAG_ALT,
	KBD_FLAG_NONE,
	KBD_FLAG_CTRL
};

// indice nella keymap in base a flags
inline int KBD_SC_IDX(int flags)
{
	return (flags)&KBD_FLAG_SHIFT ?
		KBD_MOD_SHIFT: KBD_MOD_NONE;
}


extern "C" void abilita_tastiera(void);

// Interruzione hardware della tastiera
const int KBD_IRQ = 1;


// sottrazione modulo 'mod' (il '%' del C/C++ calcola il valore sbagliato)
// p1 e p2 devono essere compresi tra 0 e mod-1
inline int circular_sub(int p1, int p2, int mod)
{
	int dist = p1 - p2;
	if (dist < 0) dist += mod;
	return dist;
}

// somma modulo 'mod'
inline int circular_sum(int p1, int p2, int mod)
{
	return (p1 + p2) % mod;
}



///////////////////////////////////////////////////////////////////////////////
//    TASTIERE VIRTUALI                                                      //
///////////////////////////////////////////////////////////////////////////////

const unsigned int VKBD_BUF_SIZE = 20;

struct des_vkbd {
	int mutex;
	int intr;
	bool intr_enabled;
	char buf[VKBD_BUF_SIZE];
	int first;
	int last;
	int nchar;
	unsigned int flags;
};

bool vkbd_init(des_vkbd* k)
{
	if ( (k->mutex = sem_ini(1)) == 0)
		return false;
	if ( (k->intr = sem_ini(0)) == 0)
		return false;
	k->intr_enabled = false;
	k->first = k->last = k->nchar = 0;
	k->flags = 0;
	return true;
}

void vkbd_wfi(des_vkbd* k)
{
	sem_wait(k->intr);
}

unsigned short vkbd_decode(des_vkbd* p_des, unsigned char c, bool& valido)
{
	bool rilasciato;
	unsigned short code;

	// - decodifica del carattere
	rilasciato = (c & 0x80);
	c &= 0x7f;
	code = kbd_keymap[KBD_SC_IDX(p_des->flags)][c];

	valido = false;
	if (KBD_IS_MOD(code))
		if (rilasciato) 
			p_des->flags &= ~kbd_modflag_tab[KBD_MOD(code)];
		else
			p_des->flags |=  kbd_modflag_tab[KBD_MOD(code)];
	else if (!rilasciato)
		if (code < 0xff)
			valido = true;
	else
		code = 0;
	return code;
}

unsigned short vkbd_nuovo_car(des_vkbd* k, char c)
{
	bool valido;
	unsigned short code;

	sem_wait(k->mutex);

	code = vkbd_decode(k, c, valido);

	if (!valido)
		goto out;


	if (k->nchar >= VKBD_BUF_SIZE)
		goto out;

	c = static_cast<char>(code);

	k->buf[k->last] = c;
	k->last = circular_sum(k->last, 1, VKBD_BUF_SIZE);
	k->nchar++;

	if (k->intr_enabled && k->nchar == 1) 
		sem_signal(k->intr);
out:
	sem_signal(k->mutex);
	return code;
}

void vkbd_leggi_car(des_vkbd*k, char& c)
{
	sem_wait(k->mutex);

	if (k->nchar <= 0)
		goto out;

	c = k->buf[k->first];
	k->first = circular_sum(k->first, 1, VKBD_BUF_SIZE);
	k->nchar--;

	if (k->nchar > 0 && k->intr_enabled)
		sem_signal(k->intr);
out:
	sem_signal(k->mutex);
}

void vkbd_intr_enable(des_vkbd* k)
{
	sem_wait(k->mutex);
	if (!k->intr_enabled && k->nchar > 0)
		sem_signal(k->intr);
	k->intr_enabled = true;
	sem_signal(k->mutex);
}

void vkbd_intr_disable(des_vkbd* k)
{
	sem_wait(k->mutex);
	k->intr_enabled = false;
	sem_signal(k->mutex);
}

void vkbd_switch(des_vkbd* k)
{
	k->flags = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  MONITOR VIRTUALI                                                         //
///////////////////////////////////////////////////////////////////////////////

unsigned const int VMON_ROW_NUM = 100;
unsigned const int VMON_COL_NUM = 80;
unsigned const int VMON_SIZE = VMON_ROW_NUM * VMON_COL_NUM;

// descrittore di monitor virtuale
struct des_vmon {
	unsigned char video[VMON_SIZE]; // buffer circolare
	int pos;	// posizione (nel buffer circolare) in cui verra' aggiunto il prox. car
	int fpos;	// primo char significativo nel buffer circolare
	int vpos;	// primo char da mostrare sul video (-1 in modalita' follow)
};

// distanza in senso antiorario tra  le posizioni p2 e p1 nel buffer circolare
inline int vmon_distance(int p1, int p2)
{
	return circular_sub(p1, p2, VMON_SIZE);
}

// distanza in senzo antiorario tra le righe r2 e r1 nel buffer circolare
inline int vmon_row_distance(int r1, int r2)
{
	return circular_sub(r1, r2, VMON_ROW_NUM);
}

// riga corrispondente alla posizione p
inline int vmon_row(int p)
{
	return p / VMON_COL_NUM;
}

// posizione corrispondente alla riga r e colonna c
inline int vmon_pos(int r, int c)
{
	return r * VMON_COL_NUM + c;
}

// incrementa la riga r nel buffer circolare
inline int vmon_inc_row(int r, int s = 1)
{
	return circular_sum(r, s, VMON_ROW_NUM);

}

// incrementa la posizione p nel buffer circolare
inline int vmon_inc_pos(int p, int s = 1)
{
	return circular_sum(p, s, VMON_SIZE);
}


// aggiunge una nuova linea al contenuto del buffer circolare
void vmon_new_line(des_vmon* t)
{
	// calcoliamo la riga corrente nel buffer circolare
	int y = vmon_row(t->pos);

	// e quella che deve essere la nuova
	int new_y = vmon_inc_row(y);

	// se si sovrappone alla prima salvata, ne perdiamo una
	if (new_y == vmon_row(t->fpos)) 
		t->fpos = vmon_pos(vmon_inc_row(vmon_row(t->fpos)), 0);

	// riempiamo la nuova linea con spazi
	t->pos = vmon_pos(new_y, 0);
	for (int i = 0; i < VMON_COL_NUM; i++)
		t->video[t->pos + i] = ' ';
}

// aggiunge un carattere sul monitor virtuale
void vmon_put_char(des_vmon* t, char ch)
{
	int tab_pos;

	switch(ch) {
		case '\n':
			vmon_new_line(t);
			break;
		case '\b':
			if (t->pos != t->fpos)
				t->pos = vmon_distance(t->pos, 1);
			t->video[t->pos] = ' ';
			break;
		case '\t':
			tab_pos = (t->pos / 8 + 1) * 8;
			for (int i = t->pos + 1; i < tab_pos; i++)
				vmon_put_char(t, ' ');
			break;
		default:
			if(ch < 31 || ch < 0)
				return;

			t->video[t->pos] = ch;
			int new_pos = vmon_inc_pos(t->pos);
			if (new_pos == t->fpos)
				vmon_new_line(t);
			else
				t->pos = new_pos;
	}
}


// monitor reale
unsigned const int CON_ROW_NUM = 25;
unsigned const int CON_COL_NUM = 80;
unsigned const int CON_SIZE = CON_ROW_NUM * CON_COL_NUM;

struct des_console {
	des_vmon* t;  // terminale virtuale mostrato sulla console
	int vpos;     // primo char del terminale mostrato sulla console
	int off;      // offset nel monitor del prossimo char da scrivere
};

des_console console;

// copia 'quanti' byte, a partire dalla posizione 'pos', dal monitor virtuale 
// corrente alla console
void vmon_copy(int pos, int quanti) {

	if (pos + quanti <= VMON_SIZE) {
		writevid_n(console.off, &console.t->video[pos], quanti);
	} else {
		int first  = VMON_SIZE - pos, second = quanti - first;
		writevid_n(console.off, &console.t->video[pos], first);
		writevid_n(console.off + first, &console.t->video[0], second);
	}
}

extern "C" void console_cursor(int off);

// sincronizza la console con il contenuto del monitor virtuale corrente
void console_sync(des_vmon* t)
{
	if (console.t != t)
		return;

	bool refresh = false;
	// calcoliamo il numero di caratteri significativi nel buffer del 
	// monitor virtuale
	int dist = vmon_distance(t->pos, t->fpos);

	// quindi, calcoliamo l'offset (nel buffer virtuale) del primo 
	// carattere della prima riga che vogliamo visualizzare
	int vpos = t->vpos;
	if (vpos < 0) // se negativo, il monitor e' in modalita' follow:
		      // la prima riga va calcolata in base alla pos. corrente 
		      // di t->pos (cio' causera' lo scrolling del video quando 
		      // saranno state riempite CON_ROW_NUM righe)
		vpos = (dist < CON_SIZE) ?
			t->fpos :
		        vmon_pos(vmon_row_distance(vmon_row(t->pos), CON_ROW_NUM - 1), 0);

	if (console.vpos != vpos) {
		// la prima linea sulla console non e' quella richiesta dal 
		// monitor: ricopiamo tutto da quella riga in poi
		console.off = 0;
		console.vpos = vpos;
		refresh = true;
	} 
	// calcoliamo il numero di caratteri significativi, nel buffer 
	// virtuale, successivi al primo da visualizzare (se sono piu' di 
	// quelli visualizzabili, assumiamo il massimo)
	int n_vmon = vmon_distance(t->pos, vpos);
	if (n_vmon > CON_SIZE)
		n_vmon = CON_SIZE;

	// calcoliamo il numero di caratteri di differenza tra quelli 
	// significativi da visualizzare e quelli gia' visualizzati
	int quanti = n_vmon - console.off;
	if (quanti > 0) {
		// ci sono nuovi caratteri da copiare
		vmon_copy(vmon_inc_pos(console.vpos, console.off), quanti);
		console.off += quanti;
	} else if (quanti < 0) {
		// alcuni caratteri sono stati eliminati
		console.off = n_vmon;
		vmon_copy(t->pos, -quanti);
	}

	// se e' necessario un refresh, dobbiamo copiare l'intera schermata, e 
	// non solo i caratteri nuovi
	if (refresh && console.off < CON_SIZE)
		vmon_copy(vmon_inc_pos(console.vpos, console.off), CON_SIZE - console.off);

	console_cursor(console.off);
}

void vmon_switch(des_vmon* t)
{
	if (console.t == t)
		return;

	console.t = t;
	console.vpos = -1;
	console.off = 0;
	console_sync(t);
}

bool vmon_init(des_vmon* t)
{
	t->pos = t->fpos = 0;
	t->vpos = -1;
	for (int i = 0; i < VMON_SIZE; i++)
		t->video[i] = ' ';
	return true;
}

void vmon_write_n(des_vmon* t, char vetti[], int quanti)
{
	for (int i = 0; i < quanti; i++)
		vmon_put_char(t, vetti[i]);
	console_sync(t);
}

////////////////////////////////////////////////////////////////////////////////
//                          TERMINALI VIRTUALI                               
////////////////////////////////////////////////////////////////////////////////


struct des_vterm {
	int mutex_r;
	int sincr;
	des_vkbd vkbd;
	int cont;
	ind_b punt;
	funz funzione;
	unsigned int flags;
	int orig_cont;
	bool echo;
	int mutex_w;
	des_vmon vmon;
};


const int N_VTERM = 12;

des_vterm vterm[N_VTERM];
des_vterm* vterm_active;


void input_term(int h)
{
	char c;
	bool fine, valido, echo;
	des_vterm *p_des;

	p_des = &vterm[h];
	vkbd_wfi(&p_des->vkbd);

	for(;;) {
		fine = false;
		vkbd_intr_disable(&p_des->vkbd);
		vkbd_leggi_car(&p_des->vkbd, c);

		echo = false;
		if (c == '\b') {
			if (p_des->cont < p_des->orig_cont) {
				p_des->cont++;
				p_des->punt--;
				echo = p_des->echo;
			} 
		} else {
			echo = p_des->echo;
			
			if (p_des->funzione == input_n) {
				*p_des->punt = c;
				p_des->punt++;
				p_des->cont--;
				if(p_des->cont == 0)
					fine = true;
			} else if (p_des->funzione == input_ln) {
				if (c == '\r' || c == '\n') {
					fine = true;
					p_des->cont = p_des->orig_cont - p_des->cont;
					*p_des->punt = 0;
				} else {
					*p_des->punt = c;
					p_des->punt++;
					p_des->cont--;
					if (p_des->cont == 0) {
						fine = true;
						p_des->cont = p_des->orig_cont;
					}
				}
			}
		}

		if (echo)
			vmon_write_n(&p_des->vmon, &c, 1);

		if (fine == true) {
			sem_signal(p_des->sincr);
		} else
			vkbd_intr_enable(&p_des->vkbd);

		vkbd_wfi(&p_des->vkbd);
	}
}

extern "C" void c_writevterm_n(int term, char vetti[], int quanti)
{
	des_vterm *p_des;

	if(term < 0 || term >= N_VTERM || !verifica_area(vetti, quanti, false))
		abort_p();

	des_vterm* t = &vterm[term];

	sem_wait(t->mutex_w);
	vmon_write_n(&t->vmon, vetti, quanti);
	sem_signal(t->mutex_w);
}

void startvterm_in(des_vterm *p_des, char vetti[], int quanti, funz op, bool echo)
{
	p_des->cont = p_des->orig_cont = quanti;
	p_des->punt = vetti;
	p_des->funzione = op;
	p_des->echo = echo;
	vkbd_intr_enable(&p_des->vkbd);
}


extern "C" void c_readvterm_n(int term, char vetti[], int quanti, bool echo)
{
	des_vterm *p_des;

	if(term < 0 || term >= N_VTERM ||
			!verifica_area(vetti, quanti, true))
		abort_p();

	if (quanti <= 0)
		return;

	p_des = &vterm[term];
	sem_wait(p_des->mutex_r);
	startvterm_in(p_des, vetti, quanti, input_n, echo);
	sem_wait(p_des->sincr);
	sem_signal(p_des->mutex_r);
}

extern "C" void c_readvterm_ln(int term, char vetti[], int &quanti, bool echo)
{
	des_vterm *p_des;

	if(term < 0 || term >= N_VTERM ||
			!verifica_area(&quanti, sizeof(int), true) ||
			!verifica_area(vetti, quanti, true))
		abort_p();

	if (quanti <= 0)
		return;

	p_des = &vterm[term];
	sem_wait(p_des->mutex_r);
	startvterm_in(p_des, vetti, quanti, input_ln, echo);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	sem_signal(p_des->mutex_r);
}


void vterm_switch(int t)
{
	vterm_active = &vterm[t];
	vkbd_switch(&vterm_active->vkbd);
	vmon_switch(&vterm_active->vmon);
}


extern "C" char kbd_read();

// smistatore dei caratteri dalla tastiera reale alle tastiere virtuali
void estern_kbd(int h)
{
	char ch;
	unsigned short code;
	des_vkbd* vkbd;

	for(;;) {
		ch = kbd_read();
		code = vkbd_nuovo_car(&vterm_active->vkbd, ch);				
		if (code >= KBD_F1 && code <= KBD_F12) 
				vterm_switch(KBD_FNUM(code));
		nwfi(master);
	}
}

// inizializzazione
int vterm_init()
{
	short id;

	for (int i = 0; i < N_VTERM; i++) {
		des_vterm* p_des = &vterm[i];

		if ( (p_des->mutex_r = sem_ini(1)) == 0)
			return -301;
		if ( (p_des->mutex_w = sem_ini(1)) == 0)
			return -302;
		if ( (p_des->sincr = sem_ini(0)) == 0)
			return -303;
		if (!vkbd_init(&p_des->vkbd))
			return -304;
		if (!vmon_init(&p_des->vmon))
			return -305;
		if ( (id = activate_p(input_term, i, PRIO_ESTERN_BASE, LIV_SISTEMA)) == 0)
			return -306;
	}

	vterm_active = &vterm[0];
	if ( (id = activate_pe(estern_kbd, 0, PRIO_ESTERN_BASE + IRQ_MAX - KBD_IRQ, LIV_SISTEMA, tastiera)) == 0)
		return -307;

	abilita_tastiera();
	return 0;
}



////////////////////////////////////////////////////////////////////////////////
//                 INIZIALIZZAZIONE DEL SOTTOSISTEMA DI I/O                   //
////////////////////////////////////////////////////////////////////////////////

// inizializza i gate usati per le chiamate di IO
//
extern "C" void fill_io_gates(void);

// eseguita in fase di inizializzazione
//
extern "C" int cmain(void)
{
	int error;

	fill_io_gates();
	error = vterm_init();
	if (error < 0) return error;
	error = com_init();
	if (error < 0) return error;

	return 0;
}

