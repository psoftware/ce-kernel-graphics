// sistema.cpp
//
#include "costanti.h"
#include "libce.h"

///////////////////////////////////////////////////////////////////////////////////
//                   INIZIALIZZAZIONE                                            //
///////////////////////////////////////////////////////////////////////////////////
const natl MAX_PRIORITY	= 0xfffffff;
const natl MIN_PRIORITY	= 0x0000001;
const natl DUMMY_PRIORITY = 0x0000000;

const natq HEAP_START = 1024 * 1024U;
extern "C" natq start;
const natq HEAP_SIZE = (natq)&start - HEAP_START;

natq N_DPF;
natq DIM_M1;
natq DIM_M2;
/////////////////////////////////////////////////////////////////////////////////
//                     PROCESSI                                                //
/////////////////////////////////////////////////////////////////////////////////
const int N_REG = 16;	//

// descrittore di processo
struct des_proc {	
	natl riservato1;	
	addr punt_nucleo;
	// due quad  a disposizione (puntatori alle pile ring 1 e 2)
	natq disp1[2];		
	natq riservato2;
	//entry della IST, non usata
	natq disp2[7];
	natq riservato3;
	//finiti i campi obbligatori
	addr cr3;
	natq contesto[N_REG];
	natl cpl;
}__attribute__((packed));

volatile natl processi;		//
extern "C" natl activate_p(void f(int), int a, natl prio, natl liv); //
extern "C" void terminate_p();	//

//indici nell'array contesto
enum { I_RAX, I_RCX, I_RDX, I_RBX,
	I_RSP, I_RBP, I_RSI, I_RDI, I_R8, I_R9, I_R10, 
	I_R11, I_R12, I_R13, I_R14, I_R15 };
// )

// elemento di una coda di processi
struct proc_elem {
	natl id;
	natl precedenza;
	proc_elem *puntatore;
};
proc_elem *esecuzione;
proc_elem *pronti;

void inserimento_lista(proc_elem *&p_lista, proc_elem *p_elem)
{
// ( inserimento in una lista semplice ordinata
//   (tecnica dei due puntatori)
	proc_elem *pp, *prevp;

	pp = p_lista;
	prevp = 0;
	while (pp != 0 && pp->precedenza >= p_elem->precedenza) {
		prevp = pp;
		pp = pp->puntatore;
	}

	p_elem->puntatore = pp;

	if (prevp == 0)
		p_lista = p_elem;
	else
		prevp->puntatore = p_elem;

// )
}

//
void rimozione_lista(proc_elem *&p_lista, proc_elem *&p_elem)
{
// ( estrazione dalla testa
	p_elem = p_lista;  	// 0 se la lista e' vuota

	if (p_lista)
		p_lista = p_lista->puntatore;

	if (p_elem)
		p_elem->puntatore = 0;
// )
}

extern "C" void inspronti()
{
// (
	inserimento_lista(pronti, esecuzione);
// )
}

//
extern "C" void schedulatore(void)
{
// ( poiche' la lista e' gia' ordinata in base alla priorita',
//   e' sufficiente estrarre l'elemento in testa
	rimozione_lista(pronti, esecuzione);
// )
}

/////////////////////////////////////////////////////////////////////////////////
//                     SEMAFORI                                                //
/////////////////////////////////////////////////////////////////////////////////

// descrittore di semaforo
struct des_sem {
	int counter;
	proc_elem *pointer;
};

// vettore dei descrittori di semaforo
des_sem array_dess[MAX_SEM];

// - per sem_ini, si veda [P_SEM_ALLOC] avanti

//   abort_p: termina forzatamente un processo (vedi [P_PROCESS] avanti)
extern "C" void abort_p() __attribute__ (( noreturn ));
//   sem_valido: restituisce true se sem e' un semaforo effettivamente allocato
bool sem_valido(natl sem);
// )


//
extern "C" void c_sem_wait(natl sem)
{
	des_sem *s;

// (* una primitiva non deve mai fidarsi dei parametri
	if (!sem_valido(sem)) {
		flog(LOG_WARN, "semaforo errato: %d", sem);
		abort_p();
	}
// *)

	s = &array_dess[sem];
	(s->counter)--;

	if ((s->counter) < 0) {
		inserimento_lista(s->pointer, esecuzione);
		schedulatore();
	}
}

//
extern "C" void c_sem_signal(natl sem)
{
	des_sem *s;
	proc_elem *lavoro;

// (* una primitiva non deve mai fidarsi dei parametri
	if (!sem_valido(sem)) {
		flog(LOG_WARN, "semaforo errato: %d", sem);
		abort_p();
	}
// *)

	s = &array_dess[sem];
	(s->counter)++;

	if ((s->counter) <= 0) {
		rimozione_lista(s->pointer, lavoro);
		inserimento_lista(pronti, lavoro);
		inspronti();	// preemption
		schedulatore();	// preemption
	}
}

extern "C" natl sem_ini(int);		//
extern "C" void sem_wait(natl);		//
extern "C" void sem_signal(natl);	//


/////////////////////////////////////////////////////////////////////////////////
//                         TIMER                                               //
/////////////////////////////////////////////////////////////////////////////////

// richiesta al timer
struct richiesta {
	natl d_attesa;
	richiesta *p_rich;
	proc_elem *pp;
};

richiesta *p_sospesi; //

void inserimento_lista_attesa(richiesta *p); //
// parte "C++" della primitiva delay
extern "C" void c_delay(natl n)
{
	richiesta *p;

	p = static_cast<richiesta*>(alloca(sizeof(richiesta)));
	p->d_attesa = n;	
	p->pp = esecuzione;

	inserimento_lista_attesa(p);
	schedulatore();
}

// inserisce P nella coda delle richieste al timer
void inserimento_lista_attesa(richiesta *p)
{
	richiesta *r, *precedente;
	bool ins;

	r = p_sospesi;
	precedente = 0;
	ins = false;

	while (r != 0 && !ins)
		if (p->d_attesa > r->d_attesa) {
			p->d_attesa -= r->d_attesa;
			precedente = r;
			r = r->p_rich;
		} else
			ins = true;

	p->p_rich = r;
	if (precedente != 0)
		precedente->p_rich = p;
	else
		p_sospesi = p;

	if (r != 0)
		r->d_attesa -= p->d_attesa;
}


// (* in caso di errori fatali, useremo la segunte funzione, che blocca il sistema:
extern "C" void panic(cstr msg) __attribute__ (( noreturn ));
// implementazione in [P_PANIC]
// *)
extern "C" void init_idt();

#ifdef AUTOCORR
int MAX_LOG = 4;
#else
int MAX_LOG = 5;
#endif

extern "C" void c_log(log_sev sev, const char* buf, natl quanti)
{
	do_log(sev, buf, quanti);
}



