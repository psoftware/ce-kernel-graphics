// sistema.cpp
//

//////////////////////////////////////////////////////////////////////////////
//                             STRUTTURE DATI                               //
//////////////////////////////////////////////////////////////////////////////

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

// priorita' di base per i processi esterni. i processi interni devono
//  avere priorita' minore
// Usato in io.cpp
//
const int PRIO_ESTERN_BASE = 0x400;

// livelli di privilegio (valori Intel)
//
const char LIV_SISTEMA = 0, LIV_UTENTE = 3;

// processo in esecuzione
//
extern proc_elem *esecuzione;

// coda dei processi pronti
//
extern proc_elem *pronti;

// numero di semafori disponibili
// Usato in sistema.s
//
const int MAX_SEM = 64;

// vettore dei descrittori di semaforo
//
extern des_sem array_dess[MAX_SEM];

// manipolazione delle code di processi
//
extern void inserimento_coda(proc_elem *&p_coda, proc_elem *p_elem);
extern void rimozione_coda(proc_elem *&p_coda, proc_elem *&p_elem);

// schedulatore
//
extern "C" void schedulatore(void);

// stampa msg a schermo e blocca il sistema
//
extern "C" void panic(const char *msg);

// stampa a schermo formattata
//
extern "C" int printk(const char *fmt, ...);

// verifica i diritti del processo in esecuzione sull' area di memoria
// specificata dai parametri
//
extern "C" bool verifica_area(void *area, unsigned int dim, bool write);

// priorita' del processo creato da begin_p (deve essere la piu' bassa)
//
const int PRIO_MAIN = 0;

// priorita' del processo dummy (deve essere la stessa usata in utente.cpp)
// Usata da utente.cpp
//
const int PRIO_DUMMY = 1;

// identificatore del processo creato da begin_p
//
const int ID_MAIN = 0x28;

// tipo per il gestore dell' allocazione di memoria
//
struct zone_t;

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
struct des_proc {
//	short id;
	unsigned int link;
//	int punt_nucleo;
	unsigned int esp0, ss0;
	unsigned int esp1, ss1;
	unsigned int esp2, ss2;

	unsigned int cr3;

	unsigned int eip;
	unsigned int eflags;

//	int contesto[N_REG];
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;

	unsigned int es;
	unsigned int cs;
	unsigned int ss;
	unsigned int ds;
	unsigned int fs;
	unsigned int gs;
	unsigned int ldt;
//	char cpl;

	unsigned int io_map;

	zone_t *vzone;
	unsigned long vend;

	struct {
		unsigned int cr;
		unsigned int sr;
		unsigned int tr;
		unsigned int ip;
		unsigned int is;
		unsigned int op;
		unsigned int os;
		char regs[80];
	} fpu;

	char liv;
};

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

////////////////////////////////////////////////////////////////////////////////
//                         FUNZIONI AD USO INTERNO                            //
////////////////////////////////////////////////////////////////////////////////

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
	proc_elem *pp;

	rimozione_coda(pronti, pp);
	esecuzione = pp;
}

////////////////////////////////////////////////////////////////////////////////
//      Dichiarazioni di tipi e funzioni usati dalle chiamate di sistema      //
////////////////////////////////////////////////////////////////////////////////

// Tipi e funzioni di libreria
//
typedef unsigned int size_t;		// tipo usato per le dimensioni

// imposta n bytes a c a partire dall' indirizzo dest
//
void *memset(void *dest, int c, size_t n);

// Allocatori di memoria
//

// allocatore generico
//
struct block_t;
typedef block_t pool_t;

void *pool_alloc(pool_t *pool, size_t size);
void pool_free(pool_t *pool, void *addr);

// allocatore a mappa di bit
//
struct bm_t;

bool bm_alloc(bm_t *bm, unsigned int *pos, int size = 1);
void bm_free(bm_t *bm, unsigned int pos, int size = 1);

// zone allocator (per gli heap utente)
//
zone_t *zone_create(void *start, void *end);
bool zone_grow(zone_t *zone, void *newend);
void *zone_alloc(zone_t *zone, size_t size);
void zone_free(zone_t *zone, void *addr);
void zone_destroy(zone_t *zone);

// Funzioni di supporto
//

// abort_p() serve per terminare un processo se richiede operazioni
//  non corrette o avviene un  errore che non puo' essere notificato
//  tramite parametri di ritorno
//
extern "C" void abort_p();

// ritorna il descrittore del processo in esecuzione
//
extern "C" des_proc *cur_des(void);

// ritorna il descrittore del processo id
//
extern "C" des_proc *des_p(short id);

// alloca tutte le risorse necessarie ad un nuovo processo
//
bool crea_proc(void f(int), int a, char liv, short &id);

// rilascia le risorse usate dal processo riferito da p (incluso il
//  suo proc_elem)
//
void canc_proc(proc_elem *p);

// cambia le dimensioni (se possibile) dell' area di memoria virtuale allocata
//  da mem_alloc e mem_free (inizialmente nulla)
//
bool mod_vzone(des_proc *p_des, int dim, bool crea);

// ritorna true se pv e' un indirizzo nello heap del processo
//
inline bool heap_addr(void *pv);

////////////////////////////////////////////////////////////////////////////////
// Oggetti usati dalle chiamate di sistema
//

// pool della memoria del kernel
//
pool_t *kernel_pool;

// bitmap dei descrittori di semaforo
//
bm_t *sem_bm;

////////////////////////////////////////////////////////////////////////////////
//                            CHIAMATE DI SISTEMA                             //
////////////////////////////////////////////////////////////////////////////////

