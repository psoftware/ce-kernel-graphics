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
}__attribute__((packed));

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



extern "C" addr readCR2();
extern "C" addr readCR3();
int i_tab(addr, int lib);
natq& get_entry(addr,natl);
addr extr_IND_FISICO(natq);
// gestore generico di eccezioni (chiamata da tutti i gestori di eccezioni in
// sistema.S, tranne il gestore di page fault)
extern "C" void gestore_eccezioni(int tipo, natq errore,
					addr rip, natq cs, natq rflag)
{
	flog(LOG_WARN, "Eccezione %d, errore %x", tipo, errore);
	flog(LOG_WARN, "rflag = %x, rip = %p, cs = %x", rflag, rip, cs);
	if(tipo==14)
	{
		addr CR2 = readCR2();
		flog(LOG_WARN, "CR2=   %p",CR2);
		addr CR3 = readCR3();
		flog(LOG_WARN, "CR3=   %p",CR3);
		natq pml4e = get_entry(CR3,i_tab(CR2, 4));
		flog(LOG_WARN, "pml4e= %p",pml4e);
		addr pdp = extr_IND_FISICO(pml4e);
		
		natq pdpe = get_entry(pdp,i_tab(CR2, 3));
		flog(LOG_WARN, "pdpe=  %p",pdpe);
		addr pd = extr_IND_FISICO(pdpe);
		
		natq pde = get_entry(pd,i_tab(CR2, 2));
		flog(LOG_WARN, "pde=   %p",pde);
		addr pt = extr_IND_FISICO(pde);

		natq pte = get_entry(pt,i_tab(CR2, 1));
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

//per semplicita permettiamo di avere sono interi pdp condivisi e assegnamo ad 
//ogni zona logicamente distinta di memoria un intera entry nel pml4 (tanto ce
//ne sono ben 512 grandi 512Gb ciascuna per un totale di 256Tb indirizzabili)
//NB: gli indirizzi finali saranno in forma canonica, quindi le entry dalla 256
//in poi si trovano nella meta alta dello spazio di indirizzamento

#define I_finestra_FM  		0     //per ora coincide con sistema_condiviso
#define I_sistema_privato  	1
#define I_IO_condiviso   	2
#define I_PCI_condiviso  	3
#define I_utente_condiviso    510     //higher half
#define I_utente_privato      511     //higher half

#define ADDRESS(index) reinterpret_cast<addr>((index<256)?\
                       ((natq)index << 39):\
                       (((natq)(index) << 39) | (0xffffL<<48)))


const addr inizio_finestra_FM = ADDRESS(I_finestra_FM); 
const addr inizio_sistema_privato = ADDRESS(I_sistema_privato);
const addr inizio_io_condiviso = ADDRESS(I_IO_condiviso);
const addr inizio_pci_condiviso = ADDRESS(I_PCI_condiviso);
const addr inizio_utente_condiviso = ADDRESS(I_utente_condiviso);
const addr inizio_utente_privato = ADDRESS(I_utente_privato);

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

int i_tab(addr ind_virt, int liv)
{
	int shift = 12 + (liv - 1) * 9;
	natq mask = 0x1ffUL << shift;
	return ((natq)ind_virt & mask) >> shift;
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
		addr tab = pml4;
		for (int j = 4; j >= 2; j--) {
			natq& e = get_entry(tab, i_tab(indv, j));
			if (! extr_P(e)) {
				des_pf* ppf = alloca_pagina_fisica_libera();
				if (ppf == 0)
					goto error;
				ppf->contenuto = TABELLA_CONDIVISA;
				ppf->pt.residente = true;
				addr ntab = indirizzo_pf(ppf);
				memset(ntab, 0, DIM_PAGINA);

				e = ((natq)ntab & ADDR_MASK) | flags | BIT_P;
			}
			tab = extr_IND_FISICO(e);
		}
		natq pte = ((natq)indf & ADDR_MASK) | flags | BIT_P;
		set_entry(tab, i_tab(indv, 1), pte);
	}

	return true;

error:
	flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
	return false;
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

extern "C" void ioapic_send_EOI()
{
        *ioapic.EOI = 0;
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

addr crea_pila(addr pml4,int dim, bool utente) 
{
	natq flags = BIT_RW;
	
	addr pila_virt = inizio_sistema_privato;
	if(utente == true)
	{
		flags |= BIT_US;
		pila_virt = inizio_utente_privato;    //per ora  hardcodato
	}

	addr pila_phys = 0;
	for(int i = 0; i<dim;i+=DIM_PAGINA)
	{
		des_pf* ppf = alloca_pagina_fisica_libera();
		if (ppf == 0) {
			panic("impossibile allocare pila");
		}
		ppf->contenuto = PAGINA_PRIVATA;
		ppf->pt.residente = true;   //per ora no swap
		pila_phys = indirizzo_pf(ppf);
		
		addr pag_pila = reinterpret_cast<addr>((natq)pila_virt+i);
		sequential_map(pml4,pila_phys,pag_pila,1,flags);
	}

	return reinterpret_cast<addr>((natq)pila_phys + DIM_PAGINA );
	
}

addr crea_pml4()
{
	des_pf* ppf = alloca_pagina_fisica_libera();
	if (ppf == 0) {
		flog(LOG_ERR, "Impossibile allocare pml4");
		panic("errore");
	}
	ppf->contenuto = PML4;
	ppf->pt.residente = true;
	addr pml4 = indirizzo_pf(ppf);
	memset(pml4, 0, DIM_PAGINA);

	return pml4;
}


///////////////////////////////////////////////////////////////////////////////
//                          TESTS                                            //
///////////////////////////////////////////////////////////////////////////////
void copia_pagine_condivise(addr srcpml4, addr destpml4)
{
	natq finestra_FM = get_entry(srcpml4,I_finestra_FM);
	set_entry(destpml4,I_finestra_FM,finestra_FM);
	
	natq PCI_condiviso = get_entry(srcpml4,I_PCI_condiviso);
	set_entry(destpml4,I_PCI_condiviso,PCI_condiviso);
	
	natq utente_condiviso = get_entry(srcpml4,I_utente_condiviso);
	set_entry(destpml4,I_utente_condiviso,utente_condiviso);
}



extern "C" void goto_user();
extern "C" natl alloca_tss(des_proc*);
const natl BIT_IF = 1L << 9;
proc_elem* crea_processo(addr phys_start,natl precedenza, natq param)
{
	proc_elem* pe;
	des_proc* dp;
	natl id;	
	natq* pila_sistema_iretq;

	addr pml4 = crea_pml4();
	//flog(LOG_INFO,"pml4=%x",pml4);

	addr pml4_padre = readCR3();

	copia_pagine_condivise(pml4_padre,pml4);

	addr pila_sistema = crea_pila(pml4,DIM_SYS_STACK,false);
	crea_pila(pml4,DIM_USR_STACK, true ); //pila_tuente

	sequential_map(pml4,phys_start,inizio_utente_condiviso,1,BIT_RW | BIT_US);

	dp = static_cast<des_proc*>(alloca(sizeof(des_proc)));
	if (dp == 0) goto errore;
	memset(dp, 0, sizeof(des_proc));
	dp->cr3 = pml4;
	dp->punt_nucleo = reinterpret_cast<addr>((natq)inizio_sistema_privato+DIM_SYS_STACK);

	pe = static_cast<proc_elem*>(alloca(sizeof(proc_elem)));
	if (pe == 0) goto errore;
	
	id = alloca_tss(dp);

	if (id == 0) goto errore;
	
	pe->id = id;
	pe->precedenza = precedenza;
	pe->puntatore = 0;

	pila_sistema_iretq = static_cast<natq*>(pila_sistema);

	*(pila_sistema_iretq-1) = SEL_DATI_UTENTE;
	*(pila_sistema_iretq-2) = (natq)inizio_utente_privato + DIM_USR_STACK;
	*(pila_sistema_iretq-3) = BIT_IF; //flags
	*(pila_sistema_iretq-4) = SEL_CODICE_UTENTE;
	*(pila_sistema_iretq-5) = (natq)inizio_utente_condiviso;

	dp->contesto[I_RSP] = (natq)inizio_sistema_privato + DIM_SYS_STACK - 8*5; 
	dp->contesto[I_RDI] = param;

	return pe;

errore:
	return 0;
}

extern "C" natb proc0;
extern "C" natb dummy_proc;
void test_userspace()
{
	proc_elem* d = crea_processo(&dummy_proc,0,13);
	flog(LOG_INFO, "Creo il processo dummy");
	inserimento_lista(pronti,d);
	for(int i = 1; i< 20; i++)
	{
		proc_elem* e = crea_processo(&proc0,i,i);
		flog(LOG_INFO, "Creo il processo proc0(%d)",i);
		inserimento_lista(pronti,e);
	}
	
	schedulatore();
	flog(LOG_INFO, "Salto a spazio utente");
	goto_user();
}

// driver del timer [4.16][9.6]
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

// super blocco (vedi [10.5] e [P_SWAP] avanti)
struct superblock_t {
	char	magic[8];
	natq	bm_start;
	natq	blocks;
	natq	directory;
	natq	user_entry;
	natq	user_end;
	natq	io_entry;
	natq	io_end;
	int	checksum;
};

// descrittore di swap (vedi [P_SWAP] avanti)
struct des_swap {
	natl *free;		// bitmap dei blocchi liberi
	superblock_t sb;	// contenuto del superblocco 
} swap_dev; 	// c'e' un unico oggetto swap
bool swap_init();
extern "C" void cmain ()
{
	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v4.02");
	init_gdt();
	flog(LOG_INFO, "gdt inizializzata!");

	// (* Assegna allo heap di sistema HEAP_SIZE byte nel primo MiB
	heap_init((addr)HEAP_START, HEAP_SIZE);
	flog(LOG_INFO, "Heap di sistema: %x B @%x", HEAP_SIZE, HEAP_START);
	// *)

	// ( il resto della memoria e' per le pagine fisiche (parte M2, vedi [1.10])
	init_dpf();
	flog(LOG_INFO, "Pagine fisiche: %d", N_DPF);
	// )

	addr initpml4 = crea_pml4();

	if(!crea_finestra_FM(initpml4))
			goto error;
	flog(LOG_INFO, "Creata finestra FM!");
	if(!crea_finestra_PCI(initpml4))
			goto error;
	flog(LOG_INFO, "Creata finestra PCI!");
	loadCR3(initpml4);
	flog(LOG_INFO, "Caricato CR3!");

	ioapic_init();
	flog(LOG_INFO, "APIC inizializzato!");
	
	// ( inizializzazione dello swap, che comprende la lettura
	//   degli entry point di start_io e start_utente (vedi [10.4])
	if (!swap_init())
			goto error;
	flog(LOG_INFO, "sb: blocks=%d user=%p/%p io=%p/%p", 
			swap_dev.sb.blocks,
			swap_dev.sb.user_entry,
			swap_dev.sb.user_end,
			swap_dev.sb.io_entry,
			swap_dev.sb.io_end);
	// )

	attiva_timer(DELAY);
	flog(LOG_INFO, "timer attivato!");

	test_userspace();

	flog(LOG_INFO, "Uscita!");
	return;

error:
	flog(LOG_ERR,"Error!");
	panic("Error!");

}

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
	natb s;
	do 
		inputb(iSTS, s);
	while (s & 0x80);

	outputb(lba_0, iSNR); 			// indirizzo del settore e selezione drive
	outputb(lba_1, iCNL);
	outputb(lba_2, iCNH);
	natb hnd = (lba_3 & 0x0F) | 0xE0;
	outputb(hnd, iHND);
	outputb(quanti, iSCR); 			// numero di settori
	outputb(0x0A, iDCR);			// disabilitazione richieste di interruzione
	outputb(0x20, iCMD);			// comando di lettura

	for (int i = 0; i < quanti; i++) {
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
	natb s;
	do 
		inputb(iSTS, s);
	while (s & 0x80);
	outputb(lba_0, iSNR);					// indirizzo del settore e selezione drive
	outputb(lba_1, iCNL);
	outputb(lba_2, iCNH);
	natb hnd = (lba_3 & 0x0F) | 0xE0;
	outputb(hnd, iHND);
	outputb(quanti, iSCR);					// numero di settori
	outputb(0x0A, iDCR);					// disabilitazione richieste di interruzione
	outputb(0x30, iCMD);					// comando di scrittura
	for (int i = 0; i < quanti; i++) {
		do
			inputb(iSTS, s);
		while ((s & 0x88) != 0x08);
		for (int j = 0; j < 512/2; j++)
			outputw(vetto[i*512/2 + j], iBR);
	}
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
void scrivi_speciale(addr src, natl blocco);
void leggi_speciale(addr dest, natl blocco);

natl ceild(natl v, natl q)
{
	return v / q + (v % q != 0 ? 1 : 0);
}

// vedi [10.5]
natq alloca_blocco()
{
	natl i = 0;
	natq risu = 0;
	natq vecsize = ceild(swap_dev.sb.blocks, sizeof(natl) * 8);
	static natb pagina_di_zeri[DIM_PAGINA] = { 0 };

	// saltiamo le parole lunghe che contengono solo zeri (blocchi tutti occupati)
	while (i < vecsize && swap_dev.free[i] == 0) i++;
	if (i < vecsize) {
		natl pos = __builtin_ffs(swap_dev.free[i]) - 1;
		swap_dev.free[i] &= ~(1UL << pos);
		risu = pos + sizeof(natl) * 8 * i;
	} 
	if (risu) {
		scrivi_speciale(pagina_di_zeri, risu);
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
void leggi_speciale(addr dest, natl blocco)
{
	natl sector = blocco * 8;

	leggisett(sector, 8, static_cast<natw*>(dest));
}

void scrivi_speciale(addr src, natl blocco)
{
	natl sector = blocco * 8;

	scrivisett(sector, 8, static_cast<natw*>(src));
}

// lettura dallo swap (da utilizzare nella fase di inizializzazione)
bool leggi_swap(addr buf, natl first, natl bytes)
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
	return leggi_swap(swap_dev.free,
		swap_dev.sb.bm_start * 8, pages * DIM_PAGINA);
}
// )
