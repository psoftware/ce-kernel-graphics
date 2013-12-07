// sistema64.cpp
//
#include "mboot.h"		// *****
#include "costanti.h"		// *****

#include "libce.h"

///////////////////////////////////////////////////////////////////////////////////
//                   INIZIALIZZAZIONE [10]                                       //
///////////////////////////////////////////////////////////////////////////////////
const natq HEAP_START = 1024 * 1024U;
extern "C" natq start;
const natq HEAP_SIZE = (natq)&start - HEAP_START;


/////////////////////////////////////////////////////////////////////////////////
//                     PROCESSI [4]                                            //
/////////////////////////////////////////////////////////////////////////////////
const int N_REG = 16;	// [4.6]

// descrittore di processo [4.6]
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
};

//indici nell'array contesto
enum { I_RAX, I_RCX, I_RDX, I_RBX,
       I_RSP, I_RBP, I_RSI, I_RDI, I_R8, I_R9, I_R10, 
       I_R11, I_R12, I_R13, I_R14, I_R15 };
// )

// elemento di una coda di processi [4.6]
struct proc_elem {
	natl id;
	natl precedenza;
	proc_elem *puntatore;
};
extern proc_elem *esecuzione;	// [4.6]
extern proc_elem *pronti;	// [4.6]

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

// [4.8]
extern "C" void schedulatore(void)
{
// ( poiche' la lista e' gia' ordinata in base alla priorita',
//   e' sufficiente estrarre l'elemento in testa
	rimozione_lista(pronti, esecuzione);
// )
}

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

	inspronti();
	schedulatore();
}

// (* in caso di errori fatali, useremo la segunte funzione, che blocca il sistema:
extern "C" void panic(cstr msg) __attribute__ (( noreturn ));
// implementazione in [P_PANIC]
// *)
extern "C" void init_idt();

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
{
	natw CBITR = 0x000C;		// 9600 bit/sec.
	natb dummy;
	outputb(0x80, iLCR);		// DLAB 1
	outputb(CBITR, iDLR_LSB);
	outputb(CBITR >> 8, iDLR_MSB);
	outputb(0x03, iLCR);		// 1 bit STOP, 8 bit/car, parita  dis, DLAB 0
	outputb(0x00, iIER);		// richieste di interruzione disabilitate
	inputb(iRBR, dummy);		// svuotamento buffer RBR
}

void serial_o(natb c)
{
	natb s;
	do
	{
		inputb(iLSR, s);
	}
	while (! (s & 0x20));
	outputb(c, iTHR);
}

// invia un nuovo messaggio sul log
extern "C" void do_log(log_sev sev, const char* buf, natl quanti)
{
	const char* lev[] = { "DBG", "INF", "WRN", "ERR", "USR" };
	if (sev > MAX_LOG) {
		flog(LOG_WARN, "Livello di log errato: %d", sev);
		//panic("abort");
	}
	const natb* l = (const natb*)lev[sev];
	while (*l)
		serial_o(*l++);
	serial_o((natb)'\t');
	natb idbuf[10];
	//snprintf((char*)idbuf, 10, "%d", esecuzione->id);
	snprintf((char*)idbuf, 10, "%d", 0);
	l = idbuf;
	while (*l)
		serial_o(*l++);
	serial_o((natb)'\t');

	for (natl i = 0; i < quanti; i++)
		serial_o(buf[i]);
	serial_o((natb)'\n');
}
extern "C" void c_log(log_sev sev, const char* buf, natl quanti)
{
	do_log(sev, buf, quanti);
}





// (* indirizzo del primo byte che non contiene codice di sistema (vedi "sistema.S")
extern "C" addr fine_codice_sistema;

