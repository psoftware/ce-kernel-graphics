// sistema.cpp
//
#include "multiboot.h"
#include "costanti.h"
#include "elf.h"

extern "C" void terminate_p();
extern "C" void sem_wait(int);
extern "C" void sem_signal(int);

///////////////////////////////////////////////////
// FUNZIONI PER LA MANIPOLAZIONE DEGLI INDIRIZZI //
///////////////////////////////////////////////////


// purtroppo, lo standard C++ non prevede tipi
// predefiniti che possano essere usati come
// "indirizzi di memoria". I candidati sono "void*",
// "char*" e "unsigned int", ma "char*" e "unsigned int"
// costringono a continui reinterpet_cast, mentre
// "void*" non puo' essere usato in espressioni aritmetiche.
// Come compromesso, si e' deciso di usare "void*" come
// tipo per gli indirizzi (per ridurre al minimo la 
// necessita' dei cast) e di definire le seguenti
// funzioni per svolgere le elementari funzioni arimetiche


// somma v byte all'indirizzo a
inline
void* add(const void* a, unsigned int v) {
	v += reinterpret_cast<unsigned int>(a);
	return reinterpret_cast<void*>(v);
};

// sottrae v byte all'indirizzo a
inline
void* sub(const void* a, unsigned int v) {
	unsigned int a1 = reinterpret_cast<unsigned int>(a) - v;
	return reinterpret_cast<void*>(a1);
}

// calcola il numero di byte tra a1 (incluso)
// e a2 (escluso)
inline
int distance(const void* a1, const void* a2) {
	return reinterpret_cast<unsigned int>(a1) - 
	       reinterpret_cast<unsigned int>(a2);
}

// converte v in un indirizzo
inline
void* addr(unsigned int v) {
	return reinterpret_cast<void*>(v);
}

// converte un indirizzo in un unsigned int
inline
unsigned int uint(void* v) {
	return reinterpret_cast<unsigned int>(v);
}

