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
	{	inputb(iLSR, s);    }
	while (! (s & 0x20));
	outputb(c, iTHR);
}

// invia un nuovo messaggio sul log
extern "C" void do_log(log_sev sev, const char* buf, natl quanti)
{
	const char* lev[] = { "DBG", "INF", "WRN", "ERR", "USR" };
	if (sev > MAX_LOG) {
		flog(LOG_WARN, "Livello di log errato: %d", sev);
		//abort_p();
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

// gestore generico di eccezioni (chiamata da tutti i gestori di eccezioni in 
// sistema.S, tranne il gestore di page fault)
extern "C" void gestore_eccezioni(int tipo, unsigned errore,
				  addr rip, unsigned cs, unsigned rflag)
{
	//if (rip < fine_codice_sistema) {
	//	flog(LOG_ERR, "Eccezione %d, eip = %x, errore = %x", tipo, rip, errore);
		//panic("eccezione dal modulo sistema");
	//}
	flog(LOG_WARN, "Eccezione %d, errore %x", tipo, errore);
	flog(LOG_WARN, "rflag = %x, eip = %x, cs = %x", rflag, rip, cs);
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

	prima_pf_utile = (addr)DIM_M1;

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

natq& singolo_des(addr iff, natl index) // [6.3]
{
	natq *pd = static_cast<natq*>(iff);
	return pd[index];
}

natq& get_PML4E(addr PML4, addr ind_virt) // [6.3]
{
	return singolo_des(PML4, i_PML4(ind_virt));
}

natq& get_PDPE(addr PDP, addr ind_virt) // [6.3]
{
	return singolo_des(PDP, i_PDP(ind_virt));
}

natq& get_PDE(addr PD, addr ind_virt) // [6.3]
{
	return singolo_des(PD, i_PD(ind_virt));
}

natq& get_PTE(addr PT, addr ind_virt) // [6.3]
{
	return singolo_des(PT, i_PT(ind_virt));
}

// ( [P_MEM_VIRT]
//
// mappa le ntab pagine virtuali a partire dall'indirizzo virt_start agli 
// indirizzi fisici
// che partono da phys_start, in sequenza.
bool sequential_map(addr pml4,addr phys_start, addr virt_start, natl npag, natl flags)
{
	natb *indv = static_cast<natb*>(virt_start),
	     *indf = static_cast<natb*>(phys_start);
	for (natl i = 0; i < npag; i++, indv += DIM_PAGINA, indf += DIM_PAGINA) {
		natl dt = get_destab(direttorio, indv);
		addr tabella;
		if (! extr_P(dt)) {
			des_pf* ppf = alloca_pagina_fisica_libera();
			if (ppf == 0) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
				return false;
			}
			ppf->contenuto = TABELLA_CONDIVISA;
			ppf->pt.residente = true;
			tabella = indirizzo_pf(ppf);

			dt = ((natl)tabella & ADDR_MASK) | flags | BIT_P;
			set_destab(direttorio, indv, dt);
		} else {
			tabella = extr_IND_FISICO(dt);
		}
		natl dp = ((natl)indf & ADDR_MASK) | flags | BIT_P;
		set_despag(tabella, indv, dp);
	}
	return true;
}



//// ( [P_IOAPIC]
//#include "apic.h"
//
//bool ioapic_init()
//{
//        if (!apic_init())
//                return false;
//        natl offset = inizio_pci_condiviso - (natb*)PCI_startmem;
//        IOREGSEL = (natl*)((natb*)IOREGSEL + offset);
//        IOWIN = (natl*)((natb*)IOWIN + offset);
//        IOAPIC_EOIR = (natl*)((natb*)IOAPIC_EOIR + offset);
//        apic_reset();
//        apic_set_VECT(0, VETT_0);
//        apic_set_VECT(1, VETT_1);
//        apic_set_VECT(2, VETT_2);
//        apic_set_VECT(3, VETT_3);
//        apic_set_VECT(4, VETT_4);
//        apic_set_VECT(5, VETT_5);
//        apic_set_VECT(6, VETT_6);
//        apic_set_VECT(7, VETT_7);
//        apic_set_VECT(8, VETT_8);
//        apic_set_VECT(9, VETT_9);
//        apic_set_VECT(10, VETT_10);
//        apic_set_VECT(11, VETT_11);
//        apic_set_VECT(12, VETT_12);
//        apic_set_VECT(13, VETT_13);
//        apic_set_VECT(14, VETT_14);
//        apic_set_VECT(15, VETT_15);
//        apic_set_VECT(16, VETT_16);
//        apic_set_VECT(17, VETT_17);
//        apic_set_VECT(18, VETT_18);
//        apic_set_VECT(19, VETT_19);
//        apic_set_VECT(20, VETT_20);
//        apic_set_VECT(21, VETT_21);
//        apic_set_VECT(22, VETT_22);
//        apic_set_VECT(23, VETT_23);
//        return true;
//}
//
//// )


extern "C" void cmain ()
{
	
	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v4.02");

	
	// (* Assegna allo heap di sistema HEAP_SIZE byte nel primo MiB
	heap_init((addr)4096, HEAP_SIZE);
	flog(LOG_INFO, "Heap di sistema: %d B", HEAP_SIZE);
	// *)

	// ( il resto della memoria e' per le pagine fisiche (parte M2, vedi [1.10])
	init_dpf();
	flog(LOG_INFO, "Pagine fisiche: %d", N_DPF);
	// )

	flog(LOG_INFO, "Uscita!");
	return;

}