extern "C" addr readCR2();
extern "C" addr readCR3();
int i_PML4(addr);
int i_PDP(addr);
int i_PD(addr);
int i_PT(addr);
natq& get_entry(addr,natl);
addr extr_IND_FISICO(natq);
// gestore generico di eccezioni (chiamata da tutti i gestori di eccezioni in
// sistema.S, tranne il gestore di page fault)
extern "C" void gestore_eccezioni(int tipo, natq errore,
				  addr rip, natq cs, natq rflag)
{
	//if (rip < fine_codice_sistema) {
	//	flog(LOG_ERR, "Eccezione %d, eip = %x, errore = %x", tipo, rip, errore);
		//panic("eccezione dal modulo sistema");
	//}
	flog(LOG_WARN, "Eccezione %d, errore %x", tipo, errore);
	flog(LOG_WARN, "rflag = %x, rip = %p, cs = %x", rflag, rip, cs);
	if(tipo==14)
	{
		addr CR2 = readCR2();
		flog(LOG_WARN, "CR2=   %p",CR2);
		addr CR3 = readCR3();
		flog(LOG_WARN, "CR3=   %p",CR3);
		natq pml4e = get_entry(CR3,i_PML4(CR2));
		flog(LOG_WARN, "pml4e= %p",pml4e);
		addr pdp = extr_IND_FISICO(pml4e);
		
		natq pdpe = get_entry(pdp,i_PDP(CR2));
		flog(LOG_WARN, "pdpe=  %p",pdpe);
		addr pd = extr_IND_FISICO(pdpe);
		
		natq pde = get_entry(pd,i_PD(CR2));
		flog(LOG_WARN, "pde=   %p",pde);
		addr pt = extr_IND_FISICO(pde);

		natq pte = get_entry(pt,i_PT(CR2));
		flog(LOG_WARN, "pte=   %p",pte);
	}
	
}



/////////////////////////////////////////////////////////////////////////////////
//                         PAGINE FISICHE [6]                                  //
/////////////////////////////////////////////////////////////////////////////////


enum tt { LIBERA, PML4, TABELLA_CONDIVISA, TABELLA_PRIVATA,
	PAGINA_CONDIVISA, PAGINA_PRIVATA }; // [6.3]
struct des_pf {			// [6.3]
	tt contenuto;	// uno dei valori precedenti
	union {
		struct {
			bool	residente;	// pagina residente o meno
			natl	processo;	// identificatore di processo
			natl	ind_massa;	// indirizzo della pagina in memoria di massa
			addr	ind_virtuale;
			// indirizzo virtuale della pagina (ultimi 12 bit a 0)
			// o della prima pagina indirizzata da una tabella (ultimi 22 bit uguali a 0)
			natl	contatore;	// contatore per le statistiche
		} pt; 	// rilevante se "contenuto" non vale LIBERA
		struct  { // informazioni relative a una pagina libera
			des_pf*	prossima_libera;// indice del descrittore della prossima pagina libera
		} avl;	// rilevante se "contenuto" vale LIBERA
	};
};

des_pf dpf[N_DPF];	// vettore di descrittori di pagine fisiche [9.3]
addr prima_pf_utile;	// indirizzo fisico della prima pagina fisica di M2 [9.3]
des_pf* pagine_libere;	// indice del descrittore della prima pagina libera [9.3]

// [9.3]
des_pf* descrittore_pf(addr indirizzo_pf)
{
	natq indice = ((natq)indirizzo_pf - (natq)prima_pf_utile) / DIM_PAGINA;
	return &dpf[indice];
}

// [9.3]
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

	prima_pf_utile = (addr)(DIM_M1);

	pagine_libere = &dpf[0];
	for (natl i = 0; i < N_DPF - 1; i++) {
		dpf[i].contenuto = LIBERA;
		dpf[i].avl.prossima_libera = &dpf[i + 1];
	}
	dpf[N_DPF - 1].avl.prossima_libera = 0;

	return true;
}

des_pf* alloca_pagina_fisica_libera()	// [6.4]
{
	des_pf* p = pagine_libere;
	if (pagine_libere != 0)
		pagine_libere = pagine_libere->avl.prossima_libera;
	return p;
}