extern "C" addr readCR2();
extern "C" addr readCR3();
int i_tab(addr, int lib);
natq& get_entry(addr,natl);
addr extr_IND_FISICO(natq);
bool extr_P(natq);
// gestore generico di eccezioni (chiamata da tutti i gestori di eccezioni in
// sistema.S, tranne il gestore di page fault)
extern "C" void gestore_eccezioni(int tipo, natq errore,
					addr rip, natq cs, natq rflag)
{
	flog(LOG_WARN, "Eccezione %d, errore %x", tipo, errore);
	flog(LOG_WARN, "rflag = %x, rip = %p, cs = %x", rflag, rip, cs);
	abort_p();
}
// (*il microprogramma di gestione delle eccezioni di page fault lascia in cima 
//   alla pila (oltre ai valori consueti) una doppia parola, i cui 4 bit meno 
//   significativi specificano piu' precisamente il motivo per cui si e' 
//   verificato un page fault. Il significato dei bit e' il seguente:
//   - prot: se questo bit vale 1, il page fault si e' verificato per un errore 
//   di protezione: il processore si trovava a livello utente e la pagina (o la 
//   tabella) era di livello sistema (bit US = 0). Se prot = 0, la pagina o la 
//   tabella erano assenti (bit P = 0)
//   - write: l'accesso che ha causato il page fault era in scrittura (non 
//   implica che la pagina non fosse scrivibile)
//   - user: l'accesso e' avvenuto mentre il processore si trovava a livello 
//   utente (non implica che la pagina fosse invece di livello sistema)
//   - res: uno dei bit riservati nel descrittore di pagina o di tabella non 
//   avevano il valore richiesto (il bit D deve essere 0 per i descrittori di 
//   tabella, e il bit pgsz deve essere 0 per i descrittori di pagina)
struct pf_error {
	natq prot  : 1;
	natq write : 1;
	natq user  : 1;
	natq res   : 1;
	natq pad   : 60; // bit non significativi
};
// *)