extern "C" void c_sem_ini(int &index_des_s, int val, bool &risu)
{
	unsigned int pos;

	// se risu non e' scrivibile non c'e' modo di segnalare l' errore
	if(!verifica_area(&risu, sizeof(bool), true))
		abort_p();

	if(!verifica_area(&index_des_s, sizeof(int), true)) {
		risu = false;
		return;
	}

	if(!bm_alloc(sem_bm, &pos)) {
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

	if(sem < 0 || sem >= MAX_SEM)
		abort_p();

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

	if(sem < 0 || sem >= MAX_SEM)
		abort_p();

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

extern "C" void *c_mem_alloc(int dim)
{
	des_proc *p_des = cur_des();
	void *rv;

	if(!p_des->vzone)		// prima allocazione
		if(!mod_vzone(p_des, dim, true))
			return 0;

	rv = zone_alloc(p_des->vzone, dim);
	if(rv == 0) {
		if(!mod_vzone(p_des, dim, false))
			return 0;

		rv = zone_alloc(p_des->vzone, dim);
	}

	return rv;
}

extern "C" void c_mem_free(void *pv)
{
	des_proc *p_des = cur_des();

	// la terminazione sarebbe stata piu' coerente, ma l' errore e'
	//  molto comune. Non e' nemmeno l' unica condizione di errore
	//  possibile: l' indirizzo puo' non essere stato allocato con
	//  mem_alloc. In questo caso il processo si trova uno heap
	//  in stato inconsistente dopo l' esecuzione di pool_free, ma
	//  non ci sono conseguenze per gli altri processi o per il
	//  sistema.
	//
	if(!heap_addr(pv))
		return;

	zone_free(p_des->vzone, pv);
}

void inserimento_coda_timer(richiesta *p);

extern "C" void c_delay(int n)
{
	richiesta *p;

	p = new richiesta;
	p->d_attesa = n;
	p->pp = esecuzione;

	inserimento_coda_timer(p);
	schedulatore();
}

extern "C" void c_activate_p(void f(int), int a, int prio, char liv,
       short &id, bool &risu)
{
	proc_elem *p;

	if(!verifica_area(&risu, sizeof(bool), true))
		abort_p();

	// se prio < PRIO_DUMMY processi non torna mai ad 1 e dummy impedisce
	//  la chiusura del sistema
	if(!verifica_area(&id, sizeof(short), true) ||
			prio < PRIO_DUMMY || prio >= PRIO_ESTERN_BASE ||
			!(liv == LIV_SISTEMA || liv == LIV_UTENTE)) {
		risu = false;
		return;
	}

        p = new proc_elem;
        if(!p) {
                 risu = false;
                 return;
        }

        if((risu = crea_proc(f, a, liv, id)) == false) {
        	delete p;
        	return;
        }

        p->identifier = id;
        p->priority = prio;

        inserimento_coda(pronti, p);

        ++processi;
        // risu e' true dopo la chiamata a crea_proc
}

extern "C" void c_terminate_p()
{
	// quando main chiama terminate_p tutti gli altri processi sono
	//  terminati, invece di riavviare direttamente si entra nello stato
	//  di HALT, accettando eventuali interruzioni (per esempio per
	//  riavviare il sistema con CTRL-ALT-CANC)
	//
	if(esecuzione->identifier == ID_MAIN) {
		printk("\nTutti i processi sono terminati.\n");
		printk("E' possibile spegnere il calcolatore o riavviarlo con CTRL-ALT-CANC.\n");
		asm("sti;1: hlt; jmp 1b");	// fine dell' elaborazione
	}

	canc_proc(esecuzione);

	--processi;
	schedulatore();
}

extern "C" void c_begin_p()
{
        inserimento_coda(pronti, esecuzione);
        schedulatore();
}

// nel testo non si controlla la locazione di lav
//
extern "C" void c_give_num(int &lav)
{
	if(!verifica_area(&lav, sizeof(int), true))
		abort_p();

        lav = processi;
}

// Valori usati in io.cpp
//
enum estern_id {
	tastiera,
	com1_in,
	com1_out,
	com2_in,
	com2_out
};

void aggiungi_pe(proc_elem *p, estern_id interf);

extern "C" void c_activate_pe(void f(int), int a, int prio, char liv,
	short &identifier, estern_id interf, bool &risu)
{
	proc_elem *pe;

	// nessuna verifica sui parametri, activate_pe viene chiamata
	//  sicuramente da livello sistema

        pe = new proc_elem;
        if(!pe) {
                 risu = false;
                 return;
        }

        if((risu = crea_proc(f, a, liv, identifier)) == false) {
        	delete pe;
        	return;
        }

        pe->identifier = identifier;
        pe->priority = prio;

	aggiungi_pe(pe, interf); 
        // non si incrementa processi perche' i processi esterni non terminano
}

//////////////////////////////////////////////////////////////////////////////
//                         FUNZIONI AD USO INTERNO                          //
//////////////////////////////////////////////////////////////////////////////

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

	schedulatore();
}

////////////////////////////////////////////////////////////////////////////////
//        ALLOCAZIONE E DEALLOCAZIONE DELLA MEMORIA DINAMICA DEL NUCLEO       //
////////////////////////////////////////////////////////////////////////////////

void *operator new(unsigned int size)
{
	return pool_alloc(kernel_pool, size);
}

void *operator new[](unsigned int size)
{
        return pool_alloc(kernel_pool, size);
}

void operator delete(void *ptr)
{
        pool_free(kernel_pool, ptr);
}

void operator delete[](void *ptr)
{
        pool_free(kernel_pool, ptr);
}

////////////////////////////////////////////////////////////////////////////////
//                             FUNZIONI DI SUPPORTO                           //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Allocatore di memoria first-fit
//

// un blocco e' una zona di memoria
//
struct block_t {
	block_t *prev;
	block_t *next;
	unsigned int size;
	unsigned int flags;
};

const unsigned int BLOCK_FREEMASK = 0x00000001;

inline block_t *BLOCK_HEADER(void *addr)
{
	return (block_t *)(addr) - 1;
}

inline void *BLOCK_STARTADDR(block_t *b)
{
	return (void *)(b + 1);
}

inline bool BLOCK_ISFREE(block_t *b)
{
	return (b->flags&BLOCK_FREEMASK) == 1;
}

inline void BLOCK_SETFREE(block_t *b)
{
	b->flags |= BLOCK_FREEMASK;
}

inline void BLOCK_CLEARFREE(block_t *b)
{
	b->flags &= ~BLOCK_FREEMASK;
}

const unsigned int POOL_MIN_SIZE = 4096;

inline const unsigned int POOL_MIN_FREE_SPACE(void)
{
	return 4 * sizeof(block_t);
}

// crea un pool contenente la zona di memoria inclusa tra gli
//  estremi specificati
//
pool_t *pool_create(void *start, void *end)
{
	pool_t *pool;
	block_t *first;

	if((char *)end - (char *)start < POOL_MIN_SIZE)
		return 0;

	pool = (pool_t *)start;
	
	first = pool;
	first->prev = first->next = 0;
	first->size = (char *)end - (char *)BLOCK_STARTADDR(first);
	BLOCK_SETFREE(first);

	return pool;
}

// sposta la fine del pool
//
bool pool_grow(pool_t *pool, void *newend)
{
	block_t *block, *newblock;

	block = pool;
	while(block->next)
		block = block->next;

	if(BLOCK_ISFREE(block)) {
		block->size = (char *)newend - (char *)BLOCK_STARTADDR(block);
	} else {
		newblock = (block_t *)((char *)BLOCK_STARTADDR(block) +
			block->size);

		block->next = newblock;

		newblock->prev = block;
		newblock->next = 0;
		newblock->size = (char *)newend -
			(char *)BLOCK_STARTADDR(newblock);
		BLOCK_SETFREE(newblock);
	}

	return true;
}

// alloca SIZE bytes in POOL
//
void *pool_alloc(pool_t *pool, size_t size)
{
	block_t *cur_b, *new_b;
	unsigned int free_space;

	size = size & 3 ? (size & ~3) + 4: size;

	cur_b = pool;
	while(cur_b && (cur_b->size < size || !BLOCK_ISFREE(cur_b)))
		cur_b = cur_b->next;

	if(!cur_b)
		return 0;

	free_space = cur_b->size - size;
	if(free_space > POOL_MIN_FREE_SPACE()) {
		free_space -= sizeof(block_t);
		cur_b->size = size;
		new_b = (block_t *)((char *)BLOCK_STARTADDR(cur_b) + size);
		new_b->size = free_space;

		BLOCK_SETFREE(new_b);
		new_b->next = cur_b->next;
		cur_b->next = new_b;
		new_b->prev = cur_b;

		if(new_b->next)
			new_b->next->prev = new_b;
	}

	BLOCK_CLEARFREE(cur_b);

	return BLOCK_STARTADDR(cur_b);
}

// rilascia ADDR
//
void pool_free(pool_t *pool, void *addr)
{
	block_t *cur_b;

	if(!addr)
		return;

	cur_b = BLOCK_HEADER(addr);
	BLOCK_SETFREE(cur_b);
	if(cur_b->prev && BLOCK_ISFREE(cur_b->prev)) {
		cur_b->prev->size += cur_b->size + sizeof(block_t);
		cur_b->prev->next = cur_b->next;
		cur_b = cur_b->prev;

		if(cur_b->next)
			cur_b->next->prev = cur_b;
	}

	if(cur_b->next && BLOCK_ISFREE(cur_b->next)) {
		cur_b->size += cur_b->next->size + sizeof(block_t);
		cur_b->next = cur_b->next->next;

		if(cur_b->next)
			cur_b->next->prev = cur_b;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Allocatore a mappa di bit
//

struct bm_t {
	unsigned int *vect;
	unsigned int size;
};

inline unsigned int VALUE(bm_t *bm, unsigned int pos)
{
	return bm->vect[pos / 32] & (1UL << (pos % 32));
}

inline void SET(bm_t *bm, unsigned int pos)
{
	bm->vect[pos / 32] |= (1UL << (pos % 32));
}

inline void CLEAR(bm_t *bm, unsigned int pos)
{
	bm->vect[pos / 32] &= ~(1UL << (pos % 32));
}

// dimensione del vettore di interi che contiene la mappa di bit
//
inline const unsigned int BM_BUFSIZE(unsigned int bits)
{
	return (bits >> 2) + ((bits & 0x03) != 0);
}

// crea la mappa BM, usando BUFFER come vettore; SIZE e' il numero di bit
//  nella mappa
//
void bm_create(bm_t *bm, unsigned int *buffer, unsigned int size)
{
	int v_size = BM_BUFSIZE(size), i;

	bm->vect = buffer;
	bm->size = size;

	for(i = 0; i < v_size; ++i)
		bm->vect[i] = 0;
}

// mettono a zero/uno le posizioni della mappa
//
void bm_set(bm_t *bm, unsigned int start, unsigned int end)
{
	int i;

	for(i = start; i < end && i < bm->size; ++i)
		SET(bm, i);
}

void bm_clear(bm_t *bm, unsigned int start, unsigned int end)
{
	int i;

	for(i = start; i < end && i < bm->size; ++i)
		CLEAR(bm, i);
}

bool bm_alloc(bm_t *bm, unsigned int *pos, int size)
{
	int i, l;

	for(i = 0; i <= bm->size - size; ++i) {
		if(VALUE(bm, i))
			continue;

		for(l = 0; !VALUE(bm, i + l) && l < size; ++l)
			;

		if(l == size) {
			*pos = i;
			for(l = 0; l < size; ++l)
				SET(bm, i + l);

			return true;
		}
	}

	return false;
}

void bm_free(bm_t *bm, unsigned int pos, int size)
{
	for(int i = 0; i < size && pos + i < bm->size; ++i)
		CLEAR(bm, pos + i);
}

// memcpy
//
void *memcpy(void *dest, const void *src, unsigned int n)
{
	char *dest_ptr = (char *)dest, *src_ptr = (char *)src;
	int i;

	for(i = 0; i < n; ++i)
		*dest_ptr++ = *src_ptr++;

	return dest;
}

// memset
//
void *memset(void *dest, int c, size_t n)
{
        size_t i;

        for(i = 0; i < n; ++i)
              ((char *)dest)[i] = (char)c;

        return dest;
}

//////////////////////////////////////////////////////////////////////////////
//                         GESTIONE SEGMENTI IN GDT                         //
//////////////////////////////////////////////////////////////////////////////

// selettori
// Valori usati da sistema.s
//
#define KERNEL_CODE     0x08
#define KERNEL_DATA     0x10
#define USER_CODE       0x1b
#define USER_DATA       0x23

// ID_MAIN = 0x28

#define FIRST_FREE      0x30

#define SEG_SEL_GDT	0
#define SEG_SEL_RPL0	0

#define seg_index(sel) 	((sel) >> 3)
#define seg_sel(idx, ti, rpl) (((idx) << 3) | ((ti) << 2) | (rpl))

#define SEG_DPL0	0x0000
#define SEG_DPL3	0x6000

#define SEG_G		0x00800000
#define SEG_P		0x00008000

#define SEG_TYPE_DATA	0x00401000	/* b = 1 s = 1 type = data */
#define SEG_TYPE_CODE	0x00401800	/* d = 1 s = 1 type = code */

#define SEG_DATA_W	0x0200

#define SEG_CODE_C	0x0400
#define SEG_CODE_R	0x0200

#define SEG_TYPE_LDT	0x0200
#define SEG_TYPE_TSS	0x0900

// imposta i valori del descrittore desc
//
#define seg_fill_desc(desc, base, limit, type, dpl, g) do {\
	(desc)->data[0] = ((limit) & 0xffff) | (((base) & 0xffff) << 16);\
	(desc)->data[1] = ((base) & 0xff000000) | (g) |\
		((limit) & 0x000f0000) | SEG_P | (dpl) | (type) |\
		(((base) & 0x00ff0000) >> 16);\
} while(0)

// imposta i valori per un descrittore di un segmento assente
//
#define gdt_fill_np_desc(sel)\
	seg_fill_np_desc(&gdt[seg_index(sel)]);

// imposta i valori per il descrittore di un tss
//
#define gdt_fill_tss_desc(sel, base, limit)\
	seg_fill_desc(&gdt[seg_index(sel)], base, limit, SEG_TYPE_TSS, SEG_DPL0, 0)

// si utilizza la dimensione massima della gdt
#define GDT_SIZE	0x2000

// tipo per il descrittore di segmento
//
typedef struct seg_desc {
	unsigned long data[2];
} seg_desc_t;

// puntatore nella memoria lineare alla gdt (la gdt occupa gli indirizzi
//  0 - 0x00001000)
seg_desc_t *gdt = 0;

// bitmap per l' allocazione dei descrittori della gdt
//
bm_t gdt_bitmap;

// allocazione di un descrittore (ritorna l' indice in gdt)
//
int gdt_alloc_index()
{
        unsigned int pos;
        if(!bm_alloc(&gdt_bitmap, &pos))
                return -1;

        return (int)pos;
}

// rilascio di un descrittore (tramite il suo indice)
//
void gdt_free_index(int i)
{
        bm_free(&gdt_bitmap, i);
}

//////////////////////////////////////////////////////////////////////////////
//                     GESTIONE (MINIMA) DELLA MEMORIA                      //
//////////////////////////////////////////////////////////////////////////////

extern unsigned io_end;			// fine del modulo di IO

const int PAGE_SIZE = 4096;
#define MEM_LOW io_end			// fine regione dedicata al
					//  modulo di IO
#define MEM_HIGH 0x02000000		// usa 32MB di ram

// pagine allocabili
#define FREE_PAGES ((MEM_HIGH - MEM_LOW) / PAGE_SIZE)

bm_t page_bitmap;                       // mappa di bit delle pagine fisiche
unsigned int *page_bitmap_buf;		// buffer per la mappa

// alloca piu' pagine fisiche consecutive
//
void *mem_page_alloc(int n = 1)
{
        unsigned int pos;
        if(!bm_alloc(&page_bitmap, &pos, n))
                return 0;

        return (void *)(MEM_LOW + pos * PAGE_SIZE);
}

// rilascia piu' pagine fisiche consecutive
//
void mem_page_free(void *p, int n = 1)
{
        bm_free(&page_bitmap, ((unsigned int)p - MEM_LOW) / PAGE_SIZE, n);
}

//////////////////////////////////////////////////////////////////////////////
//                        SUPPORTO ALLA PAGINAZIONE                         //
//////////////////////////////////////////////////////////////////////////////

typedef unsigned int pdb_t;	// Page Directory Base (cr3)

typedef unsigned int pde_t;	// Page Directory Entry
typedef unsigned int pte_t;	// Page Table Entry

#define PG_P		1	// maschera con bit P a 1
#define PG_WRITE	2	// maschera con bit W/R a 1
#define PG_USER		4	// maschera con bit U/S a 1

// base del direttorio del nucleo
//
// kernel_pdb viene usato dal nucleo in fase di inizializzazione (subito dopo
//  l' attivazione della paginazione), da main (dopo averci aggiunto le sezioni
//  condivise) e nella creazione dei direttori per gli altri processi,
//  come template
//
pdb_t kernel_pdb;

// PWT e PCD = 0
inline pdb_t pg_pdb(void *base) { return (pdb_t)base & 0xfffff000; }

// ritorna il descrittore di tabella delle pagine per la tabella all' indirizzo
//  pa, con i flag flags
//
inline pde_t pg_pde(unsigned int pa, int flags)
{
	return pa & 0xfffff000 | flags;
}

// ritorna il descrittore di pagina per la pagina all' indirizzo pa, con i flag
//  flags
//
inline pte_t pg_pte(unsigned int pa, int flags)
{
	return pa & 0xfffff000 | flags;
}

// ritorna l' indirizzo del descrittore di tabella delle pagine per l' indirizzo
//  lineare va nel direttorio indirizzato da pdb
//
inline pde_t *pg_pde_addr(pdb_t pdb, unsigned int va)
{
	pde_t *pd_base = (pde_t *)(pdb & 0xfffff000);
	return &pd_base[va >> 22];
}

// ritorna l' indirizzo del descrittore della tabella delle pagine per
//  l' indirizzo lineare va nello spazio di indirizzi individuato da pdb
// si assume che la tabella sia presente
//
inline pte_t *pg_pte_addr(pdb_t pdb, unsigned int va)
{
	pde_t *pde = pg_pde_addr(pdb, va);
	pte_t *pt_base = (pte_t *)(*pde & 0xfffff000);
	return &pt_base[(va & 0x003ff000) >> 12];
}

// ritorna l' indirizzo fisico che corrisponde a va in pdb
// si assume che la pagina sia presente
//
inline unsigned int pg_pa(pdb_t pdb, unsigned int va)
{
	pte_t *pte = pg_pte_addr(pdb, va);
	return (*pte & 0xfffff000) | (va & 0x00000fff);
}

// aggiunge la pagina fisica pa all' indirizzo logico va in pdb
//
inline bool pg_add(pdb_t pdb, unsigned int pa, unsigned int va, int flags)
{
	pde_t *pde = pg_pde_addr(pdb, va);

	if(!(*pde & PG_P)) {		// tabella delle pagine assente
		void *tab = mem_page_alloc();
		if(!tab)
			return false;

		memset(tab, 0, PAGE_SIZE);
		*pde = pg_pde((unsigned int)tab, flags | PG_WRITE | PG_P);
	}

	// se si vuole aggiungere una pagina utente la tabella deve essere
	//  accessibile all' utente stesso (le pagine sistema sono protette
 	//  dai relativi descrittori, con U/S a 0)
 	//
	if(!(*pde & PG_USER) && (flags & PG_USER))
		*pde |= PG_USER;

	pte_t *pte = pg_pte_addr(pdb, va);
	bool old_p = *pte & PG_P;
	*pte = pg_pte(pa, flags | PG_P);

	// se il direttorio e' in uso e la pagina era presente e' necessario
	//  invalidare l' entrata del tlb relativa alla pagina appena inserita
	// NOTA: tale eventualita' non si presenta mai
	//
	if(cur_des()->cr3 == pdb && old_p)
		asm("invlpg %0" : : "m"(*(char *)(va)));

	return true;
}

// aggiunge n pagine consecutive a pdb, a partire da pa
// NOTA: il fallimento di un inserimento lascia pde in uno stato non definito,
//  ma avviene solo in situazioni "catastrofiche" (out of memory in fase di
//  inizializzazione)
//
inline bool pg_add_region(pdb_t pdb, unsigned int pa,
		unsigned int va, int n, int flags)
{
	for(int i = 0; i < n; ++i)
		if(!pg_add(pdb, pa + i * PAGE_SIZE, va + i * PAGE_SIZE, flags))
			return false;

	return true;
}

// toglie da pdb la pagina all' indirizzo lineare va
//
inline bool pg_remove(pdb_t pdb, unsigned int va)
{
	pde_t *pde = pg_pde_addr(pdb, va);

	if(!(*pde & PG_P))		// tabella delle pagine assente
		return false;

	pte_t *pte = pg_pte_addr(pdb, va);
	if(!(*pte & PG_P))		// pagina gia' rimossa
		return false;

	*pte = 0;

	// se il direttorio e' in uso e' necessario invalidare l' entrata
	//  del tlb relativa alla pagina appena inserita
	//
	// NOTA: tale situazione non si dovrebbe mai presentare, dato che
	//  attualmente le pagine vengono rimosse solo alla fine di un processo
	//  (la rimozione viene fatta in pg_vfree)
	//
	if(cur_des()->cr3 == pdb)
		asm("invlpg %0" : : "m"(*(char *)(va)));

	return true;
}

// toglie la regione individuata da va,n da pdb
//
inline bool pg_remove_region(pdb_t pdb, unsigned int va, int n)
{
	for(int i = 0; i < n; ++i)
		if(!pg_remove(pdb, va + i * PAGE_SIZE))
			return false;

	return true;
}

// alloca pages pagine fisiche e le inserisce nello spazio logico individuato
//  da pdb a partire dall' indirizzo lineare start
//
// le pagine fisiche sono consecutive (garantito dall' allocatore)
//
bool pg_valloc(pdb_t pdb, unsigned int start, int pages, int flags)
{
	int i;
	unsigned int va;
	void *pa;

	pa = mem_page_alloc(pages);
	if(!pa || !pg_add_region(pdb, (unsigned)pa, start, pages, flags))
		return false;

	return true;
}

// dealloca pages pagine fisiche riferite dagli indirizzi logici successivi a
//  start, in pdb
//
// si assume un allocatore per le pagine fisiche a mappa di bit
//
void pg_vfree(pdb_t pdb, unsigned int start, int pages)
{
	int i;

	// si dealloca una pagina per volta perche' le pagine potrebbero
	//  non essere contigue
	for(i = 0; i < pages; ++i)
		mem_page_free((void *)pg_pa(pdb, (start + i * PAGE_SIZE)));

	// pdb non verra' piu' usato, dato che pg_vfree viene chiamata
	//  solo al termine di un processo, comunque in generale e' necessario
	//  aggiornare le tabelle delle pagine
	//
	pg_remove_region(pdb, start, pages);
}

// inizializzazione della paginazione, inizialmente le tabelle delle pagine
//  contengono solo la memoria fisica in memoria virtuale (in cui risiede
//  il nucleo).
//
void pg_init(void)
{
	void *dir = mem_page_alloc();

	if(!dir)
		panic("Impossibile allocare il direttorio delle pagine");

	memset(dir, 0, PAGE_SIZE);
	kernel_pdb = pg_pdb(dir);

	// inserimento della memoria fisica in memoria virtuale
	//
	if(!pg_add_region(kernel_pdb, 0, 0, MEM_HIGH / PAGE_SIZE, PG_WRITE))
		panic("Memoria insufficiente");

	// caricamento di cr3
	//
	asm("movl %0, %%cr3" : : "r"(kernel_pdb));

	// abilitazione della paginazione (PSE = 0, PAE = 0, WP = 0)
	//
	asm("movl %0, %%cr4\n\t"
		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"
		"movl %%eax, %%cr0\n\t" : : "r"(0));
}

////////////////////////////////////////////////////////////////////////////////
// Inizializzazione della gestione della memoria
//

// limiti della memoria bassa ( < 1MB ) usata dal nucleo per
//  l' allocazione dinamica
//
#define FINE_IDT        ((void *)0x10800)
#define INIZIO_MEM_VIDEO ((void *)0xa0000)

void mem_init(void)
{
        kernel_pool = pool_create(FINE_IDT, INIZIO_MEM_VIDEO);
        page_bitmap_buf = new unsigned int[BM_BUFSIZE(FREE_PAGES)];
        bm_create(&page_bitmap, page_bitmap_buf, FREE_PAGES);
        pg_init();
}

////////////////////////////////////////////////////////////////////////////////
//                    CARICAMENTO DEI PROGRAMMI UTENTE                        //
////////////////////////////////////////////////////////////////////////////////

// i programmi utente sono contenuti nel floppy di avviamento subito dopo
//  l' immagine del nucleo e sono caricati in memoria da boot.s
//
// il contenuto della memoria immediatamente successiva al nucleo e':
//  - testo condiviso per il livello utente
//  - testo condiviso per il livello sistema
//  - testo dei programmi utente
//  - dati condivisi per il livello utente
//  - dati condivisi per il lovello sistema
//  - dati dei programmi utente
//
// NOTA: il libro non prevede testo condiviso (tra processi con corpo diverso),
//  questo si rende necessario per poter avere una sola copia delle librerie
//  usando piu' file sorgente per i programmi utente (invece del
//  solo utente.cpp)
//  

// indirizzo della funzione main
// NOTA: per evitare il collegamento il valore di questo puntatore viene
//  scritto nell' immagine dopo la compilazione dei programmi utente;
//  ovviamente questo simbolo non e' il simbolo main di utente.cpp (altrimenti
//  sarebbe stato extern void main(void))
//
extern void (*main)(void);

// le seguenti variabili sono gli indirizzi logici dei limiti delle zone
//  citate in precedenza DOPO il caricamento
//

extern unsigned u_usr_shtext_end;	// fine testo utente condiviso
extern unsigned u_sys_shtext_end;	// fine testo sistema condiviso
extern unsigned u_data;			// inizio dati utente
extern unsigned u_usr_shdata_end;	// fine dati utente condiviso
extern unsigned u_sys_shdata_end;	// fine dati sistema condivisi
extern unsigned u_end;			// fine dati utente

extern int end;				// variabile alla fine del nucleo

// indirizzi logici a cui effettuare il caricamento
//
#define USER_LOADADDR		0x02000000
#define IO_LOADADDR		0x00198000
#define KERN_LOADADDR		0x00100000

#define SHUSRTEXT_START		USER_LOADADDR
#define SHSYSTEXT_START		u_usr_shtext_end
#define TEXT_START		u_sys_shtext_end
#define SHUSRDATA_START		u_data
#define SHSYSDATA_START		u_usr_shdata_end
#define DATA_START		u_sys_shdata_end
#define DATA_END		u_end

// indirizzo in memoria (fisica) dei programmi utente
//
#define SRC_UTEXT		((unsigned)&end + io_end - IO_LOADADDR)

// dimensione di utente.bin
//
#define USER_SIZE		(DATA_END - USER_LOADADDR)

// primo indirizzo virtuale libero in ogni spazio di indirizzi
//
#define FREE_VIRT_START		(USER_LOADADDR + USER_SIZE)

// fine in memoria (fisica) dei programmi utente
//
#define SRC_UDATA_END		(SRC_UTEXT + USER_SIZE)

// indirizzi in memoria (fisica) del testo utente condiviso
//
#define SRC_SHUSRTEXT_START	SRC_UTEXT
#define SRC_SHUSRTEXT_END	(SRC_UTEXT + u_usr_shtext_end - USER_LOADADDR)
#define SRC_SHUSRTEXT_SIZE	(SRC_SHUSRTEXT_END - SRC_SHUSRTEXT_START)
#define SRC_SHUSRTEXT_PAGES	(SRC_SHUSRTEXT_SIZE / PAGE_SIZE)

// indirizzi in memoria del testo sistema condiviso
//
#define SRC_SHSYSTEXT_START	SRC_SHUSRTEXT_END
#define SRC_SHSYSTEXT_END	(SRC_UTEXT + u_sys_shtext_end - USER_LOADADDR)
#define SRC_SHSYSTEXT_SIZE	(SRC_SHSYSTEXT_END - SRC_SHSYSTEXT_START)
#define SRC_SHSYSTEXT_PAGES	(SRC_SHSYSTEXT_SIZE / PAGE_SIZE)

// indirizzi in memoria del testo dei programmi utente
//
#define SRC_TEXT_START		SRC_SHSYSTEXT_END
#define SRC_TEXT_END		(SRC_UTEXT + u_data - USER_LOADADDR)
#define SRC_TEXT_SIZE		(SRC_TEXT_END - SRC_TEXT_START)
#define SRC_TEXT_PAGES		(SRC_TEXT_SIZE / PAGE_SIZE)

// indirizzi in memoria dei dati utente condivisi
//
#define SRC_SHUSRDATA_START	SRC_TEXT_END
#define SRC_SHUSRDATA_END	(SRC_UTEXT + u_usr_shdata_end - USER_LOADADDR)
#define SRC_SHUSRDATA_SIZE	(SRC_SHUSRDATA_END - SRC_SHUSRDATA_START)
#define SRC_SHUSRDATA_PAGES	(SRC_SHUSRDATA_SIZE / PAGE_SIZE)

// indirizzi in memoria dei dati sistema condivisi
//
#define SRC_SHSYSDATA_START	SRC_SHUSRDATA_END
#define SRC_SHSYSDATA_END	(SRC_UTEXT + u_sys_shdata_end - USER_LOADADDR)
#define SRC_SHSYSDATA_SIZE	(SRC_SHSYSDATA_END - SRC_SHSYSDATA_START)
#define SRC_SHSYSDATA_PAGES	(SRC_SHSYSDATA_SIZE / PAGE_SIZE)

// indirizzi in memoria dei dati dei programmi utente
//
#define SRC_DATA_START		SRC_SHSYSDATA_END
#define SRC_DATA_END		SRC_UDATA_END
#define SRC_DATA_SIZE		(SRC_DATA_END - SRC_DATA_START)
#define SRC_DATA_PAGES		(SRC_DATA_SIZE / PAGE_SIZE)

// creazione di un nuovo direttorio (clone di kernel_pdb)
//
bool pg_new_pd(pdb_t *pdb)
{
	void *dir = mem_page_alloc(), *pt;
	unsigned int va, pa;
	pte_t *spte;

	if(!dir)
		return false;

	*pdb = pg_pdb(dir);

	memset(dir, 0, PAGE_SIZE);

	// le tabelle delle pagine con la memoria fisica sono condivise
	memcpy(dir, (void *)(kernel_pdb & 0xfffff000),
 		sizeof(pde_t) * MEM_HIGH / (1024 * PAGE_SIZE));

	for(va = MEM_HIGH; va < FREE_VIRT_START; va += PAGE_SIZE) {
		spte = pg_pte_addr(kernel_pdb, va);
		if(*spte & PG_P)
			if(!pg_add(*pdb, pg_pa(kernel_pdb, va),
   					va, *spte & 0xfff))
				return false;
	}

	return true;
}

// rilascia il direttorio e le tabelle delle pagine private (le pagine private
//  devono essere gia' state deallocate)
//
void pg_delete_pd(pdb_t pdb)
{
	unsigned int va;
	pde_t *pde;

	for(va = MEM_HIGH; va < 0x10000000; va += 1024 * PAGE_SIZE) {
		pde = pg_pde_addr(pdb, va);
		if(*pde & PG_P)
			mem_page_free((void *)(*pde & 0xfffff000));
	}

	mem_page_free((void *)(pdb & 0xfffff000));
}

// caricamento nello spazio di memoria individuato da kernel_pdb delle sezioni
//  condivise da tutti i programmi utente (durante l' inizializzazione)
//
// il caricamento vero e proprio da floppy viene fatto dal bootloader, qui si
//  inseriscono in kernel_pdb le zone di memoria virtuale interessate
//
void load_shared()
{
	// testo utente condiviso (sola lettura, livello utente)
	if(!pg_add_region(kernel_pdb, SRC_SHUSRTEXT_START, SHUSRTEXT_START,
 			SRC_SHUSRTEXT_PAGES, PG_USER))
		panic("Impossibile caricare i programmi utente (testo utente condiviso)");

	// testo sistema condiviso (sola lettura (ignorato), livello sistema)
	if(!pg_add_region(kernel_pdb, SRC_SHSYSTEXT_START, SHSYSTEXT_START,
 			SRC_SHSYSTEXT_PAGES, 0))
		panic("Impossibile caricare i programmi utente (testo sistema condiviso)");

	// dati utente condivisi (livello utente)
	if(!pg_add_region(kernel_pdb, SRC_SHUSRDATA_START, SHUSRDATA_START,
 			SRC_SHUSRDATA_PAGES, PG_USER|PG_WRITE))
		panic("Impossibile caricare i programmi utente (dati utente condivisi)");

	// dati sistema condivisi (livello sistema)
	if(!pg_add_region(kernel_pdb, SRC_SHSYSDATA_START, SHSYSDATA_START,
 			SRC_SHSYSDATA_PAGES, PG_WRITE))
		panic("Impossibile caricare i programmi utente (dati sistema condivisi)");
}

////////////////////////////////////////////////////////////////////////////////
//                             INIZIALIZZAZIONE                               //
////////////////////////////////////////////////////////////////////////////////

// buffer usati per le bitmap
//
unsigned int *gdt_bm_buf;
unsigned int *sem_bm_buf;

// buffer statico per sem_bm (per l' oggetto, non per la bitmap)
//
bm_t sem_bm_mem;

// inizializzazione di componenti secondari
//
void misc_init()
{
	gdt_bm_buf = new unsigned int[BM_BUFSIZE(GDT_SIZE)];
	sem_bm_buf = new unsigned int[BM_BUFSIZE(MAX_SEM)];

	if(gdt_bm_buf == 0 || sem_bm_buf == 0)
		panic("Memoria insufficiente per le bitmap del nucleo\n.");

	// bitmap della gdt
        bm_create(&gdt_bitmap, gdt_bm_buf, GDT_SIZE);
        bm_set(&gdt_bitmap, 0, FIRST_FREE >> 3);

        // bitmap dei semafori
        bm_create(&sem_bm_mem, sem_bm_buf, MAX_SEM);
        sem_bm = &sem_bm_mem;

        // fpu
        asm("fninit");
}

// Descrittore del processo in cui verra' trasformato main. Nonostante nel
//  momento in cui inizia l' esecuzione di main() non ci sia alcun processo
//  si predispongono una pila per il livello utente ed una per il livello
//  sistema ed il direttorio delle pagine per permettere a main, eseguito
//  a livello utente, di fare chiamate di sistema; per semplificare begin_p
//  si creano fin dall' inizio il proc_elem ed il des_proc per main, per poter
//  usare salva_stato per salvare lo stato di main quando ancora non
//  e' un processo
//
des_proc main_des;

// proc_elem per il processo creato da main
//
proc_elem main_proc = { ID_MAIN, PRIO_MAIN, 0 };

// trasferimento dell' esecuzione a main, con passaggio a livello utente
//
extern "C" void call_main(void);

#define MAIN_STACK_PAGES 4	// pagine per la pila di livello utente di main
#define MAIN_SYSSTACK_PAGES 1	// pagine per la pila sistema di main

// prepara l' esecuzione di main e chiama call_main
//
void run_main(void)
{
	void *sp;

	// allocazione delle pile
	if(!pg_valloc(kernel_pdb, FREE_VIRT_START, MAIN_STACK_PAGES,
 			PG_USER|PG_WRITE) ||
    			(sp = mem_page_alloc(MAIN_SYSSTACK_PAGES)) == 0)
		panic("Impossibile eseguire main, memoria insufficiente");

	// valori usati nel passaggio a livello sistema
	main_des.ss0 = KERNEL_DATA;
	main_des.esp0 = (unsigned int)sp + MAIN_SYSSTACK_PAGES * PAGE_SIZE;

	main_des.ss = USER_DATA;
	main_des.esp = FREE_VIRT_START + MAIN_STACK_PAGES * PAGE_SIZE;

	main_des.cr3 = kernel_pdb;
	main_des.liv = LIV_UTENTE;

	gdt_fill_tss_desc(ID_MAIN, (int)&main_des, sizeof(des_proc));

	// per permettere a begin_p di usare salva_stato
	esecuzione = &main_proc;

	// per permettere il passaggio a livello sistema tramite interruzione
	asm("ltr %0": : "m"(ID_MAIN));

	call_main();
}

// inizializzazione della console
//
void con_init(void);

// caricamento del modulo di IO
//
extern "C" void load_io(void);

// inizializzazione modulo di IO
// la modalita' di accesso e' quella di main()
//
extern "C" void (*io_init)(void);

// inizializzazione ad alto livello del sistema, chiamata da sistema.s
// (entry point di sistema.cpp durante la fase di inizializzazione)
//
extern "C" void cpp_init(void)
{
	con_init();		// rende possibile chiamare printk e panic

	printk("Inizializzazione del sistema in corso...\n\n");

	printk("Gestore della memoria.\n");
	mem_init();		// inizializzazione gest. memoria

	printk("Componenti secondarie.\n");
	misc_init();		// varie

	printk("Caricamento del modulo di IO.\n");
	load_io();		// caricamento modulo IO

	printk("Caricamento sezioni condivise.\n");
	load_shared();		// caricamento sez. condivise

	printk("Inizializzazione modulo di IO.\n");
	io_init();		// dopo il caricamento delle sez. condivise,
				//  per farle usare ai processi esterni

        printk("\nEsecuzione di main\n");

        run_main();		// esecuzione di main a liv. utente
}

// template per i descrittori dei processi sistema
// (*) indica i campi da inizializzare
//
des_proc kptempl = {
	0,				// link
	0, 0,				// esp0, ss0
	0, 0,				// esp1, ss1
	0, 0,				// esp2, ss2
	0,				// cr3 (*)
	0,				// eip
	0,				// eflags
	0,				// eax
	0,				// ecx
	0,				// edx
	0,				// ebx
	0,				// esp (*)
	0,				// ebp
	0,				// esi
	0,				// edi
	KERNEL_DATA,			// es
	KERNEL_CODE,			// cs (prelevato dalla pila)
	KERNEL_DATA,			// ss
	KERNEL_DATA,			// ds
	0,				// fs
	0,				// gs
	0,				// ldt
	0,				// io_map
	0,				// vzone
	0,				// vend
	{ 0x037f, 0, 0xffff, },		// fpu
	LIV_SISTEMA			// liv
};

// template per i descrittori dei processi utente
// (*) indica i campi da inizializzare
//
// i processi utente iniziano a livello sistema, quindi hanno
//  inizialmente cs = KERNEL_CODE e ss = KERNEL_DATA. In effetti
//  cs non viene utilizzator, dato che il suo valore viene salvato
//  e prelevato dalla pila.
//
des_proc uptempl = {
	0,				// link
	0, KERNEL_DATA,			// esp0 (*), ss0
	0, 0,				// esp1, ss1
	0, 0,				// esp2, ss2
	0,				// cr3 (*)
	0,				// eip
	0,				// eflags
	0,				// eax
	0,				// ecx
	0,				// edx
	0,				// ebx
	0,				// esp (*)
	0,				// ebp
	0,				// esi
	0,				// edi
	USER_DATA,			// es
	KERNEL_CODE,			// cs (prelevato dalla pila)
	KERNEL_DATA,			// ss
	USER_DATA,			// ds
	0,				// fs
	0,				// gs
	0,				// ldt
	0,				// io_map
	0,				// vzone
	0,				// vend
	{ 0x037f, 0, 0xffff, },		// fpu
	LIV_UTENTE			// liv
};

#define OWN_STACK_PAGES	4	// pagine per la pila del liv. di priv. proprio
#define SYS_STACK_PAGES 1	// pagine per la pila sistema nei proc. utente

// creazione delle pile necessarie per il processo (una per i processi di
//  livello sistema, due per quelli di livello utente) e loro inizializzazione
//
// NOTA: il testo specifica la terminazione dei processi utente tramite la
//  chiamata di sistema terminate_p(); sarebbe possibile, data la struttura
//  dei programmi utente (sono funzioni C++, terminano con ret) inserire
//  nella pila del livello di privilegio proprio, come ultimo indirizzo di
//  ritorno (penultima doppia parola), l' indirizzo di una routine terminate_p()
//  che effettui, tramite interruzione software, la relativa chiamata di sistema.
//
bool crea_pile(des_proc *dp, void (*f)(int), int a, char liv)
{
	void *sp0;
	unsigned int *stack;

	switch(liv) {
		case LIV_SISTEMA:
			sp0 = mem_page_alloc(OWN_STACK_PAGES);
			if(!sp0)
				return false;

			memset(sp0, 0, OWN_STACK_PAGES * PAGE_SIZE);

			dp->esp = (unsigned int)sp0 + OWN_STACK_PAGES *
   				PAGE_SIZE - 20;

   			dp->esp0 = (unsigned int)sp0;	// per la deallocazione

   			stack = (unsigned int *)dp->esp;
   			stack[0] = (unsigned int)f;
   			stack[1] = KERNEL_CODE;
   			stack[2] = 0x00000200;
   			stack[3] = 0xffffffff;
   			stack[4] = a;

			break;
		case LIV_UTENTE:
			sp0 = mem_page_alloc(SYS_STACK_PAGES);
			if(!sp0)
				return false;
			if(!pg_valloc(dp->cr3, FREE_VIRT_START, OWN_STACK_PAGES,
					PG_USER|PG_WRITE)) {
				mem_page_free(sp0, SYS_STACK_PAGES);
				return false;
			}

			memset(sp0, 0, SYS_STACK_PAGES * PAGE_SIZE);
			memset((void *)pg_pa(dp->cr3, FREE_VIRT_START), 0,
				OWN_STACK_PAGES * PAGE_SIZE);

			dp->esp0 = (unsigned int)sp0 + SYS_STACK_PAGES *
   				PAGE_SIZE;
   			dp->esp = (unsigned int)sp0 + SYS_STACK_PAGES *
      				PAGE_SIZE - 20;

      			stack = (unsigned int *)dp->esp;
		      	stack[0] = (unsigned int)f;
			stack[1] = USER_CODE;
			stack[2] = 0x00000200;
			stack[3] = FREE_VIRT_START + OWN_STACK_PAGES *
   				PAGE_SIZE - 8;
			stack[4] = USER_DATA;

			stack = (unsigned int *)pg_pa(dp->cr3,
   				FREE_VIRT_START + OWN_STACK_PAGES
       				* PAGE_SIZE - 8);
			stack[0] = 0xffffffff;
			stack[1] = a;

			break;
	}

	return true;
}

// crea un processo (descrittore, direttorio, pile)
//
bool crea_proc(void f(int), int a, char liv, short &id)
{
	int idx;
	des_proc *dp;
	pdb_t pdb;

	// allocazione dell' indice del tss, del descrittore e del direttorio
	if((idx = gdt_alloc_index()) == -1 || !(dp = new des_proc) ||
 			!pg_new_pd(&pdb))
		return false;

	// impostazione dei valori in gdt
	gdt_fill_tss_desc(idx << 3, (unsigned)dp, sizeof(des_proc));

	// inizializzazione del descrittore
	*dp = liv == LIV_UTENTE ? uptempl: kptempl;

	// indirizzo del direttorio delle pagine
	dp->cr3 = pdb;

	// inizializzazione delle pile
	if(!crea_pile(dp, f, a, liv))
		goto errore;

	// inserimento della sezione testo nello spazio logico del processo
	if(!pg_add_region(dp->cr3, SRC_TEXT_START, TEXT_START,
 			SRC_TEXT_PAGES, liv == LIV_UTENTE ? PG_USER: 0))
		goto errore;

	// inserimento della sezione dati (ogni processo ne ha una copia)
	if(!pg_valloc(dp->cr3, DATA_START, SRC_DATA_PAGES,
 			liv == LIV_UTENTE ? (PG_USER|PG_WRITE): PG_WRITE))
		goto errore;

	memcpy((void *)pg_pa(dp->cr3, DATA_START), (void *)SRC_DATA_START,
 		SRC_DATA_PAGES * PAGE_SIZE);

	// selettore del tss usato come id del nuovo processo
	id = seg_sel(idx, SEG_SEL_GDT, SEG_SEL_RPL0);

        return true;

errore:
	mem_page_free((void *)((unsigned)dp->cr3 & 0xfffff000));
	delete dp;
	gdt_free_index(idx);

	return false;
}

const unsigned int HEAP_START = 0x10000000;	// va bene qualsiasi indirizzo
						//  piu' alto della pila utente

// rilascia le risorse associate al processo riferito da p
//
void canc_proc(proc_elem *p)
{
	des_proc *p_des = des_p(p->identifier);

	// rilascia heap
	if(p_des->vzone) {
		pg_vfree(p_des->cr3, HEAP_START,
  			(p_des->vend - HEAP_START) / PAGE_SIZE);
		zone_destroy(p_des->vzone);
	}

	// il direttorio attuale viene deallocato
	asm("movl %0, %%cr3" : : "r"(kernel_pdb));

	// rilascio della copia privata della sezione dati
	pg_vfree(p_des->cr3, DATA_START, SRC_DATA_PAGES);

	// rilascio delle pile
	if((p_des->liv) == LIV_UTENTE) {
		pg_vfree(p_des->cr3, FREE_VIRT_START, OWN_STACK_PAGES);
		mem_page_free((void *)(p_des->esp0 - SYS_STACK_PAGES * PAGE_SIZE),
  			SYS_STACK_PAGES);
	} else
		mem_page_free((void *)p_des->esp0, OWN_STACK_PAGES);

	// deallocazione del direttorio e delle tabelle delle pagine
	pg_delete_pd(p_des->cr3);

	// rilascio del selettore del tss
	gdt_free_index(seg_index(p->identifier));

	delete p_des;
	delete p;
}

// modifica le dimensioni della zona di memoria usata da mem_alloc
//
bool mod_vzone(des_proc *p_des, int dim, bool crea)
{
	unsigned int est_size = dim << 2;
	unsigned int pages;

	est_size = (est_size & 0xfffff000) + ((est_size & 0xfff) ? 0x1000: 0);
	pages = est_size / PAGE_SIZE;

	if(crea) {
		if(!pg_valloc(p_des->cr3, HEAP_START, pages,
 				(p_des->liv == LIV_UTENTE? PG_USER: 0)|PG_WRITE))
			return false;
		p_des->vend = HEAP_START + est_size;

		memset((void *)pg_pa(p_des->cr3, HEAP_START), 0,
  				pages * PAGE_SIZE);

  		return (p_des->vzone = zone_create((void *)HEAP_START,
			(void *)p_des->vend)) != 0;
	} else {
		if(!pg_valloc(p_des->cr3, p_des->vend, pages,
 				(p_des->liv == LIV_UTENTE? PG_USER: 0)|PG_WRITE))
			return false;
		memset((void *)pg_pa(p_des->cr3, p_des->vend), 0,
  				pages * PAGE_SIZE);
		p_des->vend += est_size;

		return zone_grow(p_des->vzone, (void *)p_des->vend);
	}
}

// true se pv e' un indirizzo dello heap del processo
//
inline bool heap_addr(void *pv)
{
	if((unsigned int)pv >= HEAP_START)
		return true;

	return false;
}

// verifica i permessi del processo sui parametri passati (problema del
//  Cavallo di Troia)
//
// NOTA: puo' essere chiamata ad interruzioni abilitate
//
extern "C" bool verifica_area(void *area, unsigned int dim, bool write)
{
	unsigned int p, cs;
	int pages;
	pdb_t cr3;
	char liv;

	if(esecuzione != 0) {
		// sistema avviato, esecuzione e' valido
		cr3 = cur_des()->cr3;
		liv = cur_des()->liv;
	} else {
		// sistema in avviamento, si inizializzano semafori
		//  per IO, ecc...
		cr3 = kernel_pdb;
		liv = LIV_SISTEMA;
	}

	p = (unsigned int)area;
	dim += p & 0x00000fff;
	pages = dim / PAGE_SIZE + (dim % PAGE_SIZE) != 0;
	p &= 0xfffff000;

	while(pages > 0) {
		pde_t *pde = pg_pde_addr(cr3, p);

		if(!(*pde & PG_P))
			return false;

		pte_t *pte = pg_pte_addr(cr3, p);

		if(!(*pte & PG_P))
			return false;

		// si impedisce ai processi sistema di scrivere in pagine a sola
		//  lettura tramite chiamate di sistema
		if(liv == LIV_UTENTE && (!(*pte & PG_USER) ||
				write && !(*pte & PG_WRITE)))
			return false;

		p += PAGE_SIZE;
		--pages;
	}

	return true;
}

// gestione delle eccezioni hardware
//
extern "C" void hwexc(int num, unsigned int codice, unsigned int ind)
{
	printk("Eccezione hardware %d (codice di errore %x), EIP = %x\n",
		num, codice, ind);
	printk("Il processo %x verra' terminato\n\n", esecuzione->identifier);

	abort_p();
}

// stampa MSG su schermo e termina le elaborazioni del sistema
//
extern "C" void panic(const char *msg)
{
	printk("%s\n", msg);
	asm("cli;hlt");
}

////////////////////////////////////////////////////////////////////////////////
//                          FUNZIONI DI LIBRERIA                              //
////////////////////////////////////////////////////////////////////////////////

typedef char *va_list;

// Versione semplificata delle macro per manipolare le liste di parametri
//  di lunghezza variabile; funziona solo se gli argomenti sono di
//  dimensione multipla di 4, ma e' sufficiente per le esigenze di printk.
//
#define va_start(ap, last_req) (ap = (char *)&(last_req) + sizeof(last_req))
#define va_arg(ap, type) ((ap) += sizeof(type), *(type *)((ap) - sizeof(type)))
#define va_end(ap)

int strlen(const char *s)
{
	int l = 0;

	while(*s++)
		++l;

	return l;
}

char *strncpy(char *dest, const char *src, size_t l)
{
	size_t i;

	for(i = 0; i < l && src[i]; ++i)
		dest[i] = src[i];

	return dest;
}

static const char hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

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

proc_elem *pe_tast;
proc_elem *in_com[S], *out_com[S];

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
		default:
			; // activate_pe chiamata con parametri scorretti
	}
}

