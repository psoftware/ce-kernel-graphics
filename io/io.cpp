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
// La priorita' di un processo esterno e' calcolata aggiungendo a questa
//  costante la priorita' dell' interruzione a cui e' associato (la priorita'
//  delle interruzioni e' inversamente proporzionale al loro tipo, quindi si
//  somma IRQ_MAX - TIPO alla priorita' di base).
// Deve coincidere con il valore in sistema.cpp
//
const int PRIO_ESTERN_BASE = 0x400;

// Identificatori dei processi esterni
// Devono coincidere con i valori in sistema.cpp
//
enum estern_id {
	tastiera,
	com1_in,
	com1_out,
	com2_in,
	com2_out,
	ata0,
	ata1
};

typedef char* ind_b;		// indirizzo di un byte (una porta)
typedef short* ind_w;		// indirizzo di una parola
typedef int* ind_l;		// indirizzo di una parola lunga
typedef unsigned int ind_fisico;// locazione di memoria fisica (per il DMA)
typedef unsigned int size_t;

////////////////////////////////////////////////////////////////////////////////
//                        CHIAMATE DI SISTEMA USATE                           //
////////////////////////////////////////////////////////////////////////////////

extern "C" void terminate_p(void);
extern "C" void sem_ini(int &index_des_s, int val, bool &risu);
extern "C" void sem_wait(int sem);
extern "C" void sem_signal(int sem);

////////////////////////////////////////////////////////////////////////////////
//               INTERFACCIA OFFERTA DAL NUCLEO AL MODULO DI IO               //
////////////////////////////////////////////////////////////////////////////////

extern "C" void activate_pe(void f(int), int a, int prio, char liv,
	short &identifier, estern_id interf, bool &risu);

enum controllore { master=0, slave=1 };
extern "C" void nwfi(controllore c);

extern "C" bool verifica_area(void *area, unsigned int dim, bool write);
extern "C" void trasforma(ind_l vetti, ind_fisico &iff);
extern "C" void panic(const char *msg);
extern "C" void fill_gate(int gate, void (*f)(void), int tipo, int dpl);
extern "C" void reboot(void);
extern "C" void ndelay(unsigned int nano_sec);

//
// Chiamate per l' IO da console
//

const int CON_BUF_SIZE = 0x0fa0;

struct con_status {
	unsigned char buffer[CON_BUF_SIZE];
	short x, y;
};

extern "C" void con_read(char &ch, bool &risu);
extern "C" void con_write(const char *vett, int quanti);

extern "C" void con_save(con_status *cs);
extern "C" void con_load(const con_status *cs);
extern "C" void con_update(con_status *cs, const char *vett, int quanti);
extern "C" void con_init(con_status *cs);

////////////////////////////////////////////////////////////////////////////////
//                      FUNZIONI GENERICHE DI SUPPORTO                        //
////////////////////////////////////////////////////////////////////////////////


// protezione dalla concorrenza con le interruzioni
//
inline void lock() { asm("cli"); }
inline void unlock() { asm("sti"); }

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

void *memcpy(void *dest, const void *src, unsigned int n);
void *memset(void *dest, int c, size_t n);

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
		return;

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
		return;

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

