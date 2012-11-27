// sistema.cpp
//
#include "mboot.h"		// *****
#include "costanti.h"		// *****

/////////////////////////////////////////////////////////////////////////////////
//                     PROCESSI [4]                                            //
/////////////////////////////////////////////////////////////////////////////////
#include "tipo.h"		// [4.4]

const int N_REG = 45;	// [4.6]

// descrittore di processo [4.6]
struct des_proc {	
	natl id;
	addr punt_nucleo;
	natl riservato;	
	// quattro parole lunghe a disposizione
	natl disp[4];				// (*)
	addr cr3;
	natl contesto[N_REG];
	natl cpl;
};

// (*) le quattro parole lunghe "a disposizione" 
//   servono a contenere gli indirizzi logici delle pile
//   per gli altri due livelli previsti nella segmentazione

// ( indici dei vari registri all'interno dell'array "contesto"
//   Nota: in fondo all'array c'e' lo spazio sufficiente
//   a contenere i 10 registri, da 10 byte ciascuno, della FPU
enum { I_EIP = 0, I_EFLAGS, I_EAX, I_ECX, I_EDX, I_EBX,
       I_ESP, I_EBP, I_ESI, I_EDI, I_ES, I_CS, I_SS,
       I_DS, I_FS, I_GS, I_LDT,  I_IOMAP,
       I_FPU_CR, I_FPU_SR, I_FPU_TR, I_FPU_IP,
       I_FPU_IS, I_FPU_OP, I_FPU_OS };
// )

// elemento di una coda di processi [4.6]
struct proc_elem {
	natl id;
	natl precedenza;
	proc_elem *puntatore;
};
extern proc_elem *esecuzione;	// [4.6]
extern proc_elem *pronti;	// [4.6]

extern "C" natl activate_p(void f(int), int a, natl prio, natl liv); // [4.6]
extern "C" void terminate_p();	// [4.6]
// - per la crezione/terminazione di un processo, si veda [P_PROCESS] piu' avanti
// - per la creazione del processo start_utente, si veda "main_sistema" avanti

extern volatile natl processi;		// [4.7]
extern "C" void end_program();	// [4.7]
// corpo del processo dummy	// [4.7]
void dd(int i)
{
	while (processi != 1)
		;
	end_program();
}

// - per salva_stato/carica_stato, si veda il file "sistema.S"

// [4.8]
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

	if (prevp == 0)
		p_lista = p_elem;
	else
		prevp->puntatore = p_elem;

	p_elem->puntatore = pp;
// )
}

// [4.8]
void rimozione_lista(proc_elem *&p_lista, proc_elem *&p_elem)
{
// ( estrazione dalla testa
	p_elem = p_lista;  	// 0 se la lista e' vuota

	if (p_lista)
		p_lista = p_lista->puntatore;
// )
}

// [4.8]
extern "C" void schedulatore(void)
{
// ( poiche' la lista e' gia' ordinata in base alla priorita',
//   e' sufficiente estrarre l'elemento in testa
	rimozione_lista(pronti, esecuzione);
// )
}

/////////////////////////////////////////////////////////////////////////////////
//                     SEMAFORI [4]                                            //
/////////////////////////////////////////////////////////////////////////////////

// descrittore di semaforo [4.11]
struct des_sem {
	int counter;
	proc_elem *pointer;
};

// vettore dei descrittori di semaforo [4.11]
extern des_sem array_dess[MAX_SEM];

// - per sem_ini, si veda [P_SEM_ALLOC] avanti

// ( per la gestione degli errori/debugging, useremo le seguenti funzioni:
//   flog: invia un messaggio formatto al log (si veda [P_LOG] per l'implementazione)
extern "C" void flog(log_sev, cstr fmt, ...);
//   abort_p: termina forzatamente un processo (vedi [P_PROCESS] avanti)
extern "C" void abort_p() __attribute__ (( noreturn ));
//   sem_valido: restituisce true se sem e' un semaforo effettivamente allocato
bool sem_valido(natl sem);
// )


// [4.11]
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

// [4.11]
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
		if ( (lavoro->precedenza) <= (esecuzione->precedenza) )
			inserimento_lista(pronti, lavoro);
		else { // preemption
			inserimento_lista(pronti, esecuzione);
			esecuzione = lavoro;
		}
	}
}

extern "C" natl sem_ini(int);		// [4.11]
extern "C" void sem_wait(natl);		// [4.11]
extern "C" void sem_signal(natl);	// [4.11]

/////////////////////////////////////////////////////////////////////////////////
//               MEMORIA DINAMICA [4]                                          //
/////////////////////////////////////////////////////////////////////////////////

void *alloca(natl dim);			// [4.14]
void dealloca(void* p);			// [4.14]
// (per l'implementazione, si veda [P_SYS_HEAP] avanti)

/////////////////////////////////////////////////////////////////////////////////
//                         TIMER [4][9]                                        //
/////////////////////////////////////////////////////////////////////////////////

// richiesta al timer [4.16]
struct richiesta {
	natl d_attesa;
	richiesta *p_rich;
	proc_elem *pp;
};

richiesta *p_sospesi; // [4.16]

void inserimento_lista_attesa(richiesta *p); // [4.16]
// parte "C++" della primitiva delay [4.16]
extern "C" void c_delay(natl n)
{
	richiesta *p;

	p = static_cast<richiesta*>(alloca(sizeof(richiesta)));
	p->d_attesa = n;	
	p->pp = esecuzione;

	inserimento_lista_attesa(p);
	schedulatore();
}

// inserisce P nella coda delle richieste al timer [4.16]
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

// driver del timer [4.16][9.6]
extern "C" void c_driver_td(void)
{
	richiesta *p;

	if(p_sospesi != 0)
		p_sospesi->d_attesa--;

	while(p_sospesi != 0 && p_sospesi->d_attesa == 0) {
		inserimento_lista(pronti, p_sospesi->pp);
		p = p_sospesi;
		p_sospesi = p_sospesi->p_rich;
		dealloca(p);
	}

	schedulatore();
}


/////////////////////////////////////////////////////////////////////////////////
//                         PAGINE FISICHE [6]                                  //
/////////////////////////////////////////////////////////////////////////////////


enum cont_pf { LIBERA, DIRETTORIO, TABELLA, PAGINA, TABELLA_FM }; // [6.3]
struct des_pf {			// [6.3]
	cont_pf	contenuto;	// uno dei valori precedenti
	union {
		struct { 
			bool	residente;	// pagina residente o meno
			natl	processo;	// identificatore di processo
			natl	ind_massa;	// indirizzo della pagina in memoria di massa
			addr	ind_virtuale;
			// indirizzo virtuale della pagina (ultimi 12 bit a 0)
			// o della prima pagina indirizzata da una tabella (ultimi 22 bit uguali a 0)
			natl	contatore;	// contatore per le statistiche
		} pt; 	// rilevante se "contenuto" vale TABELLA o PAGINA
		struct  { // informazioni relative a una pagina libera
			natl	prossima_libera;// indice del descrittore della prossima pagina libera
		} avl;	// rilevante se "contenuto" vale LIBERA
	};
};

des_pf* dpf;		// puntatore al vettore di descrittori di pagine fisiche [6.3]
natl pagine_libere;	// indice del descrittore della prima pagina libera [6.3]
addr prima_pf_utile;	// indirizzo fisico della prima pagina utile [6.3]
// (
natl NUM_DPF;		// numero di descrittori di pagine fisiche
// )

// [6.3]
natl indice_dpf(addr ind_fisico)
{
	return ((natl)ind_fisico - (natl)prima_pf_utile) / DIM_PAGINA;
}

// [6.3]
addr indirizzo_pf(natl indice)
{
	return (addr)((natl)prima_pf_utile + indice * DIM_PAGINA);
}

// ( inizializza i descrittori di pagina fisica (vedi [P_MEM_PHYS])
bool init_dpf();
// )

/////////////////////////////////////////////////////////////////////////////////
//                         PAGINAZIONE [6]                                     //
/////////////////////////////////////////////////////////////////////////////////

//   ( definiamo alcune costanti utili per la manipolazione dei descrittori
//     di pagina e di tabella. Assegneremo a tali descrittori il tipo "natl"
//     e li manipoleremo tramite maschere e operazioni sui bit.
const natl BIT_P    = 1U << 0; // il bit di presenza
const natl BIT_RW   = 1U << 1; // il bit di lettura/scrittura
const natl BIT_US   = 1U << 2; // il bit utente/sistema(*)
const natl BIT_PWT  = 1U << 3; // il bit Page Wright Through
const natl BIT_PCD  = 1U << 4; // il bit Page Cache Disable
const natl BIT_A    = 1U << 5; // il bit di accesso
const natl BIT_D    = 1U << 6; // il bit "dirty"

// (*) attenzione, la convenzione Intel e' diversa da quella
// illustrata nel libro: 0 = sistema, 1 = utente.

const natl ACCB_MASK  = 0x000000FF; // maschera per il byte di accesso
const natl ADDR_MASK  = 0xFFFFF000; // maschera per l'indirizzo
const natl INDMASS_MASK = 0xFFFFFFE0; // maschera per l'indirizzo in mem. di massa
const natl INDMASS_SHIFT = 5;	    // primo bit che contiene l'ind. in mem. di massa
// )

// per le implementazioni mancanti, vedi [P_PAGING] avanti
natl& get_destab(natl processo, addr ind_virt); // [6.3]
natl& get_despag(natl processo, addr ind_virt); // [6.3]
natl& get_des(addr iff, natl index);		// [6.3]
natl& get_desent(natl processo, cont_pf tipo, addr ind_virt); // [6.3]
addr  get_INDTAB(natl indice);			// [6.3]
bool  extr_P(natl descrittore)			// [6.3]
{ // (
	return (descrittore & BIT_P); // )
} 
bool extr_D(natl descrittore)			// [6.3]
{ // (
	return (descrittore & BIT_D); // )
}
bool extr_A(natl descrittore)			// [6.3]
{ // (
	return (descrittore & BIT_A); // )
}
addr extr_IND_F(natl descrittore)		// [6.3]
{ // (
	return (addr)(descrittore & ADDR_MASK); // )
}
natl extr_IND_M(natl descrittore)		// [6.3]
{ // (
	return (descrittore & INDMASS_MASK) >> INDMASS_SHIFT; // )
}
void set_P(natl& descrittore, bool bitP)	// [6.3]
{ // (
	if (bitP)
		descrittore |= BIT_P;
	else
		descrittore &= ~BIT_P; // )
}
void set_A(natl& descrittore, bool bitA)	// [6.3]
{ // (
	if (bitA)
		descrittore |= BIT_A;
	else
		descrittore &= ~BIT_A; // )
}
// (* definiamo anche la seguente funzione:
//    clear_IND_M: azzera il campo M (indirizzo in memoria di massa)
void clear_IND_M(natl& descrittore)
{
	descrittore &= ~INDMASS_MASK;
}
// *)
void  set_IND_F(natl& descrittore, addr ind_fisico) // [6.3]
{ // (
	clear_IND_M(descrittore);
	descrittore |= ((natl)(ind_fisico) & ADDR_MASK); // )
}
void set_IND_M(natl& descrittore, natl ind_massa) // [6.3]
{ // (
	clear_IND_M(descrittore);
	descrittore |= (ind_massa << INDMASS_SHIFT); // )
}

void set_D(natl& descrittore, bool bitD) // [6.3]
{ // (
	if (bitD)
		descrittore |= BIT_D;
	else
		descrittore &= ~BIT_D; // )
}