// (* indirizzo del primo byte che non contiene codice di sistema (vedi "sistema.S")
extern "C" addr fine_codice_sistema; 
// *)
void c_routine_pf();
extern "C" void c_pre_routine_pf(	//
	// (* prevediamo dei parametri aggiuntivi:
		pf_error errore,	/* vedi sopra */
		addr rip		/* ind. dell'istruzione che ha causato il fault */
	// *)
	)
{
	// (* il sistema non e' progettato per gestire page fault causati 
	//   dalle primitie di nucleo , quindi, se cio' si e' verificato, 
	//   si tratta di un bug
	if (rip < fine_codice_sistema || errore.res == 1) {
		flog(LOG_ERR, "rip: %p, page fault a %p", rip, readCR2());
		flog(LOG_ERR, "dettagli: %s, %s, %s, %s",
			errore.prot  ? "protezione"	: "pag/tab assente",
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema",
			errore.res   ? "bit riservato"	: "");
		panic("page fault dal modulo sistema");
	}
	// *)
	
	// (* l'errore di protezione non puo' essere risolto: il processo ha 
	//    tentato di accedere ad indirizzi proibiti (cioe', allo spazio 
	//    sistema)
	if (errore.prot == 1) {
		flog(LOG_WARN, "errore di protezione: eip=%x, ind=%x, %s, %s", rip, readCR2(),
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema");
		abort_p();
	}
	// *)
	

	c_routine_pf();
}


/////////////////////////////////////////////////////////////////////////////////
//                         PAGINE FISICHE                                      //
/////////////////////////////////////////////////////////////////////////////////

struct des_pf {
	int	livello;	// 0=pagina, -1=libera
	bool	residente;	// pagina residente o meno
	natl	processo;	// identificatore di processo
	natl	contatore;	// contatore per le statistiche
	natq	ind_massa;
	union {
		addr	ind_virtuale;
		des_pf*	prossima_libera;
	};
};

des_pf* dpf;		// vettore di descrittori di pagine fisiche
addr prima_pf_utile;	// indirizzo fisico della prima pagina fisica di M2
des_pf* pagine_libere;	// indice del descrittore della prima pagina libera

//
des_pf* descrittore_pf(addr indirizzo_pf)
{
	if (indirizzo_pf < prima_pf_utile)
		return 0;
	natq indice = ((natq)indirizzo_pf - (natq)prima_pf_utile) / DIM_PAGINA;
	return &dpf[indice];
}

//
addr indirizzo_pf(des_pf* ppf)
{
	natq indice = ppf - &dpf[0];
	return (addr)((natq)prima_pf_utile + indice * DIM_PAGINA);
}

// ( [P_MEM_PHYS]
// init_dpf viene chiamata in fase di inizalizzazione.  Tutta la
// memoria non ancora occupata viene usata per le pagine fisiche.  La funzione
// si preoccupa anche di allocare lo spazio per i descrittori di pagina fisica,
// e di inizializzarli in modo che tutte le pagine fisiche risultino libere
bool init_dpf()
{

	N_DPF = (MEM_TOT - 5*MiB) / (DIM_PAGINA + sizeof(des_pf));
	natq m1 = 5*MiB + N_DPF * sizeof(des_pf);
	DIM_M1 = (m1 + DIM_PAGINA - 1) & ~(DIM_PAGINA - 1);
	DIM_M2 = MEM_TOT - DIM_M1;
	dpf = (des_pf*)(5*MiB);

	prima_pf_utile = (addr)DIM_M1;

	pagine_libere = &dpf[0];
	for (natl i = 0; i < N_DPF - 1; i++) {
		dpf[i].livello = -1;
		dpf[i].prossima_libera = &dpf[i + 1];
	}
	dpf[N_DPF - 1].prossima_libera = 0;

	return true;
}

des_pf* alloca_pagina_fisica_libera()	//
{
	des_pf* p = pagine_libere;
	if (pagine_libere != 0)
		pagine_libere = pagine_libere->prossima_libera;
	return p;
}

// (* rende di nuovo libera la pagina fisica il cui descrittore di pagina fisica
//    ha per indice "i"
void rilascia_pagina_fisica(des_pf* ppf)
{
	ppf->livello = -1;
	ppf->prossima_libera = pagine_libere;
	pagine_libere = ppf;
}

des_pf* scegli_vittima(natl proc, int liv, addr ind_virtuale); //
bool scollega(des_pf* ppf);	//
void scarica(des_pf* ppf); //
des_pf* alloca_pagina_fisica(natl proc, int livello, addr ind_virt)
{
	des_pf *ppf = alloca_pagina_fisica_libera();
	if (ppf == 0) {
		if ( (ppf = scegli_vittima(proc, livello, ind_virt)) && scollega(ppf) )
			scarica(ppf);
	}
	return ppf;
}
// *)
// )


/////////////////////////////////////////////////////////////////////////////////
//                         PAGINAZIONE                                         //
/////////////////////////////////////////////////////////////////////////////////

static const natq BIT_SEGNO = (1UL << 47);
static const natq BIT_MODULO = BIT_SEGNO - 1;
static inline addr norm(addr a)
{
	natq v = (natq)a;
	return (addr)((v & BIT_SEGNO) ? (v | ~BIT_MODULO) : (v & BIT_MODULO));
}

static inline natq dim_pag(int liv)
{
	natq v = 1UL << ((liv - 1) * 9 + 12);
	return v;
}

static inline addr base(addr a, int liv)
{
	natq v = (natq)a;
	natq mask = dim_pag(liv + 1) - 1;
	return (addr)(v & ~mask);
}

void copy_des(addr src, addr dst, natl i, natl n)
{
	natq *pdsrc = static_cast<natq*>(src),
	     *pddst = static_cast<natq*>(dst);
	for (natl j = i; j < i + n && j < 512; j++)
		pddst[j] = pdsrc[j];
}

const addr ini_sis_c = norm((addr)(I_SIS_C * dim_pag(4)));
const addr ini_sis_p = norm((addr)(I_SIS_P * dim_pag(4)));
const addr ini_mio_c = norm((addr)(I_MIO_C * dim_pag(4)));
const addr ini_utn_c = norm((addr)(I_UTN_C * dim_pag(4)));
const addr ini_utn_p = norm((addr)(I_UTN_P * dim_pag(4)));

const addr fin_sis_c = (addr)((natq)ini_sis_c + dim_pag(4) * N_SIS_C);
const addr fin_sis_p = (addr)((natq)ini_sis_p + dim_pag(4) * N_SIS_P);
const addr fin_mio_c = (addr)((natq)ini_mio_c + dim_pag(4) * N_MIO_C);
const addr fin_utn_c = (addr)((natq)ini_utn_c + dim_pag(4) * N_UTN_C);
const addr fin_utn_p = (addr)((natq)ini_utn_p + dim_pag(4) * N_UTN_P);

//   ( definiamo alcune costanti utili per la manipolazione dei descrittori
//     di pagina e di tabella. Assegneremo a tali descrittori il tipo "natl"
//     e li manipoleremo tramite maschere e operazioni sui bit.
const natq BIT_P    = 1U << 0; // il bit di presenza
const natq BIT_RW   = 1U << 1; // il bit di lettura/scrittura
const natq BIT_US   = 1U << 2; // il bit utente/sistema(*)
const natq BIT_PWT  = 1U << 3; // il bit Page Wright Through
const natq BIT_PCD  = 1U << 4; // il bit Page Cache Disable
const natq BIT_A    = 1U << 5; // il bit di accesso
const natq BIT_D    = 1U << 6; // il bit "dirty"
const natq BIT_ZERO = 1U << 7; // (* nuova pagina, da azzerare *)

// (*) attenzione, la convenzione Intel e' diversa da quella
// illustrata nel libro: 0 = sistema, 1 = utente.

//TODO bit 63 NX (no-exec): che fare?
const natq ACCB_MASK  = 0x00000000000000FF; // maschera per il byte di accesso
const natq ADDR_MASK  = 0x7FFFFFFFFFFFF000; // maschera per l'indirizzo
const natq INDMASS_MASK = 0x7FFFFFFFFFFFF000; // maschera per l'indirizzo in mem. di massa
const natq INDMASS_SHIFT = 12;	    // primo bit che contiene l'ind. in mem. di massa
// )

bool  extr_P(natq descrittore)			//
{ // (
	return (descrittore & BIT_P); // )
}
bool extr_D(natq descrittore)			//
{ // (
	return (descrittore & BIT_D); // )
}
bool extr_A(natq descrittore)			//
{ // (
	return (descrittore & BIT_A); // )
}
bool extr_ZERO(natq descrittore)			//
{ // (
	return (descrittore & BIT_ZERO); // )
}
addr extr_IND_FISICO(natq descrittore)		//
{ // (
	return (addr)(descrittore & ADDR_MASK); // )
}
natq extr_IND_MASSA(natq descrittore)		//
{ // (
	return (descrittore & INDMASS_MASK) >> INDMASS_SHIFT; // )
}
void set_P(natq& descrittore, bool bitP)	//
{ // (
	if (bitP)
		descrittore |= BIT_P;
	else
		descrittore &= ~BIT_P; // )
}
void set_A(natq& descrittore, bool bitA)	//
{ // (
	if (bitA)
		descrittore |= BIT_A;
	else
		descrittore &= ~BIT_A; // )
}
void set_ZERO(natq& descrittore, bool bitZERO)
{
	if (bitZERO)
		descrittore |= BIT_ZERO;
	else
		descrittore &= ~BIT_ZERO;
}
// (* definiamo anche la seguente funzione:
//    clear_IND_M: azzera il campo M (indirizzo in memoria di massa)
void clear_IND_MASSA(natq& descrittore)
{
	descrittore &= ~INDMASS_MASK;
}
// *)
void  set_IND_FISICO(natq& descrittore, addr ind_fisico) //
{ // (
	clear_IND_MASSA(descrittore);
	descrittore |= ((natq)(ind_fisico) & ADDR_MASK); // )
}
void set_IND_MASSA(natq& descrittore, natq ind_massa) //
{ // (
	clear_IND_MASSA(descrittore);
	descrittore |= (ind_massa << INDMASS_SHIFT); // )
}

void set_D(natq& descrittore, bool bitD) //
{ // (
	if (bitD)
		descrittore |= BIT_D;
	else
		descrittore &= ~BIT_D; // )
}

int i_tab(addr ind_virt, int liv)
{
	int shift = 12 + (liv - 1) * 9;
	natq mask = 0x1ffUL << shift;
	return ((natq)ind_virt & mask) >> shift;
}
natq& get_entry(addr tab, natl index) //
{
	natq *pd = static_cast<natq*>(tab);
	return  pd[index];
}

void set_entry(addr tab, natl index, natq entry)
{
	natq *pd = static_cast<natq*>(tab);
	pd[index] = entry;
}

extern "C" des_proc* des_p(natl id);
natq& get_des(natl processo, int livello, addr ind_virt)
{
	des_proc *p = des_p(processo);
	addr tab = p->cr3;
	for (int i = 4; i > livello; i--) {
		natq e = get_entry(tab, i_tab(ind_virt, i));
		if (!extr_P(e))
			panic("P=0 non ammesso");
		tab = extr_IND_FISICO(e);
	}
	return get_entry(tab, i_tab(ind_virt, livello));
}


// ( [P_MEM_VIRT]

// carica un nuovo valore in cr3 [vedi sistema.S]
extern "C" void loadCR3(addr dir);

// restituisce il valore corrente di cr3 [vedi sistema.S]
extern "C" addr readCR3();

//invalida il TLB
extern "C" void invalida_TLB();

// mappa la memoria fisica in memoria virtuale, inclusa l'area PCI
// (copiamo la finestra gia' creata dal boot loader)
bool crea_finestra_FM(addr tab4)
{
	addr boot_dir = readCR3();
	copy_des(boot_dir, tab4, I_SIS_C, N_SIS_C);
	return true;
}

// )
const natl MAX_IRQ  = 24;
proc_elem *a_p[MAX_IRQ];  //
// )


// ( [P_IOAPIC]

void apic_fill()
{
	apic_set_VECT(0, VETT_0);
	apic_set_VECT(1, VETT_1);
	apic_set_VECT(2, VETT_2);
	apic_set_VECT(3, VETT_3);
	apic_set_VECT(4, VETT_4);
	apic_set_VECT(5, VETT_5);
	apic_set_VECT(6, VETT_6);
	apic_set_VECT(7, VETT_7);
	apic_set_VECT(8, VETT_8);
	apic_set_VECT(9, VETT_9);
	apic_set_VECT(10, VETT_10);
	apic_set_VECT(11, VETT_11);
	apic_set_VECT(12, VETT_12);
	apic_set_VECT(13, VETT_13);
	apic_set_VECT(14, VETT_14);
	apic_set_VECT(15, VETT_15);
	apic_set_VECT(16, VETT_16);
	apic_set_VECT(17, VETT_17);
	apic_set_VECT(18, VETT_18);
	apic_set_VECT(19, VETT_19);
	apic_set_VECT(20, VETT_20);
	apic_set_VECT(21, VETT_21);
	apic_set_VECT(22, VETT_22);
	apic_set_VECT(23, VETT_23);
}

// )

// timer
extern "C" void attiva_timer(natl count);
const natl DELAY = 59659;

//////////////////////////////////////////////////////////////////////////////
//							GDT												//
/////////////////////////////////////////////////////////////////////////////
extern "C" void init_gdt();

natq alloca_blocco();
des_pf* swap2(natl proc, int livello, addr ind_virt, bool residente);
bool crea(natl proc, addr ind_virt, int liv, natl priv)
{
	natq& dt = get_des(proc, liv + 1, ind_virt);
	bool bitP = extr_P(dt);
	if (!bitP) {
		natl blocco = extr_IND_MASSA(dt);
		if (!blocco) {
			if (! (blocco = alloca_blocco()) ) {
				flog(LOG_ERR, "swap pieno");
				return false;
			}
			set_IND_MASSA(dt, blocco);
			set_ZERO(dt, true);
			dt = dt | BIT_RW;
			if (priv == LIV_UTENTE) dt = dt | BIT_US;
		}
		if (liv > 0)
			swap2(proc, liv, ind_virt, (priv == LIV_SISTEMA));
	}
	return true;
}

bool crea_pagina(natl proc, addr ind_virt, natl priv)
{
	for (int i = 3; i >= 0; i--) {
		if (!crea(proc, ind_virt, i, priv))
			return false;
	}	
	return true;
}

bool crea_pila(natl proc, natb *bottom, natq size, natl priv)
{
	size = (size + (DIM_PAGINA - 1)) & ~(DIM_PAGINA - 1);

	for (natb* ind = bottom - size; ind != bottom; ind += DIM_PAGINA)
		if (!crea_pagina(proc, (addr)ind, priv))
			return false;
	return true;
}

addr carica_pila(natl proc, natb *bottom, natq size)
{
	des_pf *dp = 0;
	for (natb* ind = bottom - size; ind != bottom; ind += DIM_PAGINA)
		dp = swap2(proc, 0, ind, true);
	return (addr)((natq)indirizzo_pf(dp) + DIM_PAGINA);
}


addr crea_tab4()
{
	des_pf* ppf = alloca_pagina_fisica_libera();
	if (ppf == 0) {
		flog(LOG_ERR, "Impossibile allocare tab4");
		panic("errore");
	}
	ppf->livello = 4;
	ppf->residente = true;
	addr tab4 = indirizzo_pf(ppf);
	memset(tab4, 0, DIM_PAGINA);

	return tab4;
}


extern "C" natl alloca_tss(des_proc*);
extern "C" void rilascia_tss(int indice);
const natl BIT_IF = 1L << 9;

void crea_tab4(addr dest)
{
	addr pdir = readCR3();

	memset(dest, 0, DIM_PAGINA);

	copy_des(pdir, dest, I_SIS_C, N_SIS_C);
	copy_des(pdir, dest, I_MIO_C, N_MIO_C);
	copy_des(pdir, dest, I_UTN_C, N_UTN_C);
}

void rilascia_tutto(addr tab4, natl i, natl n);
proc_elem* crea_processo(void f(int), int a, int prio, char liv, bool IF)
{
	proc_elem	*p;			// proc_elem per il nuovo processo
	natl		identifier;		// indice del tss nella gdt 
	des_proc	*pdes_proc;		// descrittore di processo
	des_pf*		dpf_tab4;		// tab4 del processo
	addr		pila_sistema;
	

	// ( allocazione (e azzeramento preventivo) di un des_proc 
	//   (parte del punto 3 in)
	pdes_proc = static_cast<des_proc*>(alloca(sizeof(des_proc)));
	if (pdes_proc == 0) goto errore1;
	memset(pdes_proc, 0, sizeof(des_proc));
	// )

	// ( selezione di un identificatore (punto 1 in)
	identifier = alloca_tss(pdes_proc);
	if (identifier == 0) goto errore2;
	// )
	
	// ( allocazione e inizializzazione di un proc_elem
	//   (punto 3 in)
	p = static_cast<proc_elem*>(alloca(sizeof(proc_elem)));
        if (p == 0) goto errore3;
        p->id = identifier;
        p->precedenza = prio;
	p->puntatore = 0;
	// )

	// ( creazione della tab4 del processo (vedi
	dpf_tab4 = alloca_pagina_fisica(p->id, 4, 0);
	if (dpf_tab4 == 0) goto errore4;
	dpf_tab4->livello = 4;
	dpf_tab4->residente = true;
	pdes_proc->cr3 = indirizzo_pf(dpf_tab4);
	crea_tab4(pdes_proc->cr3);
	// )

	// ( creazione della pila sistema .
	if (!crea_pila(p->id, (natb*)fin_sis_p, DIM_SYS_STACK, LIV_SISTEMA))
		goto errore5;
	pila_sistema = carica_pila(p->id, (natb*)fin_sis_p, DIM_SYS_STACK);
	if (pila_sistema == 0)
		goto errore6;
	// )

	if (liv == LIV_UTENTE) {
		// ( inizializziamo la pila sistema.
		natq* pl = static_cast<natq*>(pila_sistema);

		pl[-5] = (natq)f;		// EIP (codice utente)
		pl[-4] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pl[-3] = (IF? BIT_IF : 0);	// EFLAG
		pl[-2] = (natq)fin_utn_p;
		pl[-1] = SEL_DATI_UTENTE;	// SS (pila utente)
		//   eseguendo una IRET da questa situazione, il processo
		//   passera' ad eseguire la prima istruzione della funzione f,
		//   usando come pila la pila utente (al suo indirizzo virtuale)
		// )

		// ( creazione della pila utente
		crea_pila(p->id, (natb*)fin_utn_p, DIM_USR_STACK, LIV_UTENTE);
		// )

		// ( infine, inizializziamo il descrittore di processo
		//   (punto 3 in)
		//   indirizzo del bottom della pila sistema, che verra' usato
		//   dal meccanismo delle interruzioni
		pdes_proc->punt_nucleo = fin_sis_p;

		//   inizialmente, il processo si trova a livello sistema, come
		//   se avesse eseguito una istruzione INT, con la pila sistema
		//   che contiene le 5 parole lunghe preparate precedentemente
		pdes_proc->contesto[I_RSP] = (natq)fin_sis_p - 5 * sizeof(natq);
		
		pdes_proc->contesto[I_RDI] = a;
		//pdes_proc->contesto[I_FPU_CR] = 0x037f;
		//pdes_proc->contesto[I_FPU_TR] = 0xffff;
		pdes_proc->cpl = LIV_UTENTE;
		//   tutti gli altri campi valgono 0
		// )
	} else {
		// ( inizializzazione delle pila sistema
		natq* pl = static_cast<natq*>(pila_sistema);
		pl[-5] = (natq)f;	  	// EIP (codice sistema)
		pl[-4] = SEL_CODICE_SISTEMA;   // CS (codice sistema)
		pl[-3] = (IF? BIT_IF : 0);  	// EFLAG
		pl[-2] = (natq)fin_sis_p - sizeof(natq);
		pl[-1] = 0;	// SS 
		//   i processi esterni lavorano esclusivamente a livello
		//   sistema. Per questo motivo, prepariamo una sola pila (la
		//   pila sistema)
		// )

		// ( inizializziamo il descrittore di processo
		//   (punto 3 in)
		pdes_proc->contesto[I_RSP] = (natq)fin_sis_p - 5 * sizeof(natq);
		pdes_proc->contesto[I_RDI] = a;

		//pdes_proc->contesto[I_FPU_CR] = 0x037f;
		//pdes_proc->contesto[I_FPU_TR] = 0xffff;
		pdes_proc->cpl = LIV_SISTEMA;
		//   tutti gli altri campi valgono 0
		// )
	}

	return p;

errore6:	rilascia_tutto(indirizzo_pf(dpf_tab4), I_SIS_P, N_SIS_P);
errore5:	rilascia_pagina_fisica(dpf_tab4);
errore4:	dealloca(p);
errore3:	rilascia_tss(identifier);
errore2:	dealloca(pdes_proc);
errore1:	return 0;
}

// parte "C++" della activate_p, descritta in
extern "C" natl
c_activate_p(void f(int), int a, natl prio, natl liv)
{
	proc_elem *p;			// proc_elem per il nuovo processo
	natl id = 0xFFFFFFFF;		// id da restituire in caso di fallimento

	// (* non possiamo accettare una priorita' minore di quella di dummy
	//    o maggiore di quella del processo chiamante
	if (prio < MIN_PRIORITY || prio > esecuzione->precedenza) {
		flog(LOG_WARN, "priorita' non valida: %d", prio);
		abort_p();
	}
	// *)
	
	// (* controlliamo che 'liv' contenga un valore ammesso 
	//    [segnalazione di E. D'Urso]
	if (liv != LIV_UTENTE && liv != LIV_SISTEMA) {
		flog(LOG_WARN, "livello non valido: %d", liv);
		abort_p();
	}
	// *)

	if (liv == LIV_SISTEMA && des_p(esecuzione->id)->cpl == LIV_UTENTE) {
		flog(LOG_WARN, "errore di protezione");
		abort_p();
	}

	// (* accorpiamo le parti comuni tra c_activate_p e c_activate_pe
	// nella funzione ausiliare crea_processo
	// (questa svolge, tra l'altro, i punti 1-3 in)
	p = crea_processo(f, a, prio, liv, true);
	// *)

	if (p != 0) {
		inserimento_lista(pronti, p);	// punto 4 in
		processi++;			//
		id = p->id;			// id del processo creato
						// (allocato da crea_processo)
		flog(LOG_INFO, "proc=%d entry=%p(%d) prio=%d liv=%d", id, f, a, prio, liv);
	}

	return id;
}

void rilascia_tutto(addr tab4, natl i, natl n);
void dealloca_blocco(natl blocco);
// rilascia tutte le strutture dati private associate al processo puntato da 
// "p" (tranne il proc_elem puntato da "p" stesso)
void distruggi_processo(proc_elem* p)
{
	des_proc* pdes_proc = des_p(p->id);

	addr tab4 = pdes_proc->cr3;
	rilascia_tutto(tab4, I_SIS_P, N_SIS_P);
	rilascia_tutto(tab4, I_UTN_P, N_UTN_P);
	rilascia_pagina_fisica(descrittore_pf(tab4));
	rilascia_tss(p->id);
	dealloca(pdes_proc);
}

// rilascia ntab tabelle (con tutte le pagine da esse puntate) a partire da 
// quella puntata dal descrittore i-esimo di tab4.
void rilascia_ric(addr tab, int liv, natl i, natl n)
{
	for (natl j = i; j < i + n && j < 512; j++) {
		natq dt = get_entry(tab, j);
		if (extr_P(dt)) {
			addr sub = extr_IND_FISICO(dt);
			if (liv > 1)
				rilascia_ric(sub, liv - 1, 0, 512);
			rilascia_pagina_fisica(descrittore_pf(sub));
		} else {
			natl blocco = extr_IND_MASSA(dt);
			dealloca_blocco(blocco);
		}
	}
}

void rilascia_tutto(addr tab4, natl i, natl n)
{
	rilascia_ric(tab4, 4, i, n);
}

//
// parte "C++" della terminate_p, descritta in
extern "C" void c_terminate_p()
{
	// il processo che deve terminare e' quello che ha invocato
	// la terminate_p, quindi e' in esecuzione
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;			//
	flog(LOG_INFO, "Processo %d terminato", p->id);
	dealloca(p);
	schedulatore();			//
}

// come la terminate_p, ma invia anche un warning al log (da invocare quando si 
// vuole terminare un processo segnalando che c'e' stato un errore)
extern "C" void c_abort_p()
{
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;
	flog(LOG_WARN, "Processo %d abortito", p->id);
	dealloca(p);
	schedulatore();
}
// )

// driver del timer
extern "C" void c_driver_td(void)
{
	richiesta *p;

	if(p_sospesi != 0)
	{
		p_sospesi->d_attesa--;
	}

	while(p_sospesi != 0 && p_sospesi->d_attesa == 0) {
		inserimento_lista(pronti, p_sospesi->pp);
		p = p_sospesi;
		p_sospesi = p_sospesi->p_rich;
		dealloca(p);
	}

	inspronti();
	schedulatore();
}

void scrivi_swap(addr src, natl blocco);
void leggi_swap(addr dest, natl blocco);


void carica(des_pf* ppf) //
{
	natq& e = get_des(ppf->processo, ppf->livello + 1, ppf->ind_virtuale);
	if (extr_ZERO(e)) {
		memset(indirizzo_pf(ppf), 0, DIM_PAGINA);
		set_ZERO(e, false);
	} else {
		leggi_swap(indirizzo_pf(ppf), ppf->ind_massa);
	}
}

void scarica(des_pf* ppf) //
{
	scrivi_swap(indirizzo_pf(ppf), ppf->ind_massa);
}

void collega(des_pf *ppf)	//
{
	natq& e = get_des(ppf->processo, ppf->livello + 1, ppf->ind_virtuale);
	set_IND_FISICO(e, indirizzo_pf(ppf));
	set_P(e, true);
	set_D(e, false);
	set_A(e, false);
}

extern "C" void invalida_entrata_TLB(addr ind_virtuale); //
bool scollega(des_pf* ppf)	//
{
	bool bitD;
	natq& e = get_des(ppf->processo, ppf->livello + 1, ppf->ind_virtuale);
	bitD = extr_D(e);
	bool occorre_salvare = bitD || ppf->livello > 0;
	set_IND_MASSA(e, ppf->ind_massa);
	set_P(e, false);
	invalida_entrata_TLB(ppf->ind_virtuale);
	return occorre_salvare;	//
}

void swap(int liv, addr ind_virt); //
void c_routine_pf()	//
{
	addr ind_virt = readCR2();

	for (int i = 3; i >= 0; i--) {
		natq d = get_des(esecuzione->id, i + 1, ind_virt);
		bool bitP = extr_P(d);
		if (!bitP)
			swap(i, ind_virt);
	}
}

void swap(int liv, addr ind_virt)
{
	// "ind_virt" e' l'indirizzo virtuale non tradotto
	// carica una tabella delle pagine o una pagina
	des_pf* nuovo_dpf = alloca_pagina_fisica_libera();
	if (nuovo_dpf == 0) {
		des_pf* dpf_vittima =
			scegli_vittima(esecuzione->id, liv, ind_virt);
		bool occorre_salvare = scollega(dpf_vittima);
		if (occorre_salvare)
			scarica(dpf_vittima);
		nuovo_dpf = dpf_vittima;
	}
	natq des = get_des(esecuzione->id, liv + 1, ind_virt);
	natl IM = extr_IND_MASSA(des);
	// (* non tutto lo spazio virtuale e' disponibile
	if (!IM) {
		flog(LOG_WARN, "indirizzo %p fuori dallo spazio virtuale allocato",
				ind_virt);
		rilascia_pagina_fisica(nuovo_dpf);
		abort_p();
	}
	// *)
	nuovo_dpf->livello = liv;
	nuovo_dpf->residente = false;
	nuovo_dpf->processo = esecuzione->id;
	nuovo_dpf->ind_virtuale = ind_virt;
	nuovo_dpf->ind_massa = IM;
	nuovo_dpf->contatore  = 0;
	carica(nuovo_dpf);
	collega(nuovo_dpf);
}

bool vietato(des_pf* ppf, natl proc, int liv, addr ind_virt)
{
	if (ppf->livello > liv && ppf->processo == proc &&
	    base(ppf->ind_virtuale, ppf->livello) == base(ind_virt, ppf->livello))
		return true;
	return false;
}

void stat();
des_pf* scegli_vittima(natl proc, int liv, addr ind_virtuale) //
{
	des_pf *ppf, *dpf_vittima;
	ppf = &dpf[0];
	while ( (ppf < &dpf[N_DPF] && ppf->residente) ||
			vietato(ppf, proc, liv, ind_virtuale))
		ppf++;
	if (ppf == &dpf[N_DPF]) return 0;
	dpf_vittima = ppf;
	stat();
	for (ppf++; ppf < &dpf[N_DPF]; ppf++) {
		if (ppf->residente || vietato(ppf, proc, liv, ind_virtuale))
			continue;
		if (ppf->contatore < dpf_vittima->contatore ||
		    (ppf->contatore == dpf_vittima->contatore && dpf_vittima->livello > ppf->livello))
			dpf_vittima = ppf;
	}
	return dpf_vittima;
}

void stat()
{
	des_pf *ppf1, *ppf2;
	addr ff1, ff2;
	bool bitA;

	for (natq i = 0; i < N_DPF; i++) {
		ppf1 = &dpf[i];
		if (ppf1->livello < 1)
			continue;
		ff1 = indirizzo_pf(ppf1);
		for (int j = 0; j < 512; j++) {
			natq& des = get_entry(ff1, j);
			if (!extr_P(des))
				continue;
			bitA = extr_A(des);
			set_A(des, false);
			ff2 = extr_IND_FISICO(des);
			ppf2 = descrittore_pf(ff2);
			if (!ppf2 || ppf2->residente)
				continue;
			ppf2->contatore >>= 1;
			if (bitA)
				ppf2->contatore |= 0x80000000;
		}
	}
	invalida_TLB();
}

des_pf* swap2(natl proc, int livello, addr ind_virt, bool residente)
{
	des_pf* ppf = alloca_pagina_fisica(proc, livello, ind_virt);
	if (!ppf)
		return 0;
	natq e = get_des(proc, livello + 1, ind_virt);
	natq m = extr_IND_MASSA(e);
	ppf->livello = livello;
	ppf->residente = residente;
	ppf->processo = proc;
	ppf->ind_virtuale = ind_virt;
	ppf->ind_massa = m;
	ppf->contatore = 0;
	carica(ppf);
	collega(ppf);
	return ppf;
}

bool carica_ric(natl proc, addr tab, int liv, addr ind, natl n)
{
	natq dp = dim_pag(liv);

	natl i = i_tab(ind, liv);
	for (natl j = i; j < i + n; j++, ind = (addr)((natq)ind + dp)) {
		natq e = get_entry(tab, j);
		if (!extr_IND_MASSA(e))
			continue;
		des_pf *ppf = swap2(proc, liv - 1, ind, true);
		if (!ppf)
			return false;
		if (liv > 1 && !carica_ric(proc, indirizzo_pf(ppf), liv - 1, ind, 512))
			return false;
	}
	return true;
}

bool carica_tutto(natl proc, natl i, natl n)
{
	des_proc *p = des_p(proc);

	return carica_ric(proc, p->cr3, 4, norm((addr)(i * dim_pag(4))), n);
}



// super blocco (vedi e [P_SWAP] avanti)
struct superblock_t {
	char	magic[8];
	natq	bm_start;
	natq	blocks;
	natq	directory;
	void	(*user_entry)(int);
	natq	user_end;
	void	(*io_entry)(int);
	natq	io_end;
	int	checksum;
};

// descrittore di swap (vedi [P_SWAP] avanti)
struct des_swap {
	natl *free;		// bitmap dei blocchi liberi
	superblock_t sb;	// contenuto del superblocco 
} swap_dev; 	// c'e' un unico oggetto swap
bool swap_init();

bool crea_spazio_condiviso(natl dummy_proc)
{
	
	// ( lettura del direttorio principale dallo swap
	flog(LOG_INFO, "lettura del direttorio principale...");
	addr tmp = alloca(DIM_PAGINA);
	if (tmp == 0) {
		flog(LOG_ERR, "memoria insufficiente");
		return false;
	}
	leggi_swap(tmp, swap_dev.sb.directory);
	// )

	// (  carichiamo le parti condivise nello spazio di indirizzamento del processo
	//    dummy 
	addr dummy_dir = des_p(dummy_proc)->cr3;
	copy_des(tmp, dummy_dir, I_MIO_C, N_MIO_C);
	copy_des(tmp, dummy_dir, I_UTN_C, N_UTN_C);
	dealloca(tmp);
	
	if (!carica_tutto(dummy_proc, I_MIO_C, 1))
		return false;
	if (!carica_tutto(dummy_proc, I_UTN_C, 1))
		return false;
	// )

	// ( copiamo i descrittori relativi allo spazio condiviso anche nel direttorio
	//   corrente, in modo che vengano ereditati dai processi che creeremo in seguito
	addr my_dir = des_p(esecuzione->id)->cr3;
	copy_des(dummy_dir, my_dir, I_MIO_C, N_MIO_C);
	copy_des(dummy_dir, my_dir, I_UTN_C, N_UTN_C);
	// )

	invalida_TLB();
	return true;
}

proc_elem init;

// creazione del processo dummy iniziale (usata in fase di inizializzazione del sistema)
extern "C" void end_program();	//
// corpo del processo dummy	//
void dd(int i)
{
	while (processi != 1)
		;
	end_program();
}

natl crea_dummy()
{
	proc_elem* di = crea_processo(dd, 0, DUMMY_PRIORITY, LIV_SISTEMA, true);
	if (di == 0) {
		flog(LOG_ERR, "Impossibile creare il processo dummy");
		return 0xFFFFFFFF;
	}
	inserimento_lista(pronti, di);
	processi++;
	return di->id;
}
void main_sistema(int n);
bool crea_main_sistema(natl dummy_proc)
{
	proc_elem* m = crea_processo(main_sistema, (int)dummy_proc, MAX_PRIORITY, LIV_SISTEMA, false);
	if (m == 0) {
		flog(LOG_ERR, "Impossibile creare il processo main_sistema");
	}
	inserimento_lista(pronti, m);
	processi++;
	return true;
}

// ( [P_EXTERN_PROC]
// Registrazione processi esterni
proc_elem* const ESTERN_BUSY = (proc_elem*)1;
proc_elem *a_p_save[MAX_IRQ];
// primitiva di nucleo usata dal nucleo stesso
extern "C" void wfi();

// inizialmente, tutti gli interrupt esterni sono associati ad una istanza di 
// questo processo esterno generico, che si limita ad inviare un messaggio al 
// log ogni volta che viene attivato. Successivamente, la routine di 
// inizializzazione del modulo di I/O provvedera' a sostituire i processi 
// esterni generici con i processi esterni effettivi, per quelle periferiche 
// che il modulo di I/O e' in grado di gestire.
void estern_generico(int h)
{
	for (;;) {
		flog(LOG_WARN, "Interrupt %d non gestito", h);

		wfi();
	}
}

// associa il processo esterno puntato da "p" all'interrupt "irq".
// Fallisce se un processo esterno (non generico) era gia' stato associato a 
// quello stesso interrupt
bool aggiungi_pe(proc_elem *p, natb irq)
{
	if (irq >= MAX_IRQ || a_p_save[irq] == 0)
		return false;

	a_p[irq] = p;
	distruggi_processo(a_p_save[irq]);
	dealloca(a_p_save[irq]);
	a_p_save[irq] = 0;
	apic_set_MIRQ(irq, false);
	return true;

}

extern "C" natl c_activate_pe(void f(int), int a, natl prio, natl liv, natb type)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	if (prio < MIN_PRIORITY) {
		flog(LOG_WARN, "priorita' non valida: %d", prio);
		abort_p();
	}

	p = crea_processo(f, a, prio, liv, true);
	if (p == 0)
		goto error1;
		
	if (!aggiungi_pe(p, type) ) 
		goto error2;

	flog(LOG_INFO, "estern=%d entry=%p(%d) prio=%d liv=%d type=%d",
			p->id, f, a, prio, liv, type);

	return p->id;

error2:	distruggi_processo(p);
error1:	
	return 0xFFFFFFFF;
}