// (* rende di nuovo libera la pagina fisica il cui descrittore di pagina fisica
//    ha per indice "i"
void rilascia_pagina_fisica(des_pf* ppf)
{
	ppf->contenuto = LIBERA;
	ppf->avl.prossima_libera = pagine_libere;
	pagine_libere = ppf;
}

des_pf* alloca_pagina_fisica(natl proc, tt tipo, addr ind_virt)
{
	des_pf *ppf = alloca_pagina_fisica_libera();
	if (ppf == 0) {
					panic("NO MORE PF :(" ); //TODO SWAP
		//if ( (ppf = scegli_vittima2(proc, tipo, ind_virt)) && scollega(ppf) )
		//	scarica(ppf);
	}
	return ppf;
}
// *)
// )


/////////////////////////////////////////////////////////////////////////////////
//                         PAGINAZIONE [6]                                     //
/////////////////////////////////////////////////////////////////////////////////

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

// (*) attenzione, la convenzione Intel e' diversa da quella
// illustrata nel libro: 0 = sistema, 1 = utente.

//TODO bit 63 NX (no-exec): che fare?
const natq ACCB_MASK  = 0x00000000000000FF; // maschera per il byte di accesso
const natq ADDR_MASK  = 0xFFFFFFFFFFFFF000; // maschera per l'indirizzo
const natq INDMASS_MASK = 0xFFFFFFFFFFFFFFE0; // maschera per l'indirizzo in mem. di massa
const natq INDMASS_SHIFT = 5;	    // primo bit che contiene l'ind. in mem. di massa
// )

bool  extr_P(natq descrittore)			// [6.3]
{ // (
	return (descrittore & BIT_P); // )
}
bool extr_D(natq descrittore)			// [6.3]
{ // (
	return (descrittore & BIT_D); // )
}
bool extr_A(natq descrittore)			// [6.3]
{ // (
	return (descrittore & BIT_A); // )
}
addr extr_IND_FISICO(natq descrittore)		// [6.3]
{ // (
	return (addr)(descrittore & ADDR_MASK); // )
}
natq extr_IND_MASSA(natq descrittore)		// [6.3]
{ // (
	return (descrittore & INDMASS_MASK) >> INDMASS_SHIFT; // )
}
void set_P(natq& descrittore, bool bitP)	// [6.3]
{ // (
	if (bitP)
		descrittore |= BIT_P;
	else
		descrittore &= ~BIT_P; // )
}
void set_A(natq& descrittore, bool bitA)	// [6.3]
{ // (
	if (bitA)
		descrittore |= BIT_A;
	else
		descrittore &= ~BIT_A; // )
}
// (* definiamo anche la seguente funzione:
//    clear_IND_M: azzera il campo M (indirizzo in memoria di massa)
void clear_IND_MASSA(natq& descrittore)
{
	descrittore &= ~INDMASS_MASK;
}
// *)
void  set_IND_FISICO(natq& descrittore, addr ind_fisico) // [6.3]
{ // (
	clear_IND_MASSA(descrittore);
	descrittore |= ((natq)(ind_fisico) & ADDR_MASK); // )
}
void set_IND_MASSA(natq& descrittore, natq ind_massa) // [6.3]
{ // (
	clear_IND_MASSA(descrittore);
	descrittore |= (ind_massa << INDMASS_SHIFT); // )
}

void set_D(natq& descrittore, bool bitD) // [6.3]
{ // (
	if (bitD)
		descrittore |= BIT_D;
	else
		descrittore &= ~BIT_D; // )
}


// funzione che restituisce i bit 47-39 di "ind_virt"
// (indice nel PML4)
int i_PML4(addr ind_virt)
{
	return ((natq)ind_virt & 0x0000ff8000000000) >> 39;
}