// (* useremo anche le seguenti funzioni (implementazioni mancanti in [P_PAGING])
// mset_des: copia "descrittore" nelle "n" entrate della tabella/direttorio puntata
//  da "dest" partendo dalla "i"-esima
void mset_des(addr dest, natl i, natl n, natl descrittore);
// copy_dir: funzione che copia (parte di) una/o tabella/direttorio
// ("src") in un altra/o ("dst"). Vengono copiati
// "n" descrittori a partire dall' "i"-esimo
void copy_des(addr src, addr dst, natl i, natl n);
// *)

// carica un nuovo valore in cr3 [vedi sistema.S]
extern "C" void loadCR3(addr dir);

// restituisce il valore corrente di cr3 [vedi sistema.S]
extern "C" addr readCR3();

// attiva la paginazione [vedi sistema.S]
extern "C" void attiva_paginazione();

/////////////////////////////////////////////////////////////////////////////////
//                    MEMORIA VIRTUALE [6][10]                                 //
/////////////////////////////////////////////////////////////////////////////////

// ( definiamo delle costanti per le varie parti in cui dividiamo la memoria virtuale
//   (vedi [6.1][10.1]).
//   Per semplicita' ragioniamo sempre in termini di intere "tabelle delle pagine". 
//   Ricordiamo che ogni tabella delle pagine mappa una porzione contigua dello spazio
//   di indirizzamento virtuale, grande 4MiB e allineata ai 4MiB ("macropagina").
//   Le costanti che iniziano con "ntab_" indicano il numero di tabelle delle pagine
//   dedicate alla parte di memoria virtuale corrispondente (si veda "include/costanti.h" per i valori)
const natl ntab_sistema_condiviso = NTAB_SIS_C;
const natl ntab_sistema_privato   = NTAB_SIS_P;
const natl ntab_io_condiviso	  = NTAB_MIO_C;
const natl ntab_pci_condiviso     = NTAB_PCI_C;
const natl ntab_utente_condiviso  = NTAB_USR_C;
const natl ntab_utente_privato    = NTAB_USR_P;

//   Le costanti che iniziano con "i_" contengono l'indice (all'interno dei direttori
//   di tutti i processi) della prima tabella corrispondente alla zona di memoria virtuale nominata
const natl i_sistema_condiviso = 0;
const natl i_sistema_privato   = i_sistema_condiviso + ntab_sistema_condiviso;
const natl i_io_condiviso      = i_sistema_privato   + ntab_sistema_privato;
const natl i_pci_condiviso     = i_io_condiviso      + ntab_io_condiviso;
const natl i_utente_condiviso  = i_pci_condiviso     + ntab_pci_condiviso;
const natl i_utente_privato    = i_utente_condiviso  + ntab_utente_condiviso;

//   Le costanti che iniziano con "dim_" contengono la dimensione della corrispondente
//   zona, in byte
const natl dim_sistema_condiviso = ntab_sistema_condiviso * DIM_MACROPAGINA;
const natl dim_sistema_privato   = ntab_sistema_privato   * DIM_MACROPAGINA;
const natl dim_io_condiviso	 = ntab_io_condiviso	  * DIM_MACROPAGINA;
const natl dim_utente_condiviso  = ntab_utente_condiviso  * DIM_MACROPAGINA;
const natl dim_utente_privato    = ntab_utente_privato    * DIM_MACROPAGINA;
const natl dim_pci_condiviso     = ntab_pci_condiviso     * DIM_MACROPAGINA;

//   Le costanti "inizio_" ("fine_") contengono l'indirizzo del primo byte che fa parte
//   (del primo byte che non fa piu' parte) della zona corrispondente
natb* const inizio_sistema_condiviso = 0x00000000;
natb* const fine_sistema_condiviso   = inizio_sistema_condiviso + dim_sistema_condiviso;
natb* const inizio_sistema_privato   = fine_sistema_condiviso;
natb* const fine_sistema_privato     = inizio_sistema_privato   + dim_sistema_privato;
natb* const inizio_io_condiviso      = fine_sistema_privato;
natb* const fine_io_condiviso	     = inizio_io_condiviso      + dim_io_condiviso;
natb* const inizio_pci_condiviso     = fine_io_condiviso;
natb* const fine_pci_condiviso       = inizio_pci_condiviso     + dim_pci_condiviso;
natb* const inizio_utente_condiviso  = fine_pci_condiviso;
natb* const fine_utente_condiviso    = inizio_utente_condiviso  + dim_utente_condiviso;
natb* const inizio_utente_privato    = fine_utente_condiviso;
natb* const fine_utente_privato      = inizio_utente_privato    + dim_utente_privato;
// )

// per l'inizializzazione, si veda [P_INIT] avanti

// (* in caso di errori fatali, useremo la segunte funzione, che blocca il sistema:
extern "C" void panic(cstr msg) __attribute__ (( noreturn ));
// implementazione in [P_PANIC]
// *)

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
	natl prot  : 1;
	natl write : 1;
	natl user  : 1;
	natl res   : 1;
	natl pad   : 28; // bit non significativi
};
// *)

// (* indirizzo del primo byte che non contiene codice di sistema (vedi "sistema.S")
extern "C" addr fine_codice_sistema; 
// *)