// Zone Allocator
//

const unsigned int ZONE_MIN_SIZE = 4096;
const unsigned int ZONE_MIN_FREE_SPACE = 128;

struct zone_block_t {
	zone_block_t *prev;
	zone_block_t *next;
	unsigned int size;
	void *addr;
	bool free;
};

struct zone_t {
	zone_block_t *first;
	zone_block_t *last;
	void *start;
	void *end;
};

// aggiunge un blocco alla zona
// assume che prev non sia nullo
//
inline void zone_add_block(zone_t *zone, zone_block_t *prev,
		zone_block_t *newone)
{
	newone->next = prev->next;
	newone->prev = prev;
	prev->next = newone;
	if(newone->next)
		newone->next->prev = newone;

	if(prev == zone->last)
		zone->last = newone;
}

// rimuove un blocco dalla zona
// assume che almeno un blocco (oltre a quello cancellato) sia sempre presente
//
inline void zone_remove_block(zone_t *zone, zone_block_t *old)
{
	zone_block_t *prev, *next;

	prev = old->prev;
	next = old->next;

	if(prev)
		prev->next = next;

	if(next)
		next->prev = prev;

	if(old == zone->first)
		zone->first = next;
	else if(old == zone->last)
		zone->last = prev;
}

zone_t *zone_create(void *start, void *end)
{
	zone_t *zone;
	zone_block_t *first;
	unsigned int size;

	size = (char *)end - (char *)start;
	if(size < ZONE_MIN_SIZE)
		return 0;

	if((zone = new zone_t) == 0)
		return 0;

	if((first = new zone_block_t) == 0) {
		delete zone;
		return 0;
	}

	zone->first = zone->last = first;
	zone->start = start;
	zone->end = end;

	first->prev = first->next = 0;
	first->size = size;
	first->addr = start;
	first->free = true;

	return zone;
}