// funzione che restituisce i bit 38-30 di "ind_virt"
// (indice nel PDP)
int i_PDP(addr ind_virt)
{
	return ((natq)ind_virt & 0x0000007fc0000000) >> 30;
}

// funzione che restituisce i bit 29-21 di "ind_virt"
// (indice nel PD)
int i_PD(addr ind_virt)
{
	return ((natq)ind_virt & 0x000000003fe00000) >> 21;
}

// funzione che restituisce i bit 20-12 di "ind_virt"
// (indice nel PT)
int i_PT(addr ind_virt)
{
	return ((natq)ind_virt & 0x00000000001ff000) >> 12;
}

natq& get_entry(addr tab, natl index) // [6.3]
{
	natq *pd = static_cast<natq*>(tab);
	return  pd[index];
}

void set_entry(addr tab, natl index, natq entry)
{
	natq *pd = static_cast<natq*>(tab);
	pd[index] = entry;
}

// ( [P_MEM_VIRT]

//mappa le ntab pagine virtuali a partire dall'indirizzo virt_start agli
//indirizzi fisici
//che partono da phys_start, in sequenza.
bool sequential_map(addr pml4,addr phys_start, addr virt_start, natl npag, natq flags)
{
	natb *indv = static_cast<natb*>(virt_start),
		 *indf = static_cast<natb*>(phys_start);
	for (natl i = 0; i < npag; i++, indv += DIM_PAGINA, indf += DIM_PAGINA)
	{
	//----PML4-------------------------------------------------
		natq pml4e = get_entry(pml4, i_PML4(indv));
		addr pdp;
		if (! extr_P(pml4e))
		{
			des_pf* ppf = alloca_pagina_fisica_libera();
			if (ppf == 0) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
				return false;
			}
			ppf->contenuto = TABELLA_CONDIVISA;
			ppf->pt.residente = true;
			pdp = indirizzo_pf(ppf);
			memset(pdp, 0, DIM_PAGINA);

			pml4e = ((natq)pdp & ADDR_MASK) | flags | BIT_P;
			set_entry(pml4, i_PML4(indv), pml4e);
		}
		else
		{
			pdp = extr_IND_FISICO(pml4e);
		}
	//-----------------------------------------------------
	//----PDP-------------------------------------------------
		natq pdpe = get_entry(pdp,i_PDP(indv));
		addr pd;
		if (! extr_P(pdpe))
		{
			des_pf* ppf = alloca_pagina_fisica_libera();
			if (ppf == 0) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
					return false;
			}
			ppf->contenuto = TABELLA_CONDIVISA;
			ppf->pt.residente = true;
			pd = indirizzo_pf(ppf);
			memset(pd, 0, DIM_PAGINA);

			pdpe = ((natq)pd & ADDR_MASK) | flags | BIT_P;
			set_entry(pdp, i_PDP(indv), pdpe);
		}
		else
		{
			pd = extr_IND_FISICO(pdpe);
		}
	//-----------------------------------------------------
	//----PD-------------------------------------------------
		natq pde = get_entry(pd,i_PD(indv));
		addr pt;
		if (! extr_P(pde))
		{
			des_pf* ppf = alloca_pagina_fisica_libera();
			if (ppf == 0) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
				return false;
			}
			ppf->contenuto = TABELLA_CONDIVISA;
			ppf->pt.residente = true;
			pt = indirizzo_pf(ppf);
			memset(pt, 0, DIM_PAGINA);

			pde = ((natq)pt & ADDR_MASK) | flags | BIT_P;
			set_entry(pd, i_PD(indv), pde);
		}
		else
		{
			pt = extr_IND_FISICO(pde);
		}
	//-----------------------------------------------------
	//----PT-------------------------------------------------
		natq pte = ((natq)indf & ADDR_MASK) | flags | BIT_P;
		set_entry(pt, i_PT(indv), pte);
	}

