// sistema.cpp
//
#include "mboot.h"
#include "costanti.h"
#include "tipo.h"

// funzione che permette di convertire un indirizzo in un naturale
natl a2n(addr v)
{
	return reinterpret_cast<natl>(v);
}

// e viceversa
addr n2a(natl l)
{
	return reinterpret_cast<addr>(l);
}

///////////////////////////////////////////////////////////////////////////
// funzioni di utilita'
// ////////////////////////////////////////////////////////////////////////

// allocazione a mappa di bit
// In varie occasioni e' necessario gestire l'allocazione di un unsieme di 
// risorse omogenee (ad esempio, i semafori). Per far cio', conviene utilizzare 
// una "mappa di bit": una struttura dati in cui ogni bit corrisponde ad una 
// delle risorse da allocare.  Se il bit vale 0, la corrispondente risorsa e' 
// occupata, altrimenti e' libera.
struct bm_t {
	natl *vect;	// vettore che contiene i bit della mappa
	natl size;	// numero di bit nella mappa
	natl vecsize;	// corrispondente dimensione del vettore
};
// inizializza una mappa di bit 
void bm_create(bm_t *bm, natl *buffer, natl size);
// alloca una risorsa dalla mappa bm, restituendone il numero in pos. La 
// funzione ritorna false se tutte le risorse sono gia' occupate (in quel caso, 
// pos non e' significativa)
bool bm_alloc(bm_t *bm, natl& pos);
// libera la risorsa numero pos nella mappa bm
void bm_free(bm_t *bm, natl pos);
/////


// funzioni che eseguono alcuni calcoli richiesti abbastanza frequentemente
//
// restituisce il minimo naturale maggiore o uguale a v/q
natl ceild(natl v, natl q)
{
	return v / q + (v % q != 0 ? 1 : 0);
}

// restituisce il valore di "v" allineato a un multiplo di "a"
natl allinea(natl v, natl a)
{
	return (v % a == 0 ? v : ((v + a - 1) / a) * a);
}
addr allineav(addr v, natl a)
{
	return n2a(allinea(a2n(v), a));
}

// funzioni per la manipolazione di stringhe
//
extern "C" void *memcpy(void* dest, const void* src, unsigned int n);
// copia c nei primi n byte della zona di memoria puntata da dest
extern "C" void *memset(void* dest, int c, natl n);
// restituisce true se le due stringe first e second sono uguali
bool str_equal(const char* first, const char* second);

// funzioni utili alla gestione degli errori
// 
// invia un messaggio formatto al log
extern "C" void flog(log_sev, const char* fmt, ...);
// restituisce l'ultimo messaggio inviato al log
const char* last_log();
// restituisce l'ultimo messaggio di errore inviato al log
const char* last_log_err();
// blocca il sistema, stampando msg e altre informazioni di debug
extern "C" void panic(const char *msg);
// termina forzatamente un processo
extern "C" void abort_p();

///////////////////////////////////////////////////////////////////////
// ALLOCAZIONE DELLA MEMORIA FISICA                                  //
// ////////////////////////////////////////////////////////////////////

// La memoria fisica viene gestita in due fasi: durante l'inizializzazione, si
// tiene traccia dell'indirizzo dell'ultimo byte non "occupato", tramite il
// puntatore mem_upper. Tale puntatore viene fatto avanzare mano a mano che si
// decide come utilizzare la memoria fisica a disposizione:
// 1) all'inizio, poiche' il sistema e' stato caricato in memoria fisica dal
// bootstrap loader, il primo byte non occupato e' quello successivo all'ultimo
// indirizzo occupato dal sistema stesso (e' il linker, tramite il simbolo
// predefinito "_end", che ci permette di conoscere, staticamente, questo
// indirizzo [vedi sistema.S])
// 2) di seguito al modulo sistema, viene allocato l'array di descrittori di 
// pagine fisiche [vedi avanti], la cui dimensione e' calcolata dinamicamente, 
// in base al numero di pagine fisiche rimanenti.
// 3) tutta la memoria fisica restante, a partire dal primo indirizzo multiplo
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

// forward declaration
// (assegna quanti byte, a partire da indirizzo, allo heap di sistema)
void free_interna(addr indirizzo, natl quanti);

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
	addr appoggio = n2a(a2n(mem_upper) + quanti);
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
		saltati = a2n(indirizzo) - a2n(mem_upper);
		free_interna(mem_upper, saltati);
		mem_upper = indirizzo;
	}
	return saltati; // se e' negativo, "indirizzo" era minore di mem_upper
			// (ovvero, "indirizzo" era gia' occupato), oppure era
			// maggiore della memoria disponibile
}


