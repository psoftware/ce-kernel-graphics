// io.cpp
//

////////////////////////////////////////////////////////////////////////////////
//                                 COSTANTI                                   //
////////////////////////////////////////////////////////////////////////////////

// Livelli di privilegio dei processi
// Devono coincidere con i valori in sistema.cpp
//
const char LIV_UTENTE = 3, LIV_SISTEMA = 0;

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
	com2_out
};

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

enum controllore { master, slave };
extern "C" void nwfi(controllore c);

extern "C" bool verifica_area(void *area, unsigned int dim, bool write);
extern "C" void panic(const char *msg);
extern "C" void fill_gate(int gate, void (*f)(void), int tipo, int dpl);
extern "C" void reboot(void);

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

typedef char *ind_b;	// indirizzo di una porta
typedef unsigned int size_t;

// protezione dalla concorrenza con le interruzioni
//
inline void lock() { asm("cli"); }
inline void unlock() { asm("sti"); }

// ingresso di un byte da una porta di IO
extern "C" void inputb(ind_b reg, char &a);

// uscita di un byte su una porta di IO
extern "C" void outputb(char a, ind_b reg);

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
//	kickse_out(p_des);	// usare solo per la seriale di Bochs
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