//	flog(LOG_WARN, "phys_start=  %p",phys_start);
//	flog(LOG_WARN, "virt_start=  %p",virt_start);
//	flog(LOG_WARN, "pml4=        %p",pml4);
//	natq pml4e = get_entry(pml4,i_PML4(virt_start));
//	flog(LOG_WARN, "pml4e=       %p",pml4e);
//	addr pdp = extr_IND_FISICO(pml4e);
//	
//	natq pdpe = get_entry(pdp,i_PDP(virt_start));
//	flog(LOG_WARN, "pdpe=        %p",pdpe);
//	addr pd = extr_IND_FISICO(pdpe);
//	
//	natq pde = get_entry(pd,i_PD(virt_start));
//	flog(LOG_WARN, "pde=         %p",pde);
//	addr pt = extr_IND_FISICO(pde);
//
//	natq pte = get_entry(pt,i_PT(virt_start));
//	flog(LOG_WARN, "pte=         %p",pte);
	
	return true;
}


// mappa tutti gli indirizzi a partire da start (incluso) fino ad end (escluso)
// in modo che l'indirizzo virtuale coincida con l'indirizzo fisico.
// start e end devono essere allineati alla pagina.
bool identity_map(addr pml4, addr start, addr end, natq flags)
{
	natl npag = (static_cast<natb*>(end) - static_cast<natb*>(start)) / DIM_PAGINA;
	return sequential_map(pml4, start, start, npag, flags);
}
// mappa la memoria fisica, dall'indirizzo 0 all'indirizzo max_mem, nella
// memoria virtuale gestita dal direttorio pdir
// (la funzione viene usata in fase di inizializzazione)
bool crea_finestra_FM(addr pml4)
{
	return identity_map(pml4, (addr)DIM_PAGINA, (addr)MEM_TOT, BIT_RW);
}


// carica un nuovo valore in cr3 [vedi sistema.S]
extern "C" void loadCR3(addr dir);

// restituisce il valore corrente di cr3 [vedi sistema.S]
extern "C" addr readCR3();

//invalida il TLB
extern "C" void invalida_TLB();


// )
const natl MAX_IRQ  = 24;
// )


/////////////////////////////////////////////////////////////////////////////////
//                    SUPPORTO PCI                                             //
/////////////////////////////////////////////////////////////////////////////////
const ioaddr PCI_CAP = 0x0CF8;
const ioaddr PCI_CDP = 0x0CFC;

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
		panic("abort");
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
		panic("abort");
	}
}
/////////////////////////////////////////////////////////////////////////////////
//                    SUPPORTO PCI                                             //
/////////////////////////////////////////////////////////////////////////////////
natl dim_pci_condiviso = 20*MiB;
const addr PCI_startmem = reinterpret_cast<addr>(0x00000000fec00000);
natb* const inizio_pci_condiviso = reinterpret_cast<natb*>(0x00000000fec00000);

// ( [P_PCI]