// init_pe viene chiamata in fase di inizializzazione, ed associa una istanza 
// di processo esterno generico ad ogni interrupt
bool init_pe()
{
	for (natl i = 0; i < MAX_IRQ; i++) {
		proc_elem* p = crea_processo(estern_generico, i, 1, LIV_SISTEMA, true);
		if (p == 0) {
			flog(LOG_ERR, "Impossibile creare i processi esterni generici");
			return false;
		}
		a_p_save[i] = a_p[i] = p;
	}
	aggiungi_pe(ESTERN_BUSY, 2);
	return true;
}
// )

extern "C" void c_panic(const char *msg)
{
	flog(LOG_WARN, "PANIC: %s", msg);
	end_program();
}

extern "C" addr c_trasforma(addr ind_virt)
{
	return 0;
}

extern "C" void salta_a_main();
extern "C" void cmain ()
{
	natl dummy_proc;

	// (* anche se il primo processo non e' completamente inizializzato,
	//    gli diamo un identificatore, in modo che compaia nei log
	init.id = 0;
	init.precedenza = MAX_PRIORITY;
	esecuzione = &init;
	// *)

	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v4.02");
	init_gdt();
	flog(LOG_INFO, "gdt inizializzata");

	// (* Assegna allo heap di sistema HEAP_SIZE byte nel secondo MiB
	heap_init((addr)HEAP_START, HEAP_SIZE);
	flog(LOG_INFO, "Heap di sistema: %x B @%x", HEAP_SIZE, HEAP_START);
	// *)

	// ( il resto della memoria e' per le pagine fisiche (parte M2, vedi)
	init_dpf();
	flog(LOG_INFO, "Pagine fisiche: %d", N_DPF);
	// )
	
	flog(LOG_INFO, "sis/cond [%p, %p)", ini_sis_c, fin_sis_c);
	flog(LOG_INFO, "sis/priv [%p, %p)", ini_sis_p, fin_sis_p);
	flog(LOG_INFO, "io /cond [%p, %p)", ini_mio_c, fin_mio_c);
	flog(LOG_INFO, "usr/cond [%p, %p)", ini_utn_c, fin_utn_c);
	flog(LOG_INFO, "usr/priv [%p, %p)", ini_utn_p, fin_utn_p);

	addr inittab4 = crea_tab4();

	if(!crea_finestra_FM(inittab4))
			goto error;
	loadCR3(inittab4);
	flog(LOG_INFO, "Caricato CR3");

	apic_init(); // in libce
	apic_reset(); // in libce
	apic_fill();
	flog(LOG_INFO, "APIC inizializzato");
	
	// ( inizializzazione dello swap, che comprende la lettura
	//   degli entry point di start_io e start_utente 
	if (!swap_init())
			goto error;
	flog(LOG_INFO, "sb: blocks = %d", swap_dev.sb.blocks);
	flog(LOG_INFO, "sb: user   = %p/%p",
			swap_dev.sb.user_entry,
			swap_dev.sb.user_end);
	flog(LOG_INFO, "sb: io     = %p/%p", 
			swap_dev.sb.io_entry,
			swap_dev.sb.io_end);
	// )
	//
	// ( creazione del processo dummy
	dummy_proc = crea_dummy();
	if (dummy_proc == 0xFFFFFFFF)
		goto error;
	flog(LOG_INFO, "Creato il processo dummy");
	// )

	// (* processi esterni generici
	if (!init_pe())
		goto error;
	flog(LOG_INFO, "Creati i processi esterni generici");
	// *)

	// ( creazione del processo main_sistema
	if (!crea_main_sistema(dummy_proc))
		goto error;
	flog(LOG_INFO, "Creato il processo main_sistema");
	// )

	// (* selezioniamo main_sistema
	schedulatore();
	// *)
	// ( esegue CALL carica_stato; IRET (, vedi "sistema.S")
	salta_a_main(); 
	// )

error:
	panic("Errore di inizializzazione");
}