natl pf_mutex;			// [6.4]
extern "C" addr readCR2();	// [6.4]
addr swap(natl processo, addr ind_virt);  // [6.4]
// (* punti in cui possiamo accettare un page fault dal modulo sistema (vedi "sistema.S")
extern "C" addr possibile_pf1, possibile_pf2, possibile_pf3;
// *)
extern "C" void c_routine_pf(	// [6.4]
	// (* prevediamo dei parametri aggiuntivi:
		pf_error errore,	/* vedi sopra */
		addr eip		/* ind. dell'istruzione che ha causato il fault */
	// *)
	)
{
	addr risu;
	addr ind_virt_non_tradotto = readCR2();

	// (* il sistema non e' progettato per gestire page fault causati 
	//   dalle primitie di nucleo (vedi [6.5]), quindi, se cio' si e' verificato, 
	//   si tratta di un bug
	if (eip < fine_codice_sistema || errore.res == 1) {
		flog(LOG_WARN, "eip: %x, page fault a %x: %s, %s, %s, %s", eip, ind_virt_non_tradotto,
			errore.prot  ? "protezione"	: "pag/tab assente",
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema",
			errore.res   ? "bit riservato"	: "");
	}
	// *)
	
	// (* l'errore di protezione non puo' essere risolto: il processo ha 
	//    tentato di accedere ad indirizzi proibiti (cioe', allo spazio 
	//    sistema)
	if (errore.prot == 1) {
		flog(LOG_WARN, "errore di protezione: eip=%x, ind=%x, %s, %s", eip, ind_virt_non_tradotto,
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema");
		abort_p();
	}
	// *)

	risu = swap(esecuzione->id, ind_virt_non_tradotto);
	if (risu == 0) 
		// ( aborto del processo in esecuzione
		abort_p();
		// )
}

addr swap_ent(natl proc, cont_pf tipo, addr ind_virt); // [6.4]
addr swap(natl proc, addr ind_virt)	// [6.4][10.2]
{
	bool bitP;
	natl dt, dp;
	addr ind_fis_tab;

	dt = get_destab(proc, ind_virt);
	bitP = extr_P(dt);
	if (!bitP)
		ind_fis_tab = swap_ent(proc, TABELLA, ind_virt);
	else
		ind_fis_tab = extr_IND_F(dt);
	if (ind_fis_tab == 0)
		return 0;
	dp = get_despag(proc, ind_virt);
	bitP = extr_P(dp);
	if (!bitP)
		return swap_ent(proc, PAGINA, ind_virt);
	else
		return extr_IND_F(dp);

}

natl alloca_pagina_fisica_libera();	// [6.4]
void collega(natl indice);		// [6.4]
bool scollega(natl indice);		// [6.4]
void carica(natl indice);		// [6.4]
void scarica(natl indice);		// [6.4]
natl scegli_vittima(natl indice);	// [6.4]
addr get_parent(natl proc, cont_pf tipo, addr ind_virt); // [6.4]
void aggiusta_parent(natl indice);	// [6.4]

// [6.4]
addr swap_ent(natl proc, cont_pf tipo, addr ind_virt)
{
	// "ind_virt" e' l'indirizzo virtuale non tradotto
	// carica una tabella delle pagine o una pagina
	des_pf *ppf;
	natl nuovo_indice = alloca_pagina_fisica_libera();
	if (nuovo_indice == 0xFFFFFFFF) {
		// non puo' essere scelto come vittima l'indice del descrittore di pagina fisica
		// il cui indirizzo virtuale e' stato restituito da get_parent()
		addr escludi = get_parent(proc, tipo, ind_virt);
		natl indice_vittima = scegli_vittima(indice_dpf(escludi));
		if (indice_vittima == 0xFFFFFFFF)
			return 0;
		bool occorre_salvare = scollega(indice_vittima);
		if (occorre_salvare)
			scarica(indice_vittima);
		nuovo_indice = indice_vittima;
	}
	natl des = get_desent(proc, tipo, ind_virt);
	ppf = &dpf[nuovo_indice];
	ppf->contenuto = tipo;
	ppf->pt.residente = false;
	ppf->pt.processo = proc;
	ppf->pt.ind_virtuale = ind_virt;
	ppf->pt.ind_massa = extr_IND_M(des);
	ppf->pt.contatore  = 0x80000000;
	aggiusta_parent(nuovo_indice);
	carica(nuovo_indice);
	collega(nuovo_indice);
	return indirizzo_pf(nuovo_indice);
	// indice_vittima e nuovo_indice contengono lo stesso indice di descrittore di pagina fisica
}

addr get_parent(natl proc, cont_pf tipo, addr ind_virt) // [6.4]
{
// (
	switch (tipo) {
	case PAGINA:
		return extr_IND_F(get_destab(proc, ind_virt));
	case TABELLA:
	// (* per completezza consideriamo anche il caso DIRETTORIO
	case DIRETTORIO:
	// *)
		return (addr)0xFFFFFFFF;
	default:
		flog(LOG_WARN, "chiamata get_parent(%d, %d, %x)", proc, tipo, ind_virt);
		abort_p();
	}
// )
}

void aggiusta_parent(natl indice)	// [6.4]
{
	des_pf *ppf = &dpf[indice];
	if (ppf->contenuto == PAGINA) {
		// aggiornamento del contatore anche nel descrittore di pagina fisica
		// contenente la tabella delle pagine coinvolta
		natl dt = get_destab(ppf->pt.processo, ppf->pt.ind_virtuale);
		addr ind_fis_tab = extr_IND_F(dt);
		ppf = &dpf[indice_dpf(ind_fis_tab)];
		ppf->pt.contatore |= 0x80000000;
	}
}


natl alloca_pagina_fisica_libera()	// [6.4]
{
	natl p = pagine_libere;
	if (pagine_libere != 0xFFFFFFFF)
		pagine_libere = dpf[pagine_libere].avl.prossima_libera;
	return p;
}

// (* rende di nuovo libera la pagina fisica il cui descrittore di pagina fisica
//    ha per indice "i"
void rilascia_pagina_fisica(natl i)
{
	des_pf* p = &dpf[i];
	p->contenuto = LIBERA;
	p->avl.prossima_libera = pagine_libere;
	pagine_libere = i;
}
// *)


void collega(natl indice)	// [6.4]
{
	des_pf* ppf = &dpf[indice];
	natl& des = get_desent(ppf->pt.processo, ppf->contenuto, ppf->pt.ind_virtuale);
	set_IND_F(des, indirizzo_pf(indice));
	set_P(des, true);
	set_D(des, false);
	set_A(des, false);
}

extern "C" void invalida_entrata_TLB(addr ind_virtuale); // [6.4]
natl alloca_blocco();	// [10.5]
bool scollega(natl indice)	// [6.4][10.5]
{
	bool bitD;
	des_pf *ppf =&dpf[indice];
	natl &des = get_desent(ppf->pt.processo, ppf->contenuto, ppf->pt.ind_virtuale);
	bitD = extr_D(des);
	bool occorre_salvare = bitD || ppf->contenuto == TABELLA;
	// (* 
	if (occorre_salvare && ppf->pt.ind_massa == 0) {
		ppf->pt.ind_massa = alloca_blocco();
		if (ppf->pt.ind_massa == 0) {
			panic("spazio nello swap insufficiente");
		}
	}
	// *)
	set_IND_M(des, ppf->pt.ind_massa);
	set_P(des, false);
	invalida_entrata_TLB(ppf->pt.ind_virtuale);
	return occorre_salvare;	// [10.5]
}

// (* oltre ad allocare, vogliamo anche deallocare blocchi
//    (quando un processo termina, possiamo deallocare tutti blocchi che
//    contengono le sue pagine private)
void dealloca_blocco(natl blocco);
// *)
// (* non usiamo direttamente readhd_n/writehd_n, ma le seguenti funzioni,
//    che tengono conto anche delle partizioni del disco rigido
//    leggi_blocco: legge il blocco numero "blocco" dall'area di swap e lo copia
//    	a partire dall'indirizzo "dest"
void leggi_blocco(natl blocco, void* dest);
//    scrivi_blocco: copia il blocco in memoria puntato da "dest" nel blocco numero
//      "blocco" nell'area di swap
void scrivi_blocco(natl blocco, void* dest);
// (per le implementazioni si veda [P_SWAP] avanti)
// *)

void carica(natl indice) // [6.4][10.5]
{
	/* natb ERR non serve, perche' non usiamo direttamente readhd_n */
	addr dest = indirizzo_pf(indice);
	des_pf *ppf = &dpf[indice];
	switch (ppf->contenuto) {
	case PAGINA:
		if (ppf->pt.ind_massa == 0) {
			for (natl i = 0; i < DIM_PAGINA; i++)
				static_cast<natb*>(dest)[i] = 0;
		} else {
			leggi_blocco(ppf->pt.ind_massa, dest); /* vedi sopra */
		}
		break;
	case TABELLA:
		// (* gestiamo l'allocazione dinamica anche per le tabelle.
		//    Se ppf->pt.ind_massa e' zero allochiamo una nuova tabella.
		//    Tutte le entrate della nuova tabella hanno P=0, ppf->pt.ind_massa=0
		//    e i bit S/U, R/W, PCD e PWT copiati dall'entrata del direttorio.
		if (ppf->pt.ind_massa == 0) {
			/* costruiamo il descrittore che andra' copiato in tutte le entrate
			 * della nuova tabella:
			 *  - copiamo il descrittore nel direttorio (per avere
			 *   una copia dei bit S/U, R/W, PWT e PCD */
			natl ndes = get_desent(ppf->pt.processo, ppf->contenuto, ppf->pt.ind_virtuale);
			/* - azzeriamo il bit P nella copia */
			set_P(ndes, false);
			/* - azzeriamo l'indirizzo in memoria di massa */
			clear_IND_M(ndes);
			/* Copiamo il descrittore cosi' ottenuto in tutte le entrate */
			mset_des(dest, 0,  1024, ndes);
		} else {
			leggi_blocco(ppf->pt.ind_massa, dest); /* vedi sopra */
		}
		// *)
		break;
	// (* gestiamo anche il caso del direttorio. Un nuovo direttorio
	//    viene creato quando si crea un nuovo processo. In questo
	//    caso (si veda [10.3]), le parti condivise vengono copiate dalle corrispondenti
	//    parti del processo padre (il cui direttorio e' puntato da CR3),
	//    mentre le parti private vengono inizializzate nel seguente modo
	//    - parte sistema_privata: tutti i descrittori hanno P=0, ppf->pt.ind_massa=0,
	//      S/U=0 (sistema), R/W=1, PWT=PCD=0
	//    - parte utente_privata: tutti i descrittori hanno P=0, ppf->pt.ind_massa=0,
	//      S/U=1 (utente), R/W=1, PWT=PCD=0
	case DIRETTORIO:
		{ addr pdir = readCR3();
		  copy_des(pdir, dest, i_sistema_condiviso, ntab_sistema_condiviso);
		  mset_des(      dest, i_sistema_privato,   ntab_sistema_privato, BIT_RW);
		  copy_des(pdir, dest, i_io_condiviso,      ntab_io_condiviso);
		  copy_des(pdir, dest, i_pci_condiviso,     ntab_pci_condiviso);
		  copy_des(pdir, dest, i_utente_condiviso,  ntab_utente_condiviso);
		  mset_des(	 dest, i_utente_privato,    ntab_utente_privato, BIT_RW|BIT_US);
		}
		break;
	// *)
	default:
		abort_p();
	}
}

void scarica(natl indice) // [6.4]
{
	des_pf *ppf =&dpf[indice];
	scrivi_blocco(ppf->pt.ind_massa, indirizzo_pf(indice));
}

natl scegli_vittima(natl indice_vietato) // [6.4]
{
	// "indice_vietato" non puo' essere scelto come indice del descrittore
	// della pagina fisica vittma
	natl i, indice_vittima;
	des_pf *ppf, *pvittima;
	i = 0;
	while ( (i < NUM_DPF && dpf[i].pt.residente) || i == indice_vietato)
		i++;
	if (i >= NUM_DPF) return 0xFFFFFFFF;
	indice_vittima = i;
	for (i++; i < NUM_DPF; i++) {
		ppf = &dpf[i];
		pvittima = &dpf[indice_vittima];
		if (ppf->pt.residente || i == indice_vietato)
			continue;
		switch (ppf->contenuto) {
		case PAGINA:
			if (ppf->pt.contatore < pvittima->pt.contatore ||
			    (ppf->pt.contatore == pvittima->pt.contatore &&
			    		pvittima->contenuto == TABELLA))
				indice_vittima = i;
			break;
		case TABELLA:
			if (ppf->pt.contatore < pvittima->pt.contatore) 
				indice_vittima = i;
			break;
		default:
			break;
		}
	}
	return indice_vittima;
}

extern "C" void invalida_TLB(); // [6.6]
extern "C" void delay(natl t);

void routine_stat();		// [6.6]

void routine_stat()		// [6.6]
{
	des_pf *ppf1, *ppf2;
	addr ff1, ff2;
	bool bitA;

	for (natl i = 0; i < NUM_DPF; i++) {
		ppf1 = &dpf[i];
		switch (ppf1->contenuto) {
		case DIRETTORIO:
		case TABELLA:
			ff1 = indirizzo_pf(i);
			for (int j = 0; j < 1024; j++) {
				natl& des = get_des(ff1, j);
				if (extr_P(des)) {
					ff2 = extr_IND_F(des);
					bitA = extr_A(des);
					set_A(des, false);
					ppf2 = &dpf[indice_dpf(ff2)];
					ppf2->pt.contatore >>= 1;
					if (bitA)
						ppf2->pt.contatore |= 0x80000000;
				}
			}
			break;
		default:
			break;
		}
	}
	invalida_TLB();
}


natl proc_corrente()
{
	return esecuzione->id;
}


/////////////////////////////////////////////////////////////////////////////////
//                    PROCESSI ESTERNI [7]                                     //
/////////////////////////////////////////////////////////////////////////////////

// (
const natl MAX_IRQ  = 24;
// )
proc_elem *a_p[MAX_IRQ];  // [7.1]

// per la parte "C++" della activate_pe, si veda [P_EXTERN_PROC] avanti


/////////////////////////////////////////////////////////////////////////////////
//                    SUPPORTO PCI                                             //
/////////////////////////////////////////////////////////////////////////////////
const ioaddr PCI_CAP = 0x0CF8;
const ioaddr PCI_CDP = 0x0CFC;
const addr PCI_startmem = reinterpret_cast<addr>(0x00000000 - dim_pci_condiviso);

extern "C" void inputb(ioaddr reg, natb &a);	// [9.3.1]
extern "C" void outputb(natb a, ioaddr reg);	// [9.3.1]
// (*
extern "C" void inputw(ioaddr reg, natw &a);	
extern "C" void outputw(natw a, ioaddr reg);
extern "C" void inputl(ioaddr reg, natl &a);	
extern "C" void outputl(natl a, ioaddr reg);
// *)

natl make_CAP(natw w, natb off)
{
	return 0x80000000 | (w << 8) | (off & 0xFC);
}

natb pci_read_confb(natw w, natb off)
{
	natl l = make_CAP(w, off);
	outputl(l, PCI_CAP);
	natb ret;
	inputb(PCI_CDP + (off & 0x03), ret);
	return ret;
}

natw pci_read_confw(natw w, natb off)
{
	natl l = make_CAP(w, off);
	outputl(l, PCI_CAP);
	natw ret;
	inputw(PCI_CDP + (off & 0x03), ret);
	return ret;
}

natl pci_read_confl(natw w, natb off)
{
	natl l = make_CAP(w, off);
	outputl(l, PCI_CAP);
	natl ret;
	inputl(PCI_CDP, ret);
	return ret;
}

void pci_write_confb(natw w, natb off, natb data)
{
	natl l = make_CAP(w, off);
	outputl(l, PCI_CAP);
	outputb(data, PCI_CDP + (off & 0x03));
}

void pci_write_confw(natw w, natb off, natw data)
{
	natl l = make_CAP(w, off);
	outputl(l, PCI_CAP);
	outputw(data, PCI_CDP + (off & 0x03));
}

void pci_write_confl(natw w, natb off, natl data)
{
	natl l = make_CAP(w, off);
	outputl(l, PCI_CAP);
	outputl(data, PCI_CDP);
}

bool pci_find_dev(natw& w, natw devID, natw vendID)
{
	for( ; w != 0xFFFF; w++) {
		natw work;

		if ( (work = pci_read_confw(w, 0)) == 0xFFFF ) 
			continue;
		if ( work == vendID && pci_read_confw(w, 2) == devID) 
			return true;
	}
	return false;
}

bool pci_find_class(natw& w, natb code[])
{
	for ( ; w != 0xFFFF; w++) {
		if (pci_read_confw(w, 0) == 0xFFFF)
			continue;
		natb work[3];
		natl i;
		for (i = 0; i < 3; i++) {
			work[i] = pci_read_confb(w, 2 * 4 + i + 1);
			if (code[i] != 0xFF && code[i] != work[i])
				break;
		}
		if (i == 3) {
			for (i = 0; i < 3; i++)
				code[i] = work[i];
			return true;
		}
	} 
	return false;
}

extern "C" natl c_pci_find(natl code, natw i)
{
	natb* pcode = (natb*)&code;
	natw w, j = 0;
	for(w = 0; w != 0xFFFF; w++) {
		if (! pci_find_class(w, pcode)) 
			return 0xFFFFFFFF;
		if (j == i)
			return w;
		j++;
	} 
	return 0xFFFFFFFF;
}

extern "C" natl c_pci_read(natw l, natw regn, natl size)
{
	natl res;
	switch (size) {
	case 1:
		res = pci_read_confb(l, regn);
		break;
	case 2:
		res = pci_read_confw(l, regn);
		break;
	case 4:
		res = pci_read_confl(l, regn);
		break;
	default:
		flog(LOG_WARN, "pci_read(%x, %d, %d): parametro errato", l, regn, size);
		abort_p();
	}
	return res;
}

extern "C" void c_pci_write(natw l, natw regn, natl res, natl size)
{
	switch (size) {
	case 1:
		pci_write_confb(l, regn, res);
		break;
	case 2:
		pci_write_confw(l, regn, res);
		break;
	case 4:
		pci_write_confl(l, regn, res);
		break;
	default:
		flog(LOG_WARN, "pci_write(%x, %d, %d, %d): parametro errato", l, regn, res, size);
		abort_p();
	}
}

///////////////////////////////////////////////////////////////////////////////////
//                   INIZIALIZZAZIONE [10]                                       //
///////////////////////////////////////////////////////////////////////////////////
const natl MAX_PRIORITY	= 0xfffffff;
const natl MIN_PRIORITY	= 0x0000001;
const natl DUMMY_PRIORITY = 0x0000000;
const natl DEFAULT_HEAP_SIZE = 1024 * 1024;
const natl DELAY = 59659;

extern "C" void *memset(void* dest, int c, natl n);
// restituisce true se le due stringe first e second sono uguali
extern addr max_mem_lower;
extern addr max_mem_upper;
extern addr mem_upper;
extern proc_elem init;
natl heap_mem = DEFAULT_HEAP_SIZE;
int salta_a(addr indirizzo);
extern "C" void init_8259();
bool crea_finestra_FM(addr direttorio, addr max_mem);
bool crea_finestra_PCI(addr direttorio);
natl crea_dummy();
bool init_pe();
bool crea_main_sistema(natl dummy_proc);
extern "C" void salta_a_main();
extern "C" void c_panic(cstr msg, natl eip1, natw cs, natl eflags, natl eip2);
// timer
extern natl ticks;
extern natl clocks_per_usec;
extern "C" void attiva_timer(natl count);
void ini_COM1();
extern "C" void cmain (natl magic, multiboot_info_t* mbi)
{
	int indice;
	addr direttorio;
	natl dummy_proc;
	
	// (* anche se il primo processo non e' completamente inizializzato,
	//    gli diamo un identificatore, in modo che compaia nei log
	init.id = 0;
	init.precedenza = MAX_PRIORITY;
	esecuzione = &init;
	// *)
	
	ini_COM1();

	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v1.0");

	// (* controlliamo di essere stati caricati
	//    da un bootloader che rispetti lo standard multiboot
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		flog(LOG_ERR, "Numero magico non valido: 0x%x", magic);
		goto error;
	}
	// *)

	// (* vediamo se il boot loader ci ha passato l'informazione su quanta
	//    memoria fisica e' installata nel sistema
	if (mbi->flags & 1) {
		max_mem_lower = addr(mbi->mem_lower * 1024);
		max_mem_upper = addr(mbi->mem_upper * 1024 + 0x100000);
	} else {
		flog(LOG_WARN, "Quantita' di memoria sconosciuta, assumo 32 MiB");
		max_mem_lower = addr(639 * 1024);
		max_mem_upper = addr(32 * 1024 * 1024);
	}
	flog(LOG_INFO, "Memoria fisica: %d byte (%d MiB)", max_mem_upper, (natl)max_mem_upper >> 20 );
	// *)

	// ( per come abbiamo organizzato il sistema non possiamo gestire piu' di
	//   1GiB di memoria fisica (vedi [10.1])
	if (max_mem_upper > fine_sistema_condiviso) {
		max_mem_upper = fine_sistema_condiviso;
		flog(LOG_WARN, "verranno gestiti solo %d byte di memoria fisica", max_mem_upper);
	}
	// )
	
	// (* Assegna allo heap di sistema heap_mem byte a partire dalla fine del modulo sistema
	salta_a(static_cast<natb*>(mem_upper) + heap_mem);
	flog(LOG_INFO, "Heap di sistema: %d B", heap_mem);
	// *)

	// ( il resto della memoria e' per le pagine fisiche (parte M2, vedi [1.10])
	if (!init_dpf())
		goto error;
	flog(LOG_INFO, "Pagine fisiche: %d", NUM_DPF);
	// )

	// ( creiamo il direttorio "D" (vedi [10.4])
	indice = alloca_pagina_fisica_libera();
	if (indice == -1) {
		flog(LOG_ERR, "Impossibile allocare il direttorio principale");
		goto error;
	}
	direttorio = indirizzo_pf(indice);
	dpf[indice].contenuto = DIRETTORIO;
	dpf[indice].pt.residente = true;
	// azzeriamo il direttorio appena creato
	memset(direttorio, 0, DIM_PAGINA);
	// )

	// ( memoria fisica in memoria virtuale (vedi [1.8] e [10.4])
	if (!crea_finestra_FM(direttorio, max_mem_upper))
		goto error;
	flog(LOG_INFO, "Mappata memoria fisica in memoria virtuale");
	// )

	if (!crea_finestra_PCI(direttorio))
		goto error;
	flog(LOG_INFO, "Mappata memoria PCI in memoria virtuale");
	// ( inizializziamo il registro CR3 con l'indirizzo del direttorio ([10.4])
	loadCR3(direttorio);
	// )
	// ( attiviamo la paginazione ([10.4])
	attiva_paginazione();
	flog(LOG_INFO, "Paginazione attivata");
	// )

	// ( stampa informativa
	flog(LOG_INFO, "Semafori: %d", MAX_SEM);
	// )

	// (* inizializziamo il controllore delle interruzioni [vedi sistema.S]
	init_8259();
	flog(LOG_INFO, "Controllore delle interruzioni inizializzato");
	// *)
	
	// ( creazione del processo dummy [10.4]
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

	// ( creazione del processo main_sistema [10.4]
	if (!crea_main_sistema(dummy_proc))
		goto error;
	flog(LOG_INFO, "Creato il processo main_sistema");
	// )

	// (* selezioniamo main_sistema
	schedulatore();
	// *)
	// ( esegue CALL carica_stato; IRET ([10.4], vedi "sistema.S")
	salta_a_main(); 
	// )
	return;

error:
	c_panic("Errore di inizializzazione", 0, 0, 0, 0);
}