// HEAP DI SISTEMA
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
			addr pnuovo = n2a(a2n(p) + quanti);
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
	if (prec != 0 && n2a(a2n(prec + 1) + prec->dimensione) == indirizzo) {

		// controlliamo se la zona e' unificabile anche alla eventuale
		// zona che la segue
		if (scorri != 0 && n2a(a2n(indirizzo) + quanti) == scorri) {
			
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

	} else if (scorri != 0 && n2a(a2n(indirizzo) + quanti) == scorri) {

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

/////////////////////////////////////////////////////////////////////////
// PAGINAZIONE                                                         //
/////////////////////////////////////////////////////////////////////////
// definiamo alcune costanti utili per la manipolazione dei descrittori
// di pagina e di tabella. Assegneremo a tali descrittori il tipo "natl"
// e li manipoleremo tramite maschere e operazioni sui bit.
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

// i descrittori di pagina/tabella con P=0 contengono l'indirizzo
// in memoria di massa della corrispondente pagina/tabella.
// Tale indirizzo consiste nell'indice di un blocco nel dispositivo
// di swap. L'indice viene memorizzato nei bit 31-5. I 5 bit
// meno significativi vengono usati per il bit P e per memorizzare
// alcune informazioni che e' necessario conoscere anche per le pagine non presenti.
// Queste informazioni corrispondono ai valori per i bit RW, US, PWT e PCD.

const natl BLOCK_MASK = 0xFFFFFFE0; // maschera per l'indirizzo in mem. di massa
const natl BLOCK_SHIFT = 5;	    // primo bit che contiene l'ind. in mem. di massa

// funzione che restituisce i 10 bit piu' significativi di "ind_virt"
// (indice nel direttorio)
int i_dir(addr ind_virt)
{
	return (a2n(ind_virt) & 0xFFC00000) >> 22;
}

// funzione che restituisce i bit 22-12 di "ind_virt"
// (indice nella tabella delle pagine)
int i_tab(addr ind_virt)
{
	return (a2n(ind_virt) & 0x003FF000) >> 12;
}

// funzione che restiuisce il descrittore di indice
// "index" nel direttorio o tabella di indirizzo "dirtab"
natl& get_des(addr dirtab, int index)
{
	// convertiamo "dirtab" in un array di descrittori
	natl *pd = static_cast<natl*>(dirtab);
	return pd[index];
}

// funzione che sovrascrive il descrittore di indice
// "index" nel direttorio o tabella di indirizzo "dirtab"
// con il nuovo valore dato da "des"
void set_des(addr dirtab, int index, natl des)
{
	natl *pd = static_cast<natl*>(dirtab);
	pd[index] = des;
}

// funzione di comodo che restituisce il descrittore
// di tabella associato all'indirizzo "ind_virt"
// nel direttorio di indirizzo "dir"
natl& get_destab(addr dir, addr ind_virt)
{
	return get_des(dir, i_dir(ind_virt));
}

// funzione di comodo che sovrascrive il descrittore
// di tabella associato all'indirizzo "ind_virt"
// nel direttorio di indirizzo "dir" con il nuovo
// valore dato da "destab"
void set_destab(addr dir, addr ind_virt, natl destab)
{
	set_des(dir, i_dir(ind_virt), destab);
}

// funzione di comodo che restituisce il descrittore
// di pagina associato all'indirizzo "ind_virt"
// nella tabella delle pagine di indirizzo "tab"
natl& get_despag(addr tab, addr ind_virt)
{
	return get_des(tab, i_tab(ind_virt));
}

// funzione di comodo che sovrascrive il descrittore
// di pagina associato all'indirizzo "ind_virt"
// nella tabella delle pagine di indirizzo "dir" con il nuovo
// valore dato da "despag"
void set_despag(addr tab, addr ind_virt, natl despag)
{
	set_des(tab, i_tab(ind_virt), despag);
}

// funzione che sovrascrive tutti i descrittori
// di pagina/tabella nel direttorio o tabella
// di indirizzo "dirtab" con il nuovo valore dato
// da "des". (Utile in fase di inizializzazione)
void set_all_des(addr dirtab, natl des)
{
	natl *pd = static_cast<natl*>(dirtab);
	for (int i = 0; i < 1024; i++)
		pd[i] = des;
}

// funzione che copia (parte di) un direttorio
// ("src") in un altro ("dst"). Vengono copiati
// tutti i descrittori a partire da quello
// associato all'indirizzo "start" (incluso)
// fino a quello associato all'indirizzo "stop" (escluso).
// (Utile in fase di inizializzazione)
void copy_dir(addr src, addr dst, addr start, addr stop)
{
	natl *pdsrc = static_cast<natl*>(src),
	     *pddst = static_cast<natl*>(dst);
	for (int i = i_dir(start); i < i_dir(stop); i++)
		pddst[i] = pdsrc[i];
}

bool ext_P(natl des)
{
	return (des & BIT_P);
}

void set_P(natl& des)
{
	des |= BIT_P;
}

void reset_P(natl& des)
{
	des &= ~BIT_P;
}

bool ext_D(natl des)
{
	return (des & BIT_D);
}

void set_D(natl& des)
{
	des |= BIT_D;
}

bool ext_A(natl des)
{
	return (des & BIT_A);
}

void reset_A(natl& des)
{
	des &= ~BIT_A;
}

addr ext_indfis(natl des)
{
	return reinterpret_cast<addr>(des & ADDR_MASK);
}

void clear_block(natl& des)
{
	des &= ~BLOCK_MASK;
}


void  set_indfis(natl& des, addr ind_fis)
{
	clear_block(des);
	des |= (reinterpret_cast<natl>(ind_fis) & ADDR_MASK);
}

natl ext_block(natl des)
{
	return (des & BLOCK_MASK) >> BLOCK_SHIFT;
}

void set_block(natl& des, natl block)
{
	clear_block(des);
	des |= (block << BLOCK_SHIFT);
}

addr get_dir(natl proc);
addr get_tab(natl proc, addr ind_virt);

natl& get_destab(natl proc, addr ind_virt)
{
	return get_destab(get_dir(proc), ind_virt);
}

natl& get_despag(natl proc, addr ind_virt)
{
	natl dt = get_destab(proc, ind_virt);
	return get_despag(ext_indfis(dt), ind_virt);
}

// carica un nuovo valore in cr3 [vedi sistema.S]
extern "C" void loadCR3(addr dir);

// restituisce il valore corrente di cr3 [vedi sistema.S]
extern "C" addr readCR3();

// attiva la paginazione [vedi sistema.S]
extern "C" void attiva_paginazione();

// possibili contenuti delle pagine fisiche
enum cont_pf {
	LIBERA,			// pagina allocabile
	DIRETTORIO,		// direttorio delle tabelle
	TABELLA,		// tabella delle pagine
	PAGINA_VIRTUALE };	// pagina virtuale

// descrittore di pagina fisica
struct des_pf {
	cont_pf		contenuto;	// uno dei valori precedenti
	bool		residente;
	union {
		struct { // informazioni relative a una pagina virtuale o tabella
			natl	blocco;		// blocco in memoria di massa
			natl	processo;	// processo a cui appartiene la pagina (tabella)
			addr	ind_virtuale;   // indirizzo virtuale della pagina (tabella)
			natl	contatore;	// contatore per le statistiche LRU
		} pt;
		struct  { // informazioni relative a una pagina libera
			int		prossima_libera;
		} avl;
		// non ci sono informazioni per il caso contenuto==DIRETTORIO
	};
};


des_pf* pagine_fisiche;
natl num_pagine_fisiche;
natl pagine_libere = 0xFFFFFFFF;

addr prima_pf_utile;

natl indice_pf(addr pagina)
{
	return (a2n(pagina) - a2n(prima_pf_utile)) / DIM_PAGINA;
}

addr indirizzo_pf(natl indice)
{
	return n2a(a2n(prima_pf_utile) + indice * DIM_PAGINA);
}


// init_pagine_fisiche viene chiamata in fase di inizalizzazione.  Tutta la
// memoria non ancora occupata viene usata per le pagine fisiche.  La funzione
// si preoccupa anche di allocare lo spazio per i descrittori di pagina fisica,
// e di inizializzarli in modo che tutte le pagine fisiche risultino libere
bool init_pagine_fisiche()
{

	// allineamo mem_upper alla linea, per motivi di efficienza
	salta_a(allineav(mem_upper, sizeof(int)));

	// calcoliamo quanta memoria principale rimane
	int dimensione = a2n(max_mem_upper) - a2n(mem_upper);

	if (dimensione <= 0) {
		flog(LOG_ERR, "Non ci sono pagine libere");
		return false;
	}

	// calcoliamo quante pagine fisiche possiamo definire (tenendo conto
	// del fatto che ogni pagina fisica avra' bisogno di un descrittore)
	natl quante = dimensione / (DIM_PAGINA + sizeof(des_pf));

	// allochiamo i corrispondenti descrittori di pagina fisica
	pagine_fisiche = reinterpret_cast<des_pf*>(occupa(sizeof(des_pf) * quante));

	// riallineamo mem_upper a un multiplo di pagina
	salta_a(allineav(mem_upper, DIM_PAGINA));

	// ricalcoliamo quante col nuovo mem_upper, per sicurezza
	// (sara' minore o uguale al precedente)
	quante = (a2n(max_mem_upper) - a2n(mem_upper)) / DIM_PAGINA;

	// occupiamo il resto della memoria principale con le pagine fisiche;
	// ricordiamo l'indirizzo della prima pagina fisica e il loro numero
	prima_pf_utile = occupa(quante * DIM_PAGINA);
	num_pagine_fisiche = quante;

	// se resta qualcosa (improbabile), lo diamo all'allocatore a lista
	salta_a(max_mem_upper);

	// costruiamo la lista delle pagine fisiche libere
	pagine_libere = 0;
	for (natl i = 0; i < quante - 1; i++) {
		pagine_fisiche[i].contenuto = LIBERA;
		pagine_fisiche[i].avl.prossima_libera = i + 1;
	}
	pagine_fisiche[quante - 1].avl.prossima_libera = -1;

	return true;
}

// funzione di allocazione generica di una pagina
// Nota: restituisce l'indice del *descrittore di pagina fisica* della pagina
// (uno dei "des_pf" di cui sopra), non un puntatore alla pagina
natl alloca_pagina_fisica()
{
	natl p = pagine_libere;
	if (pagine_libere != 0xFFFFFFFF)
		pagine_libere = pagine_fisiche[pagine_libere].avl.prossima_libera;
	return p;
}

// rende di nuovo libera la pagina fisica il cui descrittore di pagina fisica
// ha per indice "i"
void rilascia_pagina_fisica(natl i)
{
	des_pf* p = &pagine_fisiche[i];
	p->contenuto = LIBERA;
	p->avl.prossima_libera = pagine_libere;
	pagine_libere = i;
}

natl& get_entdes(natl indice)
{
	des_pf *ppf =&pagine_fisiche[indice];
	if (ppf->contenuto == TABELLA) 
		return get_destab(ppf->pt.processo, ppf->pt.ind_virtuale);
	return get_despag(ppf->pt.processo, ppf->pt.ind_virtuale);
}


addr collega(natl indice)
{
	des_pf* ppf = &pagine_fisiche[indice];
	natl& des = get_entdes(indice);

	ppf->pt.blocco = ext_block(des);
	set_indfis(des, indirizzo_pf(indice));
	set_P(des);
	ppf->pt.contatore  = 0x10000000;
	if (ppf->contenuto == PAGINA_VIRTUALE) {
		addr ind_fis_tab = get_tab(ppf->pt.processo, ppf->pt.ind_virtuale);
		ppf = &pagine_fisiche[indice_pf(ind_fis_tab)];
		ppf->pt.contatore |= 0x10000000;
	}
}

extern "C" void invalida_entrata_TLB(addr ind_virtuale);

bool scollega(natl indice)
{
	bool bitD;

	des_pf *ppf =&pagine_fisiche[indice];
	natl &des = get_entdes(indice);
	bitD = ext_D(des);
	set_block(des, ppf->pt.blocco);
	reset_P(des);
	invalida_entrata_TLB(ppf->pt.ind_virtuale);
	return bitD || ppf->contenuto == TABELLA;
}


// mappa la memoria fisica, dall'indirizzo 0 all'indirizzo max_mem, nella 
// memoria virtuale gestita dal direttorio pdir
// (la funzione viene usata in fase di inizializzazione)
bool mappa_mem_fisica(addr direttorio, addr max_mem)
{
	for (addr ind = n2a(0); ind < max_mem; ind = n2a(a2n(ind) + DIM_PAGINA)) {
		natl dt = get_destab(direttorio, ind);
		addr tabella;
		if (! ext_P(dt)) {
			natl indice = alloca_pagina_fisica();
			if (indice == 0xFFFFFFFF) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
				return false;
			}
			des_pf *ppf = &pagine_fisiche[indice];
			// evitiamo di marcare la pagina come TABELLA
			// in modo che "aggiorna_statistiche" non la veda
			// ppf->contenuto = TABELLA;
			ppf->residente = true;
			tabella = indirizzo_pf(indice);

			dt = (a2n(tabella) & ADDR_MASK) | BIT_RW | BIT_P;
			set_destab(direttorio, ind, dt);
		} else {
			tabella = ext_indfis(dt);
		}
		natl dp = get_despag(tabella, ind);
		dp = (a2n(ind) & ADDR_MASK) | BIT_RW | BIT_P; // | BIT_GLOBAL
		set_despag(tabella, ind, dp);
	}
	return true;
}
	



/////////////////////////////////////////////////////////////////////////////
// MEMORIA VIRTUALE                                                        //
/////////////////////////////////////////////////////////////////////////////
//
// ACCESSO ALLO SWAP
//
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
// contenuto nello swap (serve come indirizzo di partenza dello heap utente)
// - l'indirizzo virtuale dell'entry point del modulo io contenuto nello swap
// - l'indirizzo virtuale successivo all'ultimo byte occupato dal modulo io
// - checksum: somma dei valori precedenti (serve ad essere ragionevolmente 
// sicuri che quello che abbiamo letto dall'hard disk e' effettivamente un 
// superblocco di questo sistema, e che il superblocco stesso non e' stato 
// corrotto)
//

// descrittore di una partizione dell'hard disk
struct partizione {
	int  type;	// tipo della partizione
	natl first;	// primo settore della partizione
	natl dim;	// dimensione in settori
	partizione*  next;
};

// funzioni per la lettura/scrittura da hard disk
extern "C" void readhd_n(int ata, int drv, natw vetti[],
		natl primo, natb quanti, natb &errore);
extern "C" void writehd_n(int ata, int drv, natw vetto[],
		natl primo, natb quanti, natb &errore);
// utile per il debug: invia al log un messaggio relativo all'errore che e' 
// stato riscontrato
void hd_print_error(int i, int d, int sect, natb errore);
// cerca la partizione specificata
partizione* hd_find_partition(short ind_ata, short drv, int p);


// super blocco (vedi sopra)
struct superblock_t {
	char		magic[4];
	natl	bm_start;
	int		blocks;
	natl	directory;
	void		(*user_entry)(int);
	addr		user_end;
	void		(*io_entry)(int);
	addr		io_end;
	natl	checksum;
};

// descrittore di swap (vedi sopra)
struct des_swap {
	short channel;		// canale: 0 = primario, 1 = secondario
	short drive;		// dispositivo: 0 = master, 1 = slave
	partizione* part;	// partizione all'interno del dispositivo
	bm_t free;		// bitmap dei blocchi liberi
	superblock_t sb;	// contenuto del superblocco 
} swap_dev; 	// c'e' un unico oggetto swap


natl alloca_blocco()
{
	natl blocco;
 	if (!bm_alloc(&swap_dev.free, blocco))
		return 0;
	return blocco;
}
void dealloca_blocco(natl blocco)
{
	if (blocco != 0)
		bm_free(&swap_dev.free, blocco);
}

void wait_gdb() {}

// legge dallo swap il blocco il cui indice e' passato come primo parametro, 
// copiandone il contenuto a partire dall'indirizzo "dest"
void leggi_blocco(natl blocco, void* dest)
{
	natb errore;

	// ogni blocco (4096 byte) e' grande 8 settori (512 byte)
	// calcoliamo l'indice del primo settore da leggere
	natl sector = blocco * 8 + swap_dev.part->first;

	// il seguente controllo e' di fondamentale importanza: l'hardware non 
	// sa niente dell'esistenza delle partizioni, quindi niente ci 
	// impedisce di leggere o scrivere, per sbaglio, in un'altra partizione
	if (blocco < 0 || sector + 8 > (swap_dev.part->first + swap_dev.part->dim)) {
		wait_gdb();
		flog(LOG_ERR, "Accesso al di fuori della partizione");
		// fermiamo tutto
		panic("Errore interno");
	}

	readhd_n(swap_dev.channel, swap_dev.drive, static_cast<natw*>(dest), sector, 8, errore);

	if (errore != 0) { 
		flog(LOG_ERR, "Impossibile leggere il blocco %d", blocco);
		hd_print_error(swap_dev.channel, swap_dev.drive, blocco * 8, errore);
		panic("Errore sull'hard disk");
	}
}


// scrive nello swap il blocco il cui indice e' passato come primo parametro, 
// copiandone il contenuto a partire dall'indirizzo "dest"
void scrivi_blocco(natl blocco, void* dest)
{
	natb errore;
	natl sector = blocco * 8 + swap_dev.part->first;

	if (blocco < 0 || sector + 8 > (swap_dev.part->first + swap_dev.part->dim)) {
		flog(LOG_ERR, "Accesso al di fuori della partizione");
		// come sopra
		panic("Errore interno");
	}
	
	writehd_n(swap_dev.channel, swap_dev.drive, static_cast<natw*>(dest), sector, 8, errore);

	if (errore != 0) { 
		flog(LOG_ERR, "Impossibile scrivere il blocco %d", blocco);
		hd_print_error(swap_dev.channel, swap_dev.drive, blocco * 8, errore);
		panic("Errore sull'hard disk");
	}
}

// lettura dallo swap (da utilizzare nella fase di inizializzazione)
bool leggi_swap(void* buf, natl first, natl bytes, const char* msg)
{
	natb errore;
	natl sector = first + swap_dev.part->first;

	if (first < 0 || first + bytes > swap_dev.part->dim) {
		flog(LOG_ERR, "Accesso al di fuori della partizione: %d+%d", first, bytes);
		return false;
	}
	
	readhd_n(swap_dev.channel, swap_dev.drive, static_cast<natw*>(buf), sector, ceild(bytes, 512), errore);

	if (errore != 0) { 
		flog(LOG_ERR, "\nImpossibile leggere %s", msg);
		hd_print_error(swap_dev.channel, swap_dev.drive, sector, errore);
		return false;
	}
	return true;
}

// inizializzazione del descrittore di swap
char read_buf[512];
bool swap_init(int swap_ch, int swap_drv, int swap_part)
{
	partizione* part;

	// l'utente *deve* specificare una partizione
	if (swap_ch == -1 || swap_drv == -1 || swap_part == -1) {
		flog(LOG_ERR, "Partizione di swap non specificata!");
		return false;
	}

	part = hd_find_partition(swap_ch, swap_drv, swap_part);
	if (part == 0) {
		flog(LOG_ERR, "Swap: partizione non esistente o non rilevata");
		return false;
	}
		
	// se la partizione non comprende l'intero hard disk (swap_part > 0), 
	// controlliamo che abbia il tipo giusto
	if (swap_part && part->type != 0x3f) {
		flog(LOG_ERR, "Tipo della partizione di swap scorretto (%d)", part->type);
		return false;
	}

	swap_dev.channel = swap_ch;
	swap_dev.drive   = swap_drv;
	swap_dev.part    = part;

	// lettura del superblocco
	flog(LOG_DEBUG, "lettura del superblocco dall'area di swap...");
	if (!leggi_swap(read_buf, 1, sizeof(superblock_t), "il superblocco"))
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

	flog(LOG_DEBUG, "lettura della bitmap dei blocchi...");

	// calcoliamo la dimensione della mappa di bit in pagine/blocchi
	natl pages = ceild(swap_dev.sb.blocks, DIM_PAGINA * 8);

	// quindi allochiamo in memoria un buffer che possa contenerla
	natl* buf = static_cast<natl*>(alloca((pages * DIM_PAGINA)));
	if (buf == 0) {
		flog(LOG_ERR, "Impossibile allocare la bitmap dei blocchi");
		return false;
	}

	// inizializziamo l'allocatore a mappa di bit
	bm_create(&swap_dev.free, buf, swap_dev.sb.blocks);

	// infine, leggiamo la mappa di bit dalla partizione di swap
	return leggi_swap(buf, swap_dev.sb.bm_start * 8, pages * DIM_PAGINA, "la bitmap dei blocchi");
}


// ROUTINE DEL TIMER per la paginazione
void aggiorna_statistiche()
{
	des_pf *ppf1, *ppf2;
	addr tab, pag;
	natl bitA;

	for (natl i = 0; i < num_pagine_fisiche; i++) {
		ppf1 = &pagine_fisiche[i];
		switch (ppf1->contenuto) {
		case TABELLA:
		case DIRETTORIO:
			tab = indirizzo_pf(i);
			for (int j = 0; j < 1024; j++) {
				natl& des = get_des(tab, j);
				if (ext_P(des)) {
					pag = ext_indfis(des);
					bitA = ext_A(des);
					reset_A(des);
					ppf2 = &pagine_fisiche[indice_pf(pag)];
					ppf2->pt.contatore >>= 1;
					ppf2->pt.contatore |= bitA << 31U;
				}
			}
			break;
		default:
			break;
		}
	}
	loadCR3(readCR3());
}

int pf_mutex;
extern "C" int sem_ini(int);
extern "C" void sem_wait(int);
extern "C" void sem_signal(int);
extern "C" int activate_p(void f(int), int a, natl prio, natb liv);
extern "C" void terminate_p();

int scegli_vittima(int indice_vietato);
bool carica(natl indice);
void scarica(natl indice);
addr get_parent(natl proc, cont_pf tipo, addr ind_virt)
{
	if (tipo == TABELLA)
		return get_dir(proc);
	return ext_indfis(get_destab(proc, ind_virt));
}

addr swap_ent(natl proc, cont_pf tipo, addr ind_virt, bool residente) {
	des_pf *ppf;

	natl indice = alloca_pagina_fisica();
	if (indice == 0xFFFFFFFF) {
		addr escludi = get_parent(proc, tipo, ind_virt);
		natl vittima = scegli_vittima(indice_pf(escludi));
		if (vittima == 0xFFFFFFFF)
			return 0;
		bool modificata = scollega(vittima);
		if (modificata)
			scarica(vittima);
		indice = vittima;
	}
	ppf = &pagine_fisiche[indice];
	ppf->contenuto = tipo;
	ppf->residente = residente;
	ppf->pt.processo = proc;
	ppf->pt.ind_virtuale = ind_virt;
	collega(indice);
	if (!carica(indice))
		return 0;
	return indirizzo_pf(indice);
}

addr swap(natl proc, addr ind_virtuale)
{
	bool bitP;
	natl dt, dp;
	addr ind_fis_tab, ind_fis_pag;
	des_pf *ppf;

	dt = get_destab(proc, ind_virtuale);
	bitP = ext_P(dt);
	if (!bitP)
		ind_fis_tab = swap_ent(proc, TABELLA, ind_virtuale, false);
	else
		ind_fis_tab = ext_indfis(dt);
	if (ind_fis_tab == 0)
		return 0;
	ppf =&pagine_fisiche[indice_pf(ind_fis_tab)];
	dp = get_despag(ppf->pt.processo, ind_virtuale);
	bitP = ext_P(dp);
	if (!bitP)
		return swap_ent(ppf->pt.processo, PAGINA_VIRTUALE,  ind_virtuale, false);
	return ext_indfis(dp);

}

bool carica(natl indice)
{
	des_pf *ppf =&pagine_fisiche[indice];
	addr dest = indirizzo_pf(indice);
	if (ppf->pt.blocco == 0) {
		if (!ppf->residente) {
			ppf->pt.blocco = alloca_blocco();
			if (ppf->pt.blocco == 0) {
				flog(LOG_WARN, "spazio nello swap insufficiente");
				return false;
			}
		}
		if (ppf->contenuto == PAGINA_VIRTUALE) {
			memset(dest, 0, DIM_PAGINA);
		} else {
			natl des = get_entdes(indice);
			reset_P(des);
			clear_block(des);
			set_all_des(dest, des);
		}
	} else 
		leggi_blocco(ppf->pt.blocco, dest);
	return true;
}

void scarica(natl indice)
{
	des_pf *ppf =&pagine_fisiche[indice];
	scrivi_blocco(ppf->pt.blocco, indirizzo_pf(indice));
}

extern "C" void invalida_entrata_TLB(addr ind_virtuale);

int scegli_vittima(int indice_vietato)
{
	natl i, vittima;
	des_pf *ppf, *pvittima;
	i = 0;
	while (i < num_pagine_fisiche && pagine_fisiche[i].residente)
		i++;
	if (i >= num_pagine_fisiche) return 0xFFFFFFFF;
	vittima = i;
	for (i++; i < num_pagine_fisiche; i++) {
		ppf = &pagine_fisiche[i];
		pvittima = &pagine_fisiche[vittima];
		if (ppf->residente || i == indice_vietato)
			continue;
		switch (ppf->contenuto) {
		case PAGINA_VIRTUALE:
			if (ppf->pt.contatore < pvittima->pt.contatore ||
			    ppf->pt.contatore == pvittima->pt.contatore &&
			    		pvittima->contenuto == TABELLA)
				vittima = i;
			break;
		case TABELLA:
			if (ppf->pt.contatore < pvittima->pt.contatore) 
				vittima = i;
			break;
		default:
			break;
		}
	}
	return vittima;
}

natl proc_corrente();

extern "C" bool c_resident(addr ind_virt, natl quanti)
{
	addr ind_virt_pag, ind_fis_pag;
	des_pf* ppf;
	natl np;
	natl proc = proc_corrente();

	// usa "ind_virt" e "quanti" per calcolare l'indirizzo virtuale
	// della prima pagina (variabile "ind_virt_pag") e il numero
	// di pagine (variabile "np") da rendere residenti
	ind_virt_pag = n2a(a2n(ind_virt) & ~(DIM_PAGINA -1));
	np = ceild(a2n(ind_virt) + quanti - a2n(ind_virt_pag), DIM_PAGINA);
	for (int i = 0; i < np; i++) {
		sem_wait(pf_mutex);
		ind_fis_pag = swap(proc, ind_virt_pag);
		if (ind_fis_pag != 0) {
			ppf = &pagine_fisiche[indice_pf(ind_fis_pag)];
			ppf->residente = true;
		}
		sem_signal(pf_mutex);
		if (ind_fis_pag == 0)
			return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////////////
// CREAZIONE PROCESSI                                                           //
//////////////////////////////////////////////////////////////////////////////////

// descrittore di processo
// (scriviamo come commento il campo che si trova nel libro, e 
// di seguito il/i vero/i campo/i che tiene/tengono
// conto della struttura completa
// del descrittore di processo)
struct des_proc {		// offset:
//	natw nome;
	natw link;		// 0
//	addr punt_nucleo;
	addr esp0;		// 4
//	addr riservato;
	natl ss0;		// 8
	addr esp1;		// 12
	natl ss1;		// 16
	addr esp2;		// 20
	natl ss2;		// 24

//	addr cr3;
	addr cr3;		// 28

//	natl contesto[N_REG];
	natl eip;		// 32, non utilizzato
	natl eflags;		// 36, non utilizzato

	natl eax;		// 40
	natl ecx;		// 44
	natl edx;		// 48
	natl ebx;		// 52
	natl esp;		// 56
	natl ebp;		// 60
	natl esi;		// 64
	natl edi;		// 68

	natl es;		// 72
	natl cs; 		// 76
	natl ss;		// 80
	natl ds;		// 84
	natl fs;		// 86
	natl gs;		// 90
	natl ldt;		// 94, non utilizzato

	natl io_map;		// 100, non utilizzato

	struct {
		natl cr;	// 104
		natl sr;	// 108
		natl tr;	// 112
		natl ip;	// 116
		natl is;	// 120
		natl op;	// 124
		natl os;	// 128
		char regs[80];		// 132
	} fpu;

	natb cpl;			// 212
}; // dimensione 212 + 1 + 3(allineamento) = 216

// numero di processi attivi
int processi = 0;

// elemento di una coda di processi
struct proc_elem {
	natw nome;
	natl precedenza;
	proc_elem *puntatore;
};
extern proc_elem *esecuzione;
extern proc_elem *pronti;

// manipolazione delle code di processi
void inserimento_lista(proc_elem *&p_lista, proc_elem *p_elem);
void rimozione_lista(proc_elem *&p_lista, proc_elem *&p_elem);


// funzioni usate da crea_processo
addr crea_pila_utente(natl proc);
addr crea_pila_sistema(natl proc);
void rilascia_tutto(addr direttorio, addr start, int ntab);
extern "C" int alloca_tss(des_proc* p);
extern "C" void rilascia_tss(int indice);

// ritorna il descrittore del processo id
extern "C" des_proc *des_p(short id);

// restituisce l'indirizzo fisico del direttorio del processo proc
addr get_dir(natl proc)
{
	des_proc *p = des_p(proc);
	return p->cr3;
}

addr get_tab(natl proc, addr ind_virt)
{
	natl dt = get_destab(proc, ind_virt);
	return ext_indfis(dt);
}

natl proc_corrente()
{
	return esecuzione->nome;
}

// elemento di coda e descrittore del primo processo (quello che esegue il 
// codice di inizializzazione e la funzione main)
proc_elem init;
des_proc des_main;

proc_elem* crea_processo(void f(int), int a, int prio, char liv)
{
	proc_elem	*p;			// proc_elem per il nuovo processo
	int		identifier;		// indice del tss nella gdt 
	des_proc	*pdes_proc;		// descrittore di processo
	addr		pdirettorio;		// direttorio del processo
	addr		ppdir;			// direttorio del padre

	addr		pila_sistema,		// punt. di lavoro
			pila_utente;		// punt. di lavoro
	natl		*pl;
	int		indice;
	
	p = static_cast<proc_elem*>(alloca(sizeof(proc_elem)));
        if (p == 0) goto errore1;

	pdes_proc = static_cast<des_proc*>(alloca(sizeof(des_proc)));
	if (pdes_proc == 0) goto errore2;
	memset(pdes_proc, 0, sizeof(des_proc));

	identifier = alloca_tss(pdes_proc);
	if (identifier == 0) goto errore3;

        p->nome = identifier << 3U;
        p->precedenza = prio;

	// pagina fisica per il direttorio del processo
	indice = alloca_pagina_fisica();
	if (indice == 0xFFFFFFFF) {
		int vittima = scegli_vittima(0xFFFFFFFF);
		if (vittima == 0xFFFFFFFF)
			goto errore4;
		bool modificata = scollega(vittima);
		if (modificata)
			scarica(vittima);
		indice = vittima;
	}
	pdirettorio = indirizzo_pf(indice);
	pagine_fisiche[indice].contenuto = DIRETTORIO;
	pagine_fisiche[indice].residente = true;
	pdes_proc->cr3 = pdirettorio;

	// il direttorio viene inizialmente copiato dal direttorio principale
	// (in questo modo, il nuovo processo acquisisce automaticamente gli
	// spazi virtuali condivisi, sia a livello utente che a livello
	// sistema, tramite condivisione delle relative tabelle delle pagine)
	ppdir = readCR3();
	memcpy(pdirettorio, ppdir, DIM_PAGINA);
	
	pila_sistema = crea_pila_sistema(p->nome);
	if (pila_sistema == 0) goto errore5;

	if (liv == LIV_UTENTE) {
		pl = static_cast<natl*>(pila_sistema);

		pl[1019] = (natl)f;		// EIP (codice utente)
		pl[1020] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pl[1021] = 0x00000200;		// EFLAG
		pl[1022] = fine_utente_privato - 2 * sizeof(int); // ESP (pila utente)
		pl[1023] = SEL_DATI_UTENTE;	// SS (pila utente)
		// eseguendo una IRET da questa situazione, il processo
		// passera' ad eseguire la prima istruzione della funzione f,
		// usando come pila la pila utente (al suo indirizzo virtuale)

		pila_utente = crea_pila_utente(p->nome);
		if (pila_utente == 0) goto errore6;

		// dobbiamo ora fare in modo che la pila utente si trovi nella
		// situazione in cui si troverebbe dopo una CALL alla funzione
		// f, con parametro a:
		pl = static_cast<natl*>(pila_utente);
		pl[1022] = 0xffffffff;	// ind. di ritorno non sign.
		pl[1023] = a;		// parametro del proc.

		// infine, inizializziamo il descrittore di processo

		// indirizzo del bottom della pila sistema, che verra' usato
		// dal meccanismo delle interruzioni
		pdes_proc->esp0 = n2a(fine_sistema_privato);
		pdes_proc->ss0  = SEL_DATI_SISTEMA;

		// inizialmente, il processo si trova a livello sistema, come
		// se avesse eseguito una istruzione INT, con la pila sistema
		// che contiene le 5 parole lunghe preparate precedentemente
		pdes_proc->esp = fine_sistema_privato - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_UTENTE;
		pdes_proc->es  = SEL_DATI_UTENTE;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->cpl = LIV_UTENTE;
		// tutti gli altri campi valgono 0
	} else {
		pl = static_cast<natl*>(pila_sistema);
		pl[1019] = (natl)f;	  	// EIP (codice sistema)
		pl[1020] = SEL_CODICE_SISTEMA;  // CS (codice sistema)
		pl[1021] = 0x00000200;  	// EFLAG
		pl[1022] = 0xffffffff;		// indirizzo ritorno?
		pl[1023] = a;			// parametro
		// i processi esterni lavorano esclusivamente a livello
		// sistema. Per questo motivo, prepariamo una sola pila (la
		// pila sistema)

		pdes_proc->esp = fine_sistema_privato - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_SISTEMA;
		pdes_proc->es  = SEL_DATI_SISTEMA;
		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->cpl = LIV_SISTEMA;
	}

	return p;

errore6:	rilascia_tutto(pdirettorio, n2a(inizio_sistema_privato), ntab_sistema_privato);
errore5:	rilascia_pagina_fisica(indice_pf(pdirettorio));
errore4:	rilascia_tss(identifier);
errore3:	dealloca(pdes_proc);
errore2:	dealloca(p);
errore1:	return 0;
}

// corpo del processo dummy iniziale
void dummy_init(int i)
{
	while (processi > 1)
		;
	terminate_p();
}

// creazione del processo dummy iniziale (usata in fase di inizializzazione del sistema)
bool crea_dummy_init()
{
	proc_elem* di = crea_processo(dummy_init, 0, DUMMY_PRIORITY, LIV_SISTEMA);
	if (di == 0) {
		flog(LOG_ERR, "Impossibile creare il processo dummy_init");
		return false;
	}
	inserimento_lista(pronti, di);
	return true;
}
void main_proc(int n);
bool crea_main()
{
	proc_elem* m = crea_processo(main_proc, 0, MAX_PRIORITY, LIV_SISTEMA);
	if (m == 0) {
		flog(LOG_ERR, "Impossibile creare il processo main");
	}
	processi = 1;
	inserimento_lista(pronti, m);
	return true;
}

// creazione della pila utente
addr crea_pila_utente(natl proc)
{
	addr ind_virtuale, ind_fisico, tabella;
	addr direttorio = get_dir(proc);

	// prepariamo i descrittori di tabella per tutto lo spazio 
	// utente/privato. Viene usata l'ottimizzazione address = 0
	// (le tabelle verranno create solo se effettivamente utilizzate)
	for (natl ind = inizio_utente_privato; ind < fine_utente_privato; ind += DIM_SUPERPAGINA)
		set_destab(direttorio, n2a(ind), BIT_US | BIT_RW);

	// l'ultima pagina della pila va preallocata e precaricata, in quanto 
	// la crea processo vi dovra' scrivere le parole lunghe di 
	// inizializzazione
	ind_virtuale = n2a(fine_utente_privato - DIM_PAGINA);
	tabella = swap_ent(proc, TABELLA, ind_virtuale, false);
	if (tabella == 0)
		return 0;
	ind_fisico = swap_ent(proc, PAGINA_VIRTUALE, ind_virtuale, false);
	if (ind_fisico == 0) {
		rilascia_pagina_fisica(indice_pf(tabella));
		return 0;
	}
	natl& dp = get_despag(tabella, ind_virtuale);
	set_D(dp);

	return ind_fisico;
}

// crea la pila sistema di un processo
// la pila sistema e' grande una sola pagina ed e' sempre residente
addr crea_pila_sistema(natl proc)
{
	addr ind_virtuale, ind_fisico, tabella;
	addr direttorio = get_dir(proc);

	// l'indirizzo della cima della pila e' una pagina sopra alla fine 
	// dello spazio sistema/privato
	ind_virtuale = n2a(fine_sistema_privato - DIM_PAGINA);
	set_destab(direttorio, ind_virtuale, BIT_RW);
	tabella = swap_ent(proc, TABELLA, ind_virtuale, true);
	if (tabella == 0)
		return 0;
	ind_fisico = swap_ent(proc, PAGINA_VIRTUALE, ind_virtuale, true);
	if (ind_fisico == 0) {
		rilascia_pagina_fisica(indice_pf(tabella));
		return 0;
	}

	return ind_fisico;
}
	
// rilascia tutte le strutture dati private associate al processo puntato da 
// "p" (tranne il proc_elem puntato da "p" stesso)
void distruggi_processo(proc_elem* p)
{
	des_proc* pdes_proc = des_p(p->nome);

	addr direttorio = pdes_proc->cr3;
	rilascia_tutto(direttorio, n2a(inizio_sistema_privato), ntab_sistema_privato);
	rilascia_tutto(direttorio, n2a(inizio_utente_privato),  ntab_utente_privato);
	rilascia_pagina_fisica(indice_pf(direttorio));
	rilascia_tss(p->nome >> 3);
	dealloca(pdes_proc);
}

// rilascia ntab tabelle (con tutte le pagine da esse puntate) a partire da 
// quella che mappa l'indirizzo start, nel direttorio pdir
// (start deve essere un multiplo di 4 MiB)
void rilascia_tutto(addr direttorio, addr start, int ntab)
{
	addr ind = start;
	for (int i = 0; i < ntab; i++)
	{
		natl dt = get_destab(direttorio, ind);
		if (ext_P(dt)) {
			addr tabella = ext_indfis(dt);
			for (int j = 0; j < 1024; j++) {
				natl dp = get_despag(tabella, ind);
				if (ext_P(dp)) {
					addr pagina = ext_indfis(dp);
					rilascia_pagina_fisica(indice_pf(pagina));
				} else {
					natl blocco = ext_block(dp);
					if (blocco != 0)
						dealloca_blocco(blocco);
				}
				ind = n2a(a2n(ind) + DIM_PAGINA);
			}
			rilascia_pagina_fisica(indice_pf(tabella));
		} else
			ind = n2a(a2n(ind) + DIM_SUPERPAGINA);
	}
}


//////////////////////////////////////////////////////////////////////////////////
// PRIMITIVE                                                                    //
//////////////////////////////////////////////////////////////////////////////////
// le primivie sono quelle introdotte nel testo. In piu' rispetto al testo, 
// vengono controllati i valori dei parametri passati alle primitive, per 
// evitare problemi di "cavallo di Troia"

// schedulatore
extern "C" void schedulatore(void);

// resetta controllore interruzioni
extern "C" void reset_8259();

extern "C" natw
c_activate_p(void f(int), int a, natl prio, natb liv)
{
	proc_elem	*p;			// proc_elem per il nuovo processo
	short id = 0;

	if (prio < MIN_PRIORITY) {
		flog(LOG_WARN, "priorita' non valida: %d", prio);
		abort_p();
	}

	sem_wait(pf_mutex);

	p = crea_processo(f, a, prio, liv);

	if (p != 0) {
		inserimento_lista(pronti, p);
		processi++;
		id = p->nome;
		flog(LOG_INFO, "proc=%d entry=%x(%d) prio=%d liv=%d", id, f, a, prio, liv);
	}

	sem_signal(pf_mutex);

	return id;
}

extern "C" void c_terminate_p()
{
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;
	flog(LOG_INFO, "Processo %d terminato", p->nome);
	dealloca(p);
	schedulatore();
}


// come la terminate_p, ma invia anche un warning al log (da invocare quando si 
// vuole terminare un processo segnalando che c'e' stato un errore)
extern "C" void c_abort_p()
{
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;
	flog(LOG_WARN, "Processo %d abortito", p->nome);
	dealloca(p);
	schedulatore();
}

// Registrazione processi esterni
const int MAX_IRQ  = 16;
proc_elem* const ESTERN_BUSY = (proc_elem*)1;
proc_elem *a_p[MAX_IRQ];
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
bool aggiungi_pe(proc_elem *p, int irq)
{
	if (irq < 0 || irq >= MAX_IRQ || a_p_save[irq] == 0)
		return false;

	a_p[irq] = p;
	distruggi_processo(a_p_save[irq]);
	dealloca(a_p_save[irq]);
	a_p_save[irq] = 0;
	return true;

}


extern "C" natw c_activate_pe(void f(int), int a, int prio, char liv, int irq)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	if (prio < MIN_PRIORITY) {
		flog(LOG_WARN, "priorita' non valida: %d", prio);
		abort_p();
	}

	sem_wait(pf_mutex);

	p = crea_processo(f, a, prio, liv);
	if (p == 0)
		goto error1;
		
	if (!aggiungi_pe(p, irq) ) 
		goto error2;

	flog(LOG_INFO, "estern=%d entry=%x(%d) prio=%d liv=%d irq=%d", p->nome, f, a, prio, liv, irq);

	sem_signal(pf_mutex);

	return p->nome;

error2:	distruggi_processo(p);
error1:	sem_signal(pf_mutex);
	return 0;
}

// init_pe viene chiamata in fase di inizializzazione, ed associa una istanza 
// di processo esterno generico ad ogni interrupt
bool init_pe()
{
	for (int i = 0; i < MAX_IRQ; i++) {
		proc_elem* p = crea_processo(estern_generico, i, 1, LIV_SISTEMA);
		if (p == 0) {
			flog(LOG_ERR, "Impossibile creare i processi esterni generici");
			return false;
		}
		a_p_save[i] = a_p[i] = p;
	}
	return true;
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
	addr start;	
	union {
		natl dimensione;
		natl occupato : 1;
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


// cerca una regione di dimensione almeno pari a dim nello heap utente e ne
// resituisce l'indirizzo del primo byte (0 in caso di fallimento)
extern "C" void *c_mem_alloc(int dim)
{
	des_heap* nuovo;
	addr p;

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

	if (scorri == 0)     // se non abbiamo trovato la regione
	    	return 0;    // l'allocazione fallisce

	// assert(scorri != 0 && !scorri->occupato && scorri->dimensione >= dim)
		
	p = scorri->start;

	// se scorri->dimensione e' sufficientemente piu' grande di dim, e
	// riusciamo ad allocare un nuovo descrittore di heap, non occupiamo
	// tutta la regione, ma la dividiamo in due parti: una di dimensione
	// pari a dim, occupata, e il resto libero
	if (scorri->dimensione - dim >= sizeof(des_heap) &&
	    (nuovo = static_cast<des_heap*>(alloca(sizeof(des_heap)))) != 0)
	{
		// inseriamo nuovo nella lista, dopo scorri
		nuovo->start = n2a(a2n(scorri->start) + dim); // rispettiamo (2)
		nuovo->dimensione = scorri->dimensione - dim;
		scorri->dimensione = dim;
		nuovo->next = scorri->next; 
		scorri->next = nuovo;
		nuovo->occupato = 0; 
	} 
	scorri->occupato = 1; // questo va fatto per ultimo, perche'
	                      // modifica scorri->dimensione 
	return reinterpret_cast<void*>(p);
}

// libera la regione dello heap il cui indirizzo di partenza e' pv
// nota: se pv e' 0, o pv non e' l'indirizzo di partenza di nessuna regione,
// oppure se tale regione e' gia' libera, non si esegue alcuna azione
extern "C" void c_mem_free(void* pv)
{
	if (pv == 0) return;
	addr p = pv;

	des_heap *prec = 0, *scorri = &heap;
	while (scorri != 0 && (!scorri->occupato || scorri->start != p)) {
		prec = scorri;
		scorri = scorri->next;
	}
	if (scorri == 0) return;
	// assert(scorri != 0 && scorri->occupato && pv == scorri->start);
	
	scorri->occupato = 0;	// questo va fatto per primo, in modo
				// che scorri->dimensione sia valido
	if (!prec->occupato) {
		// assert(prec->start + prec->dimensione == pv) // relazione (1)
		// dobbiamo riunire la regione con la precedente, per rispettare (2)
		prec->dimensione += scorri->dimensione;
		prec->next = scorri->next;
		dealloca(scorri);
		scorri = prec;
	}
	des_heap *next = scorri->next;
	// qualunque ramo dell'if sia stato preso, possiamo affermare:
	// assert(next == 0 || scorri->start + scorri->dimensione == next->dimensione);
	if (next != 0 && !next->occupato) {
		// dobbiamo riunire anche la regione successiva
		scorri->dimensione += next->dimensione;
		scorri->next = next->next;
		dealloca(next);
	}
}


extern "C" natl c_give_num()
{
        return processi;
}


// descrittore di semaforo
struct des_sem {
	int counter;
	proc_elem *pointer;
};


// vettore dei descrittori di semaforo
extern des_sem array_dess[MAX_SEMAFORI];

// bitmap per l'allocazione dei semafori
natl sem_buf[MAX_SEMAFORI / (sizeof(int) * 8) + 1];
bm_t sem_bm = { sem_buf, MAX_SEMAFORI };

// alloca un nuovo semaforo e ne restiutisce l'indice (da usare con sem_wait e 
// sem_signal). Se non ci sono piu' semafori disponibili, restituisce l'indice 
// 0 (NOTA: la firma della funzione e' diversa da quella del testo, in quanto 
// sono stati eliminati i parametri di ritorno. Il testo verra' aggiornato alla 
// prossima edizione).
extern "C" int c_sem_ini(int val)
{
	natl pos;
	int index_des_s = 0;

	if (bm_alloc(&sem_bm, pos)) {
		index_des_s = pos;
		array_dess[index_des_s].counter = val;
	}

	return index_des_s;
}

extern "C" void c_sem_wait(int sem)
{
	des_sem *s;

	if(sem < 0 || sem >= MAX_SEMAFORI) {
		flog(LOG_WARN, "semaforo errato: %d", sem);
		abort_p();
	}

	s = &array_dess[sem];
	s->counter--;

	if(s->counter < 0) {
		inserimento_lista(s->pointer, esecuzione);
		schedulatore();
	}
}

extern "C" void c_sem_signal(int sem)
{
	des_sem *s;
	proc_elem *lavoro;

	if(sem < 0 || sem >= MAX_SEMAFORI) {
		flog(LOG_WARN, "semaforo errato: %d", sem);
		abort_p();
	}

	s = &array_dess[sem];
	s->counter++;

	if(s->counter <= 0) {
		rimozione_lista(s->pointer, lavoro);
		if(lavoro->precedenza <= esecuzione->precedenza)
			inserimento_lista(pronti, lavoro);
		else { // preemption
			inserimento_lista(pronti, esecuzione);
			esecuzione = lavoro;
		}
	}
}

// richiesta al timer
struct richiesta {
	natl d_attesa;
	richiesta *p_rich;
	proc_elem *pp;
};

void inserimento_lista_timer(richiesta *p);
extern "C" void c_delay(natl n)
{
	richiesta *p;

	p = static_cast<richiesta*>(alloca(sizeof(richiesta)));
	p->d_attesa = n;	
	p->pp = esecuzione;

	inserimento_lista_timer(p);
	schedulatore();
}


// Il log e' un buffer circolare di messaggi, dove ogni messaggio e' composto 
// da tre campi:
// - sev: la severita' del messaggio (puo' essere un messaggio di debug, 
// informativi, un avviso o un errore)
// - identifier: l'identificatore del processo che era in esecuzione quando il 
// messaggio e' stato inviato
// - msg: il messaggio vero e proprio

// numero di messaggi nella coda circolare
const natl LOG_MSG_NUM = 100;

// descrittore del log
struct des_log {
	log_msg buf[LOG_MSG_NUM]; // coda circolare di messaggi
	int first, last;	  // primo e ultimo messaggio
	int nmsg;		  // numero di messaggi
	int mutex;
	int sync;
} log_buf; 

// legge un messaggio dal log, bloccandosi se non ve ne sono
extern "C" void c_readlog(log_msg& m)
{
	sem_wait(log_buf.mutex);
	if (log_buf.nmsg == 0) {
		sem_wait(log_buf.sync);
	}
	m = log_buf.buf[log_buf.first];
	log_buf.first = (log_buf.first + 1) % LOG_MSG_NUM;
	log_buf.nmsg--;
	sem_signal(log_buf.mutex);
}

// il microprogramma di gestione delle eccezioni di page fault lascia in cima 
// alla pila (oltre ai valori consueti) una doppia parola, i cui 4 bit meno 
// significativi specificano piu' precisamente il motivo per cui si e' 
// verificato un page fault. Il significato dei bit e' il seguente:
// - prot: se questo bit vale 1, il page fault si e' verificato per un errore 
// di protezione: il processore si trovava a livello utente e la pagina (o la 
// tabella) era di livello sistema (bit US = 0). Se prot = 0, la pagina o la 
// tabella erano assenti (bit P = 0)
// - write: l'accesso che ha causato il page fault era in scrittura (non 
// implica che la pagina non fosse scrivibile)
// - user: l'accesso e' avvenuto mentre il processore si trovava a livello 
// utente (non implica che la pagina fosse invece di livello sistema)
// - res: uno dei bit riservati nel descrittore di pagina o di tabella non 
// avevano il valore richiesto (il bit D deve essere 0 per i descrittori di 
// tabella, e il bit pgsz deve essere 0 per i descrittori di pagina)
extern addr fine_codice_sistema;
struct page_fault_error {
	natl prot  : 1;
	natl write : 1;
	natl user  : 1;
	natl res   : 1;
	natl pad   : 28; // bit non significativi
};


// c_page_fault viene chiamata dal gestore dell'eccezione di page fault (in 
// sistema.S), che provvede a salvare in pila il valore del registro %cr2. Tale 
// valore viene qui letto nel parametro "indirizzo_virtuale". "errore" e' il 
// codice di errore descritto prima, mentre "eip" e' l'indirizzo 
// dell'istruzione che ha causato il fault. Sia "errore" che "eip" vengono 
// salvati in pila dal microprogramma di gestione dell'eccezione
extern "C" void c_page_fault(addr indirizzo_virtuale, page_fault_error errore, addr eip)
{
	bool risu;

	if (eip < fine_codice_sistema || errore.res == 1) {
		// *** il sistema non e' progettato per gestire page fault causati 
		// *** dalle primitie di nucleo, quindi, se cio' si e' verificato, 
		// *** si tratta di un bug
		flog(LOG_ERR, "eip: %x, page fault a %x: %s, %s, %s, %s", eip, indirizzo_virtuale,
			errore.prot  ? "protezione"	: "pag/tab assente",
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema",
			errore.res   ? "bit riservato"	: "");
		panic("page fault dal modulo sistema");
	}
	// *** l'errore di protezione non puo' essere risolto: il processo ha 
	// *** tentato di accedere ad indirizzi proibiti (cioe', allo spazio 
	// *** sistema)
	if (errore.prot == 1) {
		flog(LOG_WARN, "errore di protezione: eip=%x, ind=%x, %s, %s", eip, indirizzo_virtuale,
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema");
		abort_p();
	}

	// in tutti gli altri casi, proviamo a trasferire la pagina mancante
	sem_wait(pf_mutex);
	risu = swap(proc_corrente(), indirizzo_virtuale);
	sem_signal(pf_mutex);

	// se non e' stato possibile caricare la pagina, il processo
	// non puo' proseguire
	if (risu == false) 
		abort_p();
}

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

//////////////////////////////////////////////////////////////////////////////
//                         FUNZIONI AD USO INTERNO                          //
//////////////////////////////////////////////////////////////////////////////

// inserisce P_ELEM in P_CODA, mantenendola ordinata per priorita' decrescente
void inserimento_lista(proc_elem *&p_lista, proc_elem *p_elem)
{
	proc_elem *pp, *prevp;

	pp = p_lista;
	prevp = 0;
	while (pp && pp->precedenza >= p_elem->precedenza) {
		prevp = pp;
		pp = pp->puntatore;
	}

	if (!prevp)
		p_lista = p_elem;
	else
		prevp->puntatore = p_elem;

	p_elem->puntatore = pp;
}

// rimuove il primo elemento da P_CODA e lo mette in P_ELEM
void rimozione_lista(proc_elem *&p_lista, proc_elem *&p_elem)
{
	p_elem = p_lista;

	if (p_lista)
		p_lista = p_lista->puntatore;
}

// scheduler a priorita'
extern "C" void schedulatore(void)
{
	rimozione_lista(pronti, esecuzione);
}


// coda delle richieste al timer
richiesta *descrittore_timer;

// inserisce P nella coda delle richieste al timer
void inserimento_lista_timer(richiesta *p)
{
	richiesta *r, *precedente;
	bool ins;

	r = descrittore_timer;
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
		descrittore_timer = p;

	if (r != 0)
		r->d_attesa -= p->d_attesa;
}


// driver del timer
extern "C" void c_driver_t(void)
{
	static natl istanza = 0;
	richiesta *p;

	if(descrittore_timer != 0)
		descrittore_timer->d_attesa--;

	while(descrittore_timer != 0 && descrittore_timer->d_attesa == 0) {
		inserimento_lista(pronti, descrittore_timer->pp);
		p = descrittore_timer;
		descrittore_timer = descrittore_timer->p_rich;
		dealloca(p);
	}

	// aggiorna le statitische per la memoria virtuale
	if (istanza % T_STAT == 0)
		aggiorna_statistiche();
	istanza++;
		

	schedulatore();
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
//
// copia n byte da src a dest
void *memcpy(void *dest, const void *src, unsigned int n)
{
	char       *dest_ptr = static_cast<char*>(dest);
	const char *src_ptr  = static_cast<const char*>(src);

	for (int i = 0; i < n; i++)
		dest_ptr[i] = src_ptr[i];

	return dest;
}

// scrive n byte pari a c, a partire da dest
void *memset(void *dest, int c, natl n)
{
	char *dest_ptr = static_cast<char*>(dest);

        for (int i = 0; i < n; i++)
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
int strlen(const char *s)
{
	int l = 0;

	while (*s++)
		++l;

	return l;
}

// copia al piu' l caratteri dalla stringa src alla stringa dest
char *strncpy(char *dest, const char *src, natl l)
{

	for (int i = 0; i < l && src[i]; ++i)
		dest[i] = src[i];

	return dest;
}

// restituisce true se le stringhe puntate da first e second
// sono uguali
bool str_equal(const char* first, const char* second)
{

	while (*first && *second && *first++ == *second++)
		;

	return (!*first && !*second);
}

// isola nella stringa puntata da src la prima parola (dove ogni parola e' 
// delimitata da spazi) e ne restituisce il puntatore al primo carattere.
// In cont viene restituito, invece, il puntatore al primo carattere successivo 
// alla parola trovata
char* str_token(char* src, char** cont)
{
	if (src == 0) {
		if (cont != 0) *cont = 0;
		return 0;
	}
	// assert(src != 0);

	char *stok = src, *etok;
	// assert(stok != 0);

	// eliminiamo gli spazi iniziali
	while (*stok == ' ') stok++;
	// assert(stok != 0 && *stok != ' ');
	etok = stok;
	// assert(etok != 0 && *etok != ' ');
	// quindi, raggiungiamo la fine della parola trovata
	while (*etok != '\0' && *etok != ' ') etok++;
	// assert(etok != 0 && (*etok == '\0' || *etok == ' ');
	if (*etok != '\0' && cont != 0)
		*cont = etok + 1;
	else
		*cont = 0;
	*etok = '\0';
	return stok;
}

static const char hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

// converte l in stringa (notazione esadecimale)
static void htostr(char *buf, unsigned long l)
{
	for (int i = 7; i >= 0; --i) {
		buf[i] = hex_map[l % 16];
		l /= 16;
	}
}

// converte l in stringa (notazione decimale)
static void itostr(char *buf, natl len, long l)
{
	int i, div = 1000000000, v, w = 0;

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
int vsnprintf(char *str, natl size, const char *fmt, va_list ap)
{
	int in = 0, out = 0, tmp;
	char *aux, buf[DEC_BUFSIZE];

	while (out < size - 1 && fmt[in]) {
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
						if(out > size - 9)
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
extern "C" int snprintf(char *buf, natl n, const char *fmt, ...)
{
	va_list ap;
	int l;
	va_start(ap, fmt);
	l = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return l;
}

////////////////////////////////////////////////////////////////////////////////
// Allocatore a mappa di bit
////////////////////////////////////////////////////////////////////////////////


inline natl bm_isset(bm_t *bm, natl pos)
{
	return !(bm->vect[pos / 32] & (1UL << (pos % 32)));
}

inline void bm_set(bm_t *bm, natl pos)
{
	bm->vect[pos / 32] &= ~(1UL << (pos % 32));
}

inline void bm_clear(bm_t *bm, natl pos)
{
	bm->vect[pos / 32] |= (1UL << (pos % 32));
}

// crea la mappa BM, usando BUFFER come vettore; SIZE e' il numero di bit
//  nella mappa
void bm_create(bm_t *bm, natl *buffer, natl size)
{
	bm->vect = buffer;
	bm->size = size;
	bm->vecsize = ceild(size, sizeof(natl) * 8);

	for (int i = 0; i < bm->vecsize; ++i)
		bm->vect[i] = 0xffffffff;
}


// usa l'istruzione macchina BSF (Bit Scan Forward) per trovare in modo
// efficiente il primo bit a 1 in v
extern "C" int trova_bit(natl v);

bool bm_alloc(bm_t *bm, natl& pos)
{

	int i = 0;
	bool risu = true;

	while (i < bm->vecsize && !bm->vect[i]) i++;
	if (i < bm->vecsize) {
		pos = trova_bit(bm->vect[i]);
		bm->vect[i] &= ~(1UL << pos);
		pos += sizeof(natl) * 8 * i;
	} else 
		risu = false;
	return risu;
}

void bm_free(bm_t *bm, natl pos)
{
	bm_clear(bm, pos);
}



////////////////////////////////////////////////////////////////////////////////
//                           GESTIONE DEL VIDEO                               //
////////////////////////////////////////////////////////////////////////////////

unsigned char *VIDEO_MEM_BASE = (unsigned char *)0x000b8000;
const int VIDEO_MEM_SIZE = 0x00000fa0;


// copia in memoria video, a partire dall'offset "off" (in caratteri) i 
// "quanti" caratteri puntati da "vett"
extern "C" void writevid_n(int off, const char* vett, int quanti)
{
	unsigned char* start = VIDEO_MEM_BASE + off * 2, *stop;

	if (start < VIDEO_MEM_BASE)
		return;

	stop = (start + quanti * 2 > VIDEO_MEM_BASE + VIDEO_MEM_SIZE) ? 
			VIDEO_MEM_BASE + VIDEO_MEM_SIZE :
			start + quanti * 2;

	for (unsigned char* ptr = start; ptr < stop; ptr += 2)
		*ptr = *vett++;
}

// la funzione backtrace stampa su video gli indirizzi di ritorno dei record di 
// attivazione presenti sulla pila sistema
extern "C" void backtrace(int off);

// panic mostra un'insieme di informazioni di debug sul video e blocca il 
// sistema. I parametri "eip1", "cs", "eflags" e "eip2" sono in pila per 
// effetto della chiamata di sistema
extern "C" void c_panic(const char *msg,
		        natl	 eip1,
			unsigned short	 cs,
			natl	 eflags,
			natl 	 eip2)
{
	unsigned char* ptr;
	static char buf[80];
	int l, nl = 0;

	// puliamo lo schermo, sfondo rosso
	ptr = VIDEO_MEM_BASE;
	while (ptr < VIDEO_MEM_BASE + VIDEO_MEM_SIZE) {
		*ptr++ = ' ';
		*ptr++ = 0x4f;
	}

	// in cima scriviamo il messaggio
	writevid_n(nl++ * 80, "PANIC", 5);
	writevid_n(nl++ * 80, msg, strlen(msg));
	writevid_n(nl++ * 80, "ultimo errore:", 14);
	writevid_n(nl++ * 80, last_log_err(), strlen(last_log_err()));

	nl++;

	des_proc* p = des_p(esecuzione->nome);
	// scriviamo lo stato dei registri
	l = snprintf(buf, 80, "EAX=%x  EBX=%x  ECX=%x  EDX=%x",	
		 p->eax, p->ebx, p->ecx, p->edx);
	writevid_n(nl++ * 80, buf, l);
	l = snprintf(buf, 80, "ESI=%x  EDI=%x  EBP=%x  ESP=%x",	
		 p->esi, p->edi, p->ebp, p->esp);
	writevid_n(nl++ * 80, buf, l);
	l = snprintf(buf, 80, "CS=%x DS=%x ES=%x FS=%x GS=%x SS=%x",
			cs, p->ds, p->es, p->fs, p->gs, p->ss);
	writevid_n(nl++ * 80, buf, l);
	l = snprintf(buf, 80, "EIP=%x  EFLAGS=%x", eip2, eflags);
	writevid_n(nl++ * 80, buf, l);

	nl++;

	backtrace(nl++ * 80);
	
}

//////////////////////////////////////////////////////////////////////////
// GESTIONE DEL LOG
/////////////////////////////////////////////////////////////////////////

// log_init_usr viene chiamata prima di passare a livello utente, per 
// inizializzare i semafori necessari alla primitiva che permette di leggere, 
// da livello utente, i messaggi del log
bool log_init_usr()
{
	if ( (log_buf.mutex = c_sem_ini(1)) == 0)
		goto error1;

	if ( (log_buf.sync = c_sem_ini(0)) == 0)
		goto error2;

	return true;

error2:	bm_free(&sem_bm, log_buf.mutex);
error1: flog(LOG_ERR, "Semafori insufficienti in log_init_usr");
	return false;
}


// accoda un nuovo messaggio e sveglia un eventuale processo che era in attesa
void do_log(log_sev sev, const char* buf, int quanti)
{
	static int num = 0;

	if (quanti > LOG_MSG_SIZE)
		quanti = LOG_MSG_SIZE;

	log_buf.buf[log_buf.last].sev = sev;
	log_buf.buf[log_buf.last].identifier = esecuzione->nome;
	strncpy(log_buf.buf[log_buf.last].msg, buf, quanti);
	log_buf.buf[log_buf.last].msg[quanti] = 0;

	log_buf.last = (log_buf.last + 1) % LOG_MSG_NUM;
	log_buf.nmsg++;
	if (log_buf.last == log_buf.first) {
		log_buf.first = (log_buf.first + 1) % LOG_MSG_NUM;
		log_buf.nmsg--;
	}
	if (log_buf.sync && log_buf.nmsg > 0) {
		proc_elem* lavoro;
		des_sem *s = &array_dess[log_buf.sync];
		if (s->counter < 0) {
			s->counter++;
			rimozione_lista(s->pointer, lavoro);
			inserimento_lista(pronti, lavoro);
		}
	}
}
extern "C" void c_log(log_sev sev, const char* buf, int quanti)
{
	inserimento_lista(pronti, esecuzione);
	do_log(sev, buf, quanti);
	schedulatore();
}

// log formattato
void flog(log_sev sev, const char *fmt, ...)
{
	va_list ap;
	char buf[LOG_MSG_SIZE];

	va_start(ap, fmt);
	int l = vsnprintf(buf, LOG_MSG_SIZE, fmt, ap);
	va_end(ap);

	if (l > 1)
		do_log(sev, buf, l - 1);
}


// restituisce un puntatore all'ultimo messaggio inviato al log
const char* last_log()
{
	int last = log_buf.last - 1;
	if (last < 0) last += LOG_MSG_NUM;
	return log_buf.buf[last].msg;
}

// restituisce un puntatore all'ultimo messaggio di errore inviato al log
const char* last_log_err()
{
	int last = log_buf.last - 1;
	while (last != log_buf.first) {
		if (log_buf.buf[last].sev == LOG_ERR)
			return log_buf.buf[last].msg;
		last--;
		if (last < 0) last += LOG_MSG_NUM;
	}
	return "";
}

///////////////////////////////////////////////////////////////////////////////////////
// DRIVER SWAP                                                                      
///////////////////////////////////////////////////////////////////////////////////////
// ingresso di un byte da una porta di IO
extern "C" void inputb(ioaddr reg, natb &a);
// uscita di un byte su una porta di IO
extern "C" void outputb(natb a, ioaddr reg);
// ingresso di una stringa di word da un buffer di IO
extern "C" void inputbw(ioaddr reg, natw vetti[], int quanti);
// uscita di una stringa di word su un buffer di IO
extern "C" void outputbw(natw vetto[], int quanti, ioaddr reg);

#define STRICT_ATA

#define HD_MAX_TX 256

#define	HD_NONE 0x00		//Maschere per i biti fondamentali dei registri
#define HD_STS_ERR 0x01
#define HD_STS_DRQ 0x08
#define HD_STS_DRDY 0x40
#define HD_STS_BSY 0x80
#define HD_DRV_MASTER 0
#define HD_DRV_SLAVE 1
enum hd_cmd {			// Comandi per i controllori degli hard disk
	NONE		= 0x00,
	IDENTIFY	= 0xEC,
	WRITE_SECT	= 0x30,
	READ_SECT	= 0x20
};

struct interfata_reg {
	ioaddr iBR;			
	ioaddr iCNL, iCNH, iSNR, iHND, iSCR, iERR,
	       iCMD, iSTS, iDCR, iASR;
};

struct drive {				// Drive su un canale ATA
	bool presente;
	bool dma;
	natl tot_sett;
	partizione* part;
};

struct des_ata {		// Descrittore operazione per i canali ATA
	interfata_reg indreg;
	drive disco[2];		// Ogni canale ATA puo' contenere 2 drive
	hd_cmd comando;
	natb errore;
	int mutex;
	int sincr;
	natb cont;
	addr punt;	
};

const int A = 2;
extern "C" des_ata hd[A];	// 2 canali ATA

extern "C" void hd_gohd_inout(ioaddr i_dev_ctl);
extern "C" void halthd_inout(ioaddr i_dev_ctl);
extern "C" bool test_canale(ioaddr st, ioaddr stc, ioaddr stn);
extern "C" int leggi_signature(ioaddr stc, ioaddr stn, ioaddr cyl, ioaddr cyh);
extern "C" void umask_hd(ioaddr h);
extern "C" void mask_hd(ioaddr h);
extern "C" bool hd_software_reset(ioaddr dvctrl);
extern "C" void hd_read_device(ioaddr i_drv_hd, int& ms);
extern "C" void hd_select_device(short ms, ioaddr iHND);
extern "C" void hd_write_address(des_ata *p, natl primo);	
extern "C" void hd_write_command(hd_cmd cmd, ioaddr iCMD);


// seleziona il dispostivo drv e controlla che la selezione abbia avuto 
// successo
bool hd_sel_drv(des_ata* p_des, int drv)
{
	natb stato;
	int curr_drv;

	hd_select_device(drv, p_des->indreg.iHND);

	do {
		inputb(p_des->indreg.iSTS, stato);
	} while ( (stato & HD_STS_BSY) || (stato & HD_STS_DRQ) );

	hd_read_device(p_des->indreg.iHND, curr_drv);

	return  (curr_drv == drv);
}

// aspetta finche' non e' possibile leggere dal registro dei dati
// (DRQ deve essere 1, BSY 0 e ERR 0)
bool hd_wait_data(ioaddr iSTS)
{
	natb stato;

	do {
		inputb(iSTS, stato);
	} while (stato & HD_STS_BSY);

	return ( (stato & HD_STS_DRQ) && !(stato & HD_STS_ERR) );
}

// invia al log di sistema un messaggio che spiega l'errore passato come 
// argomento
void hd_print_error(int i, int d, int sect, natb error)
{
	des_ata* p = &hd[i];
	if (error == D_ERR_NONE)
		return;

	flog(LOG_ERR, "Errore su hard disk:");
	if (i < 0 || i > A || d < 0 || d > 2) {
		flog(LOG_ERR, "valori errati (%d, %d)");
	} else {
		flog(LOG_ERR, "%s/%s: ", (i ? "secondario" : "primario"), (d ? "slave"      : "master"));
		switch (error) {
		case D_ERR_PRESENCE:
			flog(LOG_ERR, "assente o non rilevato");
			break;
		case D_ERR_BOUNDS:
			flog(LOG_ERR, "accesso al settore %d fuori dal range [0, %d)", sect, p->disco[d].tot_sett);
			break;
		case D_ERR_GENERIC:
			flog(LOG_ERR, "errore generico (DRQ=0)");
			break;
		default:
			if (error & 4) 
				flog(LOG_ERR, "comando abortito");
			else
				flog(LOG_ERR, "error register = %d", error);
			break;
		}
	}
}

void starthd_in(des_ata *p_des, int drv, natw vetti[], natl primo, natb quanti)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->comando = READ_SECT;
	p_des->errore = D_ERR_NONE;
	hd_sel_drv(p_des, drv);
	hd_write_address(p_des, primo);
	outputb(quanti, p_des->indreg.iSCR);
	hd_gohd_inout(p_des->indreg.iDCR);	
	hd_write_command(READ_SECT, p_des->indreg.iCMD);
	return;
}

void starthd_out(des_ata *p_des, int drv, natw vetto[], natl primo, natb quanti)	
{
	p_des->cont = quanti;
	p_des->punt = vetto + DIM_BLOCK / 2;
	p_des->comando = WRITE_SECT;
	p_des->errore = D_ERR_NONE;
	hd_sel_drv(p_des, drv);
	hd_write_address(p_des, primo);	
	outputb(quanti, p_des->indreg.iSCR);
	hd_gohd_inout(p_des->indreg.iDCR);	
	hd_write_command(WRITE_SECT, p_des->indreg.iCMD);
	hd_wait_data(p_des->indreg.iSTS);
	outputbw(vetto, DIM_BLOCK / 2, p_des->indreg.iBR);
	return;
}


extern "C" void c_readhd_n(int ata, int drv, natw vetti[], natl primo, natb quanti, natb &errore)
{
	des_ata *p_des;

	// Controllo sulla protezione
	if (ata < 0 || ata >= A || drv < 0 || drv >= 2) {
		errore = D_ERR_PRESENCE;
		return;
	}

	p_des = &hd[ata];

	// Controllo sulla selezione di un drive presente
	if (!p_des->disco[drv].presente) {
		errore = D_ERR_PRESENCE;
		return;
	}

	// Controllo sull'indirizzamento
	if (primo + quanti > p_des->disco[drv].tot_sett) {
		errore = D_ERR_BOUNDS;
		return;
	}

	sem_wait(p_des->mutex);
	starthd_in(p_des, drv, vetti, primo, quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}

extern "C" void c_writehd_n(int ata, int drv, natw vetti[], natl primo, natb quanti, natb &errore)
{
	des_ata *p_des;
	
	// Controllo sulla protezione
	if (ata < 0 || ata >= A || drv < 0 || drv >= 2) {
		errore = D_ERR_PRESENCE;
		return;
	}

	p_des = &hd[ata];


	// Controllo sulla selezione di un drive presente
	if (!p_des->disco[drv].presente) {
		errore = D_ERR_PRESENCE;
		return;
	}
	// Controllo sull'indirizzamento
	if (primo + quanti > p_des->disco[drv].tot_sett) {
		errore=D_ERR_BOUNDS;
		return;
	}

	sem_wait(p_des->mutex);
	starthd_out(p_des, drv, vetti, primo, quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}



extern "C" void c_driver_hd(int ata)			// opera su desintb[interf]
{	
	proc_elem* lavoro; des_sem* s; des_ata* p_des;
	natb stato;
	p_des = &hd[ata];
	p_des->cont--; 
	if (p_des->cont == 0) {	
		halthd_inout(p_des->indreg.iDCR);	
		s = &array_dess[p_des->sincr];
		s->counter++;
		if (s->counter <= 0) {
			rimozione_lista(s->pointer, lavoro);
			inserimento_lista(pronti, lavoro);
		}
	}
	p_des->errore = D_ERR_NONE;
	inputb(p_des->indreg.iSTS, stato);
	if (p_des->comando == READ_SECT) { 
		if (!hd_wait_data(p_des->indreg.iSTS)) 
			inputb(p_des->indreg.iERR, p_des->errore);
		else
			inputbw(p_des->indreg.iBR, static_cast<natw*>(p_des->punt), DIM_BLOCK / 2);
	} else {
		if (p_des->cont != 0) {
			if (!hd_wait_data(p_des->indreg.iSTS)) 
				inputb(p_des->indreg.iERR, p_des->errore);
			else
				outputbw(static_cast<natw*>(p_des->punt), DIM_BLOCK / 2, p_des->indreg.iBR);
		}
	}
	p_des->punt = static_cast<natw*>(p_des->punt) + DIM_BLOCK / 2;
	schedulatore();
}



// descrittore di partizione. Le uniche informazioni che ci interessano sono 
// "offset" e "sectors"
struct des_part {
	natl active	: 8;
	natl head_start	: 8;
	natl sec_start	: 6;
	natl cyl_start_H: 2;
	natl cyl_start_L: 8;
	natl type	: 8;
	natl head_end	: 8;
	natl sec_end	: 6;
	natl cyl_end_H	: 2;
	natl cyl_end_L	: 8;
	natl offset;
	natl sectors;
};

bool leggi_partizioni(int ata, int drv)
{
	natb errore;
	des_part* p;
	partizione *estesa, **ptail;
	des_ata* p_des = &hd[ata];
	static natb buf[512];
	natl settore;
	partizione* pp;

	// lettura del Master Boot Record (LBA = 0)
	settore = 0;
	readhd_n(ata, drv, reinterpret_cast<natw*>(buf), settore, 1, errore);
	if (errore != 0)
		goto errore;
		
	p = reinterpret_cast<des_part*>(buf + 446);
	// interpretiamo i descrittori delle partizioni primarie
	estesa = 0;
	pp = static_cast<partizione*>(alloca(sizeof(partizione)));
	// la partizione 0 corrisponde all'intero hard disk
	pp->first = 0;
	pp->dim = p_des->disco[drv].tot_sett;
	p_des->disco[drv].part = pp;
	ptail = &pp->next;
	for (int i = 0; i < 4; i++) {
		pp = *ptail = static_cast<partizione*>(alloca(sizeof(partizione)));

		pp->type  = p->type;
		pp->first = p->offset;
		pp->dim   = p->sectors;
		
		if (pp->type == 5)
			estesa = *ptail;

		ptail = &pp->next;
		p++;
	}
	if (estesa != 0) {
		// dobbiamo leggere le partizioni logiche
		natl offset_estesa = estesa->first;
		natl offset_logica = offset_estesa;
		while (1) {
			settore = offset_logica;
			readhd_n(ata, drv, reinterpret_cast<natw*>(buf), settore, 1, errore);
			if (errore != 0)
				goto errore;
			p = reinterpret_cast<des_part*>(buf + 446);
			
			*ptail = static_cast<partizione*>(alloca(sizeof(partizione)));
			
			(*ptail)->type  = p->type;
			(*ptail)->first = p->offset + offset_logica;
			(*ptail)->dim   = p->sectors;

			ptail = &(*ptail)->next;
			p++;

			if (p->type != 5) break;

			offset_logica = p->offset + offset_estesa;
		}
	}
	*ptail = 0;
	
	return true;

errore:
	hd_print_error(ata, drv, settore, errore);
	return false;
}

partizione* hd_find_partition(short ch, short drv, int p)
{
	des_ata* pdes_ata = &hd[ch];
	partizione* part;

	if (!pdes_ata->disco[drv].presente) 
		return 0;
	int i;
	for (i = 0, part = pdes_ata->disco[drv].part;
	     i < p && part != 0;
	     i++, part = part->next)
		;
	if (i != p)
		return 0;

	return part;
}


// esegue un software reset di entrambi gli hard disk collegati al canale 
// descritto da p_des
void hd_reset(des_ata* p_des)
{
	// selezione del drive 0 (master)
	hd_select_device(0, p_des->indreg.iHND);
	// invio del comando di reset
	hd_software_reset(p_des->indreg.iDCR);
	// se il drive 0 (master) era stato trovato, aspettiamo che porti i bit 
	// BSY e DRQ a 0
	if (p_des->disco[0].presente) {
		natb stato;
		do {
			inputb(p_des->indreg.iSTS, stato);
		} while ( (stato & HD_STS_BSY) || (stato & HD_STS_DRQ) );
	}

	// se il drive 1 (slave) era stato trovato, aspettiamo che permetta 
	// l'accesso ai registri
	if (p_des->disco[1].presente) {
		hd_select_device(1, p_des->indreg.iHND);
		do {
			natb b1, b2;

			inputb(p_des->indreg.iSCR, b1);
			inputb(p_des->indreg.iSNR, b2);
			if (b1 == 0x01 && b2 == 0x01) break;
			// ... timeout usando i ticks
		} while (p_des->disco[1].presente);
		// ora dobbiamo controllare che lo slave abbia messo 
		// busy a 0
		if (p_des->disco[1].presente) {
			natb stato;

			inputb(p_des->indreg.iSTS, stato);
			if ( (stato & HD_STS_BSY) != 0 ) 
				p_des->disco[1].presente = false;
		}
	}
}


// Inizializzazione e autoriconoscimento degli hard disk collegati ai canali ATA
// 
bool hd_init()
{
	des_ata *p_des;
	aggiungi_pe(ESTERN_BUSY, 14);
	aggiungi_pe(ESTERN_BUSY, 15);
	for (int i = 0; i < A; i++) {			// primario/secondario
		p_des = &hd[i];

		halthd_inout(p_des->indreg.iDCR);	
		// cerchiamo di capire quali (tra master e slave) sono presenti
		// (inizializziamo i campi "presente" della struttura des_ata)
		for (int d = 0; d < 2; d++) {
			hd_select_device(d, p_des->indreg.iHND);	//LBA e drive drv
			p_des->disco[d].presente =
				test_canale(p_des->indreg.iSTS,
					    p_des->indreg.iSCR,
					    p_des->indreg.iSNR);
		}
		if (!p_des->disco[0].presente && !p_des->disco[1].presente)
			continue; // non abbiamo trovato niente, passiamo al prossimo canale
		// se abbiamo trovato qualcosa, inviamo un software reset
		hd_reset(p_des);
		// quindi proviamo a leggere le firme (ci interessano solo i 
		// dispositivi ATA)
		for (int d = 0; d < 2; d++) {
			if (!p_des->disco[d].presente)
				continue;

			hd_select_device(d, p_des->indreg.iHND);	//LBA e drive drv
			p_des->disco[d].presente = 
				(leggi_signature(p_des->indreg.iSCR,
						p_des->indreg.iSNR,
						p_des->indreg.iCNL,
						p_des->indreg.iCNH) == 0);
		}
		// lo standard dice che, se lo slave e' assente, il master puo' 
		// rispondere alle letture/scritture nei registri al posto 
		// dello slave. Cio' vuol dire che non possiamo essere sicuri 
		// che lo slave ci sia davvero fino a quando non inviamo un 
		// comando. Inviamo quindi il comando IDENTIFY DEVICE
		for (int d = 0; d < 2; d++) {
			unsigned short st_sett[256];
			char serial[41], *ptr = serial;
			natb stato;

			if (!p_des->disco[d].presente) 
				continue;
			halthd_inout(p_des->indreg.iDCR);	
			if (!hd_sel_drv(p_des, d))
				goto error;
			hd_write_command(IDENTIFY, p_des->indreg.iCMD);
			if (!hd_wait_data(p_des->indreg.iSTS))
				goto error;
			inputb(p_des->indreg.iSTS, stato); // ack
			inputbw(p_des->indreg.iBR, st_sett, DIM_BLOCK / 2);
			p_des->disco[d].tot_sett = (st_sett[61] << 16) + st_sett[60];
			for (int j = 27; j <= 46; j++) {
				*ptr++ = (char)(st_sett[j] >> 8);
				*ptr++ = (char)(st_sett[j]);
			}
			*ptr = 0;
			flog(LOG_INFO, "  - %s%s: %s - (%d)",
				(i ? "S" : "P"), (d ? "S" : "M"),
				serial,
				p_des->disco[d].tot_sett);

			continue;

		error:
			p_des->disco[d].presente = false;
			continue;
		}
		//mask_hd(p_des->indreg.iSTS);
		umask_hd(p_des->indreg.iSTS);			// abilita int. nel PIC
		if ( (p_des->mutex = c_sem_ini(1)) == 0 ||
		     (p_des->sincr = c_sem_ini(0)) == 0)
		{
			flog(LOG_ERR, "Impossibile allocare i semafori per l'hard disk");
			return false;
		}
		for (int d = 0; d < 2; d++) {
			if (p_des->disco[d].presente)
				leggi_partizioni(i, d);
		}
	}
	return true;
}


///////////////////////////////////////////////////////////////////////////////////
// INIZIALIZZAZIONE                                                              //
///////////////////////////////////////////////////////////////////////////////////


void parse_swap(char* arg, short& channel, short& drive, short& partition)
{
	channel = -1;
	drive   = -1;
	partition = -1;

	// il primo carattere indica il canale (primaprio/secondario)
	switch (*arg) {
	case '\0':
		flog(LOG_WARN, "Opzione -s: manca l'argomento");
		goto error;
	case 'P':
	case 'p':
		// primario
		channel = 0;
		break;
	case 'S':
	case 's':
		// secondario
		channel = 1;
		break;
	default:
		flog(LOG_WARN, "Opzione -s: il canale deve essere 'P' o 'S'");
		goto error;
	}

	arg++;

	// il secondo carattere indica il dispositivo (master/slave)
	switch (*arg) {
	case '\0':
		flog(LOG_WARN, "Opzione -s: parametro incompleto");
		goto error;
	case 'M':
	case 'm':
		// master
		drive = 0;
		break;
	case 'S':
	case 's':
		// slave
		drive = 1;
		break;
	default:
		flog(LOG_WARN, "Opzione -s: il drive deve essere 'M' o 'S'");
		goto error;
	}

	arg++;

	if (*arg == '\0') {
		flog(LOG_WARN, "Opzione -s: manca il numero di partizione");
		goto error;
	}

	partition = strtoi(arg);

	flog(LOG_INFO, "Opzione -s: swap su %s/%s/%d",
			(channel ? "secondario" : "primario"),
			(drive   ? "slave"      : "master"),
			partition);
	return;


error:
	channel = -1;
	drive = -1;
	partition = -1;
	return;
}


void parse_heap(char* arg, int& heap_mem)
{
	int dim;

	 
	// il Primo carattere indica l'unita' di misura 
	switch (*arg) {
	case '\0':
		flog(LOG_WARN, "Opzione -h: parametro incompleto");
		goto errore;
	case 'M':
	case 'm':
		// Megabyte
		dim = 1024*1024;
		break;
	case 'K':
	case 'k':
		// Kilobyte
		dim = 1024;
		break;
	case 'B':
	case 'b':
		// Byte
		dim = 1;
		break;
	default:
		flog(LOG_WARN, "Opzione -h: la dimensione deve essere 'M', 'K' o 'B'");
		goto errore;
	}
	arg++;
	if (*arg == '\0') {
		flog(LOG_WARN, "Opzione -h: manca la dimensione");
		goto errore;
	}
	heap_mem = strtoi(arg)*dim;

	return;
errore:
	flog(LOG_WARN, "Assumo dimensione di default per lo heap");

}

// timer
extern unsigned long ticks;
extern unsigned long clocks_per_usec;
extern "C" void attiva_timer(natl count);
extern "C" void disattiva_timer();
extern "C" unsigned long  calibra_tsc();

// inizializzazione del modulo di I/O
typedef int (*entry_t)(int);
entry_t io_entry = 0;
	
// controllore interruzioni
extern "C" void init_8259();

// gestione processi
extern "C" void salta_a_main();
void main_proc(int n);

bool carica_tutto(natl proc, natl start, natl stop, addr& last_addr)
{
	addr dir = get_dir(proc);
	for (natl ind = start; ind < stop; ind += DIM_SUPERPAGINA)
	{
		natl dt = get_destab(dir, n2a(ind));
		if (ext_P(dt)) {	  
			last_addr = n2a(ind + DIM_SUPERPAGINA);
			addr tabella = swap_ent(proc, TABELLA, n2a(ind), true);
			if (!tabella) {
				flog(LOG_ERR, "Impossibile allocare tabella residente");
				return false;
			}
			natl ind_virt_pag = ind;
			for (int i = 0; i < 1024; i++) {
				natl dp = get_despag(tabella, n2a(ind_virt_pag));
				if (ext_P(dp)) {
					if (!swap_ent(proc, PAGINA_VIRTUALE, n2a(ind_virt_pag), true)) {	
						flog(LOG_ERR, "Impossibile allocare pagina residente");
						return false;
					}
				}
				ind_virt_pag += DIM_PAGINA;
			}
		}
	}
	return true;
}

bool crea_spazio_condiviso(natl main_proc, addr& last_address)
{
	
	// lettura del direttorio principale dallo swap
	flog(LOG_INFO, "lettura del direttorio principale...");
	addr tmp = alloca(DIM_PAGINA);
	if (tmp == 0) {
		flog(LOG_ERR, "memoria insufficiente");
		return false;
	}
	if (!leggi_swap(tmp, swap_dev.sb.directory * 8, DIM_PAGINA, "il direttorio principale"))
		return false;

	addr main_dir = get_dir(main_proc);
	copy_dir(tmp, main_dir, n2a(inizio_io_condiviso),     n2a(fine_io_condiviso));
	copy_dir(tmp, main_dir, n2a(inizio_utente_condiviso), n2a(fine_utente_condiviso));
	dealloca(tmp);
	
	if (!carica_tutto(main_proc, inizio_io_condiviso, fine_io_condiviso, last_address))
		return false;
	if (!carica_tutto(main_proc, inizio_utente_condiviso, fine_utente_condiviso, last_address))
		return false;

	loadCR3(readCR3());
	return true;
}

short swap_ch = -1, swap_drv = -1, swap_part = -1;
int heap_mem = DEFAULT_HEAP_SIZE;

extern "C" void freeze();
extern "C" void end_program();
extern "C" void c_freeze()
{
	esecuzione->precedenza = FREEZE_PRIORITY;
	inserimento_lista(pronti, esecuzione);
	schedulatore();
}

// routine di inizializzazione. Viene invocata dal bootstrap loader dopo aver 
// caricato in memoria l'immagine del modulo sistema
extern "C" void cmain (unsigned long magic, multiboot_info_t* mbi)
{
	des_proc* pdes_proc;
	char *arg, *cont;
	int indice;
	addr direttorio;
	
	// anche se il primo processo non e' completamente inizializzato,
	// gli diamo un identificatore, in modo che compaia nei log
	init.nome = 0;
	init.precedenza   = MAX_PRIORITY;
	esecuzione = &init;

	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v1.0");

	// controlliamo di essere stati caricati
	// da un bootloader che rispetti lo standard multiboot
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		flog(LOG_ERR, "Numero magico non valido: 0x%x", magic);
		goto error;
	}

	// vediamo se il boot loader ci ha passato l'informazione su quanta
	// memoria fisica e' installata nel sistema
	if (mbi->flags & 1) {
		max_mem_lower = addr(mbi->mem_lower * 1024);
		max_mem_upper = addr(mbi->mem_upper * 1024 + 0x100000);
	} else {
		flog(LOG_WARN, "Quantita' di memoria sconosciuta, assumo 32 MiB");
		max_mem_lower = addr(639 * 1024);
		max_mem_upper = addr(32 * 1024 * 1024);
	}
	flog(LOG_INFO, "Memoria fisica: %d byte (%d MiB)", max_mem_upper, a2n(max_mem_upper) >> 20 );

	// interpretiamo i parametri
	for (arg = str_token(mbi->cmdline, &cont);
	     cont != 0 || arg != 0;
	     arg = str_token(cont, &cont))
	{
		if (arg[0] != '-')
			continue;
		switch (arg[1]) {
		case '\0':
			break;
		case 's':
			// indicazione sulla partizione di swap
			parse_swap(&arg[2], swap_ch, swap_drv, swap_part);
			break;
		case 'h':
			//indiazioni sullo heap
			parse_heap(&arg[2], heap_mem);
		break;
		default:
			flog(LOG_WARN, "Opzione sconosciuta: '%s'", arg[1]);
		}
	}
	
	// per come abbiamo organizzato il sistema non possiamo gestire piu' di
	// 1GiB di memoria fisica
	if (max_mem_upper > n2a(fine_sistema_privato)) {
		max_mem_upper = n2a(fine_sistema_privato);
		flog(LOG_WARN, "verranno gestiti solo %d byte di memoria fisica", max_mem_upper);
	}
	
	//Assegna allo heap heap_mem byte a partire dalla fine del modulo sistema
	salta_a(n2a(a2n(mem_upper) + heap_mem));
	flog(LOG_INFO, "Heap di sistema: %d B", heap_mem);


	// il resto della memoria e' per le pagine fisiche
	if (!init_pagine_fisiche())
		goto error;
	flog(LOG_INFO, "Pagine fisiche: %d", num_pagine_fisiche);


	// il direttorio principale viene utilizzato fino a quando non creiamo
	// il primo processo.  Poi, servira' come "modello" da cui creare i
	// direttori dei nuovi processi.
	indice = alloca_pagina_fisica();
	if (indice == -1) {
		flog(LOG_ERR, "Impossibile allocare il direttorio principale");
		goto error;
	}
	direttorio = indirizzo_pf(indice);
	pagine_fisiche[indice].contenuto = DIRETTORIO;
	pagine_fisiche[indice].residente = true;
		
	memset(direttorio, 0, DIM_PAGINA);

	// memoria fisica in memoria virtuale
	if (!mappa_mem_fisica(direttorio, max_mem_upper))
		goto error;
	flog(LOG_INFO, "Mappata memoria fisica in memoria virtuale");

	loadCR3(direttorio);
	// avendo predisposto il direttorio in modo che tutta la memoria fisica
	// si trovi gli stessi indirizzi in memoria virtuale, possiamo attivare
	// la paginazione, sicuri che avremo continuita' di indirizzamento
	attiva_paginazione();
	flog(LOG_INFO, "Paginazione attivata");


	// inizializziamo la mappa di bit che serve a tenere traccia dei
	// semafori allocati
	bm_create(&sem_bm, sem_buf, MAX_SEMAFORI);
	// 0 segnala il fallimento della sem_ini
	bm_set(&sem_bm, 0);
	flog(LOG_INFO, "Semafori: %d", MAX_SEMAFORI);

	// inizializziamo il controllore delle interruzioni [vedi sistema.S]
	init_8259();
	flog(LOG_INFO, "Controllore delle interruzioni inizializzato");
	
	// processo dummy
	if (!crea_dummy_init())
		goto error;
	flog(LOG_INFO, "Creato il processo dummy");

	// processi esterni generici
	if (!init_pe())
		goto error;
	flog(LOG_INFO, "Creati i processi esterni generici");

	if (!crea_main())
		goto error;
	flog(LOG_INFO, "Creato il processo main");

	// da qui in poi, e' il processo main che esegue
	schedulatore();
	salta_a_main();
	return;

error:
	c_panic("Errore di inizializzazione", 0, 0, 0, 0);
}

void main_proc(int n)
{
	unsigned long clocks_per_sec;
	int errore;
	addr pila_utente;
	des_proc *my_des = des_p(esecuzione->nome);
	int sem_io;

	// attiviamo il timer e calibriamo il contatore per i microdelay
	// (necessari nella corretta realizzazione del driver dell'hard disk)
	aggiungi_pe(ESTERN_BUSY, 0);
	attiva_timer(DELAY);
	clocks_per_sec = calibra_tsc();
	clocks_per_usec = ceild(clocks_per_sec, 1000000UL);
	flog(LOG_INFO, "calibrazione del tsc: %d clocks/usec", clocks_per_usec);
	disattiva_timer();

	// inizializziamo il driver dell'hard disk, in modo da poter leggere lo 
	// swap
	flog(LOG_INFO, "inizializzazione e riconoscimento hard disk...");
	if (!hd_init())
		goto error;

	// inizializzazione dello swap
	if (!swap_init(swap_ch, swap_drv, swap_part))
			goto error;
	flog(LOG_INFO, "partizione di swap: %d+%d", swap_dev.part->first, swap_dev.part->dim);
	flog(LOG_INFO, "sb: blocks=%d user=%x/%x io=%x/%x", 
			swap_dev.sb.blocks,
			swap_dev.sb.user_entry,
			swap_dev.sb.user_end,
			swap_dev.sb.io_entry,
			swap_dev.sb.io_end);

	// tabelle condivise per lo spazio utente condiviso
	flog(LOG_INFO, "creazione o lettura delle tabelle condivise...");
	addr last_address;
	if (!crea_spazio_condiviso(proc_corrente(), last_address))
		goto error;

	// inizializzazione dello heap utente
	heap.start = allineav(swap_dev.sb.user_end, sizeof(int));
	heap.dimensione = a2n(last_address) - a2n(heap.start);
	flog(LOG_INFO, "heap utente a %x, dimensione: %d B (%d MiB)",
			heap.start, heap.dimensione, heap.dimensione / (1024 * 1024));

	// semaforo per la mutua esclusione nella gestione dei page fault
	pf_mutex = sem_ini(1);
	if (pf_mutex == 0) {
		flog(LOG_ERR, "Impossibile allocare il semaforo per i page fault");
		goto error;
	}

	
	// inizializzazione del meccanismo di lettura del log
	if (!log_init_usr())
		goto error;
	// inizializzazione del modulo di io
	flog(LOG_INFO, "creazione del processo main I/O...");
	sem_io = sem_ini(0);
	if (sem_io == 0) {
		flog(LOG_ERR, "Impossibile allocare il semaforo di sincr per l'IO");
		goto error;
	}
	if (activate_p(swap_dev.sb.io_entry, sem_io, MAX_PRIORITY, LIV_SISTEMA) == 0) {
		flog(LOG_ERR, "impossibile creare il processo main I/O");
		goto error;
	}
	sem_wait(sem_io);

	flog(LOG_INFO, "creazione del processo main utente...");
	if (activate_p(swap_dev.sb.user_entry, 0, MAX_PRIORITY, LIV_UTENTE) == 0) {
		flog(LOG_ERR, "impossibile creare il processo main utente");
		goto error;
	}
	attiva_timer(DELAY);
	freeze();
	end_program();
error:
	panic("Errore di inizializzazione");
}