void gdb_breakpoint() {}

void main_sistema(int n)
{
	natl sync_io;
	natl dummy_proc = (natl)n; 


	// ( caricamento delle tabelle e pagine residenti degli spazi condivisi ()
	flog(LOG_INFO, "creazione o lettura delle tabelle e pagine residenti condivise...");
	if (!crea_spazio_condiviso(dummy_proc))
		goto error;
 	// )
	
	gdb_breakpoint();

	// ( inizializzazione del modulo di io
	flog(LOG_INFO, "creazione del processo main I/O...");
	sync_io = sem_ini(0);
	if (sync_io == 0xFFFFFFFF) {
		flog(LOG_ERR, "Impossibile allocare il semaforo di sincr per l'IO");
		goto error;
	}
	if (activate_p(swap_dev.sb.io_entry, sync_io, MAX_PRIORITY, LIV_SISTEMA) == 0xFFFFFFFF) {
		flog(LOG_ERR, "impossibile creare il processo main I/O");
		goto error;
	}
	flog(LOG_INFO, "attendo inizializzazione modulo I/O...");
	sem_wait(sync_io);
	// )

	// ( creazione del processo start_utente
	flog(LOG_INFO, "creazione del processo start_utente...");
	if (activate_p(swap_dev.sb.user_entry, 0, MAX_PRIORITY, LIV_UTENTE) == 0xFFFFFFFF) {
		flog(LOG_ERR, "impossibile creare il processo main utente");
		goto error;
	}
	// )
	// (* attiviamo il timer
	attiva_timer(DELAY);
	flog(LOG_INFO, "attivato timer (DELAY=%d)", DELAY);
	// *)
	// ( terminazione
	flog(LOG_INFO, "passo il controllo al processo utente...");
	terminate_p();
	// )
error:
	panic("Errore di inizializzazione");
}