bool aggiungi_pe(proc_elem *p, natb irq);
bool crea_spazio_condiviso(natl dummy_proc, addr& last_address);
bool swap_init();

// super blocco (vedi [10.5] e [P_SWAP] avanti)
struct superblock_t {
	char	magic[4];
	natl	bm_start;
	natl	blocks;
	natl	directory;
	void	(*user_entry)(int);
	addr	user_end;
	void	(*io_entry)(int);
	addr	io_end;
	int	checksum;
};

// descrittore di swap (vedi [P_SWAP] avanti)
struct des_swap {
	natl *free;		// bitmap dei blocchi liberi
	superblock_t sb;	// contenuto del superblocco 
} swap_dev; 	// c'e' un unico oggetto swap

extern "C" des_proc* des_p(natl id);
//
// (* restituisce il minimo naturale maggiore o uguale a v/q
natl ceild(natl v, natl q)
{
	return v / q + (v % q != 0 ? 1 : 0);
}
// *)

void main_sistema(int n)
{
	natl sync_io;
	natl dummy_proc = (natl)n; 

	// ( inizializzazione dello swap, che comprende la lettura
	//   degli entry point di start_io e start_utente (vedi [10.4])
	if (!swap_init())
			goto error;
	flog(LOG_INFO, "sb: blocks=%d user=%x/%x io=%x/%x", 
			swap_dev.sb.blocks,
			swap_dev.sb.user_entry,
			swap_dev.sb.user_end,
			swap_dev.sb.io_entry,
			swap_dev.sb.io_end);
	// )

	// ( caricamento delle tabelle e pagine residenti degli spazi condivisi ([10.4])
	flog(LOG_INFO, "creazione o lettura delle tabelle e pagine residenti condivise...");
	addr last_address;
	if (!crea_spazio_condiviso(dummy_proc, last_address))
		goto error;
 	// )

	// ( inizializzazione del modulo di io [7.1][10.4]
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
	// *)
	// ( terminazione [10.4]
	terminate_p();
	// )
error:
	panic("Errore di inizializzazione");
}


/////////////////////////////////////////////////////////////////////////////////////
//                 Implementazioni                                                 //
/////////////////////////////////////////////////////////////////////////////////////
// ( [P_LIB]
// non possiamo usare l'implementazione della libreria del C/C++ fornita con DJGPP o gcc,
// perche' queste sono state scritte utilizzando le primitive del sistema
// operativo Windows o Unix. Tali primitive non
// saranno disponibili quando il nostro nucleo andra' in esecuzione.
// Per ragioni di convenienza, ridefiniamo delle funzioni analoghe a quelle
// fornite dalla libreria del C.
//
// copia n byte da src a dest
extern "C" void *memcpy(str dest, cstr src, natl n)
{
	char       *dest_ptr = static_cast<char*>(dest);
	const char *src_ptr  = static_cast<const char*>(src);

	for (natl i = 0; i < n; i++)
		dest_ptr[i] = src_ptr[i];

	return dest;
}

// scrive n byte pari a c, a partire da dest
extern "C" void *memset(str dest, int c, natl n)
{
	char *dest_ptr = static_cast<char*>(dest);

        for (natl i = 0; i < n; i++)
              dest_ptr[i] = static_cast<char>(c);

        return dest;
}


// Versione semplificata delle macro per manipolare le liste di parametri
//  di lunghezza variabile; funziona solo se gli argomenti sono di
//  dimensione multipla di 4, ma e' sufficiente per le esigenze di printk.
//
typedef char *va_list;
#define va_start(ap, last_req) (ap = (char *)&(last_req) + sizeof(last_req))
#define va_arg(ap, type) ((ap) += sizeof(type), *(type *)((ap) - sizeof(type)))
#define va_end(ap)


// restituisce la lunghezza della stringa s (che deve essere terminata da 0)
natl strlen(cstr s)
{
	natl l = 0;
	const natb* ss = static_cast<const natb*>(s);

	while (*ss++)
		++l;

	return l;
}

// copia al piu' l caratteri dalla stringa src alla stringa dest
natb *strncpy(str dest, cstr src, natl l)
{
	natb* bdest = static_cast<natb*>(dest);
	const natb* bsrc = static_cast<const natb*>(src);

	for (natl i = 0; i < l && bsrc[i]; ++i)
		bdest[i] = bsrc[i];

	return bdest;
}

static const char hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

// converte l in stringa (notazione esadecimale)
static void htostr(str vbuf, natl l)
{
	char *buf = static_cast<char*>(vbuf);
	for (int i = 7; i >= 0; --i) {
		buf[i] = hex_map[l % 16];
		l /= 16;
	}
}

// converte l in stringa (notazione decimale)
static void itostr(str vbuf, natl len, long l)
{
	natl i, div = 1000000000, v, w = 0;
	char *buf = static_cast<char*>(vbuf);

	if (l == (-2147483647 - 1)) {
 		strncpy(buf, "-2147483648", 12);
 		return;
   	} else if (l < 0) {
		buf[0] = '-';
		l = -l;
		i = 1;
	} else if (l == 0) {
		buf[0] = '0';
		buf[1] = 0;
		return;
	} else
		i = 0;

	while (i < len - 1 && div != 0) {
		if ((v = l / div) || w) {
			buf[i++] = '0' + (char)v;
			w = 1;
		}

		l %= div;
		div /= 10;
	}

	buf[i] = 0;
}

// converte la stringa buf in intero
int strtoi(char* buf)
{
	int v = 0;
	while (*buf >= '0' && *buf <= '9') {
		v *= 10;
		v += *buf - '0';
		buf++;
	}
	return v;
}

#define DEC_BUFSIZE 12