void com_init()
{
	des_se *p_des;
	bool r1, r2;
	short id;
	int i, com_base_prio = PRIO_ESTERN_BASE + IRQ_MAX - COM2_IRQ;

	com_setup();

	for(i = 0; i < S; ++i) {
		p_des = &com[i];

		sem_ini(p_des->mutex, 1, r1);
		sem_ini(p_des->sincr, 0, r2);
		if(!r1 || !r2)
			panic("Impossibile allocare semaforo per l' IO");

		activate_pe(input_com, i, com_base_prio - i,
			LIV_SISTEMA, id, com_id[i][0], r1);
		activate_pe(output_com, i, com_base_prio - i,
			LIV_SISTEMA, id, com_id[i][1], r2);
		if(!r1 || !r2)
			panic("Impossibile creare processo esterno di uscita");
	}
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
//
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

// dimensione della coda di una tastiera
const int KBD_QUEUE_SIZE = 512;

// maschere per il campo flags
enum {
	KBD_FLAG_NONE = 0,
	KBD_FLAG_SHIFT = 0x100,
	KBD_FLAG_ALT = 0x200,
	KBD_FLAG_CTRL = 0x400
};

struct des_term;

// tastiera virtuale
struct kbd_t {
	char queue[KBD_QUEUE_SIZE];
	int in, out;
	des_term *term;
	int flags;
	int nl_cnt;
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

// tastiera virtuale attiva
static kbd_t *kbd_att = 0;

// inizializzazione di una tastiera virtuale
inline void kbd_init(kbd_t *kbd, des_term *term)
{
	memset(kbd, 0, sizeof(kbd_t));
	kbd->term = term;
	if(!kbd_att)
		kbd_att = kbd;
}

// vera se e' stato premuto il ritorno a capo
inline bool KBD_NL_READ(kbd_t *kbd)
{
	return kbd->nl_cnt > 0;
}

// indice nella keymap in base a flags
inline int KBD_SC_IDX(int flags)
{
	return (flags)&KBD_FLAG_SHIFT ?
		KBD_MOD_SHIFT: KBD_MOD_NONE;
}

// inserimento del codice nella coda (di caratteri) della tastiera attiva
//
static void kbd_put(unsigned short code)
{
	char ch;

	if(!kbd_att)
 		return;

	ch = (char)(code&0xff);
	switch(ch) {
		case '\b':
			if(kbd_att->in != kbd_att->out &&
					kbd_att->queue[kbd_att->in > 0 ?
     					kbd_att->in - 1: KBD_QUEUE_SIZE - 1] != '\n') {
				--kbd_att->in;
				kbd_att->in = kbd_att->in >= 0 ?
					kbd_att->in: KBD_QUEUE_SIZE - 1;
			}

			break;
		default:
			if(!ch)
				return;

			kbd_att->queue[kbd_att->in] = ch;
			if(ch == '\n')
				++kbd_att->nl_cnt;
			kbd_att->in = (kbd_att->in + 1) % KBD_QUEUE_SIZE;
			if(kbd_att->in == kbd_att->out)
				kbd_att->out = (kbd_att->out + 1) %
					KBD_QUEUE_SIZE;
	}
}

// lettura di un carattere dalla coda di kbd
//
static int kbd_get(kbd_t *kbd)
{
	int rv;

	if(kbd->in == kbd->out)
		return -1;

	rv = kbd->queue[kbd->out];
	if(rv == '\n')
		--kbd->nl_cnt;
	kbd->out = (kbd->out + 1) % KBD_QUEUE_SIZE;

	return rv;
}

bool term_newchar(des_term *term, unsigned short code);

// processo esterno per la gestione dell' ingresso da tastiera
//
void tast_in(int h)
{
	char ch;
	unsigned short code;
	bool r;

	for(;;) {
		con_read(ch, r);

		if(r && kbd_att) {
			code = kbd_keymap[KBD_SC_IDX(kbd_att->flags)][ch&0x7f];

			if(!(ch&0x80) && KBD_IS_MOD(code))
				kbd_att->flags |= kbd_modflag_tab[KBD_MOD(code)];
			else if((ch&0x80) && KBD_IS_MOD(code))
				kbd_att->flags &= ~kbd_modflag_tab[KBD_MOD(code)];
			else if(!(ch&0x80)) {
				if(term_newchar(kbd_att->term, code))
					kbd_put(code);
			}
		}

		nwfi(master);
	}
}

extern "C" void abilita_tastiera(void);

// Interruzione hardware della tastiera
//
const int KBD_IRQ = 1;

// inizializzazione
//
void kbd_init()
{
	short id;
	bool r;

	activate_pe(tast_in, 0, PRIO_ESTERN_BASE + IRQ_MAX - KBD_IRQ,
		LIV_SISTEMA, id, tastiera, r);
	if(!r)
		panic("Impossibile creare processo esterno della tastiera");

	// l' inizializzazione avviene ad interruzioni disabilitate, quindi
	//  non ci saranno richieste di interruzione accolte fino all' uscita
	//  da io_init
	//
	abilita_tastiera();
}

// lettura di n caratteri da kbd
//  (deve essere chiamata ad interruzioni disabilitate)
//
int kbd_read(kbd_t *kbd, char *buf, int n)
{
	int i = 0, ch;

	while(i < n && (ch = kbd_get(kbd)) != -1) {
		buf[i++] = (char)ch;
		if(ch == '\n')
			break;
	}

	return i;
}

// numero di caratteri disponibili nella coda di kbd
//
int kbd_avail(kbd_t *kbd)
{
	if(kbd->in >= kbd->out)
		return kbd->in - kbd->out;

	return KBD_QUEUE_SIZE - kbd->out + kbd->in;
}

// rende kbd la tastiera virtuale attiva
//
int kbd_activate(kbd_t *kbd)
{
	kbd_att->flags = KBD_FLAG_NONE;
	kbd_att = kbd;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//                             I/O DA TERMINALE                               //
////////////////////////////////////////////////////////////////////////////////

// descrittore di terminale virtuale
//
struct des_term {
	int mutex;	// protezione da utilizzo concorrente
	int sincr;	// sincronizzazione per la lettura da tastiera
	bool waiting;	// un processo attende su sincr

	kbd_t kbd;	// tastiera virtuale
	con_status cstat; // stato della console per il video virtuale
};

const int N_TERM = 12;
int term_att = 0;

des_term term_virt[N_TERM];

void attiva_term(int t)
{
	con_save(&term_virt[term_att].cstat);
	term_att = t;
	kbd_activate(&term_virt[term_att].kbd);
	con_load(&term_virt[term_att].cstat);
}

bool term_newchar(des_term *term, unsigned short code)
{
	char ch;

	if(code >= KBD_F1 && code <= KBD_F12) {
		attiva_term(KBD_FNUM(code));
		return false;
	}

	if(code == KBD_CANC && (term->kbd.flags&KBD_FLAG_CTRL) &&
			(term->kbd.flags&KBD_FLAG_ALT))
		reboot();

	if(code == '\b' && kbd_avail(&term->kbd) == 0)
		return false;

	if(code < 0xff) {
		ch = code&0xff;
		con_write(&ch, 1);
	}

	if(term->waiting && code == '\n') {
		term->waiting = false;
		sem_signal(term->sincr);
	}

	return true;
}

// NOTA: non richiede che vetti sia nello spazio condiviso, dato che l' handler
//  dell' interruzione scrive in un buffer interno
extern "C" void c_term_read_n(int term, char vetti[], int &quanti)
{
	des_term *p_des;
	int disp, letti;

	if(term < 0 || term >= N_TERM ||
			!verifica_area(&quanti, sizeof(int), true) ||
			!verifica_area(vetti, quanti, true))
		return;

	p_des = &term_virt[term];
	sem_wait(p_des->mutex);
	quanti = quanti > KBD_QUEUE_SIZE ? KBD_QUEUE_SIZE: quanti;

	lock();
	disp = kbd_avail(&p_des->kbd);

	if(disp >= quanti - 1 || KBD_NL_READ(&p_des->kbd)) {
		// sono gia' disponibili abbastanza caratteri, o
  		//  e' stato premuto invio
		letti = kbd_read(&p_des->kbd, vetti, quanti - 1);
	} else {
		// e' necessario bloccare
		p_des->waiting = true;
		unlock();

		sem_wait(p_des->sincr);

		lock();
		disp = kbd_avail(&p_des->kbd);
		letti = kbd_read(&p_des->kbd, vetti, quanti - 1);
	}

	unlock();

	vetti[letti] = 0;
	quanti = letti + 1;

	sem_signal(p_des->mutex);
}

// NOTA: non richiede che vetti sia nello spazio condiviso, dato che l' IO non
//  e' ad interruzione di programma
extern "C" void c_term_write_n(int term, char vetti[], int quanti)
{
	des_term *p_des;

	if(term < 0 || term >= N_TERM || !verifica_area(vetti, quanti, false))
		return;

	p_des = &term_virt[term];
	sem_wait(p_des->mutex);

	lock();
	if(term_att == term)
		con_write(vetti, quanti);
	else
		con_update(&term_virt[term].cstat, vetti, quanti);
	unlock();

	sem_signal(p_des->mutex);
}

void term_init(void)
{
	des_term *p_des;
	bool r1, r2;

	kbd_init();

	for(int i = 0; i < N_TERM; ++i) {
		p_des = &term_virt[i];

		sem_ini(p_des->mutex, 1, r1);
		sem_ini(p_des->sincr, 0, r2);
		if(!r1 || !r2)
			panic("Impossibile inizializzare i semafori per l' I/O");
		p_des->waiting = false;

		kbd_init(&p_des->kbd, p_des);
		con_init(&p_des->cstat);
   	}
}

////////////////////////////////////////////////////////////////////////////////
//GESTIONE DELL'HARD DISK               	      //
////////////////////////////////////////////////////////////////////////////////

#define STRICT_ATA

#define HD_MAX_TX 256

#define	HD_NONE 0x00		//Maschere per i biti fondamentali dei registri
#define HD_STS_ERR 0x01
#define HD_STS_DRQ 0x08
#define HD_STS_DRDY 0x40
#define HD_STS_BSY 0x80
#define HD_DRV_MASTER 0
#define HD_DRV_SLAVE 1

const int A=2;

enum hd_cmd {			// Comandi per i controllori degli hard disk
	NONE=0x00,
	IDENTIFY=0xEC,
	SET_FEATURES=0xEF,
	SET_MULT_MODE=0xC6,
	WRITE_SECT=0x30,
	READ_SECT=0x20,
	HD_SEEK=0x70,
	DIAGN=0x90
};

struct interfata_reg {
	union {				// Command Block Register
		ind_b iCMD;
		ind_b iSTS;
	}; //iCMD_iSTS;
	ind_b iDATA;
	union {
		ind_b iFEATURES;
		ind_b iERROR;
	}; //iFEATURES_iERROR
	ind_b iSTCOUNT;
	ind_b iSECT_N;
	ind_b iCYL_L_N;
	ind_b iCYL_H_N;
	union {
		ind_b iHD_N;
		ind_b iDRV_HD;
	}; //iHD_N_iDRV_HD
	union {				// Control Block Register
		ind_b iALT_STS;		
		ind_b iDEV_CTRL;
	}; //iALT_STS_iDEV_CTRL
};

struct geometria {			// Struttura 3-D di un disco
	unsigned short cil;	
	unsigned short test;
	unsigned short sett;
	unsigned int tot_sett;		// Utile in LBA
};

struct drive {				// Drive su un canale ATA
	bool presente;
	bool dma;
	geometria geom;
};

struct des_ata {		// Descrittore operazione per i controller ATA
	interfata_reg indreg;
	drive disco[2];		// Ogni controller ATA gestisce 2 drive
	hd_cmd comando;
	char errore;
	char cont;
	ind_w punt;	
	int mutex;
	int sincr;
};

extern "C" des_ata hd[A];	// 2 controller ATA

const estern_id hd_id[2]={ata0,ata1};

//Funzioni di utilita' per la gestione del disco
//
inline bool hd_drq(char &s) {		// Test di Data ReQuest
	return s&HD_STS_DRQ;
}

inline bool hd_errore(char &s) {	// Test di un eventuale errore (generico)
	return s&HD_STS_ERR;
}

inline bool hd_drdy(char &s) {		// Test di Drive ReaDY
	return s&HD_STS_DRDY;
}

// Attende una particolare configurazione di bit nel registro di stato del
//  controller; tipicamente deve resettarsi BuSY
//  
char aspetta_STS(des_ata *p_des,char bit,bool &ris) {
	char stato,test;
	int timeout=D_TIMEOUT * 1000;		// Previsto un timeout
	do {
		inputb(p_des->indreg.iALT_STS,stato);
		if (bit==HD_STS_DRDY || bit==HD_STS_DRQ)
			test=(~stato);
		else
			test=stato;
		test&=bit;
		timeout--;
		if (timeout<0) {
			ris=false;		// Scaduto il timeout!
			return stato;
		}
	} while (test!=0);
	ris=true;
	return stato;
}

// Invia un comando al controller con la temporizzazione corretta
// 
inline bool invia_cmd(des_ata *p_des,hd_cmd com) {
#ifdef STRICT_ATA
	bool ris=true;
	aspetta_STS(p_des,HD_STS_DRDY,ris);
	if (!ris)
		return false;
#endif
	p_des->comando=com;
	outputb(com,p_des->indreg.iCMD);
	ndelay(400);
	return true;
}

// Legge il buffer di uscita del controller se questo contiene dati
// 
void leggi_sett_buff(des_ata *p_des,short vetti[],char &stato) {
	if (!hd_drq(stato))		
		// DRQ _deve_ essere settato!
		panic("\nErrore Grave: DRQ non e' settato!");
	inputbuffw(p_des->indreg.iDATA,vetti,H_BLK_SIZE);
	//for (int i=0;i<H_BLK_SIZE;i++) //<=== Forma equivalente (debugging)
	//	inputw(p_des->indreg.iDATA,vetti[i]);
}

// Scrive il buffer di ingresso del controller se questo aspetta dati
// 
void scrivi_sett_buff(des_ata *p_des,short vetti[],char &stato) {
	if (!hd_drq(stato))		
		// DRQ _deve_ essere settato!
		panic("\nErrore Grave: DRQ non e' settato!");
	outputbuffw(vetti,p_des->indreg.iDATA,H_BLK_SIZE);
	//for (int i=0;i<H_BLK_SIZE;i++) //<=== Forma equivalente (debugging)
	//	outputw(vetti[i],p_des->indreg.iDATA);
}

//Funzioni per l'ingresso/uscita dati e processo estern0
//
extern "C" void go_inouthd(ind_b i_dev_ctl);

extern "C" void halt_inouthd(ind_b i_dev_ctl);

extern "C" bool test_canale(ind_b st,ind_b stc,ind_b stn);

extern "C" int leggi_signature(ind_b stc,ind_b stn,ind_b cyl,ind_b cyh);

extern "C" void umask_hd(ind_b h);

extern "C" void mask_hd(ind_b h);

extern "C" bool drive_reset(ind_b dvctrl,ind_b st);

extern "C" void set_drive(short ms,ind_b i_drv_hd);

extern "C" void get_drive(ind_b i_drv_hd,short &ms);

extern "C" void setup_addr_hd(des_ata *p,unsigned int primo);	

// Trasferimento dati in PIO Mode
//

// Avvio delle operazioni di lettura da hard dik
// 
void starthd_in(des_ata *p_des,short drv,short vetti[],unsigned int primo,unsigned char &quanti)	{
	p_des->punt=vetti;
	p_des->cont=quanti;
	bool ris;
	aspetta_STS(p_des,HD_STS_BSY,ris);	// aspetta "drive not BuSY"
	set_drive(drv,p_des->indreg.iDRV_HD);	// Selezione del drive richiesto
	char stato=aspetta_STS(p_des,HD_STS_BSY,ris);	// Aspetta di nuovo!!
	if (!hd_drdy(stato) || hd_errore(stato))
			panic("Errore nell'inizializzazione della lettura!");
	setup_addr_hd(p_des,primo); 
	outputb(quanti,p_des->indreg.iSTCOUNT);
	// Abilita le interruzioni per il controller-drive selezionato
	go_inouthd(p_des->indreg.iDEV_CTRL);	
	invia_cmd(p_des,READ_SECT);
	umask_hd(p_des->indreg.iSTS);
}

// Avvio delle operazioni di scrittura su hard disk
//
void starthd_out(des_ata *p_des,short drv,short vetti[],unsigned int primo,unsigned char &quanti)	
{	char stato;				
	bool ris;
	p_des->punt=vetti;
	p_des->cont=quanti;
	aspetta_STS(p_des,HD_STS_BSY,ris);	// Aspetta "drive not BuSY"
	set_drive(drv,p_des->indreg.iDRV_HD);	// Selezione del drive richiesto
	stato=aspetta_STS(p_des,HD_STS_BSY,ris);	// Aspetta di nuovo!
	if (!hd_drdy(stato) || hd_errore(stato))
			panic("Errore nell'inizializzazione della scrittura!");
	setup_addr_hd(p_des,primo); 
	outputb(quanti,p_des->indreg.iSTCOUNT);
	//Abilita le interruzioni per il controller-drive selezionato
	go_inouthd(p_des->indreg.iDEV_CTRL);		
	invia_cmd(p_des,WRITE_SECT);	// Asimmetria con la lettura, bisogna
	aspetta_STS(p_des,HD_STS_BSY,ris);	// Aspetta "drive not BuSY"
	// Aspetta DRQ, previsto dal protocollo di comunicazione
	stato=aspetta_STS(p_des,HD_STS_DRQ,ris);
	inputb(p_des->indreg.iSTS,stato);
	scrivi_sett_buff(p_des,p_des->punt,stato);
	p_des->punt+=H_BLK_SIZE;	// Solo dopo quest'operazione e' sicuro
	umask_hd(p_des->indreg.iSTS);	//  abilitare l'hd ad interrompere
}

// Primitiva di lettura da hard disk: legge quanti blocchi (max 256, char!)
//  a partire da primo depositandoli in vetti; il controller usato e ind_ata,
//  il drive e' drv. Riporta un fallimento in errore
//  
extern "C" void c_readhd_n(short ind_ata,short drv,short vetti[],unsigned int primo,unsigned char &quanti,char &errore)
{
	des_ata *p_des;

	p_des = &hd[ind_ata];

	// Controllo sulla protezione
	if (ind_ata<0 || ind_ata>=A || drv<0 || drv>=2)
		return;
	//if (!verifica_area(&quanti,sizeof(int),true) ||
	//!verifica_area(vetti,quanti,true) ||
	//!verifica_area(&errore,sizeof(char),true)) return;

	// Controllo sulla selezione di un drive presente
	if (p_des->disco[drv].presente==false) {
		errore=D_ERR_PRESENCE;
		return;
	}

	// Controllo sull'indirizzamento
	if (primo>p_des->disco[drv].geom.tot_sett)
		errore=D_ERR_BOUNDS;
	else {		
		sem_wait(p_des->mutex);
		p_des->errore=HD_NONE;		// Reset degli errori precedenti 
		p_des->disco[drv].dma=false;	// Trasferimento in PIO Mode
		// Abilitazione del controller all'operazione
		starthd_in(p_des,drv,vetti,primo,quanti);
		sem_wait(p_des->sincr);
		quanti=p_des->cont;
		errore=p_des->errore;
		sem_signal(p_des->mutex);
	}
}

// Primitiva di scrittura su hard disk: scrive quanti blocchi (max 256, char!)
//  a partire da primo prelevandoli da vetti; il controller usato e' ind_ata,
//  il drive e' drv. Riporta un fallimento in errore
//  
extern "C" void c_writehd_n(short ind_ata,short drv,short vetti[],unsigned int primo,unsigned char &quanti,char &errore)
{
	des_ata *p_des;

	p_des = &hd[ind_ata];

	// Controllo sulla protezione
	if (ind_ata<0 || ind_ata>=A || drv<0 || drv>=2)
		return;
	//if (!verifica_area(&quanti,sizeof(int),true) ||
	//!verifica_area(vetti,quanti,true) ||
	//!verifica_area(&errore,sizeof(char),true)) return;

	// Controllo sulla selezione di un drive presente
	if (p_des->disco[drv].presente==false) {
		errore=D_ERR_PRESENCE;
		return;
	}
	// Controllo sull'indirizzamento
	if (primo>p_des->disco[drv].geom.tot_sett)
		errore=D_ERR_BOUNDS;
	else {
		sem_wait(p_des->mutex);
		p_des->errore=HD_NONE;		// Reset degli errori precedenti
		p_des->disco[drv].dma=false;	//Trasferimento in PIO Mode
		starthd_out(p_des,drv,vetti,primo,quanti);
		sem_wait(p_des->sincr);
		quanti=p_des->cont;
		errore=p_des->errore;
		sem_signal(p_des->mutex);
	}
}

// Processo esterno per l'hard disk, valido sia per l'ingresso che per 
//  l'uscita
//
void estern_hd(int h)
{
	des_ata *p_des;
	char stato;
	short drv;
	hd_cmd curr_cmd;
	bool fine;

	p_des = &hd[h];		// In base al parametro si sceglie un controller

	for(;;) {
		curr_cmd=p_des->comando;	// Gestione delle interruzioni
		p_des->comando=NONE;		//  non richieste
		if (curr_cmd!=NONE) {
			get_drive(p_des->indreg.iDRV_HD,drv);
			bool ris;
			stato=aspetta_STS(p_des,HD_STS_BSY,ris);
			fine=false;
			inputb(p_des->indreg.iSTS,stato);
			if (!hd_errore(stato) && (curr_cmd==READ_SECT || curr_cmd==WRITE_SECT)) {
				if (curr_cmd==WRITE_SECT) {
					p_des->cont--;
					if (p_des->cont==0)
						fine=true;
					else {
						scrivi_sett_buff(p_des,p_des->punt,stato);
						p_des->comando=WRITE_SECT;
					}
				}
				else {		// Comando di READ_SECT
					leggi_sett_buff(p_des,p_des->punt,stato);
					p_des->cont--;
					if (p_des->cont==0)
						fine=true;
					else
						p_des->comando=READ_SECT;
				}
				p_des->punt+=H_BLK_SIZE;
			}
			else {
				inputb(p_des->indreg.iERROR,p_des->errore);
				fine=true;
			}
			if (fine==true) {
				mask_hd(p_des->indreg.iSTS);
				//Disabilita le interruzioni dal controller
				halt_inouthd(p_des->indreg.iDEV_CTRL);	
				if (curr_cmd!=IDENTIFY && curr_cmd!=DIAGN)
					sem_signal(p_des->sincr);
			}	
		}
		nwfi(slave); 	// Controller ATA = Slave PIC (entrambi, 14 e 15)
	}
}

const int ATA1_IRQ=15;

// Inizializzazione e autoriconoscimento degli hard disk collegati ai canali ATA
// 
void hd_init() {
	char stato=0;
	bool test;
	int init_time=D_TIMEOUT;
	short st_sett[H_BLK_SIZE];
	short id;
	bool r,r1,r2;
	int hd_base_prio=PRIO_ESTERN_BASE+IRQ_MAX-ATA1_IRQ;
	short canali=0;
	
	des_ata *p_des;
	for (int i=0;i<A;i++) {				// Per ogni controller
		p_des=&hd[i];
		// Disabilita le interruzioni
		halt_inouthd(p_des->indreg.iDEV_CTRL);	
	}
	for (int i=0;i<A;i++) {			// Per ogni controller
		p_des=&hd[i];
		short drv=HD_DRV_MASTER;
		set_drive(drv,p_des->indreg.iDRV_HD);	//LBA e drive drv
		ndelay(400);
		// Procedura diversificata una volta effettuato il test()
		if (test_canale(p_des->indreg.iSTS,p_des->indreg.iSTCOUNT,p_des->indreg.iSECT_N)) {
#ifndef BOCHS		// In sistemi reali e' necessario inviare questo comando
			set_drive(HD_DRV_MASTER,p_des->indreg.iDRV_HD);
			invia_cmd(p_des,DIAGN);		// Diagnostica dei drive
			do
				inputb(p_des->indreg.iSTS,stato);
			while (stato&HD_STS_BSY);	// Polling
			inputb(p_des->indreg.iERROR,stato);
			do
				inputb(p_des->indreg.iSTS,stato);
			while (!stato&HD_STS_DRDY);	// Polling
#endif		// Diagnostica terminata

			for (int d=0;d<2;d++) {		// Per ogni drive
				set_drive(drv,p_des->indreg.iDRV_HD);
				ndelay(400);
				p_des->disco[d].dma=false;
#ifdef BOCHS		// BOCHS ha bisogno del reset dei controller
				if (!drive_reset(p_des->indreg.iDEV_CTRL,p_des->indreg.iSTS)) {
					p_des->disco[d].presente=false;
					drv=HD_DRV_SLAVE;
					continue;
				}
				else
					p_des->disco[d].presente=true;
#endif		// A questo punto in entrambi i casi e' disponibile la signature
				if (leggi_signature(p_des->indreg.iSTCOUNT,p_des->indreg.iSECT_N,p_des->indreg.iCYL_L_N,p_des->indreg.iCYL_H_N)!=0) {
					p_des->disco[d].presente=false;
					drv=HD_DRV_SLAVE;
					continue;
				}
				else
					p_des->disco[d].presente=true;
		
		//Riconoscimento della geometria e dei parametri del disco
				if (!invia_cmd(p_des,IDENTIFY)) {	
					p_des->disco[d].presente=false;
					drv=HD_DRV_SLAVE;
					continue;
				}
				bool ris;
				aspetta_STS(p_des,HD_STS_BSY,ris);
				inputb(p_des->indreg.iSTS,stato);
				leggi_sett_buff(p_des,st_sett,stato);
				// Inizializzazione della geometria
				p_des->disco[d].geom.cil=st_sett[1];		
				p_des->disco[d].geom.test=st_sett[3];
				p_des->disco[d].geom.sett=st_sett[6];
				p_des->disco[d].geom.tot_sett=(st_sett[58]<<16)+st_sett[57];
				drv=HD_DRV_SLAVE;
			}
			mask_hd(p_des->indreg.iSTS);
			sem_ini(p_des->mutex, 1, r1);
			sem_ini(p_des->sincr, 0, r2);
			if(!r1 || !r2)
				panic("\nImpossibile allocare i semafori per l' IO");
			activate_pe(estern_hd,i,hd_base_prio-i,LIV_SISTEMA,id,hd_id[i],r);
			if(!r)
				panic("\nImpossibile creare un processo esterno dell'hard disk");
		}
		else
			p_des->disco[0].presente=p_des->disco[1].presente=false;
		p_des->comando=NONE;
	}
}

// Funzione di utilita' perche' l'utente possa conoscere se interessato,
//  le dimensioni e la struttura dei suoi hard disk
//  
extern "C" void c_geometria(int interf,short drv,unsigned short &c,unsigned short &t,unsigned short &s,int &ts,char &err) {
	if (hd[interf].disco[drv].presente==true) {
		c=hd[interf].disco[drv].geom.cil;
		t=hd[interf].disco[drv].geom.test;
		s=hd[interf].disco[drv].geom.sett;
		ts=hd[interf].disco[drv].geom.tot_sett;
		err=D_ERR_NONE;
	}
	else
		err=D_ERR_PRESENCE;
}


////////////////////////////////////////////////////////////////////////////////
//                 INIZIALIZZAZIONE DEL SOTTOSISTEMA DI I/O                   //
////////////////////////////////////////////////////////////////////////////////

// inizializza i gate usati per le chiamate di IO
//
extern "C" void fill_io_gates(void);

// eseguita in fase di inizializzazione ad interruzioni disabilitate
//
extern "C" void cmain(void)
{
	fill_io_gates();
	term_init();
	com_init();
	hd_init();
}

// Replicata in sistema.cpp
//
void *memcpy(void *dest, const void *src, unsigned int n)
{
	char *dest_ptr = (char *)dest, *src_ptr = (char *)src;
	int i;

	for(i = 0; i < n; ++i)
		*dest_ptr++ = *src_ptr++;

	return dest;
}

// Replicata in sistema.cpp
//
void *memset(void *dest, int c, size_t n)
{
        size_t i;

        for(i = 0; i < n; ++i)
              ((char *)dest)[i] = (char)c;

        return dest;
}