// ( [P_SWAP]
// lo swap e' descritto dalla struttura des_swap, che specifica il canale 
// (primario o secondario) il drive (primario o master) e il numero della 
// partizione che lo contiene. Inoltre, la struttura contiene una mappa di bit, 
// utilizzata per l'allocazione dei blocchi in cui lo swap e' suddiviso, e un 
// "super blocco".  Il contenuto del super blocco e' copiato, durante 
// l'inizializzazione del sistema, dal primo settore della partizione di swap, 
// e deve contenere le seguenti informazioni:
// - magic (un valore speciale che serve a riconoscere la partizione, per 
// evitare di usare come swap una partizione utilizzata per altri scopi)
// - bm_start: il primo blocco, nella partizione, che contiene la mappa di bit 
// (lo swap, inizialmente, avra' dei blocchi gia' occupati, corrispondenti alla 
// parte utente/condivisa dello spazio di indirizzamento dei processi da 
// creare: e' necessario, quindi, che lo swap stesso memorizzi una mappa di 
// bit, che servira' come punto di partenza per le allocazioni dei blocchi 
// successive)
// - blocks: il numero di blocchi contenuti nella partizione di swap (esclusi 
// quelli iniziali, contenenti il superblocco e la mappa di bit)
// - directory: l'indice del blocco che contiene il direttorio
// - l'indirizzo virtuale dell'entry point del programma contenuto nello swap 
// (l'indirizzo di main)
// - l'indirizzo virtuale successivo all'ultima istruzione del programma 
// contenuto nello swap
// - l'indirizzo virtuale dell'entry point del modulo io contenuto nello swap
// - l'indirizzo virtuale successivo all'ultimo byte occupato dal modulo io
// - checksum: somma dei valori precedenti (serve ad essere ragionevolmente 
// sicuri che quello che abbiamo letto dall'hard disk e' effettivamente un 
// superblocco di questo sistema, e che il superblocco stesso non e' stato 
// corrotto)
//