bool zone_grow(zone_t *zone, void *newend)
{
	zone_block_t *block, *newblock;
	unsigned int added_size;

	if(newend <= zone->end)
		return false;

	added_size = (char *)newend - (char *)zone->end;
	block = zone->last;

	if(block->free) {
		block->size += added_size;
	} else {
		if((newblock = new zone_block_t) == 0)
			return false;

		zone_add_block(zone, zone->last, newblock);
		newblock->size = added_size;
		newblock->addr = zone->end;
		newblock->free = true;
	}

	zone->end = newend;

	return true;
}

void *zone_alloc(zone_t *zone, size_t size)
{
	zone_block_t *cur_b, *new_b;
	unsigned int free_space;

	size = size & 3 ? (size & ~3) + 4: size;

	cur_b = zone->first;
	while(cur_b && (cur_b->size < size || !cur_b->free))
		cur_b = cur_b->next;

	if(!cur_b)
		return 0;

	free_space = cur_b->size - size;
	if(free_space > ZONE_MIN_FREE_SPACE) {
		if((new_b = new zone_block_t) == 0)
			return 0;

		cur_b->size = size;
		zone_add_block(zone, cur_b, new_b);
		new_b->size = free_space;
		new_b->addr = (void *)((char *)cur_b->addr + size);
		new_b->free = true;
	}

	cur_b->free = false;

	return cur_b->addr;
}

void zone_free(zone_t *zone, void *addr)
{
	zone_block_t *cur_b, *prev, *next;

	if(!addr)
		return;

	cur_b = zone->first;
	while(cur_b && cur_b->addr < addr)
		cur_b = cur_b->next;

	if(!cur_b || cur_b->addr != addr || !cur_b->free)
		return;

	cur_b->free = true;
	prev = cur_b->prev;
	next = cur_b->next;

	if(next && next->free) {
		cur_b->size += next->size;
		zone_remove_block(zone, next);
		delete next;
	}

	if(prev && prev->free) {
		prev->size += cur_b->size;
		zone_remove_block(zone, cur_b);
		delete cur_b;
	}
}

void zone_destroy(zone_t *zone)
{
	zone_block_t *block;

	block = zone->first;
	while(block) {
		block = block->next;
		delete zone->first;
		zone->first = block;
	}

	delete zone;
}