// stampa formattata su stringa
int vsnprintf(str vstr, natl size, cstr vfmt, va_list ap)
{
	natl in = 0, out = 0, tmp;
	char *aux, buf[DEC_BUFSIZE];
	char *str = static_cast<char*>(vstr);
	const char* fmt = static_cast<const char*>(vfmt);
	natl cifre;

	while (out < size - 1 && fmt[in]) {
		switch(fmt[in]) {
			case '%':
				cifre = 8;
			again:
				switch(fmt[++in]) {
					case '1':
					case '2':
					case '4':
					case '8':
						cifre = fmt[in] - '0';
						goto again;
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
						if(out > size - (cifre + 1))
							goto end;
						htostr(&str[out], tmp);
						out += 8;
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
extern "C" int snprintf(str buf, natl n, cstr fmt, ...)
{
	va_list ap;
	int l;
	va_start(ap, fmt);
	l = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return l;
}

// restituisce il valore di "v" allineato a un multiplo di "a"
natl allinea(natl v, natl a)
{
	return (v % a == 0 ? v : ((v + a - 1) / a) * a);
}
addr allineav(addr v, natl a)
{
	return (addr)allinea((natl)v, a);
}
// )

// ( [P_SEM_ALLOC] (vedi [4.11])
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

// parte "C++" della primitiva sem_ini [4.11]
extern "C" natl c_sem_ini(int val)
{
	natl i = alloca_sem();

	if (i != 0xFFFFFFFF)
		array_dess[i].counter = val;

	return i;
}
// )

// ( [P_SYS_HEAP] (vedi [4.14])
//
// Il nucleo ha bisogno di una zona di memoria gestita ad heap, per realizzare
// la funzione "alloca".
// Lo heap e' composto da zone di memoria libere e occupate. Ogni zona di memoria
// e' identificata dal suo indirizzo di partenza e dalla sua dimensione.
// Ogni zona di memoria contiene, nei primi byte, un descrittore di zona di
// memoria (di tipo des_mem), con un campo "dimensione" (dimensione in byte
// della zona, escluso il descrittore stesso) e un campo "next", significativo
// solo se la zona e' libera, nel qual caso il campo punta alla prossima zona
// libera. Si viene quindi a creare una lista delle zone libere, la cui testa
// e' puntata dalla variabile "memlibera" (allocata staticamente). La lista e'
// mantenuta ordinata in base agli indirizzi di partenza delle zone libere.

// descrittore di zona di memoria: descrive una zona di memoria nello heap di
// sistema
struct des_mem {
	natl dimensione;
	des_mem* next;
};

// testa della lista di descrittori di memoria fisica libera
des_mem* memlibera = 0;

// funzione di allocazione: cerca la prima zona di memoria libera di dimensione
// almeno pari a "quanti" byte, e ne restituisce l'indirizzo di partenza.
// Se la zona trovata e' sufficientemente piu' grande di "quanti" byte, divide la zona
// in due: una, grande "quanti" byte, viene occupata, mentre i rimanenti byte vanno
// a formare una nuova zona (con un nuovo descrittore) libera.
void* alloca(natl dim)
{
	// per motivi di efficienza, conviene allocare sempre multipli di 4 
	// byte (in modo che le strutture dati restino allineate alla linea)
	natl quanti = allinea(dim, sizeof(int));
	// allochiamo "quanti" byte, invece dei "dim" richiesti
	
	// per prima cosa, cerchiamo una zona di dimensione sufficiente
	des_mem *prec = 0, *scorri = memlibera;
	while (scorri != 0 && scorri->dimensione < quanti) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(scorri == 0 || scorri->dimensione >= quanti);

	addr p = 0;
	if (scorri != 0) {
		p = scorri + 1; // puntatore al primo byte dopo il descrittore
				// (coincide con l'indirizzo iniziale delle zona
				// di memoria)

		// per poter dividere in due la zona trovata, la parte
		// rimanente, dopo aver occupato "quanti" byte, deve poter contenere
		// almeno il descrittore piu' 4 byte (minima dimensione
		// allocabile)
		if (scorri->dimensione - quanti >= sizeof(des_mem) + sizeof(int)) {

			// il nuovo descrittore verra' scritto nei primi byte 
			// della zona da creare (quindi, "quanti" byte dopo "p")
			addr pnuovo = static_cast<natb*>(p) + quanti;
			des_mem* nuovo = static_cast<des_mem*>(pnuovo);

			// aggiustiamo le dimensioni della vecchia e della nuova zona
			nuovo->dimensione = scorri->dimensione - quanti - sizeof(des_mem);
			scorri->dimensione = quanti;

			// infine, inseriamo "nuovo" nella lista delle zone libere,
			// al posto precedentemente occupato da "scorri"
			nuovo->next = scorri->next;
			if (prec != 0) 
				prec->next = nuovo;
			else
				memlibera = nuovo;

		} else {

			// se la zona non e' sufficientemente grande per essere
			// divisa in due, la occupiamo tutta, rimuovendola
			// dalla lista delle zone libere
			if (prec != 0)
				prec->next = scorri->next;
			else
				memlibera = scorri->next;
		}
		
		// a scopo di debug, inseriamo un valore particolare nel campo
		// next (altimenti inutilizzato) del descrittore. Se tutto
		// funziona correttamente, ci aspettiamo di ritrovare lo stesso
		// valore quando quando la zona verra' successivamente
		// deallocata. (Notare che il valore non e' allineato a 4 byte,
		// quindi un valido indirizzo di descrittore non puo' assumere
		// per caso questo valore).
		scorri->next = reinterpret_cast<des_mem*>(0xdeadbeef);
		
	}

	// restituiamo l'indirizzo della zona allocata (0 se non e' stato
	// possibile allocare).  NOTA: il descrittore della zona si trova nei
	// byte immediatamente precedenti l'indirizzo "p".  Questo e'
	// importante, perche' ci permette di recuperare il descrittore dato
	// "p" e, tramite il descrittore, la dimensione della zona occupata
	// (tale dimensione puo' essere diversa da quella richiesta, quindi
	// e' ignota al chiamante della funzione).
	return p;
}

void free_interna(addr indirizzo, natl quanti);

// "dealloca" deve essere usata solo con puntatori restituiti da "alloca", e rende
// nuovamente libera la zona di memoria di indirizzo iniziale "p".
void dealloca(void* p)
{

	// e' normalmente ammesso invocare "dealloca" su un puntatore nullo.
	// In questo caso, la funzione non deve fare niente.
	if (p == 0) return;
	
	// recuperiamo l'indirizzo del descrittore della zona
	des_mem* des = reinterpret_cast<des_mem*>(p) - 1;

	// se non troviamo questo valore, vuol dire che un qualche errore grave
	// e' stato commesso (free su un puntatore non restituito da malloc,
	// doppio free di un puntatore, sovrascrittura del valore per accesso
	// al di fuori di un array, ...)
	if (des->next != reinterpret_cast<des_mem*>(0xdeadbeef))
		panic("free() errata");
	
	// la zona viene liberata tramite la funzione "free_interna", che ha
	// bisogno dell'indirizzo di partenza e della dimensione della zona
	// comprensiva del suo descrittore
	free_interna(des, des->dimensione + sizeof(des_mem));
}

// rende libera la zona di memoria puntata da "indirizzo" e grande "quanti"
// byte, preoccupandosi di creare il descrittore della zona e, se possibile, di
// unificare la zona con eventuali zone libere contigue in memoria.  La
// riunificazione e' importante per evitare il problema della "falsa
// frammentazione", in cui si vengono a creare tante zone libere, contigue, ma
// non utilizzabili per allocazioni piu' grandi della dimensione di ciascuna di
// esse.
// "free_interna" puo' essere usata anche in fase di inizializzazione, per
// definire le zone di memoria fisica che verranno utilizzate dall'allocatore
void free_interna(addr indirizzo, natl quanti)
{

	// non e' un errore invocare free_interna con "quanti" uguale a 0
	if (quanti == 0) return;

	// il caso "indirizzo == 0", invece, va escluso, in quanto l'indirizzo
	// 0 viene usato per codificare il puntatore nullo
	if (indirizzo == 0) panic("free_interna(0)");

	// la zona va inserita nella lista delle zone libere, ordinata per
	// indirizzo di partenza (tale ordinamento serve a semplificare la
	// successiva operazione di riunificazione)
	des_mem *prec = 0, *scorri = memlibera;
	while (scorri != 0 && scorri < indirizzo) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(scorri == 0 || scorri >= indirizzo)

	// in alcuni casi, siamo in grado di riconoscere l'errore di doppia
	// free: "indirizzo" non deve essere l'indirizzo di partenza di una
	// zona gia' libera
	if (scorri == indirizzo) {
		flog(LOG_ERR, "indirizzo = 0x%x", indirizzo);
		panic("double free\n");
	}
	// assert(scorri == 0 || scorri > indirizzo)

	// verifichiamo che la zona possa essere unificata alla zona che la
	// precede.  Cio' e' possibile se tale zona esiste e il suo ultimo byte
	// e' contiguo al primo byte della nuova zona
	if (prec != 0 && (natb*)(prec + 1) + prec->dimensione == indirizzo) {

		// controlliamo se la zona e' unificabile anche alla eventuale
		// zona che la segue
		if (scorri != 0 && static_cast<natb*>(indirizzo) + quanti == (addr)scorri) {
			
			// in questo caso le tre zone diventano una sola, di
			// dimensione pari alla somma delle tre, piu' il
			// descrittore della zona puntata da scorri (che ormai
			// non serve piu')
			prec->dimensione += quanti + sizeof(des_mem) + scorri->dimensione;
			prec->next = scorri->next;

		} else {

			// unificazione con la zona precedente: non creiamo una
			// nuova zona, ma ci limitiamo ad aumentare la
			// dimensione di quella precedente
			prec->dimensione += quanti;
		}

	} else if (scorri != 0 && static_cast<natb*>(indirizzo) + quanti == (addr)scorri) {

		// la zona non e' unificabile con quella che la precede, ma e'
		// unificabile con quella che la segue. L'unificazione deve
		// essere eseguita con cautela, per evitare di sovrascrivere il
		// descrittore della zona che segue prima di averne letto il
		// contenuto
		
		// salviamo il descrittore della zona seguente in un posto
		// sicuro
		des_mem salva = *scorri; 
		
		// allochiamo il nuovo descrittore all'inizio della nuova zona
		// (se quanti < sizeof(des_mem), tale descrittore va a
		// sovrapporsi parzialmente al descrittore puntato da scorri)
		des_mem* nuovo = reinterpret_cast<des_mem*>(indirizzo);

		// quindi copiamo il descrittore della zona seguente nel nuovo
		// descrittore. Il campo next del nuovo descrittore e'
		// automaticamente corretto, mentre il campo dimensione va
		// aumentato di "quanti"
		*nuovo = salva;
		nuovo->dimensione += quanti;

		// infine, inseriamo "nuovo" in lista al posto di "scorri"
		if (prec != 0) 
			prec->next = nuovo;
		else
			memlibera = nuovo;

	} else if (quanti >= sizeof(des_mem)) {

		// la zona non puo' essere unificata con nessun'altra.  Non
		// possiamo, pero', inserirla nella lista se la sua dimensione
		// non e' tale da contenere il descrittore (nel qual caso, la
		// zona viene ignorata)

		des_mem* nuovo = reinterpret_cast<des_mem*>(indirizzo);
		nuovo->dimensione = quanti - sizeof(des_mem);

		// inseriamo "nuovo" in lista, tra "prec" e "scorri"
		nuovo->next = scorri;
		if (prec != 0)
			prec->next = nuovo;
		else
			memlibera = nuovo;
	}
}
// )

// ( [P_MEM_PHYS]
// La memoria fisica viene gestita in due fasi: durante l'inizializzazione, si
// tiene traccia dell'indirizzo dell'ultimo byte non "occupato", tramite il
// puntatore mem_upper. Tale puntatore viene fatto avanzare mano a mano che si
// decide come utilizzare la memoria fisica a disposizione:
// 1. all'inizio, poiche' il sistema e' stato caricato in memoria fisica dal
// bootstrap loader, il primo byte non occupato e' quello successivo all'ultimo
// indirizzo occupato dal sistema stesso (e' il linker, tramite il simbolo
// predefinito "_end", che ci permette di conoscere, staticamente, questo
// indirizzo [vedi sistema.S])
// 2. di seguito al modulo sistema, viene allocato l'array di descrittori di 
// pagine fisiche [vedi avanti], la cui dimensione e' calcolata dinamicamente, 
// in base al numero di pagine fisiche rimanenti.
// 3. tutta la memoria fisica restante, a partire dal primo indirizzo multiplo
// di 4096, viene usata per le pagine fisiche, destinate a contenere
// descrittori, tabelle e pagine virtuali.
//
// Durante queste operazioni, si vengono a scoprire regioni di memoria fisica
// non utilizzate (per esempio, il modulo sistema viene caricato in memoria a 
// partire dall'indirizzo 0x100000, corrispondente a 1 MiB, quindi il primo 
// mega byte, esclusa la zona dedicata alla memoria video, risulta 
// inutilizzato) Queste regioni, man mano che vengono scoperte, vengono 
// aggiunte allo heap di sistema. Nella seconda fase, il sistema usa lo heap 
// cosi' ottenuto per allocare la memoria richiesta dalla funzione "alloca".

// indirizzo fisico del primo byte non riferibile in memoria inferiore e
// superiore (rappresentano la memoria fisica installata sul sistema)
// Nota: la "memoria inferiore" e' quella corrispondente al primo mega byte di 
// memoria fisica installata. Per motivi storici, non tutto il mega byte e' 
// disponibile (ad esempio, verso la fine, troviamo la memoria video).
// La "memoria superiore" e' quella successiva al primo megabyte
addr max_mem_lower;
addr max_mem_upper;

// indirizzo fisico del primo byte non occupato
extern addr mem_upper;

// allocazione sequenziale, da usare durante la fase di inizializzazione.  Si
// mantiene un puntatore (mem_upper) all'ultimo byte non occupato.  Tale
// puntatore puo' solo avanzare, tramite le funzioni 'occupa' e 'salta_a', e
// non puo' superare la massima memoria fisica contenuta nel sistema
// (max_mem_upper). Se il puntatore viene fatto avanzare tramite la funzione
// 'salta_a', i byte "saltati" vengono dati all'allocatore a lista
// tramite la funzione "free_interna"
addr occupa(natl quanti)
{
	addr p = 0;
	addr appoggio = static_cast<natb*>(mem_upper) + quanti;
	if (appoggio <= max_mem_upper) {
		p = mem_upper;
		mem_upper = appoggio;
	}
	return p; // se e' zero, non e' stato possibile spostare mem_upper di
		  // "quanti" byte in avanti
}

int salta_a(addr indirizzo)
{
	int saltati = -1;
	if (indirizzo >= mem_upper && indirizzo < max_mem_upper) {
		saltati = static_cast<natb*>(indirizzo) - static_cast<natb*>(mem_upper);
		free_interna(mem_upper, saltati);
		mem_upper = indirizzo;
	}
	return saltati; // se e' negativo, "indirizzo" era minore di mem_upper
			// (ovvero, "indirizzo" era gia' occupato), oppure era
			// maggiore della memoria disponibile
}

// init_dpf viene chiamata in fase di inizalizzazione.  Tutta la
// memoria non ancora occupata viene usata per le pagine fisiche.  La funzione
// si preoccupa anche di allocare lo spazio per i descrittori di pagina fisica,
// e di inizializzarli in modo che tutte le pagine fisiche risultino libere
bool init_dpf()
{

	// allineamo mem_upper alla linea, per motivi di efficienza
	salta_a(allineav(mem_upper, sizeof(int)));

	// calcoliamo quanta memoria principale rimane
	int dimensione = static_cast<natb*>(max_mem_upper) - static_cast<natb*>(mem_upper);

	if (dimensione <= 0) {
		flog(LOG_ERR, "Non ci sono pagine libere");
		return false;
	}

	// calcoliamo quante pagine fisiche possiamo definire (tenendo conto
	// del fatto che ogni pagina fisica avra' bisogno di un descrittore)
	natl quante = dimensione / (DIM_PAGINA + sizeof(des_pf));

	// allochiamo i corrispondenti descrittori di pagina fisica
	dpf = reinterpret_cast<des_pf*>(occupa(sizeof(des_pf) * quante));

	// riallineamo mem_upper a un multiplo di pagina
	salta_a(allineav(mem_upper, DIM_PAGINA));

	// ricalcoliamo quante col nuovo mem_upper, per sicurezza
	// (sara' minore o uguale al precedente)
	quante = (static_cast<natb*>(max_mem_upper) - static_cast<natb*>(mem_upper)) / DIM_PAGINA;

	// occupiamo il resto della memoria principale con le pagine fisiche;
	// ricordiamo l'indirizzo della prima pagina fisica e il loro numero
	prima_pf_utile = occupa(quante * DIM_PAGINA);
	NUM_DPF = quante;

	// se resta qualcosa (improbabile), lo diamo all'allocatore a lista
	salta_a(max_mem_upper);

	// costruiamo la lista delle pagine fisiche libere
	pagine_libere = 0;
	for (natl i = 0; i < quante - 1; i++) {
		dpf[i].contenuto = LIBERA;
		dpf[i].avl.prossima_libera = i + 1;
	}
	dpf[quante - 1].avl.prossima_libera = 0xFFFFFFFF;

	return true;
}
// )

// ( [P_PROCESS] (si veda [4.6][4.7])
// funzioni usate da crea_processo
void rilascia_tutto(addr direttorio, natl i, natl n);
extern "C" int alloca_tss(des_proc* p);
extern "C" void rilascia_tss(int indice);

// elemento di coda e descrittore del primo processo (quello che esegue il 
// codice di inizializzazione e la funzione main)
proc_elem init;
des_proc des_main;

const natl BIT_IF = 1L << 9;
proc_elem* crea_processo(void f(int), int a, int prio, char liv, bool IF)
{
	proc_elem	*p;			// proc_elem per il nuovo processo
	natl		identifier;		// indice del tss nella gdt 
	des_proc	*pdes_proc;		// descrittore di processo
	addr		pdirettorio;		// direttorio del processo
	addr		pila_sistema,		// punt. di lavoro
			tabella,
			pila_utente;		// punt. di lavoro
	

	// ( allocazione (e azzeramento preventivo) di un des_proc 
	//   (parte del punto 3 in [4.6])
	pdes_proc = static_cast<des_proc*>(alloca(sizeof(des_proc)));
	if (pdes_proc == 0) goto errore1;
	memset(pdes_proc, 0, sizeof(des_proc));
	// )

	// ( selezione di un identificatore (punto 1 in [4.6])
	identifier = alloca_tss(pdes_proc);
	if (identifier == 0) goto errore2;
	// )
	
	// ( allocazione e inizializzazione di un proc_elem
	//   (punto 3 in [4.6])
	p = static_cast<proc_elem*>(alloca(sizeof(proc_elem)));
        if (p == 0) goto errore3;
        p->id = identifier << 3U;
        p->precedenza = prio;
	// )

	// ( creazione del direttorio del processo (vedi [10.3]
	//   e la funzione "carica()")
	pdirettorio = swap_ent(p->id, DIRETTORIO, 0);
	if (pdirettorio == 0) goto errore4;
	dpf[indice_dpf(pdirettorio)].pt.residente = true;
	pdes_proc->cr3 = pdirettorio;
	// )

	// ( creazione della pila sistema (vedi [10.3]).
	//   La pila sistema e' grande 4MiB, ma allochiamo solo l'ultima
	//   pagina. Non chiamiamo "swap", perche' vogliamo passare "true"
	//   ad entrambe le "swap_ent".
	tabella = swap_ent(p->id, TABELLA, fine_sistema_privato - DIM_PAGINA);
	if (tabella == 0) goto errore5;
	dpf[indice_dpf(tabella)].pt.residente = true;
	pila_sistema = swap_ent(p->id, PAGINA, fine_sistema_privato - DIM_PAGINA);
	if (pila_sistema == 0) goto errore6;
	dpf[indice_dpf(pila_sistema)].pt.residente = true;
	// )

	if (liv == LIV_UTENTE) {
		// ( inizializziamo la pila sistema.
		natl* pl = static_cast<natl*>(pila_sistema);

		pl[1019] = (natl)f;		// EIP (codice utente)
		pl[1020] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pl[1021] = (IF? BIT_IF : 0);	// EFLAG
		pl[1022] = (natl)(fine_utente_privato - 2 * sizeof(int)); // ESP (pila utente)
		pl[1023] = SEL_DATI_UTENTE;	// SS (pila utente)
		//   eseguendo una IRET da questa situazione, il processo
		//   passera' ad eseguire la prima istruzione della funzione f,
		//   usando come pila la pila utente (al suo indirizzo virtuale)
		// )

		// ( creazione e inizializzazione della pila utente
		pila_utente = swap(p->id, fine_utente_privato - DIM_PAGINA);
		if (pila_utente == 0) goto errore6;

		//   dobbiamo ora fare in modo che la pila utente si trovi nella
		//   situazione in cui si troverebbe dopo una CALL alla funzione
		//   f, con parametro a:
		pl = static_cast<natl*>(pila_utente);
		pl[1022] = 0xffffffff;	// ind. di ritorno non significativo
		pl[1023] = a;		// parametro del processo

		// dobbiamo settare manualmente il bit D, perche' abbiamo
		// scritto nella pila tramite la finestra FM, non tramite
		// il suo indirizzo virtuale.
		natl& dp = get_despag(p->id, fine_utente_privato - DIM_PAGINA);
		set_D(dp, true);
		// )

		// ( infine, inizializziamo il descrittore di processo
		//   (punto 3 in [4.6])
		//   indirizzo del bottom della pila sistema, che verra' usato
		//   dal meccanismo delle interruzioni
		pdes_proc->punt_nucleo = fine_sistema_privato;
		pdes_proc->riservato  = SEL_DATI_SISTEMA;

		//   inizialmente, il processo si trova a livello sistema, come
		//   se avesse eseguito una istruzione INT, con la pila sistema
		//   che contiene le 5 parole lunghe preparate precedentemente
		pdes_proc->contesto[I_ESP] = (natl)(fine_sistema_privato - 5 * sizeof(int));
		pdes_proc->contesto[I_SS]  = SEL_DATI_SISTEMA;

		pdes_proc->contesto[I_DS]  = SEL_DATI_UTENTE;
		pdes_proc->contesto[I_ES]  = SEL_DATI_UTENTE;

		pdes_proc->contesto[I_FPU_CR] = 0x037f;
		pdes_proc->contesto[I_FPU_TR] = 0xffff;
		pdes_proc->cpl = LIV_UTENTE;
		//   tutti gli altri campi valgono 0
		// )
	} else {
		// ( inizializzazione delle pila sistema
		natl* pl = static_cast<natl*>(pila_sistema);
		pl[1019] = (natl)f;	  	// EIP (codice sistema)
		pl[1020] = SEL_CODICE_SISTEMA;  // CS (codice sistema)
		pl[1021] = (IF? BIT_IF : 0);  	// EFLAG
		pl[1022] = 0xffffffff;		// indirizzo ritorno?
		pl[1023] = a;			// parametro
		//   i processi esterni lavorano esclusivamente a livello
		//   sistema. Per questo motivo, prepariamo una sola pila (la
		//   pila sistema)
		// )

		// ( inizializziamo il descrittore di processo
		//   (punto 3 in [4.6])
		pdes_proc->contesto[I_ESP] = (natl)(fine_sistema_privato - 5 * sizeof(int));
		pdes_proc->contesto[I_SS]  = SEL_DATI_SISTEMA;

		pdes_proc->contesto[I_DS]  = SEL_DATI_SISTEMA;
		pdes_proc->contesto[I_ES]  = SEL_DATI_SISTEMA;
		pdes_proc->contesto[I_FPU_CR] = 0x037f;
		pdes_proc->contesto[I_FPU_TR] = 0xffff;
		pdes_proc->cpl = LIV_SISTEMA;
		//   tutti gli altri campi valgono 0
		// )
	}

	return p;

errore6:	rilascia_tutto(pdirettorio, i_sistema_privato, ntab_sistema_privato);
errore5:	rilascia_pagina_fisica(indice_dpf(pdirettorio));
errore4:	dealloca(p);
errore3:	rilascia_tss(identifier);
errore2:	dealloca(pdes_proc);
errore1:	return 0;
}

// parte "C++" della activate_p, descritta in [4.6]
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
	// (questa svolge, tra l'altro, i punti 1-3 in [4.6])
	p = crea_processo(f, a, prio, liv, true);
	// *)

	if (p != 0) {
		inserimento_lista(pronti, p);	// punto 4 in [4.6]
		processi++;			// [4.7]
		id = p->id;			// id del processo creato
						// (allocato da crea_processo)
		flog(LOG_INFO, "proc=%d entry=%x(%d) prio=%d liv=%d", id, f, a, prio, liv);
	}

	return id;
}


void rilascia_tutto(addr direttorio, natl i, natl n);
// rilascia tutte le strutture dati private associate al processo puntato da 
// "p" (tranne il proc_elem puntato da "p" stesso)
void distruggi_processo(proc_elem* p)
{
	des_proc* pdes_proc = des_p(p->id);

	addr direttorio = pdes_proc->cr3;
	rilascia_tutto(direttorio, i_sistema_privato, ntab_sistema_privato);
	rilascia_tutto(direttorio, i_utente_privato,  ntab_utente_privato);
	rilascia_pagina_fisica(indice_dpf(direttorio));
	rilascia_tss(p->id >> 3);
	dealloca(pdes_proc);
}

// rilascia ntab tabelle (con tutte le pagine da esse puntate) a partire da 
// quella puntata dal descrittore i-esimo di direttorio.
void rilascia_tutto(addr direttorio, natl i, natl n)
{
	for (natl j = i; j < i + n && j < 1024; j++)
	{
		natl dt = get_des(direttorio, j);
		if (extr_P(dt)) {
			addr tabella = extr_IND_F(dt);
			for (int k = 0; k < 1024; k++) {
				natl dp = get_des(tabella, k);
				if (extr_P(dp)) {
					addr pagina = extr_IND_F(dp);
					rilascia_pagina_fisica(indice_dpf(pagina));
				} else {
					natl blocco = extr_IND_M(dp);
					dealloca_blocco(blocco);
				}
			}
			rilascia_pagina_fisica(indice_dpf(tabella));
		} else {
			natl blocco = extr_IND_M(dt);
			dealloca_blocco(blocco);
		}
	}
}

// parte "C++" della terminate_p, descritta in [4.6]
extern "C" void c_terminate_p()
{
	// il processo che deve terminare e' quello che ha invocato
	// la terminate_p, quindi e' in esecuzione
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;			// [4.7]
	flog(LOG_INFO, "Processo %d terminato", p->id);
	dealloca(p);
	schedulatore();			// [4.6]
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


// ( [P_PAGING] 

// funzione che restituisce i 10 bit piu' significativi di "ind_virt"
// (indice nel direttorio)
int i_dir(addr ind_virt)
{
	return ((natl)ind_virt & 0xFFC00000) >> 22;
}

// funzione che restituisce i bit 22-12 di "ind_virt"
// (indice nella tabella delle pagine)
int i_tab(addr ind_virt)
{
	return ((natl)ind_virt & 0x003FF000) >> 12;
}

natl& get_des(addr iff, natl index) // [6.3]
{
	natl *pd = static_cast<natl*>(iff);
	return pd[index];
}

natl& get_destab(addr dir, addr ind_virt) // [6.3]
{
	return get_des(dir, i_dir(ind_virt));
}

natl& get_despag(addr tab, addr ind_virt) // [6.3]
{
	return get_des(tab, i_tab(ind_virt));
}

addr get_dir(natl proc);
addr get_tab(natl proc, addr ind_virt);

natl& get_destab(natl processo, addr ind_virt) // [6.3]
{
	return get_destab(get_dir(processo), ind_virt);
}

natl& get_despag(natl processo, addr ind_virt) // [6.3]
{
	natl dt = get_destab(processo, ind_virt);
	return get_despag(extr_IND_F(dt), ind_virt);
}

// restituisce l'indirizzo fisico del direttorio del processo proc
addr get_dir(natl proc)
{
	des_proc *p = des_p(proc);
	return p->cr3;
}

addr get_tab(natl proc, addr ind_virt)
{
	natl dt = get_destab(proc, ind_virt);
	return extr_IND_F(dt);
}

// ( si veda "case DIRETTORIO:" sotto
natl dummy_des;
// )
natl& get_desent(natl processo, cont_pf tipo, addr ind_virt)
{
	switch (tipo) {
	case PAGINA:
		return get_despag(processo, ind_virt);
	case TABELLA:
		return get_destab(processo, ind_virt);
	// ( per poter usare swap anche per caricare direttori prevediamo
	//   questo ulteriore caso (restituiamo un descrittore fasullo,
	//   perché il direttorio non e' puntato da un descrittore)
	case DIRETTORIO:
		dummy_des = 0;
		return dummy_des;
	// )
	default:
		flog(LOG_ERR, "get_desent(%d, %d, %x)", processo, tipo, ind_virt);
		panic("Errore di sistema");  // ****
	}
}

void set_des(addr dirtab, int index, natl des)
{
	natl *pd = static_cast<natl*>(dirtab);
	pd[index] = des;
}

void set_destab(addr dir, addr ind_virt, natl destab)
{
	set_des(dir, i_dir(ind_virt), destab);
}
void set_despag(addr tab, addr ind_virt, natl despag)
{
	set_des(tab, i_tab(ind_virt), despag);
}

void mset_des(addr dirtab, natl i, natl n, natl des)
{
	natl *pd = static_cast<natl*>(dirtab);
	for (natl j = i; j < i + n && j < 1024; j++)
		pd[j] = des;
}

void copy_des(addr src, addr dst, natl i, natl n)
{
	natl *pdsrc = static_cast<natl*>(src),
	     *pddst = static_cast<natl*>(dst);
	for (natl j = i; j < i + n && j < 1024; j++)
		pddst[j] = pdsrc[j];
}

extern "C" addr c_trasforma(addr ind_virt)
{
	natl dp = get_despag(esecuzione->id, ind_virt);
	natl ind_fis_pag = (natl)extr_IND_F(dp);
	return (addr)(ind_fis_pag | ((natl)ind_virt & 0x00000FFF));

}



// )

// ( [P_MEM_VIRT]
//
// mappa le ntab pagine virtuali a partire dall'indirizzo virt_start agli 
// indirizzi fisici
// che partono da phys_start, in sequenza.
bool sequential_map(addr direttorio, addr phys_start, addr virt_start, natl npag, natl flags)
{
	natb *indv = static_cast<natb*>(virt_start),
	     *indf = static_cast<natb*>(phys_start);
	for (natl i = 0; i < npag; i++, indv += DIM_PAGINA, indf += DIM_PAGINA) {
		natl dt = get_destab(direttorio, indv);
		addr tabella;
		if (! extr_P(dt)) {
			natl indice = alloca_pagina_fisica_libera();
			if (indice == 0xFFFFFFFF) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
				return false;
			}
			des_pf *ppf = &dpf[indice];
			ppf->contenuto = TABELLA_FM;
			ppf->pt.residente = true;
			tabella = indirizzo_pf(indice);

			dt = ((natl)tabella & ADDR_MASK) | flags | BIT_P;
			set_destab(direttorio, indv, dt);
		} else {
			tabella = extr_IND_F(dt);
		}
		natl dp = ((natl)indf & ADDR_MASK) | flags | BIT_P;
		set_despag(tabella, indv, dp);
	}
	return true;
}
// mappa tutti gli indirizzi a partire da start (incluso) fino ad end (escluso)
// in modo che l'indirizzo virtuale coincida con l'indirizzo fisico.
// start e end devono essere allineati alla pagina.
bool identity_map(addr direttorio, addr start, addr end, natl flags)
{
	natl npag = (static_cast<natb*>(end) - static_cast<natb*>(start)) / DIM_PAGINA;
	return sequential_map(direttorio, start, start, npag, flags);
}
// mappa la memoria fisica, dall'indirizzo 0 all'indirizzo max_mem, nella 
// memoria virtuale gestita dal direttorio pdir
// (la funzione viene usata in fase di inizializzazione)
bool crea_finestra_FM(addr direttorio, addr max_mem)
{
	return identity_map(direttorio, (addr)DIM_PAGINA, max_mem, BIT_RW);
}

// ( [P_PCI]

// mappa in memoria virtuale la porzione di spazio fisico dedicata all'I/O (PCI e altro)
bool crea_finestra_PCI(addr direttorio)
{
	return sequential_map(direttorio,
			PCI_startmem,
			inizio_pci_condiviso,
			ntab_pci_condiviso * 1024,
			BIT_RW | BIT_PCD);
}

natb pci_getbus(natw l)
{
	return l >> 8;
}

natb pci_getdev(natw l)
{
	return (l & 0x00FF) >> 3;
}

natb pci_getfun(natw l)
{
	return l & 0x0007;
}
// )

// ( [P_EXTERN_PROC]
// Registrazione processi esterni
proc_elem* const ESTERN_BUSY = (proc_elem*)1;
proc_elem *a_p_save[MAX_IRQ];
// primitiva di nucleo usata dal nucleo stesso
enum controllore { master=0, slave=1 };
extern "C" void nwfi(controllore c);

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

		if (h < 8)
			nwfi(master);
		else
			nwfi(slave);
	}
}

// associa il processo esterno puntato da "p" all'interrupt "irq".
// Fallisce se un processo esterno (non generico) era gia' stato associato a 
// quello stesso interrupt
extern "C" void unmask_irq(natb irq);
bool aggiungi_pe(proc_elem *p, natb irq)
{
	if (irq >= MAX_IRQ || a_p_save[irq] == 0)
		return false;

	a_p[irq] = p;
	distruggi_processo(a_p_save[irq]);
	dealloca(a_p_save[irq]);
	a_p_save[irq] = 0;
	unmask_irq(irq);
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

	flog(LOG_INFO, "estern=%d entry=%x(%d) prio=%d liv=%d type=%d", p->id, f, a, prio, liv, type);

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
	aggiungi_pe(ESTERN_BUSY, 0);
	return true;
}
// )

// ( [P_HARDDISK]

const ioaddr iBR = 0x01F0;
const ioaddr iCNL = 0x01F4;
const ioaddr iCNH = 0x01F5;
const ioaddr iSNR = 0x01F3;
const ioaddr iHND = 0x01F6;
const ioaddr iSCR = 0x01F2;
const ioaddr iERR = 0x01F1;
const ioaddr iCMD = 0x01F7;
const ioaddr iSTS = 0x01F7;
const ioaddr iDCR = 0x03F6;

void leggisett(natl lba, natb quanti, natw vetti[])
{	
	natb lba_0 = lba,
		lba_1 = lba >> 8,
		lba_2 = lba >> 16,
		lba_3 = lba >> 24;

	outputb(lba_0, iSNR); 			// indirizzo del settore e selezione drive
	outputb(lba_1, iCNL);
	outputb(lba_2, iCNH);
	natb hnd = (lba_3 & 0x0F) | 0xE0;
	outputb(hnd, iHND);
	outputb(quanti, iSCR); 			// numero di settori
	outputb(0x0A, iDCR);			// disabilitazione richieste di interruzione
	outputb(0x20, iCMD);			// comando di lettura

	for (int i = 0; i < quanti; i++) {
		natb s;
		do
			inputb(iSTS, s);
		while ((s & 0x88) != 0x08);
		for (int j = 0; j < 512/2; j++)
			inputw(iBR, vetti[i*512/2 + j]);
	}
}

void scrivisett(natl lba, natb quanti, natw vetto[])
{	
	natb lba_0 = lba,
		lba_1 = lba >> 8,
		lba_2 = lba >> 16,
		lba_3 = lba >> 24;
	outputb(lba_0, iSNR);					// indirizzo del settore e selezione drive
	outputb(lba_1, iCNL);
	outputb(lba_2, iCNH);
	natb hnd = (lba_3 & 0x0F) | 0xE0;
	outputb(hnd, iHND);
	outputb(quanti, iSCR);					// numero di settori
	outputb(0x0A, iDCR);					// disabilitazione richieste di interruzione
	outputb(0x30, iCMD);					// comando di scrittura
	for (int i = 0; i < quanti; i++) {
		natb s; 
		do
			inputb(iSTS, s);
		while ((s & 0x88) != 0x08);
		for (int j = 0; j < 512/2; j++)
			outputw(vetto[i*512/2 + j], iBR);
	}
}


// )
// ( [P_LOG]


// gestore generico di eccezioni (chiamata da tutti i gestori di eccezioni in 
// sistema.S, tranne il gestore di page fault)
extern "C" void gestore_eccezioni(int tipo, unsigned errore,
		addr eip, unsigned cs, short eflag)
{
	if (eip < fine_codice_sistema) {
		flog(LOG_ERR, "Eccezione %d, eip = %x, errore = %x", tipo, eip, errore);
		panic("eccezione dal modulo sistema");
	}
	flog(LOG_WARN, "Eccezione %d, errore %x", tipo, errore);
	flog(LOG_WARN, "eflag = %x, eip = %x, cs = %x", eflag, eip, cs);
}

const ioaddr iRBR = 0x03F8;		// DLAB deve essere 0
const ioaddr iTHR = 0x03F8;		// DLAB deve essere 0
const ioaddr iLSR = 0x03FD;
const ioaddr iLCR = 0x03FB;
const ioaddr iDLR_LSB = 0x03F8;		// DLAB deve essere 1
const ioaddr iDLR_MSB = 0x03F9;		// DLAB deve essere 1
const ioaddr iIER = 0x03F9;		// DLAB deve essere 0
const ioaddr iMCR = 0x03FC;
const ioaddr iIIR = 0x03FA;


void ini_COM1()
{	natw CBITR = 0x000C;		// 9600 bit/sec.
	natb dummy;
	outputb(0x80, iLCR);		// DLAB 1
	outputb(CBITR, iDLR_LSB);
	outputb(CBITR >> 8, iDLR_MSB);
	outputb(0x03, iLCR);		// 1 bit STOP, 8 bit/car, parita dis, DLAB 0
	outputb(0x00, iIER);		// richieste di interruzione disabilitate
	inputb(iRBR, dummy);		// svuotamento buffer RBR
}

void serial_o(natb c)
{	natb s;
	do 
	{	inputb(iLSR, s);    }
	while (! (s & 0x20));
	outputb(c, iTHR);
}

// invia un nuovo messaggio sul log
void do_log(log_sev sev, const natb* buf, natl quanti)
{
	const char* lev[] = { "DBG", "INF", "WRN", "ERR", "USR" };
	if (sev > MAX_LOG) {
		flog(LOG_WARN, "Livello di log errato: %d", sev);
		abort_p();
	}
	const natb* l = (const natb*)lev[sev];
	while (*l)
		serial_o(*l++);
	serial_o((natb)'\t');
	natb idbuf[10];
	snprintf(idbuf, 10, "%d", esecuzione->id);
	l = idbuf;
	while (*l)
		serial_o(*l++);
	serial_o((natb)'\t');
	if (quanti > LOG_MSG_SIZE)
		quanti = LOG_MSG_SIZE;

	for (natl i = 0; i < quanti; i++)
		serial_o(buf[i]);
	serial_o((natb)'\n');
}
extern "C" void c_log(log_sev sev, const natb* buf, natl quanti)
{
	do_log(sev, buf, quanti);
}

// log formattato
extern "C" void flog(log_sev sev, cstr fmt, ...)
{
	va_list ap;
	natb buf[LOG_MSG_SIZE];

	va_start(ap, fmt);
	int l = vsnprintf(buf, LOG_MSG_SIZE, fmt, ap);
	va_end(ap);

	if (l > 1)
		do_log(sev, buf, l - 1);
}

// )

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

// vedi [10.5]
natl alloca_blocco()
{
	natl i = 0;
	natl risu = 0;
	natl vecsize = ceild(swap_dev.sb.blocks, sizeof(natl) * 8);

	// saltiamo le parole lunghe che contengono solo zeri (blocchi tutti occupati)
	while (i < vecsize && swap_dev.free[i] == 0) i++;
	if (i < vecsize) {
		natl pos = trova_bit(swap_dev.free[i]);
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
void leggi_blocco(natl blocco, void* dest)
{
	natl sector = blocco * 8;

	leggisett(sector, 8, static_cast<natw*>(dest));
}

void scrivi_blocco(natl blocco, void* dest)
{
	natl sector = blocco * 8;

	scrivisett(sector, 8, static_cast<natw*>(dest));
}

// lettura dallo swap (da utilizzare nella fase di inizializzazione)
bool leggi_swap(void* buf, natl first, natl bytes)
{
	natl nsect = ceild(bytes, 512);

	leggisett(first, nsect, static_cast<natw*>(buf));

	return true;
}

// inizializzazione del descrittore di swap
char read_buf[512];
bool swap_init()
{
	// lettura del superblocco
	flog(LOG_DEBUG, "lettura del superblocco dall'area di swap...");
	if (!leggi_swap(read_buf, 1, sizeof(superblock_t)))
		return false;

	swap_dev.sb = *reinterpret_cast<superblock_t*>(read_buf);

	// controlliamo che il superblocco contenga la firma di riconoscimento
	if (swap_dev.sb.magic[0] != 'C' ||
	    swap_dev.sb.magic[1] != 'E' ||
	    swap_dev.sb.magic[2] != 'S' ||
	    swap_dev.sb.magic[3] != 'W')
	{
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
	return leggi_swap(swap_dev.free,
		swap_dev.sb.bm_start * 8, pages * DIM_PAGINA);
}
// )

// ( [P_PANIC]


// la funzione backtrace stampa su video gli indirizzi di ritorno dei record di 
// attivazione presenti sulla pila sistema
extern "C" void backtrace(int off);

// panic mostra un'insieme di informazioni di debug sul video e blocca il 
// sistema. I parametri "eip1", "cs", "eflags" e "eip2" sono in pila per 
// effetto della chiamata di sistema
extern "C" void c_panic(cstr     msg,
		        natl	 eip1,
			natw	 cs,
			natl	 eflags,
			natl 	 eip2)
{
	des_proc* p = des_p(esecuzione->id);
	flog(LOG_ERR, "PANIC");
	flog(LOG_ERR, "%s", msg);
	if (p) {
		flog(LOG_ERR, "EAX=%x  EBX=%x  ECX=%x  EDX=%x",	
			 p->contesto[I_EAX], p->contesto[I_EBX],
			 p->contesto[I_ECX], p->contesto[I_EDX]);
		flog(LOG_ERR,  "ESI=%x  EDI=%x  EBP=%x  ESP=%x",	
			 p->contesto[I_ESI], p->contesto[I_EDI], p->contesto[I_EBP], p->contesto[I_ESP]);
		flog(LOG_ERR, "CS=%x DS=%x ES=%x FS=%x GS=%x SS=%x",
			cs, p->contesto[I_DS], p->contesto[I_ES],
			p->contesto[I_FS], p->contesto[I_GS], p->contesto[I_SS]);
		flog(LOG_ERR, "EIP=%x  EFLAGS=%x", eip2, eflags);
		flog(LOG_ERR, "BACKTRACE:");
		backtrace(0);
	}
	end_program();
}
// )
// ( [P_INIT]
bool carica_tutto(natl proc, natl i, natl n, addr& last_addr)
{
	addr dir = get_dir(proc);
	for (natl j = i; j < i + n && j < 1024; j++)
	{
		addr ind = (addr)(j * DIM_MACROPAGINA);
		natl dt = get_des(dir, j);
		if (extr_P(dt)) {	  
			last_addr = (addr)((j + 1) * DIM_MACROPAGINA);
			addr tabella = swap_ent(proc, TABELLA, ind);
			if (!tabella) {
				flog(LOG_ERR, "Impossibile allocare tabella residente");
				return false;
			}
			dpf[indice_dpf(tabella)].pt.residente = true;
			for (int k = 0; k < 1024; k++) {
				natl dp = get_des(tabella, k);
				if (extr_P(dp)) {
					addr ind_virt = static_cast<natb*>(ind) + k * DIM_PAGINA;
					addr ind_fis = swap_ent(proc, PAGINA, ind_virt);
					if (ind_fis == 0) {
						flog(LOG_ERR, "Impossibile allocare pagina residente");
						return false;
					}
					dpf[indice_dpf(ind_fis)].pt.residente = true;
				}
			}
		}
	}
	return true;
}

bool crea_spazio_condiviso(natl dummy_proc, addr& last_address)
{
	
	// ( lettura del direttorio principale dallo swap
	flog(LOG_INFO, "lettura del direttorio principale...");
	addr tmp = alloca(DIM_PAGINA);
	if (tmp == 0) {
		flog(LOG_ERR, "memoria insufficiente");
		return false;
	}
	if (!leggi_swap(tmp, swap_dev.sb.directory * 8, DIM_PAGINA))
		return false;
	// )

	// (  carichiamo le parti condivise nello spazio di indirizzamento del processo
	//    dummy (vedi [10.2])
	addr dummy_dir = get_dir(dummy_proc);
	copy_des(tmp, dummy_dir, i_io_condiviso, ntab_io_condiviso);
	copy_des(tmp, dummy_dir, i_utente_condiviso, ntab_utente_condiviso);
	dealloca(tmp);
	
	if (!carica_tutto(dummy_proc, i_io_condiviso, ntab_io_condiviso, last_address))
		return false;
	if (!carica_tutto(dummy_proc, i_utente_condiviso, ntab_utente_condiviso, last_address))
		return false;
	// )

	// ( copiamo i descrittori relativi allo spazio condiviso anche nel direttorio
	//   corrente, in modo che vengano ereditati dai processi che creeremo in seguito
	addr my_dir = get_dir(proc_corrente());
	copy_des(dummy_dir, my_dir, i_io_condiviso, ntab_io_condiviso);
	copy_des(dummy_dir, my_dir, i_utente_condiviso, ntab_utente_condiviso);
	// )

	invalida_TLB();
	return true;
}

// creazione del processo dummy iniziale (usata in fase di inizializzazione del sistema)
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
// )