// usa l'istruzione macchina BSF (Bit Scan Forward) per trovare in modo
// efficiente il primo bit a 1 in v
extern "C" int trova_bit(natl v);
void scrivi_swap(addr src, natl blocco);
void leggi_swap(addr dest, natl blocco);

natl ceild(natl v, natl q)
{
	return v / q + (v % q != 0 ? 1 : 0);
}

natq alloca_blocco()
{
	natl i = 0;
	natq risu = 0;
	natq vecsize = ceild(swap_dev.sb.blocks, sizeof(natl) * 8);

	// saltiamo le parole lunghe che contengono solo zeri (blocchi tutti occupati)
	while (i < vecsize && swap_dev.free[i] == 0) i++;
	if (i < vecsize) {
		natl pos = __builtin_ffs(swap_dev.free[i]) - 1;
		swap_dev.free[i] &= ~(1UL << pos);
		risu = pos + sizeof(natl) * 8 * i;
	} 
	return risu;
}

void dealloca_blocco(natl blocco)
{
	if (blocco == 0)
		return;
	swap_dev.free[blocco / 32] |= (1UL << (blocco % 32));
}



// legge dallo swap il blocco il cui indice e' passato come primo parametro, 
// copiandone il contenuto a partire dall'indirizzo "dest"
void leggi_swap(addr dest, natl blocco)
{
	natl sector = blocco * 8;

	leggisett(sector, 8, static_cast<natw*>(dest));
}