unsigned int ceild(unsigned int v, unsigned int q) {
	return v / q + (v % q != 0 ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////////
//                             STRUTTURE DATI                               //
//////////////////////////////////////////////////////////////////////////////
//
// elemento di una coda di processi
//
struct proc_elem {
	short identifier;
	int priority;
	proc_elem *puntatore;
};

// descrittore di semaforo
//
struct des_sem {
	int counter;
	proc_elem *pointer;
};

// processo in esecuzione
//
extern proc_elem *esecuzione;

// coda dei processi pronti
//
extern proc_elem *pronti;

// vettore dei descrittori di semaforo
//
extern des_sem array_dess[MAX_SEMAFORI];


// manipolazione delle code di processi
//
extern void inserimento_coda(proc_elem *&p_coda, proc_elem *p_elem);
extern void rimozione_coda(proc_elem *&p_coda, proc_elem *&p_elem);

// controllore interruzioni
extern "C" void init_8259();

// timer
extern "C" void attiva_timer(unsigned long delay);

// schedulatore
//
extern "C" void schedulatore(void);

// stampa msg a schermo e blocca il sistema
//
extern "C" void panic(const char *msg);

extern "C" void abort_p();

// stampa a schermo formattata
//
extern "C" int printk(const char *fmt, ...);

// verifica i diritti del processo in esecuzione sull' area di memoria
// specificata dai parametri
//
extern "C" bool verifica_area(void *area, unsigned int dim, bool write);

extern "C" void invalida_entrata_TLB(void* ind_virtuale);

struct direttorio;
struct tabella_pagine;
struct pagina;

// descrittore di processo
// NOTA: la commutazione di contesto e' gestita quasi interamente via
//  software (del supporto hw viene usato solo tr, quindi sarebbe sufficiente
//  mantenere corrispondenza tra descrittore e tss solo fino alla pila
//  sistema). Le informazioni contenute nel tss sono in corrispondenza
//  con quelle della struttura des_proc del testo, ma era necessario mantenere
//  il puntatore alla pila sistema nella forma corretta (quando viene
//  introdotta des_proc non e' ancora stata presentata la segmentazione e
//  punt_nucleo e' su una doppia parola, senza selettore); per il resto si
//  e' mantenuta, con alcune aggiunte, la forma standard del tss anche se
//  non era necessario (alcuni campi non vengono salvati).
//
// La struttura interna e' usata in sistema.s
//
struct des_proc {			// offset:
//	short id;
	unsigned int link;		// 0
//	int punt_nucleo;
	void*        esp0;		// 4
	unsigned int ss0;		// 8
	void*        esp1;		// 12
	unsigned int ss1;		// 16
	void*        esp2;		// 20
	unsigned int ss2;		// 24

	direttorio* cr3;		// 28

	unsigned int eip;		// 32, non utilizzato
	unsigned int eflags;		// 36, non utilizzato

//	int contesto[N_REG];
	unsigned int eax;		// 40
	unsigned int ecx;		// 44
	unsigned int edx;		// 48
	unsigned int ebx;		// 52
	unsigned int esp;		// 56
	unsigned int ebp;		// 60
	unsigned int esi;		// 64
	unsigned int edi;		// 68

	unsigned int es;		// 72
	unsigned int cs; 		// 76, char cpl;
	unsigned int ss;		// 80
	unsigned int ds;		// 84
	unsigned int fs;		// 86
	unsigned int gs;		// 90
	unsigned int ldt;		// 94, non utilizzato

	unsigned int io_map;		// 100, non utilizzato

	struct {
		unsigned int cr;	// 104
		unsigned int sr;	// 108
		unsigned int tr;	// 112
		unsigned int ip;	// 116
		unsigned int is;	// 120
		unsigned int op;	// 124
		unsigned int os;	// 128
		char regs[80];		// 132
	} fpu;

	char liv;			// 212
}; // dimensione 212 + 1 + 3(allineamento) = 216
//
// vettore dei descrittori di processo
//
extern des_proc array_desp[MAX_PROCESSI];

// richiesta al timer
//
struct richiesta {
	int d_attesa;
	richiesta *p_rich;
	proc_elem *pp;
};

// coda delle richieste al timer
//
richiesta *descrittore_timer;

// numero di processi attivi
//
int processi = 0;

// tipo usato per le dimensioni
//
typedef unsigned int size_t;		


// Valori usati in io.cpp
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

// inizializzazione della console
//
void con_init(void);

// ritorna il descrittore del processo id
//
extern "C" des_proc *des_p(short id) {
	return &array_desp[id - 5];
}

////////////////////////////////////////////////////////////////////////////////
//                          FUNZIONI DI LIBRERIA                              //
////////////////////////////////////////////////////////////////////////////////

// il nucleo non puo' essere collegato alla libreria standard del C/C++,
// perche' questa e' stata scritta utilizzando le primitive del sistema
// che stiamo usando (sia esso Windows o Unix). Tali primitive non
// saranno disponibili quando il nostro nucleo andra' in esecuzione.
// Per ragioni di convenienza, ridefiniamo delle funzioni analoghe a quelle
// fornite dalla libreria del C.

// copia n byte da src a dest
void *memcpy(void *dest, const void *src, unsigned int n)
{
	char *dest_ptr = (char *)dest, *src_ptr = (char *)src;
	int i;

	for(i = 0; i < n; ++i)
		*dest_ptr++ = *src_ptr++;

	return dest;
}

// scrive n byte pari a c, a partire da dest
void *memset(void *dest, int c, size_t n)
{
        size_t i;

        for(i = 0; i < n; ++i)
              ((char *)dest)[i] = (char)c;

        return dest;
}

typedef char *va_list;

// Versione semplificata delle macro per manipolare le liste di parametri
//  di lunghezza variabile; funziona solo se gli argomenti sono di
//  dimensione multipla di 4, ma e' sufficiente per le esigenze di printk.
//
#define va_start(ap, last_req) (ap = (char *)&(last_req) + sizeof(last_req))
#define va_arg(ap, type) ((ap) += sizeof(type), *(type *)((ap) - sizeof(type)))
#define va_end(ap)


// restituisce la lunghezza della stringa s (che deve essere terminata da 0)
int strlen(const char *s)
{
	int l = 0;

	while(*s++)
		++l;

	return l;
}

// copia al piu' l caratteri dalla stringa src alla stringa dest
char *strncpy(char *dest, const char *src, size_t l)
{
	size_t i;

	for(i = 0; i < l && src[i]; ++i)
		dest[i] = src[i];

	return dest;
}

bool str_equal(const char* first, const char* second) {

	while (*first && *second && *first++ == *second++)
		;

	return (!*first && !*second);
}

static const char hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

// converte l in stringa (notazione esadecimale)
static void htostr(char *buf, unsigned long l)
{
	int i;

	buf[0] = '0';
	buf[1] = 'x';

	for(i = 9; i > 1; --i) {
		buf[i] = hex_map[l % 16];
		l /= 16;
	}
}

// converte l in stringa (notazione decimale)
static void itostr(char *buf, unsigned int len, long l)
{
	int i, div = 1000000000, v, w = 0;

	if(l == (-2147483647 - 1)) {
 		strncpy(buf, "-2147483648", 12);
 		return;
   	} else if(l < 0) {
		buf[0] = '-';
		l = -l;
		i = 1;
	} else if(l == 0) {
		buf[0] = '0';
		buf[1] = 0;
		return;
	} else
		i = 0;

	while(i < len - 1 && div != 0) {
		if((v = l / div) || w) {
			buf[i++] = '0' + (char)v;
			w = 1;
		}

		l %= div;
		div /= 10;
	}

	buf[i] = 0;
}

#define DEC_BUFSIZE 12

// stampa formattata su stringa
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	int in = 0, out = 0, tmp;
	char *aux, buf[DEC_BUFSIZE];

	while(out < size - 1 && fmt[in]) {
		switch(fmt[in]) {
			case '%':
				switch(fmt[++in]) {
					case 'd':
						tmp = va_arg(ap, int);
						itostr(buf, DEC_BUFSIZE, tmp);
						if(strlen(buf) >
								size - out - 1)
							goto end;
						for(aux = buf; *aux; ++aux)
							str[out++] = *aux;
						break;
					case 'x':
						tmp = va_arg(ap, int);
						if(out > size - 11)
							goto end;
						htostr(&str[out], tmp);
						out += 10;
						break;
					case 's':
						aux = va_arg(ap, char *);
						while(out < size - 1 && *aux)
							str[out++] = *aux++;
						break;	
				}
				++in;
				break;
			default:
				str[out++] = fmt[in++];
		}
	}
end:
	str[out++] = 0;

	return out;
}

// stampa formattata su stringa (variadica)
int snprintf(char *buf, size_t n, const char *fmt, ...)
{
	va_list ap;
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return l;
}

//////////////////////////////////////////////////////////////////////////////
//                               IO DA CONSOLE                              //
//////////////////////////////////////////////////////////////////////////////

typedef char *ind_b;	// indirizzo di una porta

// ingresso di un byte da una porta di IO
extern "C" void inputb(ind_b reg, char &a);

// uscita di un byte su una porta di IO
extern "C" void outputb(char a, ind_b reg);

const int CON_BUF_SIZE = 0x0fa0;

struct con_status {
	unsigned char buffer[CON_BUF_SIZE];
	short x, y;
};

////////////////////////////////////////////////////////////////////////////////
//                           GESTIONE DEL VIDEO                               //
////////////////////////////////////////////////////////////////////////////////

unsigned char *VIDEO_MEM_BASE = (unsigned char *)0x000b8000;
const int VIDEO_MEM_SIZE = 0x00000fa0;

const short CUR_HIGH = 0x0e;
const short CUR_LOW = 0x0f;

const ind_b ADD_P = (ind_b)0x03d4;
const ind_b DAT_P = (ind_b)0x03d5;

const int COL_NUM = 80;
const int ROW_NUM = 25;

const unsigned char COL_BLACK = 0x00;
const unsigned char COL_WHITE = 0x07;

const unsigned char COL_BRIGHT = 0x08;

struct con_pos {
	unsigned char *base;
	unsigned char *ptr;
	short x, y;
} curr_pos;

unsigned char con_attr = COL_WHITE | (COL_BLACK << 4);

inline void PUT(con_pos *cp, char ch)
{
	*cp->ptr++ = ch;
	*cp->ptr++ = con_attr;
}

inline void gotoxy(con_pos *cp, int nx, int ny)
{
	int new_pos = nx + ny * COL_NUM;

	if(cp == &curr_pos) {
		outputb(CUR_HIGH, ADD_P);
		outputb((char)(new_pos >> 8), DAT_P);
		outputb(CUR_LOW, ADD_P);
		outputb((char)(new_pos&0xff), DAT_P);
	}

	cp->ptr = cp->base + new_pos * 2;

	cp->x = nx;
	cp->y = ny;
}

inline void scroll_up(con_pos *cp)
{
	cp->ptr = cp->base;

	while(cp->ptr < cp->base + VIDEO_MEM_SIZE - COL_NUM * 2) {
		*cp->ptr = *(cp->ptr + COL_NUM * 2);
		++cp->ptr;
	}

	for(; cp->ptr < cp->base + VIDEO_MEM_SIZE;) {
		*cp->ptr++ = ' ';
		*cp->ptr++ = con_attr;
	}

	gotoxy(cp, 0, ROW_NUM - 1);
}


inline void put_char(con_pos *cp, char ch)
{
	switch(ch) {
		case '\n':
			if(cp->y < ROW_NUM - 1)
				gotoxy(cp, 0, cp->y + 1);
			else
				scroll_up(cp);
			break;
		case '\b':
			if(cp->x > 0) {
				cp->ptr -= 2;
				PUT(cp, ' ');
				gotoxy(cp, cp->x - 1, cp->y);
			}
			break;
		default:
			if(ch < 31 || ch < 0)
				return;

			if(cp->x < COL_NUM) {
				PUT(cp, ch);
				gotoxy(cp, cp->x + 1, cp->y);
			} else {
				if(cp->y == ROW_NUM - 1) {
					scroll_up(cp);
					PUT(cp, ch);
					gotoxy(cp, 1, cp->y);
				} else {
					PUT(cp, ch);
					gotoxy(cp, 1, cp->y + 1);
				}
			}
	}
}

void con_init(void)
{
	curr_pos.base = curr_pos.ptr = VIDEO_MEM_BASE;

	while(curr_pos.ptr < VIDEO_MEM_BASE + VIDEO_MEM_SIZE)
		PUT(&curr_pos, ' ');

	gotoxy(&curr_pos, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////

// NOTA: non vengono fatti controlli sui parametri perche' queste routine
//  sono disponibili solo su gate con DPL 0. Deve essere il modulo di IO
//  (l' unico che le utilizza) a verificare i diritti dell' eventuale
//  chiamante.
//

// registri dell' interfaccia della tastiera
#define KBD_RBR		((ind_b)0x60)
#define KBD_PORT_B	((ind_b)0x61)
#define KBD_STR		((ind_b)0x64)

extern "C" void c_con_read(char &ch, bool &risu)
{
	char code, val;

	inputb(KBD_STR, val);
	if(val&0x01 == 0) {
		risu = false;
		return;
	}

	inputb(KBD_RBR, code);
	inputb(KBD_PORT_B, val);
	outputb(val|0x80, KBD_PORT_B);
	outputb(val, KBD_PORT_B);

	ch = code;
	risu = true;
}

extern "C" void c_con_write(const char *vett, int quanti)
{
	int i;

	for(i = 0; i < quanti; ++i)
		put_char(&curr_pos, vett[i]);
}

extern "C" void c_con_save(con_status *cs)
{
	memcpy(cs->buffer, VIDEO_MEM_BASE, VIDEO_MEM_SIZE);
	cs->x = curr_pos.x;
	cs->y = curr_pos.y;
}

extern "C" void c_con_load(const con_status *cs)
{
	memcpy(VIDEO_MEM_BASE, cs->buffer, VIDEO_MEM_SIZE);
	gotoxy(&curr_pos, cs->x, cs->y);
}

extern "C" void c_con_update(con_status *cs, const char *vett, int quanti)
{
	con_pos cp;
	int i;

	cp.base = cs->buffer;
	cp.ptr = cs->buffer + 2 * (cs->x + cs->y * COL_NUM);
	cp.x = cs->x;
	cp.y = cs->y;

	for(i = 0; i < quanti; ++i)
		put_char(&cp, vett[i]);

	cs->x = cp.x;
	cs->y = cp.y;
}

extern "C" void c_con_init(con_status *cs)
{
	int i;

	cs->x = cs->y = 0;

	for(i = 0; i < VIDEO_MEM_SIZE; i += 2) {
		cs->buffer[i] = ' ';
		cs->buffer[i + 1] = con_attr;
	}
}

extern "C" int printk(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, 1024, fmt, ap);
	va_end(ap);

	c_con_write(buf, strlen(buf));

	return l;
}

// Registrazione processi esterni
//

const int S = 2;
const int A = 2;

proc_elem *pe_tast;
proc_elem *in_com[S], *out_com[S];
proc_elem *ata[A];

void aggiungi_pe(proc_elem *p, estern_id interf)
{
	switch(interf) {
		case tastiera:
			pe_tast= p;
			break;
		case com1_in:
			in_com[0] = p;
			break;
		case com1_out:
			out_com[0] = p;
			break;
		case com2_in:
			in_com[1] = p;
			break;
		case com2_out:
			out_com[1] = p;
			break;
		case ata0:
			ata[0]=p;
			break;
		case ata1:
			ata[1]=p;
			break;
		default:
			; // activate_pe chiamata con parametri scorretti
	}
}

////////////////////////////////////////////////////////////////////////////////
// Allocatore a mappa di bit
////////////////////////////////////////////////////////////////////////////////

struct bm_t {
	unsigned int *vect;
	unsigned int size;
	unsigned int vecsize;
};

inline unsigned int bm_isset(bm_t *bm, unsigned int pos)
{
	return !(bm->vect[pos / 32] & (1UL << (pos % 32)));
}

inline void bm_set(bm_t *bm, unsigned int pos)
{
	bm->vect[pos / 32] &= ~(1UL << (pos % 32));
}

inline void bm_clear(bm_t *bm, unsigned int pos)
{
	bm->vect[pos / 32] |= (1UL << (pos % 32));
}

// crea la mappa BM, usando BUFFER come vettore; SIZE e' il numero di bit
//  nella mappa
//
void bm_create(bm_t *bm, unsigned int *buffer, unsigned int size)
{
	bm->vect = buffer;
	bm->size = size;
	bm->vecsize = ceild(size, sizeof(unsigned int) * 8);

	for (int i = 0; i < bm->vecsize; ++i)
		bm->vect[i] = 0xffffffff;
}


extern "C" int trova_bit(unsigned int v);

bool bm_alloc(bm_t *bm, unsigned int& pos) {

	int i = 0;
	bool risu = true;

	while (i < bm->vecsize && !bm->vect[i]) i++;
	if (i < bm->vecsize) {
		pos = trova_bit(bm->vect[i]);
		bm->vect[i] &= ~(1UL << pos);
		pos += sizeof(unsigned int) * 8 * i;
	} else 
		risu = false;
	return risu;
}

void bm_free(bm_t *bm, unsigned int pos)
{
	bm_clear(bm, pos);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// ALLOCAZIONE DELLA MEMORIA FISICA                                  //
// ////////////////////////////////////////////////////////////////////

// La memoria fisica viene gestita in due fasi: durante l'inizializzazione, si
// tiene traccia dell'indirizzo dell'ultimo byte non "occupato", tramite il
// puntatore mem_upper. Tale indirizzo viene fatto avanzare mano a mano che si
// decide come utilizzare la memoria fisica a disposizione:
// 1) all'inizio, poiche' il sistema e' stato caricato in memoria fisica dal
// bootstrap loader, il primo byte non occupato e' quello successivo all'ultimo
// indirizzo occupato dal sistema stesso (e' il linker, tramite il simbolo
// predefinito "_end", che ci permette di conoscere, staticamente, questo
// indirizzo)
// 2) di seguito al sistema, il boostrap loader ha caricato i moduli, e ha
// passato la lista dei descrittori di moduli tramite il registro %ebx.
// Scorrendo tale lista, facciamo avanzare il puntatore mem_upper oltre lo
// spazio ooccupato dai moduli 
// 3) il modulo di io deve essere ricopiato all'indirizzo a cui e' stato
// collegato (molto probabilmente diverso dall'indirizzo a cui il bootloader lo
// ha caricato, in quanto il bootloader non interpreta il contenuto dei
// moduli). Nel ricopiarlo, viene occupata ulteriore memoria fisica.  
// 4) di seguito al modulo io (ricopiato), viene allocato l'array di
// descrittori di pagine fisiche (la cui dimensione e' calcolata dinamicamente,
// in base al numero di pagine fisiche rimanenti dopo le operazioni precedenti)
// 5) tutta la memoria fisica restante, a partire dal primo indirizzo multiplo
// di 4096, viene usata per le pagine fisiche, destinate a contenere
// descrittori, tabelle e pagine virtuali.
//
// Durante queste operazioni, si vengono a scoprire regioni di memoria fisica
// non utilizzate (ad esempio, quando si fa avanzare mem_upper, al passo 3, per
// portarsi all'inizio dell'indirizzo di collegamento del modulo io). Queste
// regioni, man mano che vengono scoperte, vengono aggiunte allo heap di
// sistema. Nella seconda fase, il sistema usa lo heap cosi' ottenuto per
// allocare la memoria richiesta dall'operatore new (ad es., per "new
// richiesta").

// indirizzo fisico del primo byte non riferibile in memoria inferiore e
// superiore (rappresentano la memoria fisica installata sul sistema)
void* max_mem_lower;
void* max_mem_upper;

// indirizzo fisico del primo byte non occupato
extern void* mem_upper;


// descrittore di memoria fisica libera descrive una regione non allocata dello
// heap di sistema
struct des_mem {
	unsigned int dimensione;
	des_mem* next;
};

// testa della lista di descrittori di memoria fisica libera
des_mem* memlibera = 0;

// restituisce un valore dello stesso tipo T di v, uguale al piu' piccolo
// multiplo di a maggiore o uguale a v
template <class T>
inline
T allinea(T v, unsigned int a)
{
	unsigned int v1 = reinterpret_cast<unsigned int>(v);
	return reinterpret_cast<T>(v1 % a == 0 ? v1 : ((v1 + a - 1) / a) * a);
}

// se T e' int dobbiamo specializzare il template, perche'
// reinterpret_cast<unsigned int> non e' valido su un int
template <>
inline
int allinea(int v, unsigned int a) {
	return (v % a == 0 ? v : ((v + a - 1) / a) * a);
}

void debug_malloc();

// allocatore a lista first-fit, con strutture dati immerse usato dal nucleo
// stesso per allocare proc_elem, richiesta, ...
void* malloc(unsigned int quanti) {

	unsigned int dim = allinea(quanti, sizeof(int));

	des_mem *prec = 0, *scorri = memlibera;
	while (scorri != 0 && scorri->dimensione < dim) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(scorri == 0 || scorri->dimensione >= dim);

	void* p = 0;
	if (scorri != 0) {
		p = scorri + 1;
		if (scorri->dimensione - dim >= sizeof(des_mem) + sizeof(int)) {
			des_mem* nuovo = (des_mem*)add(p, dim);
			nuovo->dimensione = scorri->dimensione - dim - sizeof(des_mem);
			scorri->dimensione = dim;
			nuovo->next = scorri->next;
			if (prec != 0) 
				prec->next = nuovo;
			else
				memlibera = nuovo;
		} else {
			if (prec != 0)
				prec->next = scorri->next;
			else
				memlibera = scorri->next;
		}
		scorri->next = (des_mem*)0xdeadbeef;
		
	}
	return p;
}


// free_interna puo' essere usata anche in fase di inizializzazione, per
// definire le zone di memoria fisica che verranno utilizzate dall'allocatore
void free_interna(void* indirizzo, unsigned int quanti) {

	if (quanti == 0) return;
	if (indirizzo == 0) panic("free_interna(0)");

	des_mem *prec = 0, *scorri = memlibera;
	while (scorri != 0 && scorri < indirizzo) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(scorri == 0 || scorri >= indirizzo)
	if (scorri == indirizzo) {
		printk("indirizzo = 0x%x\n", (void*)indirizzo);
		panic("double free\n");
	}
	// assert(scorri == 0 || scorri > indirizzo)

	if (prec != 0 && add(prec + 1, prec->dimensione) == indirizzo) {
		if (scorri != 0 && add(indirizzo, quanti) == scorri) {
			prec->dimensione += quanti + sizeof(des_mem) + scorri->dimensione;
			prec->next = scorri->next;
		} else {
			prec->dimensione += quanti;
		}
	} else if (scorri != 0 && add(indirizzo, quanti) == scorri) {
		des_mem salva = *scorri;
		des_mem* nuovo = (des_mem*)indirizzo;
		*nuovo = salva;
		nuovo->dimensione += quanti;
		if (prec != 0) 
			prec->next = nuovo;
		else
			memlibera = nuovo;
	} else if (quanti >= sizeof(des_mem)) {
		des_mem* nuovo = (des_mem*)indirizzo;
		nuovo->dimensione = quanti - sizeof(des_mem);
		nuovo->next = scorri;
		if (prec != 0)
			prec->next = nuovo;
		else
			memlibera = nuovo;
	}
}

// free deve essere usata solo con puntatori restituiti da malloc
void free(void* p) {
	if (p == 0) return;
	des_mem* des = (des_mem*)p - 1;
	if (des->next != (void*)0xdeadbeef)
		panic("free() errata");
	free_interna(des, des->dimensione + sizeof(des_mem));
}



void debug_malloc() {
	des_mem* scorri = memlibera;
	unsigned int tot = 0;
	printk("--- MEMORIA LIBERA ---\n");
	while (scorri != 0) {
		printk("%x (%d)\n", scorri, scorri->dimensione);
		tot += scorri->dimensione;
		scorri = scorri->next;
	}
	printk("TOT: %d byte (%d KB)\n", tot, tot / 1024);
	printk("----------------------\n");
}

void* operator new(unsigned int size) {
	return malloc(size);
}

void* operator new[](unsigned int size) {
	return malloc(size);
}

void operator delete(void* p) {
	free(p);
}

void operator delete[](void* p) {
	free(p);
}

// allocazione linerare, da usare durante la fase di inizializzazione.  Si
// mantiene un puntatore (mem_upper) all'ultimo byte non occupato.  Tale
// puntatore puo' solo avanzare, tramite le funzioni 'occupa' e 'salta_a', e
// non puo' superare la massima memoria fisica contenuta nel sistema
// (max_mem_upper) Se il puntatore viene fatto avanzare tramite la funzione
// 'salta_a', i byte "saltati" vengono dati all'allocatore a lista
void* occupa(unsigned int quanti) {
	void* p = 0;
	void* appoggio = add(mem_upper, quanti);
	if (appoggio <= max_mem_upper) {
		p = mem_upper;
		mem_upper = appoggio;
	}
	return p;
}

int salta_a(void* indirizzo) {
	int saltati = -1;
	if (indirizzo >= mem_upper && indirizzo < max_mem_upper) {
		saltati = distance(indirizzo, mem_upper);
		free_interna(mem_upper, saltati);
		mem_upper = indirizzo;
	}
	return saltati;
}


/////////////////////////////////////////////////////////////////////////
// PAGINAZIONE                                                         //
/////////////////////////////////////////////////////////////////////////

struct descrittore_pagina {
	unsigned int P:		1;	// bit di presenza
	unsigned int RW:	1;	// Read/Write
	unsigned int US:	1;	// User/Supervisor
	unsigned int PWT:	1;	// Page Write Through
	unsigned int PCD:	1;	// Page Cache Disable
	unsigned int A:		1;	// Accessed
	unsigned int D:		1;	// Dirty
	unsigned int pgsz:	1;	// non visto a lezione
	unsigned int global:	1;	// non visto a lezione
	unsigned int avail:	1;	// non usati
	unsigned int preload:	1;	// la pag. deve essere precaricata
	unsigned int azzera:    1;	// ottimizzazione pagine iniz. vuote
	unsigned int address:	20;	// indirizzo fisico/blocco
};

// il descrittore di tabella delle pagine e' identico al descrittore di pagina
typedef descrittore_pagina descrittore_tabella;

// Un direttorio e' un vettore di 1024 descrittori di tabella
struct direttorio {
	descrittore_tabella entrate[1024];
}; 

// Una tabella delle pagine e' un vettore di 1024 descrittori di pagina
struct tabella_pagine {
	descrittore_pagina  entrate[1024];
};

// Una pagina virtuale la vediamo come una sequenza di 4096 byte, o 1024 parole
// lunghe 
struct pagina {
	union {
		unsigned char byte[SIZE_PAGINA];
		unsigned int  parole_lunghe[SIZE_PAGINA / sizeof(unsigned int)];
	};
};

// una pagina fisica occupata puo' contenere un direttorio, una tabella delle
// pagine o una pagina virtuale.  Definiamo allora pagina_fisica come una
// unione di questi tre tipi 
union pagina_fisica {
	direttorio	dir;
	tabella_pagine	tab;
	pagina		pag;
};

// dato un puntatore a un direttorio, a una tabella delle pagine o a una pagina
// virtuale, possiamo aver bisogno di un puntatore alla pagina fisica che li
// contiene
inline pagina_fisica* pfis(direttorio* pdir)
{
	return reinterpret_cast<pagina_fisica*>(pdir);
}

inline pagina_fisica* pfis(tabella_pagine* ptab)
{
	return reinterpret_cast<pagina_fisica*>(ptab);
}

inline pagina_fisica* pfis(pagina* ppag)
{
	return reinterpret_cast<pagina_fisica*>(ppag);
}

// dato un indirizzo virtuale, ne restituisce l'indice nel direttorio
short indice_direttorio(void* indirizzo) {
	return (reinterpret_cast<unsigned int>(indirizzo) & 0xffc00000) >> 22;
}

// dato un indirizzo virtuale, ne restituisce l'indice nella tabella delle
// pagine
short indice_tabella(void* indirizzo) {
	return (reinterpret_cast<unsigned int>(indirizzo) & 0x003ff000) >> 12;
}

// dato un puntatore ad un descrittore di tabella, restituisce
// un puntatore alla tabella puntata
tabella_pagine* tabella_puntata(descrittore_tabella* pdes_tab) {
	return reinterpret_cast<tabella_pagine*>(pdes_tab->address << 12);
}

// dato un puntatore ad un descrittore di pagina, restituisce
// un puntatore alla pagina puntata
pagina* pagina_puntata(descrittore_pagina* pdes_pag) {
	return reinterpret_cast<pagina*>(pdes_pag->address << 12);
}

direttorio* direttorio_principale;


extern "C" void carica_cr3(direttorio* dir);
extern "C" direttorio* leggi_cr3();
extern "C" void attiva_paginazione();

/////////////////////////////////////////////////////////////////////////
// GESTIONE DELLE PAGINE FISICHE                                       //
// //////////////////////////////////////////////////////////////////////
// possibili contenuti delle pagine fisiche
enum cont_pf {
	PAGINA_LIBERA,		// pagina allocabile
	DIRETTORIO,		// direttorio delle tabelle
	TABELLA_PRIVATA,	// tabella appartenente a un solo processo
	TABELLA_CONDIVISA,	// tabella appartenente a piu' processi
				// (non rimpiazzabile, ma che punta a pagine
				// rimpiazzabili)
	TABELLA_RESIDENTE,	// tabella non rimpiazzabile, che punta a 
				// a pagine non rimpiazzabili
	PAGINA_VIRTUALE,	// pagina rimpiazzabile
	PAGINA_RESIDENTE };	// pagina non rimpiazzabile

// La maggior parte della memoria principale del sistema viene utilizzata per
// implementare le "pagine fisiche".  Le pagine fisiche vengono usate per
// contenere direttori, tabelle delle pagine o pagine virtuali.  Per ogni
// pagina fisica, esiste un "descrittore di pagina fisica", che contiene:
// - un campo "contenuto", che puo'assumere uno dei valori sopra elencati
// - il campo contatore degli accessi (per il rimpiazzamento LRU o LFU)
// - il campo "blocco", che contiene il numero del blocco della pagina o
//   tabella nello swap
//   
//se la pagina e' libera, il descrittore di pagina fisica contiene anche
//l'indirizzo del descrittore della prossima pagina fisica libera (0 se non ve
//ne sono)
//
//se la pagina contiene una tabella, il destrittore di pagina fisica contiene 
//anche il puntatore al direttorio che mappa la tabella, l'indice del 
//descrittore della tabella all'interno del direttorio e un campo "quante", che 
//contiene il numero di entrate con P != 0 all'interno della tabella 
//(informazioni utili per il rimpiazzamento della tabella)
//
//se la pagina contiene una pagina virtuale, il descrittore di pagina fisica 
//contiene un puntatore all'(unica) tabella che mappa la pagina e l'indirizzo 
//virtuale della pagina (informazioni utili per il rimpiazzamento della pagina)
struct des_pf {
	cont_pf		contenuto;
	unsigned int	contatore;
	unsigned int	blocco;
	union {
		struct { // info relative a una pagina
			tabella_pagine*	tabella;
			void*		indirizzo_virtuale;
		};
		struct { // info relative a una tabella
			direttorio*     dir;
			short		indice;
			short		quante;
		};
		struct  { // info relative a una pagina libera
			des_pf*		prossima_libera;
		};
	};
};


// puntatore al primo descrittore di pagina fisica
des_pf* pagine_fisiche;

// numero di pagine fisiche
unsigned long num_pagine_fisiche;

// testa della lista di pagine fisiche libere
des_pf* pagine_libere = 0;

// indirizzo fisico della prima pagina fisica
pagina_fisica* prima_pagina;

// restituisce l'indirizzo fisico della pagina fisica associata al descrittore
// passato come argomento
pagina_fisica* indirizzo(des_pf* p) {
	return static_cast<pagina_fisica*>(add(prima_pagina, (p - pagine_fisiche) * SIZE_PAGINA));
}

// dato l'indirizzo di una pagina fisica, restituisce un puntatore al
// descrittore associato
des_pf* struttura(pagina_fisica* pagina) {
	return &pagine_fisiche[distance(pagina, prima_pagina) / SIZE_PAGINA];
}

// init_pagine_fisiche viene chiamata in fase di inizalizzazione.  Tutta la
// memoria non ancora occupata viene usata per le pagine fisiche.  La funzione
// si preoccupa anche di allocare lo spazio per i descrittori di pagina fisica,
// e di inizializzarli in modo che tutte le pagine fisiche risultino libere
void init_pagine_fisiche() {

	// allineamo mem_upper, per motivi di prestazioni
	salta_a(allinea(mem_upper, sizeof(int)));

	// calcoliamo quanta memoria principale rimane
	int dimensione = distance(max_mem_upper, mem_upper);

	if (dimensione <= 0)
		panic("Non ci sono pagine libere");

	// calcoliamo quante pagine fisiche possiamo definire
	unsigned int quante = dimensione / (SIZE_PAGINA + sizeof(des_pf));

	// allochiamo i corrispondenti descrittori di pagina fisica
	pagine_fisiche = static_cast<des_pf*>(occupa(sizeof(des_pf) * quante));

	// riallineamo mem_upper a un multiplo di pagina
	salta_a(allinea(mem_upper, SIZE_PAGINA));

	// ricalcoliamo quante col nuovo mem_upper, per sicurezza
	quante = distance(max_mem_upper, mem_upper) / SIZE_PAGINA;

	// occupiamo il resto della memoria principale con le pagine fisiche
	prima_pagina = static_cast<pagina_fisica*>(occupa(quante * SIZE_PAGINA));
	num_pagine_fisiche = quante;

	// se resta qualcosa (improbabile), lo diamo all'allocatore a lista
	salta_a(max_mem_upper);

	// costruiamo la lista delle pagine fisiche libere
	des_pf* p = 0;
	for (int i = quante - 1; i >= 0; i--) {
		pagine_fisiche[i].contenuto = PAGINA_LIBERA;
		pagine_fisiche[i].prossima_libera = p;
		p = &pagine_fisiche[i];
	}
	pagine_libere = &pagine_fisiche[0];
}

des_pf* alloca_pagina() {
	des_pf* p = pagine_libere;
	if (p != 0)
		pagine_libere = pagine_libere->prossima_libera;
	return p;
}

direttorio* alloca_direttorio() {
	des_pf* p = alloca_pagina();
	if (p == 0) return 0;
	p->contenuto = DIRETTORIO;
	return &indirizzo(p)->dir;
}

tabella_pagine* alloca_tabella(cont_pf tipo = TABELLA_PRIVATA) {
	des_pf* p = alloca_pagina();
	if (p == 0) return 0;
	p->contenuto = tipo;
	return &indirizzo(p)->tab;
}

tabella_pagine* alloca_tabella_condivisa() {
	return alloca_tabella(TABELLA_CONDIVISA);
}

tabella_pagine* alloca_tabella_residente() {
	return alloca_tabella(TABELLA_RESIDENTE);
}

pagina* alloca_pagina_virtuale(cont_pf tipo = PAGINA_VIRTUALE) {
	des_pf* p = alloca_pagina();
	if (p == 0) return 0;
	p->contenuto = tipo;
	return &indirizzo(p)->pag;
}

pagina* alloca_pagina_residente() {
	return alloca_pagina_virtuale(PAGINA_RESIDENTE);
}

void rilascia_pagina(des_pf* p) {
	p->contenuto = PAGINA_LIBERA;
	p->prossima_libera = pagine_libere;
	pagine_libere = p;
}

inline
void rilascia(direttorio* d) {
	rilascia_pagina(struttura(pfis(d)));
}

inline
void rilascia(tabella_pagine* d) {
	rilascia_pagina(struttura(pfis(d)));
}

inline
void rilascia(pagina* d) {
	rilascia_pagina(struttura(pfis(d)));
}


void debug_pagine_fisiche(int prima, int quante) {
	for (int i = 0; i < quante; i++) {
		des_pf* p = &pagine_fisiche[prima + i];
		char* tipo = 0;

		printk("%d(%x<->%x): ", i + prima, p, indirizzo(p));
		switch (p->contenuto) {
		case PAGINA_LIBERA:
			printk("libera (prossima = %x)\n", p->prossima_libera);
			break;
		case DIRETTORIO:
			printk("direttorio delle tabelle\n");
			break;
		case TABELLA_PRIVATA:
			printk("tabella delle pagine, mappata da %x[%d]\n",
					p->dir, p->indice);
			break;
		case TABELLA_RESIDENTE:
			if (!tipo) tipo = "residente";
		case TABELLA_CONDIVISA:
			if (!tipo) tipo = "condivisa";
			printk("tabella delle pagine %s\n", tipo);
			break;
		case PAGINA_VIRTUALE:
			tipo = "";
		case PAGINA_RESIDENTE:
			if (!tipo) tipo = " (residente)";
			printk("pagina virtuale%s, mappata a %x da %x\n",
					tipo, p->indirizzo_virtuale, p->tabella);
			break;
		default:
			printk("contenuto sconosciuto: %d\n", p->contenuto);
			break;
		}
	}
}


// funzioni che aggiornano sia le strutture dati della paginazione che
// quelle relative alla gestione delle pagine fisiche

// collega la tabella puntata da "ptab" al direttorio puntato "pdir" all'indice
// "indice". Aggiorna anche il descrittore di pagina fisica corrispondente alla 
// pagina fisica che contiene la tabella.
// NOTA: si suppone che la tabella non fosse precedentemente collegata, e 
// quindi che il corrispondente descrittore di tabella nel direttorio contenga
// il numero del blocco della tabella nello swap
descrittore_tabella* collega_tabella(direttorio* pdir, tabella_pagine* ptab, short indice)
{
	descrittore_tabella* pdes_tab = &pdir->entrate[indice];

	// mapping inverso:
	// ricaviamo il descrittore della pagina fisica che contiene la tabella
	des_pf* ppf  = struttura(pfis(ptab));
	ppf->dir          = pdir;
	ppf->indice       = indice;
	ppf->quante       = 0; // inizialmente, nessuna pagina e' presente
	// il contatore deve essere inizializzato come se fosse appena stato 
	// effettuato un accesso (cosa che avverra' al termine della gestione 
	// del page fault). Diversamente, la pagina o tabella appena caricata 
	// potrebbe essere subito scelta per essere rimpiazzata
	ppf->contatore    = 1 << 31;
	ppf->blocco	  = pdes_tab->address; // vedi NOTA prec.
	
	// mapping diretto
	pdes_tab->address	= uint(ptab) >> 12;
	pdes_tab->D		= 0; // bit riservato nel caso di descr. tabella
	pdes_tab->pgsz		= 0; // pagine di 4KB
	pdes_tab->P		= 1; // presente

	return pdes_tab;
}

// scollega la tabella di indice "indice" dal direttorio puntato da "ptab".  
// Aggiorna anche il contatore di pagine nel descrittore di pagina fisica 
// corrispondente alla tabella
descrittore_tabella* scollega_tabella(direttorio* pdir, short indice)
{
	// poniamo a 0 il bit di presenza nel corrispondente descrittore di 
	// tabella
	descrittore_tabella* pdes_tab = &pdir->entrate[indice];
	pdes_tab->P = 0;

	// quindi scriviamo il numero del blocco nello swap al posto 
	// dell'indirizzo fisico
	des_pf* ppf = struttura(pfis(tabella_puntata(pdes_tab)));
	pdes_tab->address = ppf->blocco;

	return pdes_tab;
}

// collega la pagina puntata da "ptab" alla tabella puntata da "ptab", 
// all'indirizzo virtuale "ind_virtuale". Aggiorna anche il descrittore di 
// pagina fisica corrispondente alla pagina fisica che contiene la pagina.
// NOTA: si suppone che la pagina non fosse precedentemente collegata, e quindi 
// che il corrispondente descrittore di pagina nella tabella contenga
// il numero del blocco della pagina nello swap
descrittore_pagina* collega_pagina(tabella_pagine* ptab, pagina* pag, void* ind_virtuale) {
	
	descrittore_pagina* pdes_pag = &ptab->entrate[indice_tabella(ind_virtuale)];

	// mapping inverso
	des_pf* ppf = struttura(pfis(pag));
	ppf->tabella		= ptab;
	ppf->indirizzo_virtuale = ind_virtuale;
	ppf->contatore		= 1 << 31;
	ppf->blocco		= pdes_pag->address; // vedi NOTA prec

	// incremento del contatore nella tabella
	ppf = struttura(pfis(ptab));
	ppf->quante++;

	// mapping diretto
	pdes_pag->address	= uint(pag) >> 12;
	pdes_pag->pgsz		= 0; // bit riservato nel caso di descr. di pagina
	pdes_pag->P		= 1; // presente

	return pdes_pag;
}

// scollega la pagina di indirizzo virtuale "ind_virtuale" dalla tabella 
// puntata da "ptab". Aggiorna anche il contatore di pagine nel descrittore di 
// pagina fisica corrispondente alla tabella
descrittore_pagina* scollega_pagina(tabella_pagine* ptab, void* ind_virtuale) {

	// poniamo a 0 il bit di presenza nel corrispondente descrittore di 
	// pagina
	descrittore_pagina* pdes_pag = &ptab->entrate[indice_tabella(ind_virtuale)];
	pdes_pag->P = 0;

	// quindi scriviamo il numero del blocco nello swap al posto 
	// dell'indirizzo fisico
	des_pf* ppf = struttura(pfis(pagina_puntata(pdes_pag)));
	pdes_pag->address = ppf->blocco;

	// infine, decrementiamo il numero di pagine puntate dalla tabella
	ppf = struttura(pfis(ptab));
	ppf->quante--;

	return pdes_pag;
}
	



/////////////////////////////////////////////////////////////////////////////
// MEMORIA VIRTUALE                                                        //
/////////////////////////////////////////////////////////////////////////////
extern "C" void readhd_n(short ind_ata,short drv,void* vetti,unsigned int primo,unsigned char &quanti,char &errore);
extern "C" void writehd_n(short ind_ata,short drv,void* vetti,unsigned int primo,unsigned char &quanti,char &errore);

bm_t block_bm;

bool leggi_blocco(unsigned int blocco, void* dest) {
	unsigned char da_leggere = 8;
	char errore;

	readhd_n(0, 1, dest, blocco * 8, da_leggere, errore);
	if (da_leggere != 0 || errore != 0) { 
		printk("Impossibile leggere il blocco %d\n", blocco);
		printk("letti %d settori su 8, errore = %d\n", 8 - da_leggere, errore);
		return false;
	}
	return true;
}

bool scrivi_blocco(unsigned int blocco, void* dest) {
	unsigned char da_scrivere = 8;
	char errore;

	writehd_n(0, 1, dest, blocco * 8, da_scrivere, errore);
	if (da_scrivere != 0 || errore != 0) { 
		printk("Impossibile scrivere il blocco %d\n", blocco);
		printk("scritti %d settori su 8, errore = %d\n", 8 - da_scrivere, errore);
		return false;
	}
	return true;
}

void aggiorna_statistiche() // LRU
{
	des_pf *ppf1, *ppf2;
	tabella_pagine* ptab;
	descrittore_pagina* pp;
	static int count = 0;

	for (int i = 0; i < num_pagine_fisiche; i++) {
		ppf1 = &pagine_fisiche[i];
		int quante = 1024;
		switch (ppf1->contenuto) {
		case PAGINA_VIRTUALE:
		case PAGINA_LIBERA:
		case TABELLA_RESIDENTE:
			break;
		case TABELLA_PRIVATA:
		case TABELLA_CONDIVISA:
			quante = ppf1->quante;
		case DIRETTORIO:
			// anche se e' un direttorio, lo vediamo come tabella, 
			// tanto il bit A e' nella stessa posizione
			if (quante > 0) {
				ptab = &indirizzo(ppf1)->tab;
				for (int j = 0; j < 1024; j++) {
					pp = &ptab->entrate[j];
					if (pp->P == 1) {
						ppf2 = struttura(pfis(pagina_puntata(pp)));
						ppf2->contatore >>= 1;
						ppf2->contatore |= pp->A << 31;
						pp->A = 0;
					}
				}
			}
			break;
		default:
			break;
		}
	}
	carica_cr3(leggi_cr3());
}

struct page_fault_error {
	unsigned int prot  : 1;
	unsigned int write : 1;
	unsigned int user  : 1;
	unsigned int res   : 1;
	unsigned int pad   : 28;
};

int pf_mutex;

tabella_pagine* rimpiazzamento_tabella();
pagina* 	rimpiazzamento_pagina();


// carica la pagina descritta da pdes_pag in memoria fisica,
// all'indirizzo pag
bool carica_pagina(descrittore_pagina* pdes_pag, pagina* pag)
{
	if (pdes_pag->azzera == 1) {
		unsigned int blocco;
		if (! bm_alloc(&block_bm, blocco) ) {
			printk("spazio nello swap insufficiente");
			return false;
		}
		pdes_pag->address = blocco;
		memset(pag, 0, SIZE_PAGINA);
		pdes_pag->azzera = 0;
		return true;
	} else if (pdes_pag->address != 0)
		return leggi_blocco(pdes_pag->address, pag);
	return false;
}

// carica la tabella descritta da pdes_tab in memoria fisica, all'indirizzo
// ptab
bool carica_tabella(descrittore_tabella* pdes_tab, tabella_pagine* ptab)
{
	if (pdes_tab->azzera == 1) {
		if (! bm_alloc(&block_bm, struttura(pfis(ptab))->blocco)) {
			printk("spazio nello swap insufficiente");
			return false;
		}

		for (int i = 0; i < 1024; i++) {
			descrittore_pagina* pdes_pag = &ptab->entrate[i];
			pdes_pag->address	= 0;
			pdes_pag->D		= 0;
			pdes_pag->A		= 0;
			pdes_pag->pgsz		= 0;
			pdes_pag->RW		= pdes_tab->RW;
			pdes_pag->US		= pdes_tab->US;
			pdes_pag->azzera	= 1;
			pdes_pag->P		= 0;
		}
		pdes_tab->azzera = 0;
		return true;
	} else if (pdes_tab->address != 0)
		return leggi_blocco(pdes_tab->address, ptab);
	return false;
}

// routine di trasferimento
// trasferisce la pagina di indirizzo virtuale "indirizzo_virtuale"
// in memoria fisica
void trasferimento(void* indirizzo_virtuale, page_fault_error errore)
{
	direttorio* pdir;
	tabella_pagine* ptab;
	descrittore_tabella* pdes_tab;
	descrittore_pagina* pdes_pag;
	pagina* pag;
	
	// anche se la routine gira a interruzioni disabilitate, e' necessario 
	// che acquisisca un semaforo di mutua esclusione. Infatti, se ci sara' 
	// da scrivere e/o caricare una pagina e/o una tabella dallo swap, 
	// verra' schedulato un altro processo, che potrebbe a sua volta 
	// genereare un page fault e richiamare la routine di trasferimento
	sem_wait(pf_mutex);

	// in questa realizzazione, si accede all direttorio e alle tabelle 
	// tramite un indirizzo virtuale uguale al loro indirizzo fisico (in 
	// virtu' del fatto che la memoria fisica e' stata mappata in memoria 
	// virtuale)
	pdir = leggi_cr3(); // direttorio del processo

	// ricaviamo per prima cosa il descrittore di pagina interessato dal 
	// fault
	pdes_tab = &pdir->entrate[indice_direttorio(indirizzo_virtuale)];

	// se l'accesso era in scrittura e (tutte le pagine puntate da) la 
	// tabella e' di sola lettura, il programma ha commesso un errore e va 
	// interrotto
	if (errore.write == 1 && pdes_tab->RW == 0) {
		printk("Errore di accesso in scrittura\n");
		goto error;
	}
	
	if (pdes_tab->P == 0) {
		// la tabella e' assente
		
		// proviamo ad allocare una pagina fisica per la tabella. Se 
		// non ve ne sono, dobbiamo invocare la routine di 
		// rimpiazzamento per creare lo spazio
		ptab = alloca_tabella();
		if (ptab == 0) 
			ptab = rimpiazzamento_tabella();

		// ora possiamo caricare la tabella (operazione bloccante)
		if (! carica_tabella(pdes_tab, ptab) )
			goto error;
		
		// e collegarla al direttorio
		(void)collega_tabella(pdir, ptab, indice_tabella(indirizzo_virtuale));
		pdes_tab->A = 0;
	} else {
		// la tabella e' presente
		ptab = tabella_puntata(pdes_tab);
	}
	// ptab punta alla tabella delle pagine (sia che fosse gia' presente, 
	// sia che fosse stata appena caricata)

	// ricaviamo il descrittore di pagina interessato dal fault
	pdes_pag = &ptab->entrate[indice_tabella(indirizzo_virtuale)];
	

	// se l'accesso era in scrittura e la pagina e' di sola lettura, il 
	// programma ha commesso un errore e va interrotto
	if (errore.write == 1 && pdes_pag->RW == 0) {
		printk("Errore di accesso in scrittura\n");
		goto error;
	}

	// dobbiamo controllare che la pagina sia effettivamente assente
	// (scenario: un processo P1 causa un page fault su una pagina p, si
	// blocca per caricarla e viene schedulato P2, che causa un page fault 
	// sulla stessa pagina p e si blocca sul mutex. Quanto P1 finisce di 
	// caricare p, rilascia il mutex e sveglia P2, che ora trova pagina p 
	// presente)
	if (pdes_pag->P == 0) {

		// proviamo ad allocare una pagina fisica per la pagina. Se non 
		// ve ne sono, dobbiamo invocare la routine di rimpiazzamento 
		// per creare lo spazio
		pag = alloca_pagina_virtuale();
		if (pag == 0) 
			pag = rimpiazzamento_pagina();
		
		// proviamo a caricare la pagina (operazione bloccante)
		if (! carica_pagina(pdes_pag, pag) )
			goto error;

		// infine colleghiamo la pagina
		(void)collega_pagina(ptab, pag, indirizzo_virtuale);
		pdes_pag->D = 0;
		pdes_pag->A = 0;
	}
	sem_signal(pf_mutex);
	return;

error:
	printk("page fault non risolubile");
	// anche in caso di errore dobbiamo rilasciare il semaforo di mutua 
	// esclusione, pena il blocco di tutta la memoria virtuale
	sem_signal(pf_mutex);
	abort_p();
}

// routine di rimpiazzamento
// libera una pagina fisica rimuovoendo e (eventualmente) copiando nello swap 
// una tabella privata o una pagina virtuale
des_pf* rimpiazzamento() {
	
	des_pf *ppf, *vittima = 0;
	void* ind_virtuale;
	unsigned int blocco;
	
	// scorriamo tutti i descrittori di pagina fisica alla ricerca della 
	// prima pagina che contiene una pagina virtuale o una tabella privata
	int i = 0;
	while (i < num_pagine_fisiche &&
	       pagine_fisiche[i].contenuto != PAGINA_VIRTUALE &&
	       pagine_fisiche[i].contenuto != TABELLA_PRIVATA)
		i++;

	// se non ce ne sono, la situazione e' critica
	if (i >= num_pagine_fisiche)
		goto error;

	// partendo da quella appena trovata, cerchiamo la pagina con il valore 
	// minimo per il contatore
	vittima = &pagine_fisiche[i];
	for (i++; i < num_pagine_fisiche; i++) {
		ppf = &pagine_fisiche[i];
		switch (ppf->contenuto) {
		case PAGINA_VIRTUALE:
			// se e' una pagina virtuale, dobbiamo preferirla a una 
			// tabella, in caso di uguaglianza nei contatori
			if (ppf->contatore < vittima->contatore ||
			    ppf->contatore == vittima->contatore && vittima->contenuto == TABELLA_PRIVATA) 
				vittima = ppf;
			break;
		case TABELLA_PRIVATA:
			if (ppf->contatore < vittima->contatore)
				vittima = ppf;
			break;
		default:
			// in tutti gli altri casi la pagina non e' 
			// rimpiazzabile
			break;
		}
	}

	// se vittima e' 0 c'e' qualcosa che non va
	if (vittima == 0) goto error;

	if (vittima->contenuto == TABELLA_PRIVATA) {
		// usiamo le informazioni nel mapping inverso per ricavare 
		// subito l'entrata nel direttorio da modificare
		descrittore_tabella *pdes_tab = scollega_tabella(vittima->dir, vittima->indice);

		// "dovremmo" cancellarla e basta, ma contiene i blocchi delle
		// pagine allocate dinamicamente
		scrivi_blocco(pdes_tab->address, indirizzo(vittima));
	} else {
		// usiamo le informazioni nel mapping inverso per ricavare 
		// subito l'entrata nel direttorio da modificare
		tabella_pagine* ptab = vittima->tabella;

		// dobbiamo rendere la pagina non presente prima di cominciare 
		// a salvarla nello swap (altrimenti, qualche altro processo 
		// potrebbe cercare di scrivervi mentre e' ancora in corso 
		// l'operazione di salvataggio)
		descrittore_pagina* pdes_pag = scollega_pagina(ptab, vittima->indirizzo_virtuale);

		invalida_entrata_TLB(vittima->indirizzo_virtuale);
		if (pdes_pag->D == 1) {
			// pagina modificata, va salvata nello swap (operazione 
			// bloccante)
			scrivi_blocco(pdes_pag->address, indirizzo(vittima));
		} 
	}
	return vittima;

error:
	panic("Impossibile rimpiazzare pagine");
}

tabella_pagine* rimpiazzamento_tabella()
{
	des_pf *ppf = rimpiazzamento();

	if (ppf == 0) return 0;

	ppf->contenuto = TABELLA_PRIVATA;
	return &indirizzo(ppf)->tab;
}

pagina* rimpiazzamento_pagina() 
{
	des_pf *ppf = rimpiazzamento();

	if (ppf == 0) return 0;

	ppf->contenuto = PAGINA_VIRTUALE;
	return &indirizzo(ppf)->pag;
}
	
		
	

/////////////////////////////////////////////////////////////////////////
// ALLOCAZIONE DESCRITTORI DI PROCESSO                                 //
// //////////////////////////////////////////////////////////////////////

extern "C" int alloca_tss();
extern "C" void rilascia_tss(int indice);

//////////////////////////////////////////////////////////////////////////////
//                         FUNZIONI AD USO INTERNO                          //
//////////////////////////////////////////////////////////////////////////////

// stampa MSG su schermo e termina le elaborazioni del sistema
//
extern "C" void backtrace();
extern "C" void panic(const char *msg)
{
	printk("%s\n", msg);
	backtrace();
	asm("1: nop; jmp 1b");
}

// inserisce P_ELEM in P_CODA, mantenendola ordinata per priorita' decrescente
//
void inserimento_coda(proc_elem *&p_coda, proc_elem *p_elem)
{
	proc_elem *pp, *prevp;

	pp = p_coda;
	prevp = 0;
	while(pp && pp->priority >= p_elem->priority) {
		prevp = pp;
		pp = pp->puntatore;
	}

	if(!prevp)
		p_coda = p_elem;
	else
		prevp->puntatore = p_elem;

	p_elem->puntatore = pp;
}

// rimuove il primo elemento da P_CODA e lo mette in P_ELEM
//
void rimozione_coda(proc_elem *&p_coda, proc_elem *&p_elem)
{
	p_elem = p_coda;

	if(p_coda)
		p_coda = p_coda->puntatore;
}

// scheduler a priorita'
//
extern "C" void schedulatore(void)
{
	rimozione_coda(pronti, esecuzione);
}

// inserisce P nella coda delle richieste al timer
//
void inserimento_coda_timer(richiesta *p)
{
	richiesta *r, *precedente;
	bool ins;

	r = descrittore_timer;
	precedente = 0;
	ins = false;

	while(r != 0 && !ins)
		if(p->d_attesa > r->d_attesa) {
			p->d_attesa -= r->d_attesa;
			precedente = r;
			r = r->p_rich;
		} else
			ins = true;

	p->p_rich = r;
	if(precedente != 0)
		precedente->p_rich = p;
	else
		descrittore_timer = p;

	if(r != 0)
		r->d_attesa -= p->d_attesa;
}

// driver del timer
//
extern "C" void c_driver_t(void)
{
	richiesta *p;

	if(descrittore_timer != 0)
		descrittore_timer->d_attesa--;

	while(descrittore_timer != 0 && descrittore_timer->d_attesa == 0) {
		inserimento_coda(pronti, descrittore_timer->pp);
		p = descrittore_timer;
		descrittore_timer = descrittore_timer->p_rich;
		delete p;
	}

	aggiorna_statistiche();

	schedulatore();
}

// verifica i permessi del processo sui parametri passati (problema del
//  Cavallo di Troia)

// Questa versione di verifica_area presuppone che la memoria
// non sia virtuale

extern "C" bool verifica_area(void *area, unsigned int dim, bool write)
{
	des_proc *pdes_proc;
	direttorio* pdirettorio;
	char liv;

	pdes_proc = des_p(esecuzione->identifier);
	liv = pdes_proc->liv;
	pdirettorio = pdes_proc->cr3;

	for (void* i = area; i < add(area, dim); i = add(i, SIZE_PAGINA)) {
		descrittore_tabella *pdes_tab = &pdirettorio->entrate[indice_direttorio(i)];
		if (pdes_tab->P == 0)  // XXX: page fault esclusi
			return false;
		tabella_pagine *ptab = tabella_puntata(pdes_tab);
		descrittore_pagina *pdes_pag = &ptab->entrate[indice_tabella(i)];
		if (pdes_pag->P == 0)  // XXX: page fault esclusi
			return false;
		if (liv == LIV_UTENTE && pdes_pag->US == 0)
			return false;
		if (write && pdes_pag->RW == 0)
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////
// CREAZIONE PROCESSI                                                           //
//////////////////////////////////////////////////////////////////////////////////

pagina* crea_pila_utente(direttorio* pdir, void* ind_virtuale, int num_pagine)
{
	pagina* ind_fisico;
	tabella_pagine* ptab;
	descrittore_tabella* pdes_tab;
	descrittore_pagina*  pdes_pag;
	des_pf*	     ppf;

	int num_tab = ceild(num_pagine, 1024);
	void *ind = sub(ind_virtuale, num_pagine * SIZE_PAGINA);
	for (int i = 0; i < num_tab; i++) {

		pdes_tab = &pdir->entrate[indice_direttorio(ind_virtuale)];
		pdes_tab->azzera  = 1;
		pdes_tab->US	  = 1;
		pdes_tab->RW	  = 1;
		pdes_tab->address = 0;
		pdes_tab->PCD	  = 0;
		pdes_tab->PWT     = 0;
		pdes_tab->D	  = 0;
		pdes_tab->A	  = 0;
		pdes_tab->global  = 0;
		pdes_tab->preload = 0;

		ind = add(ind, SIZE_PAGINA * 1024);
	}

	ind = sub(ind_virtuale, SIZE_PAGINA);
	pdes_tab = &pdir->entrate[indice_direttorio(ind)];
		
	ptab = alloca_tabella();
	if (ptab == 0) goto errore1;
	if (! carica_tabella(pdes_tab, ptab) ) 
		goto errore2;

	collega_tabella(pdir, ptab, indice_direttorio(ind));

	ind_fisico = alloca_pagina_virtuale();
	if (ind_fisico == 0) goto errore2;
	pdes_pag = &ptab->entrate[indice_tabella(ind)];
	if (! carica_pagina(pdes_pag, ind_fisico) )
		goto errore3;
	collega_pagina(ptab, ind_fisico, ind);

	pdes_pag->D		= 1; // verra' modificata
	pdes_pag->A		= 1; // e acceduta

	// restituiamo un puntatore alla pila creata
	return ind_fisico;

errore3:	rilascia(ind_fisico);
errore2:	rilascia(ptab);
errore1:	return 0;

}

pagina* crea_pila_sistema(direttorio* pdir, void* ind_virtuale)
{
	pagina* ind_fisico;
	tabella_pagine* ptab;
	descrittore_tabella* pdes_tab;
	descrittore_pagina*  pdes_pag;
	des_pf*	     ppf;

	void *ind = sub(ind_virtuale, SIZE_PAGINA);

	ptab = alloca_tabella_residente();
	if (ptab == 0)
		goto errore1;

	pdes_tab = &pdir->entrate[indice_direttorio(ind)];
	pdes_tab->address = uint(ptab) >> 12;
	pdes_tab->global  = 0;
	pdes_tab->US	  = 0; // livello sistema
	pdes_tab->RW	  = 1; // scrivibile
	pdes_tab->D	  = 0;
	pdes_tab->pgsz    = 0;
	pdes_tab->P	  = 1;

	ind_fisico = alloca_pagina_residente();
	if (ind_fisico == 0)
		goto errore2;

	pdes_pag = &ptab->entrate[indice_tabella(ind)];
	pdes_pag->address = uint(ind_fisico) >> 12;
	pdes_pag->pgsz	  = 0;
	pdes_pag->US	  = 0;
	pdes_pag->RW	  = 1;
	pdes_pag->P	  = 1;

	return ind_fisico;

errore2: 	rilascia(ptab);
errore1:	return 0;
}
	


// rilascia tutte le pagine fisiche (e le relative tabelle) mappate
// dall'indirizzo virtuale start (incluso) all'indirizzo virtuale end (escluso)
// (si suppone che start e end siano allineati alla pagina)
void rilascia_tutto(direttorio* pdir, void* start, void* end) {
	for (int i = indice_direttorio(start);
	         i < indice_direttorio(end); 
	         i++)
	{
		descrittore_tabella* pdes_tab = &pdir->entrate[i];
		if (pdes_tab->P == 1) {
			tabella_pagine* ptab = tabella_puntata(pdes_tab);
			for (int j = 0; j < 1024; j++) {
				descrittore_pagina* pdes_pag = &ptab->entrate[j];
				if (pdes_pag->P == 1)
					rilascia(pagina_puntata(pdes_pag));
			}
			rilascia(ptab);
		}
	}
}


proc_elem* crea_processo(void f(int), int a, int prio, char liv)
{
	proc_elem	*p;			// proc_elem per il nuovo processo
	int		identifier;		// indice del tss nella gdt 
	des_proc	*pdes_proc;		// descrittore di processo
	direttorio	*pdirettorio;		// direttorio del processo

	pagina		*pila_sistema,		// punt. di lavoro
			*pila_utente;		// punt. di lavoro
	
	p = new proc_elem;
        if (p == 0) goto errore1;

	identifier = alloca_tss();
	if (identifier == 0) goto errore2;

        p->identifier = identifier;
        p->priority = prio;

	pdes_proc = des_p(identifier);
	memset(pdes_proc, 0, sizeof(des_proc));

	// pagina fisica per il direttorio del processo
	pdirettorio = alloca_direttorio();
	if (pdirettorio == 0) goto errore3;

	// il direttorio viene inizialmente copiato dal direttorio principale
	// (in questo modo, il nuovo processo acquisisce automaticamente gli
	// spazi virtuali condivisi, sia a livello utente che a livello
	// sistema, tramite condivisione delle relative tabelle delle pagine)
	*pdirettorio = *direttorio_principale;
	
	pila_sistema = crea_pila_sistema(pdirettorio, fine_sistema_privato);
	if (pila_sistema == 0) goto errore4;

	if (liv == LIV_UTENTE) {
		pila_utente = crea_pila_utente(pdirettorio, fine_utente_privato, 64);
		if (pila_utente == 0) goto errore5;

		pila_sistema->parole_lunghe[1019] = uint((void*)f);	// EIP (codice utente)
		pila_sistema->parole_lunghe[1020] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pila_sistema->parole_lunghe[1021] = 0x00000200;	// EFLAG
		pila_sistema->parole_lunghe[1022] = uint(fine_utente_privato) - 2 * sizeof(int);
									// ESP (pila utente)
		pila_sistema->parole_lunghe[1023] = SEL_DATI_UTENTE;	// SS (pila utente)
		// eseguendo una IRET da questa situazione, il processo
		// passera' ad eseguire la prima istruzione della funzione f,
		// usando come pila la pila utente (al suo indirizzo virtuale)

		// dobbiamo ora fare in modo che la pila utente si trovi nella
		// situazione in cui si troverebbe dopo una CALL alla funzione
		// f, con parametro a:
		pila_utente->parole_lunghe[1022] = 0xffffffff;	// ind. di ritorno?
		pila_utente->parole_lunghe[1023] = a;		// parametro del proc.

		// infine, inizializziamo il descrittore di processo

		// indirizzo del bottom della pila sistema, che verra' usato
		// dal meccanismo delle interruzioni
		pdes_proc->esp0 = fine_sistema_privato;
		pdes_proc->ss0  = SEL_DATI_SISTEMA;

		// inizialmente, il processo si trova a livello sistema, come
		// se avesse eseguito una istruzione INT, con la pila sistema
		// che contiene le 5 parole lunghe preparate precedentemente
		pdes_proc->esp = uint(fine_sistema_privato) - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_UTENTE;
		pdes_proc->es  = SEL_DATI_UTENTE;

		pdes_proc->cr3 = pdirettorio;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->liv = LIV_UTENTE;
		// tutti gli altri campi valgono 0
	} else {
		pila_sistema->parole_lunghe[1019] = uint((void*)f);	  // EIP (codice sistema)
		pila_sistema->parole_lunghe[1020] = SEL_CODICE_SISTEMA;	  // CS (codice sistema)
		pila_sistema->parole_lunghe[1021] = 0x00000200;  	  // EFLAG
		pila_sistema->parole_lunghe[1022] = 0xffffffff;		  // indirizzo ritorno?
		pila_sistema->parole_lunghe[1023] = a;			  // parametro
		// i processi esterni lavorano esclusivamente a livello
		// sistema. Per questo motivo, prepariamo una sola pila (la
		// pila sistema)

		pdes_proc->esp = uint(fine_sistema_privato) - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_SISTEMA;
		pdes_proc->es  = SEL_DATI_SISTEMA;

		pdes_proc->cr3 = pdirettorio;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->liv = LIV_SISTEMA;
	}

	return p;

errore5:	rilascia_tutto(pdirettorio,
		  sub(fine_sistema_privato, SIZE_PAGINA),
		  fine_sistema_privato);
errore4:	rilascia(pdirettorio);
errore3:	rilascia_tss(identifier);
errore2:	delete p;
errore1:	return 0;
}
//////////////////////////////////////////////////////////////////////////////////
// PRIMITIVE                                                                    //
//////////////////////////////////////////////////////////////////////////////////

typedef void (*entry_t)(int);
entry_t io_entry = 0;
extern "C" void salta_a_main();
volatile short id_main = 0;

extern "C" void
c_activate_p(void f(int), int a, int prio, char liv, short &id, bool &risu)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	if (id_main != 0 && esecuzione->identifier != id_main)
		panic("activate_p non chiamata da main");

	p = crea_processo(f, a, prio, liv);

	if (p != 0) {
		inserimento_coda(pronti, p);
		processi++;
		id = p->identifier;
		risu = true;
	} else {
		risu = false;
	}
}

void debug_heap();
void shutdown() {
	asm("sti: 1: nop; jmp 1b" : : );
}

extern "C" void c_terminate_p()
{
	des_proc* pdes_proc = des_p(esecuzione->identifier);

	direttorio* pdirettorio = pdes_proc->cr3;
	rilascia_tutto(pdirettorio, inizio_sistema_privato, fine_sistema_privato);
	rilascia_tutto(pdirettorio, inizio_utente_privato,  fine_utente_privato);
	rilascia(pdirettorio);
	rilascia_tss(esecuzione->identifier);
	delete esecuzione;
	processi--;
	if (processi <= 0) {
		printk("Tutti i processi sono terminati\n");
		shutdown();
	}
	schedulatore();
}

void aggiungi_pe(proc_elem *p, estern_id interf);

extern "C" void c_activate_pe(void f(int), int a, int prio, char liv,
	short &identifier, estern_id interf, bool &risu)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	if (id_main != 0 && esecuzione->identifier != id_main)
		panic("activate_pe non chiamata da main");

	p = crea_processo(f, a, prio, liv);
	if (p != 0) {
		aggiungi_pe(p, interf); 
		identifier = p->identifier;
		risu = true;
	} else {
		risu = false;
	}
}

// Lo heap utente e' gestito tramite un allocatore a lista, come lo heap di
// sistema, ma le strutture dati non sono immerse nello heap stesso
// (diversamente dallo heap di sistema).  Infatti, la memoria allocata dallo
// heap utente e' virtuale e, se il sistema vi scrivesse le proprie strutture
// dati, potrebbe causare page fault. Per questo motivo, i descrittori di heap
// utente vengono allocati dallo heap di sistema.

// Lo heap utente e' diviso in "regioni" contigue, di dimensioni variabili, che
// possono essere libere o occupate.  Ogni descrittore di heap utente descrive
// una regione dello heap virtuale.  Descrizione dei campi:
// - start: indirizzo del primo byte della regione
// - dimensione: dimensione in byte della regione
// - occupato: indica se la regione e' occupata (1)
//             o libera (0)
// - next: puntatore al prossimo descrittore di heap utente
struct des_heap {
	void* start;	
	union {
		unsigned int dimensione;
		unsigned int occupato : 1;
	};
	des_heap* next;
};

// I descrittori vengono mantenuti in una lista ordinata tramite il campo
// start. Inoltre, devono essere sempre valide le identita' (se des->next !=
// 0): 
// (1) des->start + des->dimensione == des->next->start (per la contiguita'
// delle regioni)
// (2) (des->occupato == 0) => (des->next->occupato == 1) (ogni regione libera
// e' massima)
// (3) des->dimensione % sizeof(int) == 0 (per motivi di efficienza)

// se sizeof(int) >= 2, l'identita' (3) permette di utilizzare il bit meno
// significativo del campo dimensione per allocarvi il campo occupato (per noi,
// sizeof(int) = 4) (vedere "union" nella struttura)

// la testa della lista dei descrittori di heap e' allocata staticamente.
// Questo implica che la lista non e' mai vuota (contiene almeno 'heap'), ma
// non che lo heap utente non e' mai vuoto (heap->dimensione puo' essere 0)
des_heap heap;  // testa della lista di descrittori di heap

// accresci_heap tenta di aumentare le dimensioni dello heap utente di almeno
// "dim" byte.  "tail" deve puntare all'ultimo descrittore di heap utente nella
// lista
des_heap* accresci_heap(des_heap* tail, int dim) {
	
	void *new_heap_end;
	descrittore_tabella* pdes_tab;
	descrittore_pagina*  pdes_pag;
	tabella_pagine* ptab;
	void *curr_end;
	pagina *ind_fisico;
	des_heap* new_des = 0;

	// indirizzo virtuale dell'ultimo byte non appartenente allo heap
	void *heap_end = add(tail->start, tail->dimensione);

	
	// se la regione descritta da tail non e' occupata, dobbiamo accrescere
	// quella e non creare un nuovo descrittore (per rispettare (2))
	if (!tail->occupato)
		dim -= tail->dimensione;
	else {
		new_des = new des_heap;
		// se non riusciamo ad allocare il descrittore, non possiamo
		// proseguire
		if (new_des == 0) goto errore1;
	}
	
	// lo heap si accresce a multipli di pagina
	dim = allinea(dim, SIZE_PAGINA);

	// lo heap non puo' crescere oltre 'fine_utente_condiviso' (perche'
	// abbiamo preallocato le tabelle condivise solo fino a
	// quell'indirizzo)
	if (distance(fine_utente_condiviso, heap_end) < dim)
		goto errore2;

	new_heap_end = add(heap_end, dim);

	// Aggiorniamo la lista dello heap
	if (!tail->occupato) {
		tail->dimensione += dim;
		return tail;
	} else {
		new_des->start = heap_end;
		new_des->dimensione = dim;
		new_des->occupato = 0;
		new_des->next = 0;
		tail->next = new_des;
		return new_des;
	}

errore2:
	delete new_des;
errore1:
	return 0;
}

void debug_heap();

// cerca una regione di dimensione almeno pari a dim nello heap utente e ne
// resituisce l'indirizzo del primo byte (0 in caso di fallimento)
extern "C" void *c_mem_alloc(int dim)
{
	des_heap* nuovo;
	void* p;

	// imponiamo il rispetto di (3)
	dim = allinea(dim, sizeof(int));

	// cerchiamo la prima regione libera di dimensioni sufficienti
	// (strategia first-fit)
	des_heap *prec = 0, *scorri = &heap;
	while (scorri != 0 && (scorri->occupato || scorri->dimensione < dim)) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(prec != 0) // perche' la lista e' sicuramente non vuota

	if (scorri == 0 && 				// se non abbiamo trovato la regione
	    (scorri = accresci_heap(prec, dim)) == 0)	// e non riusciamo neanche a crearla
	    	return 0;				// l'allocazione fallisce
	// NOTA: la chiamata a accresci_heap rispetta sicuramente il vincolo
	// sul primo parametro, in quanto sappiamo che scorri == prec-> next ==
	// 0 (cioe', prec punta alla coda della lista)

	// assert(scorri != 0 && !scorri->occupato && scorri->dimensione >= dim)
		
	p = scorri->start;

	// se scorri->dimensione e' sufficientemente piu' grande di dim, e
	// riusciamo ad allocare un nuovo descrittore di heap, non occupiamo
	// tutta la regione, ma la dividiamo in due parti: una di dimensione
	// pari a dim, occupata, e il resto libero
	if (scorri->dimensione - dim >= sizeof(des_heap) &&
	    (nuovo = new des_heap) != 0)
	{
		// inseriamo nuovo nella lista, dopo scorri
		nuovo->start = add(scorri->start, dim); // rispettiamo (2)
		nuovo->dimensione = scorri->dimensione - dim;
		scorri->dimensione = dim;
		nuovo->next = scorri->next; 
		scorri->next = nuovo;
		nuovo->occupato = 0; 
	} 
	scorri->occupato = 1; // questo va fatto per ultimo, perche'
	                      // modifica scorri->dimensione 
	return p;
}

// libera la regione dello heap il cui indirizzo di partenza e' pv
// nota: se pv e' 0, o pv non e' l'indirizzo di partenza di nessuna regione,
// oppure se tale regione e' gia' libera, non si esegue alcuna azione
extern "C" void c_mem_free(void *pv)
{
	if (pv == 0) return;

	des_heap *prec = 0, *scorri = &heap;
	while (scorri != 0 && (!scorri->occupato || scorri->start != pv)) {
		prec = scorri;
		scorri = scorri->next;
	}
	if (scorri == 0) return;
	// assert(scorri != 0 && scorri->occupato && pv == scorri->start);
	scorri->occupato = 0;
	if (!prec->occupato) {
		// assert(prec->start + prec->dimensione == pv) // relazione (1)
		// dobbiamo riunire la regione con la precedente, per rispettare (2)
		prec->dimensione += scorri->dimensione;
		prec->next = scorri->next;
		delete scorri;
		scorri = prec;
	}
	des_heap *next = scorri->next;
	// qualunque ramo dell'if sia stato preso, possiamo affermare:
	// assert(next == 0 || scorri->start + scorri->dimensione == next->dimensione);
	if (next != 0 && !next->occupato) {
		// dobbiamo riunire anche la regione successiva
		scorri->dimensione += next->dimensione;
		scorri->next = next->next;
		delete next;
	}
}

void debug_heap() {
	des_heap* scorri = &heap;
	printk("--- HEAP UTENTE ---\n");
	while (scorri != 0) {
		printk("%x (%d) %s\n", scorri->start, scorri->dimensione & ~3,
				(scorri->occupato ? "*" : ""));
		scorri = scorri->next;
	}
	printk("----------------------\n");
}

extern "C" void c_delay(int n)
{
	richiesta *p;

	p = new richiesta;
	p->d_attesa = n;
	p->pp = esecuzione;

	inserimento_coda_timer(p);
	schedulatore();
}



extern "C" void c_begin_p()
{
	//attiva_timer(DELAY);
	//io_entry(0);
        inserimento_coda(pronti, esecuzione);
        schedulatore();
}

// nel testo non si controlla la locazione di lav
//
extern "C" void c_give_num(int &lav)
{
	if(!verifica_area(&lav, sizeof(int), true))
		abort_p();

        lav = processi - 1;
}

unsigned int sem_buf[MAX_SEMAFORI / sizeof(int) + 1];
bm_t sem_bm = { sem_buf, MAX_SEMAFORI };

extern "C" void c_sem_ini(int &index_des_s, int val, bool &risu)
{
	unsigned int pos;

	if (id_main != 0 && esecuzione->identifier != id_main)
		panic("sem_ini non chiamata da main");

	if(!bm_alloc(&sem_bm, pos)) {
		risu = false;
		return;
	}

	index_des_s = pos;
	array_dess[index_des_s].counter = val;
	risu = true;
}

// nel testo non si controlla il parametro sem (se > MAX_SEM crea problemi)
// NOTA: la terminazione del processo puo' sembrare brutale, ma non c'e'
//  mezzo di segnalare l' errore.
//
extern "C" void c_sem_wait(int sem)
{
	des_sem *s;

	if(sem < 0 || sem >= MAX_SEMAFORI) {
		printk("semaforo errato %d\n", sem);
		abort_p();
	}

	s = &array_dess[sem];
	s->counter--;

	if(s->counter < 0) {
		inserimento_coda(s->pointer, esecuzione);
		schedulatore();
	}
}

// vedi sopra
//
extern "C" void c_sem_signal(int sem)
{
	des_sem *s;
	proc_elem *lavoro;

	if(sem < 0 || sem >= MAX_SEMAFORI) {
		printk("semaforo errato %d\n", sem);
		abort_p();
	}

	s = &array_dess[sem];
	s->counter++;

	if(s->counter <= 0) {
		rimozione_coda(s->pointer, lavoro);
		if(lavoro->priority <= esecuzione->priority)
			inserimento_coda(pronti, lavoro);
		else { // preemption
			inserimento_coda(pronti, esecuzione);
			esecuzione = lavoro;
		}
	}
}
extern void* fine_codice_sistema;

extern "C" void c_page_fault(void* indirizzo_virtuale, page_fault_error errore, void* eip)
{
#if 0
	printk("eip: %x, page fault a %x:", eip, indirizzo_virtuale);
	printk("%s, %s, %s, %s\n",
		errore.prot  ? "protezione"	: "pag/tab assente",
		errore.write ? "scrittura"	: "lettura",
		errore.user  ? "da utente"	: "da sistema",
		errore.res   ? "bit riservato"	: "");
#endif
	if (eip < fine_codice_sistema)
		panic("page fault dal modulo sistema");
	if (errore.res == 1)
		panic("descrittore scorretto");

	if (errore.prot == 1) {
		printk("Errore di protezione, il processo verra' terminato\n");
		terminate_p();
	}

	trasferimento(indirizzo_virtuale, errore);
}

/////////////////////////////////////////////////////////////////////////
// CARICAMENTO DEI MODULI                                              //
// //////////////////////////////////////////////////////////////////////


Elf32_Ehdr* elf32_intestazione(void* start) {

	Elf32_Ehdr* elf_h = static_cast<Elf32_Ehdr*>(start);

	// i primi 4 byte devono contenere un valore prestabilito
	if (!(elf_h->e_ident[EI_MAG0] == ELFMAG0 &&
	      elf_h->e_ident[EI_MAG1] == ELFMAG1 &&
	      elf_h->e_ident[EI_MAG2] == ELFMAG2 &&
	      elf_h->e_ident[EI_MAG2] == ELFMAG2))
	{
		printk("    Formato del modulo non riconosciuto\n");
		return 0;
	}

	if (!(elf_h->e_ident[EI_CLASS] == ELFCLASS32  &&  // 32 bit
	      elf_h->e_ident[EI_DATA]  == ELFDATA2LSB &&  // little endian
	      elf_h->e_type	       == ET_EXEC     &&  // eseguibile
	      elf_h->e_machine 	       == EM_386))	  // per Intel x86
	{ 
		printk("    Il modulo non contiene un esegubile per Intel x86\n");
		return 0;
	}

	return elf_h;
}

// copia le sezioni (.text, .data) del modulo descritto da *mod agli indirizzi
// fisici di collegamento (il modulo deve essere in formato ELF32) restituisce
// l'indirizzo dell'entry point
void* carica_modulo_io(module_t* mod)
{
	Elf32_Phdr* elf_ph;

	// leggiamo l'intestazione del file
	Elf32_Ehdr* elf_h = elf32_intestazione(mod->mod_start);
	if (elf_h == 0) return 0;


	// dall'intestazione, calcoliamo l'inizio della tabella dei segmenti di programma
	void* header_start = add(mod->mod_start, elf_h->e_phoff);
	for (int i = 0; i < elf_h->e_phnum; i++) {
		elf_ph = static_cast<Elf32_Phdr*>(add(header_start, elf_h->e_phentsize * i));
		
		// ci interessano solo i segmenti di tipo PT_LOAD
		// (corrispondenti alle sezioni .text e .data)
		if (elf_ph->p_type != PT_LOAD)
			continue;

		// ogni entrata della tabella specifica l'indirizzo a cui va
		// caricato il segmento...
		if (salta_a(elf_ph->p_vaddr) < 0) {
			printk("    Indirizzo richiesto da '%s' gia' occupato\n", mod->string);
			return 0;
		}

		// ... e lo spazio che deve occupare in memoria
		if (occupa(elf_ph->p_memsz) == 0) {
			printk("    Memoria insufficiente per '%s'\n", mod->string);
			return 0;
		}

		// ora possiamo copiare il contenuto del segmento di programma
		// all'indirizzo di memoria precedentemente individuato;
		// L'entrata corrente della tabella ci dice a che offset
		// (dall'inizio del modulo) si trova il segmento
		memcpy(elf_ph->p_vaddr,				// destinazione
		       add(mod->mod_start, elf_ph->p_offset),	// sorgente
		       elf_ph->p_filesz);			// quanti byte copiare
		printk("    Copiata sezione di %d byte all'indirizzo %x\n",
				elf_ph->p_filesz, elf_ph->p_vaddr);


		// la dimensione del segmento nel modulo (p_filesz) puo' essere
		// diversa (piu' piccola) della dimensione che il segmento deve
		// avere in memoria (p_memsz).  Cio' accade perche' i dati
		// globali che devono essere inizializzati con 0 non vengono
		// memorizzati nel modulo. Di questi dati, il formato ELF32 (ma
		// anche altri formati) specifica solo la dimensione
		// complessiva.  L'inizializzazione a 0 deve essere effettuata
		// a tempo di caricamento
		if (elf_ph->p_memsz > elf_ph->p_filesz) {
			memset(add(elf_ph->p_vaddr, elf_ph->p_filesz),  // indirizzo di partenza
			       0,				        // valore da scrivere
			       elf_ph->p_memsz - elf_ph->p_filesz);	// per quanti byte
			printk("    azzerati ulteriori %d byte\n",
					elf_ph->p_memsz - elf_ph->p_filesz);
		}
	}
	// una volta copiati i segmenti di programma all'indirizzo per cui
	// erano stati collegati, il modulo non ci serve piu'
	free_interna(mod->mod_start, distance(mod->mod_end, mod->mod_start));
	return elf_h->e_entry;
}


///////////////////////////////////////////////////////////////////////////////////
// INIZIALIZZAZIONE                                                              //
///////////////////////////////////////////////////////////////////////////////////
//

extern "C" void salta_a_main();

struct superblock_t {
	char		bootstrap[512];
	unsigned int	bm_start;
	int		blocks;
	unsigned int	directory;
	void		(*entry_point)(int);
	void*		end;
};

char read_buf[SIZE_PAGINA * 8];

void dummy(int a) {
	for(;;);
}


void leggi_swap(void* buf, unsigned int block, unsigned int bytes, const char* msg) {
	unsigned char totale, da_leggere;
	char errore;
	
	totale = da_leggere = ceild(bytes, 512);
	printk("Leggo: %s (%d settori, primo: %d)\n", msg, totale, block * 8);
	readhd_n(0, 1, buf, block * 8, da_leggere, errore);
	if (da_leggere != 0 || errore != 0) { 
		printk("letti %d settori su %d, errore = %d\n", totale - da_leggere, totale, errore);
		printk("Impossibile leggere %s\n", msg);
		panic("Fatal error");
	}
}


void main_proc(int a) {

	attiva_timer(DELAY);
	io_entry(0);

	leggi_swap(read_buf, 0, sizeof(superblock_t), "il superblocco");
	superblock_t *s = reinterpret_cast<superblock_t*>(read_buf);
	printk("    bm_start   : %d\n", s->bm_start);
	printk("    blocks     : %d\n", s->blocks);
	printk("    directory  : %d\n", s->directory);
	printk("    entry point: %x\n", s->entry_point);
	printk("    end        : %x\n", s->end);
	unsigned int pages = ceild(s->blocks, SIZE_PAGINA * 8);
	unsigned int *buf = new unsigned int[(pages * SIZE_PAGINA) / sizeof(unsigned int)];
	if (buf == 0) 
		panic("Impossibile allocare la bitmap dei blocchi");
	bm_create(&block_bm, buf, s->blocks);
	leggi_swap(buf, s->bm_start, pages * SIZE_PAGINA, "la bitmap dei blocchi");
	direttorio* tmp = new direttorio;
	if (tmp == 0)
		panic("memoria insufficiente");
	leggi_swap(tmp, s->directory, sizeof(direttorio), "il direttorio principale");
	// tabelle condivise per lo spazio utente condiviso
	printk("Creo o leggo le tabelle condivise\n");
	void* last_address;
	for (void* ind = inizio_utente_condiviso;
	     	   ind < fine_utente_condiviso;
	           ind = add(ind, SIZE_PAGINA * 1024))
	{
		descrittore_tabella
			*pdes_tab1 = &tmp->entrate[indice_direttorio(ind)],
			*pdes_tab2 = &direttorio_principale->entrate[indice_direttorio(ind)];
		tabella_pagine* ptab;
		
		*pdes_tab2 = *pdes_tab1;

		if (pdes_tab1->azzera == 0 && pdes_tab1->address == 0)
			continue;

		last_address = add(ind, SIZE_PAGINA * 1024);

		ptab = alloca_tabella_condivisa();
		if (ptab == 0)
			panic("Impossibile allocare tabella condivisa\n");
		if (! carica_tabella(pdes_tab2, ptab) )
			panic("Impossibile caricare tabella condivisa");
		pdes_tab2->address	= uint(ptab) >> 12;
		pdes_tab2->P		= 1;
		for (int i = 0; i < 1024; i++) {
			descrittore_pagina* pdes_pag = &ptab->entrate[i];
			if (pdes_pag->preload == 1) {
				pagina* pag = alloca_pagina_residente();
				if (pag == 0) 
					panic("Impossibile allocare pagina residente");
				if (! carica_pagina(pdes_pag, pag) ) 
					panic("Impossibile caricare pagina residente");
				collega_pagina(ptab, pag, add(ind, i * SIZE_PAGINA));
			}
		}

	}
	heap.start = allinea(s->end, sizeof(int));
	heap.dimensione = distance(last_address, heap.start);
	printk("Heap utente a %x, dimensione: %d B (%d MiB)\n",
			heap.start, heap.dimensione, heap.dimensione / (1024 * 1024));
	delete tmp;
	proc_elem* m = crea_processo(s->entry_point, 0, 0, LIV_UTENTE);
	if (m == 0)
		panic("Impossibile creare il processo main");
	processi++;
	inserimento_coda(pronti, m);
	id_main = m->identifier;
	terminate_p();
}

	

extern "C" void
cmain (unsigned long magic, multiboot_info_t* mbi)
{
	entry_t user_entry;
	module_t* user_mod = 0;
	des_proc* pdes_proc;

	// inizializziamo per prima cosa la console, in modo da poter scrivere
	// messaggi sullo schermo
	con_init();

	// controlliamo di essere stati caricati
	// da un bootloader che rispetti lo standard multiboot
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printk ("Numero magico non valido: 0x%x\n", magic);
		return;
	}

	// vediamo se il boot loader ci ha passato l'informazione su quanta
	// memoria fisica e' installata nel sistema
	if (mbi->flags & 1) {
		max_mem_lower = addr(mbi->mem_lower * 1024);
		max_mem_upper = addr(mbi->mem_upper * 1024 + 0x100000);
	} else {
		printk ("Quantita' di memoria sconosciuta, assumo 32MB\n");
		max_mem_lower = addr(639 * 1024);
		max_mem_upper = addr(32 * 1024 * 1024);
	}
	printk("Memoria fisica: %d byte (%d MB)\n", max_mem_upper, uint(max_mem_upper) >> 20 );
	
	// per come abbiamo organizzato il sistema non possiamo gestire piu' di
	// 1GB di memoria fisica
	if (max_mem_upper > fine_sistema_privato) {
		max_mem_upper = fine_sistema_privato;
		printk("verranno gestiti solo %d byte di memoria fisica\n", max_mem_upper);
	}

	// controlliamo se il boot loader ha caricato dei moduli
	if (mbi->flags & (1 << 3)) {
		// se si', calcoliamo prima lo spazio occupato
		printk ("Numero moduli: %d\n", mbi->mods_count);
		module_t* mod = mbi->mods_addr;
		for (int i = 0; i < mbi->mods_count; i++) {
			printk ("Modulo #%d (%s): inizia a %x, finisce a %x\n",
					i, mod->string, mod->mod_start, mod->mod_end);
			if (salta_a(mod->mod_start) < 0 ||
			    occupa(distance(mod->mod_end, mod->mod_start)) == 0) {
				panic("Errore nel caricamento (lo spazio risulta occupato)");
			}
			mod++;
		}

		// quindi, ne interpretiamo il contenuto
		mod = mbi->mods_addr;
		for (int i = 0; i < mbi->mods_count; i++) {
			if (str_equal(mod->string, "/io")) {
				printk("Sposto il modulo IO all'indirizzo di collegamento:\n");
				io_entry = (entry_t)carica_modulo_io(mod);
				if (io_entry == 0)
					panic("Impossibile caricare il modulo di IO");
			} else {
				free_interna(mod->mod_start, distance(mod->mod_end, mod->mod_start));
				printk("Modulo '%s' non riconosciuto (eliminato)");
			}
			mod++;
		}

	}


	// il resto della memoria e' per le pagine fisiche
	init_pagine_fisiche();

	// inizializziamo la mappa di bit che serve a tenere traccia dei
	// semafori allocati
	bm_create(&sem_bm, sem_buf, MAX_SEMAFORI);

	// il direttorio principale viene utilizzato fino a quando non creiamo
	// il primo processo.  Poi, servira' come "modello" da cui creare i
	// direttori dei nuovi processi.
	direttorio_principale = alloca_direttorio();
	if (direttorio_principale == 0)
		panic("Impossibile allocare il direttorio principale");
	memset(direttorio_principale, 0, SIZE_PAGINA);

	// memoria fisica in memoria virtuale
	for (void* ind = addr(0); ind < max_mem_upper; ind = add(ind, SIZE_PAGINA)) {
		descrittore_tabella* pdes_tab =
			&direttorio_principale->entrate[indice_direttorio(ind)];
		tabella_pagine* ptab;
		if (pdes_tab->P == 0) {
			ptab = alloca_tabella_residente();
			if (ptab == 0) panic("Impossibile allocare le tabelle condivise");
			pdes_tab->address   = uint(ptab) >> 12;
			pdes_tab->D	    = 0;
			pdes_tab->pgsz      = 0; // pagine di 4K
			pdes_tab->D	    = 0;
			pdes_tab->US	    = 0; // livello sistema;
			pdes_tab->RW	    = 1; // scrivibile
			pdes_tab->P 	    = 1; // presente
		} else {
			ptab = tabella_puntata(pdes_tab);
		}
		descrittore_pagina* pdes_pag = &ptab->entrate[indice_tabella(ind)];
		pdes_pag->address = uint(ind) >> 12;   // indirizzo virtuale == indirizzo fisico
		pdes_tab->pgsz    = 0;
		pdes_pag->global  = 1;
		pdes_pag->US      = 0; // livello sistema;
		pdes_pag->RW	  = 1; // scrivibile
		pdes_pag->PCD     = 0;
		pdes_pag->PWT     = 0;
		pdes_pag->P       = 1;
	}

	carica_cr3(direttorio_principale);
	// avendo predisposto il direttorio in modo che tutta la memoria fisica
	// si trovi gli stessi indirizzi in memoria virtuale, possiamo attivare
	// la paginazione, sicuri che avremo continuita' di indirizzamento
	attiva_paginazione();

	// quando abbiamo finito di usare la struttura dati passataci dal boot
	// loader, possiamo assegnare allo heap la memoria fisica di indirizzo
	// < 1MB. Lasciamo inutilizzata la prima pagina, in modo che
	// l'indirizzo 0 non venga mai allocato e possa essere utilizzato per
	// specificare un puntatore non valido
	free_interna(addr(SIZE_PAGINA), distance(max_mem_lower, addr(SIZE_PAGINA)));

	esecuzione = crea_processo(dummy, 0, -1, LIV_SISTEMA);
	inserimento_coda(pronti, esecuzione);
	esecuzione = crea_processo(main_proc, 0, 2, LIV_SISTEMA);
	bool risu;
	c_sem_ini(pf_mutex, 1, risu);
	if (!risu)
		panic("Impossibile allocare il semaforo per i page fault");
	processi = 2;
	init_8259();
	salta_a_main();
}

extern "C" void gestore_eccezioni(int tipo, unsigned errore,
		unsigned eip, unsigned cs, short eflag)
{
	printk("Eccezione %d, errore %x\n", tipo, errore);
	printk("eflag = %x, eip = %x, cs = %x\n", eflag, eip, cs);
	abort_p();
}