// mappa in memoria virtuale la porzione di spazio fisico dedicata all'I/O (PCI e altro)
bool crea_finestra_PCI(addr pml4)
{
	return sequential_map(pml4,
			PCI_startmem,
			inizio_pci_condiviso,
			dim_pci_condiviso/DIM_PAGINA,
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

// ( [P_IOAPIC]

// parte piu' significativa di una redirection table entry
const natl IOAPIC_DEST_MSK = 0xFF000000; // destination field mask
const natl IOAPIC_DEST_SHF = 24;	 // destination field shift
// parte meno significativa di una redirection table entry
const natl IOAPIC_MIRQ_BIT = (1U << 16); // mask irq bit
const natl IOAPIC_TRGM_BIT = (1U << 15); // trigger mode (1=level, 0=edge)
const natl IOAPIC_IPOL_BIT = (1U << 13); // interrupt polarity (0=high, 1=low)
const natl IOAPIC_DSTM_BIT = (1U << 11); // destination mode (0=physical, 1=logical)
const natl IOAPIC_DELM_MSK = 0x00000700; // delivery mode field mask (000=fixed)
const natl IOAPIC_DELM_SHF = 8;		 // delivery mode field shift
const natl IOAPIC_VECT_MSK = 0x000000FF; // vector field mask
const natl IOAPIC_VECT_SHF = 0;		 // vector field shift

struct ioapic_des {
	natl* IOREGSEL;
	natl* IOWIN;
	natl* EOI;
	natb  RTO;	// Redirection Table Offset
};

extern "C"  ioapic_des ioapic;

natl ioapic_in(natb off)
{
	*ioapic.IOREGSEL = off;
	return *ioapic.IOWIN;
}

void ioapic_out(natb off, natl v)
{
	*ioapic.IOREGSEL = off;
	*ioapic.IOWIN = v;
}

natl ioapic_read_rth(natb irq)
{
	return ioapic_in(ioapic.RTO + irq * 2 + 1);
}

void ioapic_write_rth(natb irq, natl w)
{
	ioapic_out(ioapic.RTO + irq * 2 + 1, w);
}

natl ioapic_read_rtl(natb irq)
{
	return ioapic_in(ioapic.RTO + irq * 2);
}

void ioapic_write_rtl(natb irq, natl w)
{
	ioapic_out(ioapic.RTO + irq * 2, w);
}

void ioapic_set_VECT(natl irq, natb vec)
{
	natl work = ioapic_read_rtl(irq);
	work = (work & ~IOAPIC_VECT_MSK) | (vec << IOAPIC_VECT_SHF);
	ioapic_write_rtl(irq, work);
}

void ioapic_set_MIRQ(natl irq, bool v)
{
	natl work = ioapic_read_rtl(irq);
	if (v)
		work |= IOAPIC_MIRQ_BIT;
	else
		work &= ~IOAPIC_MIRQ_BIT;
	ioapic_write_rtl(irq, work);
}

extern "C" void ioapic_mask(natl irq)
{
	ioapic_set_MIRQ(irq, true);
}

extern "C" void ioapic_unmask(natl irq)
{
	ioapic_set_MIRQ(irq, false);
}

void ioapic_set_TRGM(natl irq, bool v)
{
	natl work = ioapic_read_rtl(irq);
	if (v)
		work |= IOAPIC_TRGM_BIT;
	else
		work &= ~IOAPIC_TRGM_BIT;
	ioapic_write_rtl(irq, work);
}


const natw PIIX3_VENDOR_ID = 0x8086;
const natw PIIX3_DEVICE_ID = 0x7000;
const natb PIIX3_APICBASE = 0x80;
const natb PIIX3_XBCS = 0x4e;
const natl PIIX3_XBCS_ENABLE = (1U << 8);

natl apicbase_getxy(natl apicbase)
{
	return (apicbase & 0x01F) << 10U;
}

extern "C" void disable_8259();
bool ioapic_init()
{
	natw l = 0;
	// trovare il PIIX3 e
	if (!pci_find_dev(l, PIIX3_DEVICE_ID, PIIX3_VENDOR_ID)) {
		flog(LOG_WARN, "PIIX3 non trovato");
		return false;
	}
	flog(LOG_DEBUG, "PIIX3: trovato a %2x.%2x.%2x",
			pci_getbus(l), pci_getdev(l), pci_getfun(l));
	// 	inizializzare IOREGSEL e IOWIN
	natb apicbase;
	apicbase = pci_read_confb(l, PIIX3_APICBASE);
	natq tmp_IOREGSEL = (natq)ioapic.IOREGSEL | apicbase_getxy(apicbase);
	natq tmp_IOWIN    = (natq)ioapic.IOWIN    | apicbase_getxy(apicbase);
	// 	trasformiamo gli indirizzi fisici in virtuali
	ioapic.IOREGSEL = (natl*)(tmp_IOREGSEL - (natq)PCI_startmem + (natq)inizio_pci_condiviso);
	ioapic.IOWIN    = (natl*)(tmp_IOWIN    - (natq)PCI_startmem + (natq)inizio_pci_condiviso);
	ioapic.EOI	= (natl*)((natq)ioapic.EOI - (natq)PCI_startmem + (natq)inizio_pci_condiviso);
	flog(LOG_DEBUG, "IOAPIC: ioregsel %p, iowin %p", ioapic.IOREGSEL, ioapic.IOWIN);
	// 	abilitare il /CS per l'IOAPIC
	natl xbcs;
	xbcs = pci_read_confl(l, PIIX3_XBCS);
	xbcs |= PIIX3_XBCS_ENABLE;
	pci_write_confl(l, PIIX3_XBCS, xbcs);
	disable_8259();
	// riempire la redirection table
	for (natb i = 0; i < MAX_IRQ; i++) {
		ioapic_write_rth(i, 0);
		ioapic_write_rtl(i, IOAPIC_MIRQ_BIT | IOAPIC_TRGM_BIT);
	}
	ioapic_set_VECT(0, VETT_0);	ioapic_set_TRGM(0, false);
	ioapic_set_VECT(1, VETT_1);
	ioapic_set_VECT(2, VETT_2);	ioapic_set_TRGM(2, false);
	ioapic_set_VECT(3, VETT_3);
	ioapic_set_VECT(4, VETT_4);
	ioapic_set_VECT(5, VETT_5);
	ioapic_set_VECT(6, VETT_6);
	ioapic_set_VECT(7, VETT_7);
	ioapic_set_VECT(8, VETT_8);
	ioapic_set_VECT(9, VETT_9);
	ioapic_set_VECT(10, VETT_10);
	ioapic_set_VECT(11, VETT_11);
	ioapic_set_VECT(12, VETT_12);
	ioapic_set_VECT(13, VETT_13);
	ioapic_set_VECT(14, VETT_14);
	ioapic_set_VECT(15, VETT_15);
	ioapic_set_VECT(16, VETT_16);
	ioapic_set_VECT(17, VETT_17);
	ioapic_set_VECT(18, VETT_18);
	ioapic_set_VECT(19, VETT_19);
	ioapic_set_VECT(20, VETT_20);
	ioapic_set_VECT(21, VETT_21);
	ioapic_set_VECT(22, VETT_22);
	ioapic_set_VECT(23, VETT_23);
	return true;
}

extern "C" void ioapic_reset()
{
	for (natb i = 0; i < MAX_IRQ; i++) {
		ioapic_write_rth(i, 0);
		ioapic_write_rtl(i, IOAPIC_MIRQ_BIT | IOAPIC_TRGM_BIT);
	}
	natw l = 0;
	pci_find_dev(l, PIIX3_DEVICE_ID, PIIX3_VENDOR_ID);
	natl xbcs;
	xbcs = pci_read_confl(l, PIIX3_XBCS);
	xbcs &= ~PIIX3_XBCS_ENABLE;
	pci_write_confl(l, PIIX3_XBCS, xbcs);
}

// )

// timer
extern natl ticks;
extern natl clocks_per_usec;
extern "C" void attiva_timer(natl count);
const natl DELAY = 59659;

//////////////////////////////////////////////////////////////////////////////
//							GDT												//
/////////////////////////////////////////////////////////////////////////////
extern "C" void init_gdt();
extern "C" void set_tss_stack(addr stack);


addr crea_pila(addr pml4,int dim, bool utente) //per ora una sola pagina
{
	natq flags = BIT_RW;
	addr pila_virt = reinterpret_cast<addr>(0xfffffa0000000000);    //per ora è hardcodato
	if(utente == true)
	{
		flags |= BIT_US;
		pila_virt = reinterpret_cast<addr>(0xfffffb0000000000);    //per ora è hardcodato
	}

	for(int i = 0; i<dim;i+=DIM_PAGINA)
	{
		des_pf* ppf = alloca_pagina_fisica_libera();
		if (ppf == 0) {
			panic("impossibile allocare pila");
		}
		ppf->contenuto = PAGINA_PRIVATA;
		ppf->pt.residente = true;   //per ora no swap
		natq *pila_phys = static_cast<natq*>(indirizzo_pf(ppf));
		
		addr pag_pila = reinterpret_cast<addr>((natq)pila_virt+i);
		sequential_map(pml4,pila_phys,pag_pila,1,flags);
	}

	return reinterpret_cast<addr>((natq)pila_virt + dim - 8);
	
}

///////////////////////////////////////////////////////////////////////////////
//                          TESTS                                            //
///////////////////////////////////////////////////////////////////////////////
extern "C" addr pag_utente_virt;
extern "C" addr pag_utente;
extern "C" void goto_user_proc(addr rip, addr rsp);
extern "C" natl alloca_tss(des_proc*);
void test_userspace(addr testpml4)
{
	proc_elem* pe;
	des_proc* des_dd;
	natl id;
	
	natq* pila_sistema = static_cast<natq*>(crea_pila(testpml4,16*KiB,false));
	natq* pila_utente  = static_cast<natq*>(crea_pila(testpml4,16*KiB,true ));

	sequential_map(testpml4,pag_utente,pag_utente_virt,1,BIT_RW | BIT_US);

	des_dd = static_cast<des_proc*>(alloca(sizeof(des_proc)));
	if (des_dd == 0) goto errore;
	memset(des_dd, 0, sizeof(des_proc));
	des_dd->cr3 = testpml4;
	des_dd->punt_nucleo = pila_sistema;

	pe = static_cast<proc_elem*>(alloca(sizeof(proc_elem)));
	if (pe == 0) goto errore;

	id = alloca_tss(des_dd);
	flog(LOG_DEBUG,"id=%d",id);
	if (id == 0) goto errore;
	
    pe->id = id;
    pe->precedenza = 1;
	pe->puntatore = 0;

	esecuzione = pe;
	goto_user_proc(pag_utente_virt,pila_utente);

errore:
	panic("errore test_userspace");

}

extern "C" void cmain ()
{
	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v4.02");
	init_gdt();
	flog(LOG_INFO, "gdt inizializzata!");

	// (* Assegna allo heap di sistema HEAP_SIZE byte nel primo MiB
	heap_init((addr)4096, HEAP_SIZE);
	flog(LOG_INFO, "Heap di sistema: %d B", HEAP_SIZE);
	// *)

	// ( il resto della memoria e' per le pagine fisiche (parte M2, vedi [1.10])
	init_dpf();
	flog(LOG_INFO, "Pagine fisiche: %d", N_DPF);
	// )

	des_pf* ppf = alloca_pagina_fisica_libera();
	if (ppf == 0) {
		flog(LOG_ERR, "Impossibile allocare testpml4");
		panic("?");
	}
	ppf->contenuto = PML4;
	ppf->pt.residente = true;
	addr testpml4 = indirizzo_pf(ppf);
	memset(testpml4, 0, DIM_PAGINA);
	if(!crea_finestra_FM(testpml4))
			goto error;
	flog(LOG_INFO, "Creata finestra FM!");
	if(!crea_finestra_PCI(testpml4))
			goto error;
	flog(LOG_INFO, "Creata finestra PCI!");
	loadCR3(testpml4);
	flog(LOG_INFO, "Caricato CR3!");

	ioapic_init();
	//asm("sti");
	flog(LOG_INFO, "APIC inizializzato e interruzioni abilitate!");

	attiva_timer(DELAY);
	flog(LOG_INFO, "timer attivato!");

	test_userspace(testpml4);

	flog(LOG_INFO, "Uscita!");
	return;

error:
	flog(LOG_ERR,"Error!");
	panic("Error!");

}