void scrivi_swap(addr src, natl blocco)
{
	natl sector = blocco * 8;

	scrivisett(sector, 8, static_cast<natw*>(src));
}

// inizializzazione del descrittore di swap
natw read_buf[256];
bool swap_init()
{
	// lettura del superblocco
	flog(LOG_DEBUG, "lettura del superblocco dall'area di swap...");
	leggisett(1, 1, read_buf);

	swap_dev.sb = *reinterpret_cast<superblock_t*>(read_buf);

	// controlliamo che il superblocco contenga la firma di riconoscimento
	for (int i = 0; i < 8; i++)
		if (swap_dev.sb.magic[i] != "CE64SWAP"[i]) {
			flog(LOG_ERR, "Firma errata nel superblocco");
			return false;
		}

	// controlliamo il checksum
	int *w = (int*)&swap_dev.sb, sum = 0;
	for (natl i = 0; i < sizeof(swap_dev.sb) / sizeof(int); i++)
		sum += w[i];

	if (sum != 0) {
		flog(LOG_ERR, "Checksum errato nel superblocco");
		return false;
	}

	flog(LOG_DEBUG, "lettura della bitmap dei blocchi...");

	// calcoliamo la dimensione della mappa di bit in pagine/blocchi
	natl pages = ceild(swap_dev.sb.blocks, DIM_PAGINA * 8);

	// quindi allochiamo in memoria un buffer che possa contenerla
	swap_dev.free = static_cast<natl*>(alloca((pages * DIM_PAGINA)));
	if (swap_dev.free == 0) {
		flog(LOG_ERR, "Impossibile allocare la bitmap dei blocchi");
		return false;
	}
	// infine, leggiamo la mappa di bit dalla partizione di swap
	leggisett(swap_dev.sb.bm_start * 8, pages * 8, reinterpret_cast<natw*>(swap_dev.free));
	return true;
}
// )
// ( [P_SEM_ALLOC] 
// I semafori non vengono mai deallocati, quindi e' possibile allocarli
// sequenzialmente. Per far questo, e' sufficiente ricordare quanti ne
// abbiamo allocati 
natl sem_allocati = 0;
natl alloca_sem()
{
	natl i;

	if (sem_allocati >= MAX_SEM)
		return 0xFFFFFFFF;

	i = sem_allocati;
	sem_allocati++;
	return i;
}

// dal momento che i semafori non vengono mai deallocati,
// un semaforo e' valido se e solo se il suo indice e' inferiore
// al numero dei semafori allocati
bool sem_valido(natl sem)
{
	return sem < sem_allocati;
}

// parte "C++" della primitiva sem_ini
extern "C" natl c_sem_ini(int val)
{
	natl i = alloca_sem();

	if (i != 0xFFFFFFFF)
		array_dess[i].counter = val;

	return i;
}
// )
