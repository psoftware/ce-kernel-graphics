// sistema.cpp
//
#include "mboot.h"
#include "costanti.h"

// definiamo il tipo "addr" (indirizzo) come un unsigned long
typedef unsigned long addr;

// funzione che permette di convertire un qualunque puntatore
// in un indirizzo
addr toaddr(void* v)
{
	return reinterpret_cast<addr>(v);
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
	unsigned int *vect;	// vettore che contiene i bit della mappa
	unsigned int size;	// numero di bit nella mappa
	unsigned int vecsize;	// corrispondente dimensione del vettore
};
// inizializza una mappa di bit 
void bm_create(bm_t *bm, unsigned int *buffer, unsigned int size);
// alloca una risorsa dalla mappa bm, restituendone il numero in pos. La 
// funzione ritorna false se tutte le risorse sono gia' occupate (in quel caso, 
// pos non e' significativa)
bool bm_alloc(bm_t *bm, unsigned int& pos);
// libera la risorsa numero pos nella mappa bm
void bm_free(bm_t *bm, unsigned int pos);
/////


// funzioni che eseguono alcuni calcoli richiesti abbastanza frequentemente
//
// restituisce il minimo naturale maggiore o uguale a v/q
unsigned int ceild(unsigned int v, unsigned int q)
{
	return v / q + (v % q != 0 ? 1 : 0);
}

// restituisce il valore di "v" allineato a un multiplo di "a"
inline unsigned int allinea(unsigned int v, unsigned int a)
{
	return (v % a == 0 ? v : ((v + a - 1) / a) * a);
}

// funzioni per la manipolazione di stringhe
//
// copia n byte da src a dst
extern "C" void *memcpy(void *dest, const void *src, unsigned int n);
// copia c nei primi n byte della zona di memoria puntata da dest
extern "C" void *memset(void *dest, int c, unsigned int n);
// restituisce true se le due stringe first e second sono uguali
bool str_equal(const char* first, const char* second);

// funzioni utili alla gestione degli errori
// 
// invia un messaggio formatto al log
void flog(log_sev, const char* fmt, ...);
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
// cosi' ottenuto per allocare la memoria richiesta dall'operatore new (per 
// es., per "new richiesta").

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
void free_interna(addr indirizzo, unsigned int quanti);

//Indirizzo di ritorno a loader
int return_ebp;

// allocazione sequenziale, da usare durante la fase di inizializzazione.  Si
// mantiene un puntatore (mem_upper) all'ultimo byte non occupato.  Tale
// puntatore puo' solo avanzare, tramite le funzioni 'occupa' e 'salta_a', e
// non puo' superare la massima memoria fisica contenuta nel sistema
// (max_mem_upper). Se il puntatore viene fatto avanzare tramite la funzione
// 'salta_a', i byte "saltati" vengono dati all'allocatore a lista
// tramite la funzione "free_interna"
addr occupa(unsigned int quanti)
{
	addr p = 0;
	addr appoggio = mem_upper + quanti;
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
		saltati = indirizzo - mem_upper;
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
// l'operatore "new".
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
	unsigned int dimensione;
	des_mem* next;
};

// testa della lista di descrittori di memoria fisica libera
des_mem* memlibera = 0;


// funzione di allocazione: cerca la prima zona di memoria libera di dimensione
// almeno pari a "quanti" byte, e ne restituisce l'indirizzo di partenza.
// Se la zona trovata e' sufficientemente piu' grande di "quanti" byte, divide la zona
// in due: una, grande "quanti" byte, viene occupata, mentre i rimanenti byte vanno
// a formare una nuova zona (con un nuovo descrittore) libera.
void* malloc(unsigned int quanti)
{
	// per motivi di efficienza, conviene allocare sempre multipli di 4 
	// byte (in modo che le strutture dati restino allineate alla linea)
	unsigned int dim = allinea(quanti, sizeof(int));

	// allochiamo "dim" byte, invece dei "quanti" richiesti
	des_mem *prec = 0, *scorri = memlibera;
	while (scorri != 0 && scorri->dimensione < dim) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(scorri == 0 || scorri->dimensione >= dim);

	void* p = 0;
	if (scorri != 0) {
		p = scorri + 1; // puntatore al primo byte dopo il descrittore
				// (coincide con l'indirizzo iniziale delle zona
				// di memoria)

		// per poter dividere in due la zona trovata, la parte
		// rimanente, dopo aver occupato "dim" byte, deve poter contenere
		// almeno il descrittore piu' 4 byte (minima dimensione
		// allocabile)
		if (scorri->dimensione - dim >= sizeof(des_mem) + sizeof(int)) {

			// il nuovo descrittore verra' scritto nei primi byte 
			// della zona da creare (quindi, "dim" byte dopo "p")
			des_mem* nuovo = reinterpret_cast<des_mem*>(addr(p) + dim);

			// aggiustiamo le dimensioni della vecchia e della nuova zona
			nuovo->dimensione = scorri->dimensione - dim - sizeof(des_mem);
			scorri->dimensione = dim;

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

// free deve essere usata solo con puntatori restituiti da malloc, e rende
// nuovamente libera la zona di memoria di indirizzo iniziale "p".
void free(void* p)
{

	// e' normalmente ammesso invocare "free" su un puntatore nullo.
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
	free_interna(toaddr(des), des->dimensione + sizeof(des_mem));
}


// rende libera la zona di memoria puntata da "indirizzo" e grande "quanti"
// byte, preoccupandosi di creare il descrittore della zona e, se possibile, di
// unificare la zona con eventuali zone libere contigue in memoria.  La
// riunificazione e' importante per evitare il problema della "falsa
// frammentazione", in cui si vengono a creare tante zone libere, contigue, ma
// non utilizzabili per allocazioni piu' grandi della dimensione di ciascuna di
// esse.
// free_interna puo' essere usata anche in fase di inizializzazione, per
// definire le zone di memoria fisica che verranno utilizzate dall'allocatore
void free_interna(addr indirizzo, unsigned int quanti)
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
	while (scorri != 0 && toaddr(scorri) < indirizzo) {
		prec = scorri;
		scorri = scorri->next;
	}
	// assert(scorri == 0 || scorri >= indirizzo)

	// in alcuni casi, siamo in grado di riconoscere l'errore di doppia
	// free: "indirizzo" non deve essere l'indirizzo di partenza di una
	// zona gia' libera
	if (toaddr(scorri) == indirizzo) {
		flog(LOG_ERR, "indirizzo = 0x%x", indirizzo);
		panic("double free\n");
	}
	// assert(scorri == 0 || scorri > indirizzo)

	// verifichiamo che la zona possa essere unificata alla zona che la
	// precede.  Cio' e' possibile se tale zona esiste e il suo ultimo byte
	// e' contiguo al primo byte della nuova zona
	if (prec != 0 && toaddr(prec + 1) + prec->dimensione == indirizzo) {

		// controlliamo se la zona e' unificabile anche alla eventuale
		// zona che la segue
		if (scorri != 0 && indirizzo + quanti == toaddr(scorri)) {
			
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

	} else if (scorri != 0 && indirizzo + quanti == toaddr(scorri)) {

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

// ridefinizione degli operatori new e delete, in modo che utilizzino le 
// funzioni malloc e free definite precedentemente
void* operator new(size_t size)
{
	return malloc(size);
}

void* operator new[](size_t size)
{
	return malloc(size);
}

void operator delete(void* p)
{
	free(p);
}

void operator delete[](void* p)
{
	free(p);
}

////////////////////////////////////////////////////////////////////////////
// INTRODUZIONE GENERALE ALLA MEMORIA VIRTUALE                            //
////////////////////////////////////////////////////////////////////////////
//
// SPAZIO VIRTUALE DI UN PROCESSO:
// Ogni processo possiede un proprio spazio di indirizzamento, suddiviso in 
// cinque parti:
// - sistema/condivisa: contiene l'immagine di tutta la memoria fisica
// installata nel sistema
// - sistema/privata: contiene la pila sistema del processo
// - io/condiviso: contiene l'immagine del modulo io
// - utente/condivisa: contiene l'immagine dei corpi di tutti i processi, dei 
// dati globali e dello heap
// - utente/privata: contiene la pila utente del processo
// Le parti sistema e io non sono gestite tramite memoria virtuale (i bit P 
// sono permanentemente a 1 o a 0) e sono accessibili solo da livello di 
// privilegio sistema. Le parti utente sono gestite tramite memoria virtuale e 
// sono accessibili da livello utente.
// Le parti condivise sono comuni a tutti i processi. La condivisione viene 
// realizzata esclusivamente condividendo le tabelle delle pagine: i direttori 
// di tutti i processi puntano alle stesse tabelle nelle entrate relative alla 
// parte sistema/condivisa, io/condivisa e utente/condivisa.  In questo modo, 
// ogni pagina (sia essa appartenende ad uno spazio privato che ad uno spazio 
// condiviso) e' puntata sempre da una sola tabella. Cio' semplifica la 
// gestione della memoria virtuale: per rimuovere o aggiungere una pagina e' 
// necessario modificare una sola entrata (altrimenti, per rimuovere o 
// aggiungere una pagina di uno spazio condiviso, sarebbe necessario modificare 
// le entrate relative alla pagina in tutte le tabelle, di tutti i processi, 
// che la puntano). Inoltre, le tabelle condivise sono sempre preallocate e non 
// rimpiazzabili (altrimenti, per aggiungere o rimuovere una tabella condivisa, 
// sarebbe necessario modificare l'entrata relativa nel direttorio di ogni 
// processo).
//
// MEMORIA FISICA:
// La memoria fisica e' suddivisa in due parti: la prima parte contiene il 
// codice e le strutture dati, statiche e dinamiche, del nucleo, mentre la 
// seconda parte e' suddivisa in pagine fisiche, destinate a contenere i 
// direttori, le tabelle delle pagine, le pagine del modulo io e le pagine 
// virtuali dei processi.  Poiche' l'intera memoria fisica (sia la prima che la 
// seconda parte) e' mappata nello spazio sistema/condiviso di ogni processo, 
// il nucleo (sia il codice che le strutture dati) e' mappato, agli stessi 
// indirizzi, nello spazio virtuale di ogni processo.  In questo modo, ogni 
// volta che un processo passa ad eseguire una routine di nucleo  (per effetto 
// di una interruzione software, esterna o di una eccezione), la
// routine puo' continuare ad utilizzare il direttorio del processo e avere 
// accesso al proprio codice e alle proprie strutture dati.
// La seconda parte della memoria fisica e' gestita tramite una struttra dati 
// del nucleo, allocata dinamicamente in fase di inizializzazione del sistema, 
// contenente un descrittore per ogni pagina fisica. Per le pagine fisiche 
// occupate (da un direttorio, una tabella o una pagina virtuale) e 
// rimpiazzabili, tale descrittore contiene il contatore per le statistiche di 
// accesso (LRU o LFU) e il numero del blocco, in memoria di massa, su cui 
// ricopiare la pagina, in caso di rimpiazzamento. Per motivi di efficienza, i 
// descrittori delle pagine occupate e rimpiazzabili contengono anche le 
// informazioni utili a recuperare velocemente l'entrata del direttorio o della 
// tabella delle pagine che puntano alla tabella o alla pagina descritta (senza 
// questa informazione, per rimpiazzare il contenuto di una pagina fisica, 
// occorrerebbe scorrere le entrate di tutti i direttori o di tutte le tabelle, 
// alla ricerca delle entrate che puntano a quella pagina fisica).
//
// MEMORIA DI MASSA:
// La memoria di massa (swap), che fa da supporto alla memoria virtuale, e' 
// organizzata in una serie di blocchi, numerati a partire da 0. I primi 
// blocchi contengono delle strutture dati necessarie alla gestione dello swap 
// stesso, mentre i rimanenti blocchi contengono direttori, tabelle e pagine 
// virtuali.
// Se una pagina virtuale non e' presente in memoria fisica, il proprio 
// descrittore di pagina (all'interno della corrispondente tabella delle 
// pagine) contiene il numero del blocco della pagina nello swap (analogamente 
// per le tabelle non presenti) e le informazioni necessarie alla corretta 
// creazione del descrittore di pagina, qualora la pagina dovesse essere 
// caricata in memoria fisica (in particolare, i valori da assegnare ai bit US, 
// RW, PCD e PWT). Quando una pagina V, non presente, viene resa presente, 
// caricandola in una pagina fisica F, l'informazione del blocco associato alla 
// pagina viene ricopiata nel descrittore della pagina fisica F (altrimenti, 
// poiche' nel descrittore verra' scritto il byte di accesso e l'indirizzo di 
// F, tale informazione verrebbe persa). Se la pagina V dovesse essere 
// successivamente rimossa dalla memoria fisica, l'informazione relativa al 
// blocco verra' nuovamente copiata dal descrittore della pagina fisica F al 
// descrittore di pagina di V, all'interno della propria tabella.
// 
// CREAZIONE DELLO SPAZIO VIRTUALE DI UN PROCESSO:
// Inizialmente, lo swap contiene esclusivamente l'immagine della parte 
// utente/condivisa e io/condivisa, uguale per tutti i processi. Lo swap 
// contiene, infatti, un solo direttorio, di cui sono significative solo le 
// entrate relative a tali parti, le corrispondenti tabelle e le pagine puntate 
// da tali tabelle.
//
// All'avvio del sistema, viene creato un direttorio principale, nel seguente 
// modo:
// - le entrate corrispondenti allo spazio sistema/condiviso, vengono fatte 
// puntare a tabelle, opportunamente create, che mappano tutta la memoria 
// fisica in memoria virtuale
// - le entrate corrispondenti agli spazi io/condiviso e utente/condiviso, 
// vengono copiate dalle corrispondenti entrate che si trovano nel direttorio 
// nello swap. Le tabelle puntate da tali entrate vengono tutte caricate in 
// memoria fisica, cosi' come le pagine puntate dalle tabelle relative allo 
// spazio io/condiviso
// - le rimanenti entrate sono non significative
//
// Ogni volta che viene creato un nuovo processo, il suo direttorio viene prima 
// copiato dal direttorio principale (in questo modo, il nuovo processo 
// condividera' con tutti gli altri processi le tabelle, e quindi le pagine, 
// degli spazio sistema, io e utente condivisi). Gli spazi privati (sistema e 
// utente), che sono inizialmente vuoti (fatta eccezione per alcune parole 
// lunghe) vengono creati dinamicamente, allocando nuove tabelle e lo spazio 
// nello swap per le corrispondenti pagine. Se la creazione degli spazio 
// privati di un processo non fosse effettuata durante la creazione del 
// processo stesso, lo swap dovrebbe essere preparato conoscendo a priori il 
// numero di processi che verranno creati al momento dell'esecuzione di main.
//


/////////////////////////////////////////////////////////////////////////
// PAGINAZIONE                                                         //
/////////////////////////////////////////////////////////////////////////
// Vengono qui definite le strutture dati utilizzate per la gestione della 
// paginazione. Alcune (descrittore_pagina, descrittore_tabella, direttorio, 
// tabella_pagina) hanno una struttura che e' dettata dall'hardware.
//
// In questa implementazione, viene utilizzata la seguente ottimizzazione:
// una pagina virtuale non presente, il cui descrittore contiene il campo 
// address pari a 0, non possiede inizialmente un blocco in memoria di massa.  
// Se, e quando, tale pagina verra' realmente acceduta, un nuovo blocco verra' 
// allocato in quel momento e la pagina riempita con zeri. Tale ottimizzazione 
// si rende necessaria perche' molte pagine sono potenzialmente utilizzabili 
// dal processo, ma non sono realmente utilizzate quasi mai (ad esempio, la 
// maggior parte delle pagine che compongono lo heap e lo stack). Senza questa 
// ottimizzazione, bisognerebbe prevedere uno dispositivo di swap molto grande 
// (ad es., 1 Giga Byte di spazio privato per ogni processo, piu' 1 Giga Byte 
// di spazio utente condiviso) oppure limitare ulteriormente la dimensione 
// massima dello heap o dello stack. Tale ottimizzazione e' analoga a quella 
// che si trova nei sistemi con file system, in cui i dati globali 
// inizializzati a zero non sono realmente memorizzati nel file eseguibile, ma 
// il file eseguibile ne specifica solo la dimensione totale.
//
// Se il descrittore di una tabella delle pagine possiede il campo address pari 
// a 0, e' come se tutte le pagine puntate dalla tabella avessero il campo 
// address pari a 0 (quindi, la tabella stessa verra' creata dinamicamente, se 
// necessario)

struct descrittore_pagina {
	// byte di accesso
	unsigned int P:		1;	// bit di presenza
	unsigned int RW:	1;	// Read/Write
	unsigned int US:	1;	// User/Supervisor
	unsigned int PWT:	1;	// Page Write Through
	unsigned int PCD:	1;	// Page Cache Disable
	unsigned int A:		1;	// Accessed
	unsigned int D:		1;	// Dirty
	unsigned int pgsz:	1;	// non visto a lezione
	// fine byte di accesso
	
	unsigned int global:	1;	// non visto a lezione
	unsigned int avail:	3;	// non usati

	unsigned int address:	20;	// indirizzo fisico/blocco
};

// il descrittore di tabella delle pagine e' identico al descrittore di pagina
typedef descrittore_pagina descrittore_tabella;

// Un direttorio e' un vettore di 1024 descrittori di tabella
// NOTA: lo racchiudiamo in una struttura in modo che "direttorio" sia un tipo
struct direttorio {
	descrittore_tabella entrate[1024];
}; 

// Una tabella delle pagine e' un vettore di 1024 descrittori di pagina
struct tabella {
	descrittore_pagina  entrate[1024];
};

// Una pagina virtuale la vediamo come una sequenza di 4096 byte, o 1024 parole
// lunghe (utile nell'inizializzazione delle pile)
struct pagina {
	union {
		unsigned char byte[SIZE_PAGINA];
		unsigned int  parole_lunghe[SIZE_PAGINA / sizeof(unsigned int)];
	};
};


// funzioni di comodo, che convertono un indirizzo in un
// puntatore ad uno dei tipi definiti precedentemente
inline direttorio* todir(addr a)
{
	return reinterpret_cast<direttorio*>(a);
}

inline tabella* totab(addr a)
{
	return reinterpret_cast<tabella*>(a);
}

inline pagina* topag(addr a)
{
	return reinterpret_cast<pagina*>(a);
}

// dato un indirizzo virtuale, ne restituisce l'indice nel direttorio
// (i dieci bit piu' significativi dell'indirizzo)
short indice_direttorio(addr indirizzo)
{
	return (indirizzo & 0xffc00000) >> 22;
}

// dato un indirizzo virtuale, ne restituisce l'indice nella tabella delle
// pagine (i bit 12:21 dell'indirizzo)
short indice_tabella(addr indirizzo)
{
	return (indirizzo & 0x003ff000) >> 12;
}

// dato un puntatore ad un descrittore di tabella, restituisce
// un puntatore alla tabella puntata
// (campo address del descrittore esteso a 32 bit aggiungendo 12 bit a 0)
tabella* tabella_puntata(descrittore_tabella* pdes_tab)
{
	return reinterpret_cast<tabella*>(pdes_tab->address << 12);
}

// dato un puntatore ad un descrittore di pagina, restituisce
// un puntatore alla pagina puntata
// (campo address del descrittore esteso a 32 bit aggiungendo 12 bit a 0)
pagina* pagina_puntata(descrittore_pagina* pdes_pag)
{
	return reinterpret_cast<pagina*>(pdes_pag->address << 12);
}

// il direttorio principale contiene i puntatori a tutte le tabelle condivise 
// (degli spazi sistema, io e utente). Viene usato  nella fase inziale, quando 
// ancora non e' stato creato alcun processo e dal processo main.  Inoltre, 
// ogni volta che viene creato un nuovo processo, il direttorio del processo 
// viene inzialmente copiato dal direttorio principale (in modo che il nuovo 
// processo condivida tutte le tabelle condivise)
direttorio* direttorio_principale;

// carica un nuovo valore in cr3 [vedi sistema.S]
extern "C" void carica_cr3(direttorio* dir);

// restituisce il valore corrente di cr3 [vedi sistema.S]
extern "C" direttorio* leggi_cr3();

// attiva la paginazione [vedi sistema.S]
extern "C" void attiva_paginazione();

/////////////////////////////////////////////////////////////////////////
// GESTIONE DELLE PAGINE FISICHE                                       //
// //////////////////////////////////////////////////////////////////////
// La maggior parte della memoria principale del sistema viene utilizzata per
// implementare le "pagine fisiche".  Le pagine fisiche vengono usate per
// contenere direttori, tabelle delle pagine o pagine virtuali.  Per ogni
// pagina fisica, esiste un "descrittore di pagina fisica", che contiene:
// - un campo "contenuto", che puo'assumere uno dei valori sotto elencati
// - il campo contatore degli accessi (per il rimpiazzamento LRU o LFU)
// - il campo "blocco", che contiene il numero del blocco della pagina o
//   tabella nello swap
// I descrittori di pagina fisica sono organizzati in un array, in modo che il
// descrittore in posizione "i" descrive la pagina "i-esima" (considerando il
// normale ordine delle pagine fisiche in memoria fisica)
//   
//se la pagina e' libera, il descrittore di pagina fisica contiene anche
//l'indirizzo del descrittore della prossima pagina fisica libera (0 se non ve
//ne sono altre)
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
	unsigned int	contatore;	// contatore per le statistiche LRU/LFU
	union {
		struct { // informazioni relative a una pagina
			unsigned int	blocco;		// blocco in memoria di massa
			tabella*	ptab;		// tabella che punta alla pagina
			addr		ind_virtuale;   // indirizzo virtuale della pagina
		} pag;
		struct { // informazioni relative a una tabella
			unsigned int	blocco;		// blocco in memoria di massa
			direttorio*     pdir;		// direttorio che punta alla tabella
			short		indice;		// indice della tab nel direttorio
			short		quante;		// numero di pagine puntate
		} tab;
		struct  { // informazioni relative a una pagina libera
			int		prossima_libera;
		} avl;
		// non ci sono informazioni aggiuntive per una pagina 
		// contenente un direttorio
	};
};


// puntatore al primo descrittore di pagina fisica
des_pf* pagine_fisiche;

// numero di pagine fisiche
unsigned long num_pagine_fisiche;

// testa della lista di pagine fisiche libere
int pagine_libere = -1;

// indirizzo fisico della prima pagina fisica
// (necessario per calcolare l'indirizzo del descrittore associato ad una data
// pagina fisica e viceversa)
addr prima_pf;

// restituisce l'indirizzo fisico della pagina fisica associata al descrittore
// il cui indice e' passato come argomento
addr indirizzo_pf(int indice)
{
	return prima_pf + indice * SIZE_PAGINA;
}

// dato l'indirizzo di una pagina fisica, restituisce l'indice
// del descrittore associato
int indice_pf(addr pagina)
{
	return (pagina - prima_pf) / SIZE_PAGINA;
}

// overloading della funzione indice_pf, da usare
// quando l'indirizzo della pagina e' disponibile
// sotto forma di puntatore
int indice_pf(void* pagina)
{
	return indice_pf(toaddr(pagina));
}

// init_pagine_fisiche viene chiamata in fase di inizalizzazione.  Tutta la
// memoria non ancora occupata viene usata per le pagine fisiche.  La funzione
// si preoccupa anche di allocare lo spazio per i descrittori di pagina fisica,
// e di inizializzarli in modo che tutte le pagine fisiche risultino libere
bool init_pagine_fisiche()
{

	// allineamo mem_upper alla linea, per motivi di efficienza
	salta_a(allinea(mem_upper, sizeof(int)));

	// calcoliamo quanta memoria principale rimane
	int dimensione = max_mem_upper - mem_upper;

	if (dimensione <= 0) {
		flog(LOG_ERR, "Non ci sono pagine libere");
		return false;
	}

	// calcoliamo quante pagine fisiche possiamo definire (tenendo conto
	// del fatto che ogni pagina fisica avra' bisogno di un descrittore)
	unsigned int quante = dimensione / (SIZE_PAGINA + sizeof(des_pf));

	// allochiamo i corrispondenti descrittori di pagina fisica
	pagine_fisiche = reinterpret_cast<des_pf*>(occupa(sizeof(des_pf) * quante));

	// riallineamo mem_upper a un multiplo di pagina
	salta_a(allinea(mem_upper, SIZE_PAGINA));

	// ricalcoliamo quante col nuovo mem_upper, per sicurezza
	// (sara' minore o uguale al precedente)
	quante = (max_mem_upper - mem_upper) / SIZE_PAGINA;

	// occupiamo il resto della memoria principale con le pagine fisiche;
	// ricordiamo l'indirizzo della prima pagina fisica e il loro numero
	prima_pf = occupa(quante * SIZE_PAGINA);
	num_pagine_fisiche = quante;

	// se resta qualcosa (improbabile), lo diamo all'allocatore a lista
	salta_a(max_mem_upper);

	// costruiamo la lista delle pagine fisiche libere
	pagine_libere = 0;
	for (int i = 0; i < quante - 1; i++) {
		pagine_fisiche[i].contenuto = LIBERA;
		pagine_fisiche[i].avl.prossima_libera = i + 1;
	}
	pagine_fisiche[quante - 1].avl.prossima_libera = -1;

	return true;
}

// funzione di allocazione generica di una pagina
// Nota: restituisce un puntatore al *descrittore* della pagina, non alla 
// pagina
int alloca_pagina_fisica()
{
	int p = pagine_libere;
	if (pagine_libere != -1)
		pagine_libere = pagine_fisiche[pagine_libere].avl.prossima_libera;
	return p;
}

// perche' la struttura dati composta dai descrittori di pagina fisica sia
// consistente, e' necessario che, ogni volta che si alloca una pagina, si
// aggiorni il campo "contenuto" del corrispondente descrittore in base all'uso
// che si deve fare della pagina.  Per semplificare questa operazione
// ripetitiva, definiamo una funzione di allocazione specifica per ogni
// possibile uso della pagina fisica da allocare (in questo modo, otteniamo
// anche il vantaggio di poter restituire un puntatore alla pagina del tipo
// corretto)
direttorio* alloca_direttorio()
{
	int i = alloca_pagina_fisica();
	if (i == -1) return 0;
	des_pf *p = &pagine_fisiche[i];
	p->contenuto = DIRETTORIO;
	p->residente = true;

	// tramite il descrittore puntato da "p", calcoliamo l'indirizzo della 
	// pagina descritta (funzione "indirizzo"), che puntera' ad una "union 
	// pagina_fisica". Quindi, restituiamo un puntatore al campo di tipo 
	// "direttorio" all'interno della union
	direttorio *pdir = todir(indirizzo_pf(i));
	return pdir;
}

tabella* alloca_tabella(bool residente = false)
{
	int i = alloca_pagina_fisica();
	if (i == -1) return 0;
	des_pf *p = &pagine_fisiche[i];
	p->contenuto = TABELLA;
	p->residente = residente;
	p->tab.quante = 0;
	tabella *ptab = totab(indirizzo_pf(i));
	return ptab;
}

pagina* alloca_pagina_virtuale(bool residente = false)
{
	int i = alloca_pagina_fisica();
	if (i == -1) return 0;
	des_pf *p = &pagine_fisiche[i];
	p->contenuto = PAGINA_VIRTUALE;
	p->residente = residente;
	pagina* ppag = topag(indirizzo_pf(i));
	return ppag;
}
// rende libera la pagina associata al descrittore di pagina fisica puntato da
// "p"
void rilascia_pagina_fisica(int i)
{
	des_pf* p = &pagine_fisiche[i];
	p->contenuto = LIBERA;
	p->avl.prossima_libera = pagine_libere;
	pagine_libere = i;
}

// funzione di comodo per il rilascio di una pagina fisica
// dato un puntatore a un direttorio, a una tabella o ad una pagina, ne 
// calcolano prima l'indirizzo della pagina fisica che le contiene (semplice 
// conversione di tipo) tramite la funzione "pfis", quindi l'indirizzo del 
// descrittore di pagina fisica associato (funzione "struttura") e, infine, 
// invocano la funzione "rilascia_pagina" generica.
inline
void rilascia(void* d)
{
	rilascia_pagina_fisica(indice_pf(toaddr(d)));
}


// funzioni che aggiornano sia le strutture dati della paginazione che
// quelle relative alla gestione delle pagine fisiche:

// collega la tabella puntata da "ptab" al direttorio puntato "pdir" all'indice
// "indice". Aggiorna anche il descrittore di pagina fisica corrispondente alla 
// pagina fisica che contiene la tabella.
// NOTA: si suppone che la tabella non fosse precedentemente collegata, e 
// quindi che il corrispondente descrittore di tabella nel direttorio contenga
// il numero del blocco della tabella nello swap
descrittore_tabella* collega_tabella(direttorio* pdir, tabella* ptab, short indice)
{
	descrittore_tabella* pdes_tab = &pdir->entrate[indice];

	// mapping inverso:
	// ricaviamo il descrittore della pagina fisica che contiene la tabella
	des_pf* ppf  = &pagine_fisiche[indice_pf(toaddr(ptab))];
	ppf->tab.pdir   = pdir;
	ppf->tab.indice = indice;
	ppf->tab.quante = 0; // inizialmente, nessuna pagina e' presente
	
	// il contatore deve essere inizializzato come se fosse appena stato 
	// effettuato un accesso (cosa che, comunque, avverra' al termine della 
	// gestione del page fault). Diversamente, la pagina o tabella appena 
	// caricata potrebbe essere subito scelta per essere rimpiazzata, al 
	// prossimo page fault, pur essendo stata acceduta di recente (infatti, 
	// non c'e' alcuna garanzia che la routine del timer riesca ad andare 
	// in esecuzione e aggiornare il contatore prima del prossimo page 
	// fault)
	ppf->contatore       = 0x80000000;
	ppf->tab.blocco      = pdes_tab->address; // vedi NOTA prec.
	
	// mapping diretto
	pdes_tab->address    = toaddr(ptab) >> 12;
	pdes_tab->D          = 0; // bit riservato nel caso di descr. tabella
	pdes_tab->pgsz       = 0; // pagine di 4KB
	pdes_tab->P          = 1; // presente

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
	des_pf* ppf = &pagine_fisiche[indice_pf(tabella_puntata(pdes_tab))];
	pdes_tab->address = ppf->tab.blocco;

	return pdes_tab;
}

// collega la pagina puntata da "pag" alla tabella puntata da "ptab", 
// all'indirizzo virtuale "ind_virtuale". Aggiorna anche il descrittore di 
// pagina fisica corrispondente alla pagina fisica che contiene la pagina.
// NOTA: si suppone che la pagina non fosse precedentemente collegata, e quindi 
// che il corrispondente descrittore di pagina nella tabella contenga
// il numero del blocco della pagina nello swap
descrittore_pagina* collega_pagina_virtuale(tabella* ptab, pagina* pag, addr ind_virtuale)
{

	descrittore_pagina* pdes_pag = &ptab->entrate[indice_tabella(ind_virtuale)];

	// mapping inverso
	des_pf* ppf = &pagine_fisiche[indice_pf(pag)];
	ppf->pag.ptab	= ptab;
	ppf->pag.ind_virtuale   = ind_virtuale;
	// per il campo contatore, vale lo stesso discorso fatto per le tabelle 
	// delle pagine
	ppf->contatore          = 0x80000000;
	ppf->pag.blocco	= pdes_pag->address; // vedi NOTA prec

	// incremento del contatore nella tabella
	ppf = &pagine_fisiche[indice_pf(ptab)];
	ppf->tab.quante++;
	ppf->contatore		|= 0x80000000;

	// mapping diretto
	pdes_pag->address	= toaddr(pag) >> 12;
	pdes_pag->pgsz		= 0; // bit riservato nel caso di descr. di pagina
	pdes_pag->P		= 1; // presente

	return pdes_pag;
}


// scollega la pagina di indirizzo virtuale "ind_virtuale" dalla tabella 
// puntata da "ptab". Aggiorna anche il contatore di pagine nel descrittore di 
// pagina fisica corrispondente alla tabella
descrittore_pagina* scollega_pagina_virtuale(tabella* ptab, addr ind_virtuale)
{

	// poniamo a 0 il bit di presenza nel corrispondente descrittore di 
	// pagina
	descrittore_pagina* pdes_pag = &ptab->entrate[indice_tabella(ind_virtuale)];
	pdes_pag->P = 0;

	// quindi scriviamo il numero del blocco nello swap al posto 
	// dell'indirizzo fisico
	des_pf* ppf = &pagine_fisiche[indice_pf(pagina_puntata(pdes_pag))];
	pdes_pag->address = ppf->pag.blocco;

	// infine, decrementiamo il numero di pagine puntate dalla tabella
	ppf = &pagine_fisiche[indice_pf(ptab)];
	ppf->tab.quante--;

	return pdes_pag;
}

// mappa la memoria fisica, dall'indirizzo 0 all'indirizzo max_mem, nella 
// memoria virtuale gestita dal direttorio pdir
// (la funzione viene usata in fase di inizializzazione)
bool mappa_mem_fisica(direttorio* pdir, addr max_mem)
{
	descrittore_tabella* pdes_tab;
	tabella* ptab;
	descrittore_pagina *pdes_pag;

	for (addr ind = addr(0); ind < max_mem; ind = ind + SIZE_PAGINA) {
		pdes_tab = &pdir->entrate[indice_direttorio(ind)];
		if (pdes_tab->P == 0) {
			ptab = alloca_tabella(true);
			if (ptab == 0) {
				flog(LOG_ERR, "Impossibile allocare le tabelle condivise");
				return false;
			}
			pdes_tab->address   = toaddr(ptab) >> 12;
			pdes_tab->D	    = 0;
			pdes_tab->pgsz      = 0; // pagine di 4K
			pdes_tab->D	    = 0;
			pdes_tab->US	    = 0; // livello sistema;
			pdes_tab->RW	    = 1; // scrivibile
			pdes_tab->P 	    = 1; // presente
		} else {
			ptab = tabella_puntata(pdes_tab);
		}
		pdes_pag = &ptab->entrate[indice_tabella(ind)];
		pdes_pag->address = ind >> 12;   // indirizzo virtuale == indirizzo fisico
		pdes_tab->pgsz    = 0;
		pdes_pag->global  = 1; // puo' restare nel TLB, anche in caso di flush
		pdes_pag->US      = 0; // livello sistema;
		pdes_pag->RW	  = 1; // scrivibile
		pdes_pag->PCD     = 0;
		pdes_pag->PWT     = 0;
		pdes_pag->P       = 1; // presente
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
	int	     type;	// tipo della partizione
	unsigned int first;	// primo settore della partizione
	unsigned int dim;	// dimensione in settori
	partizione*  next;
};

// funzioni per la lettura/scrittura da hard disk
extern "C" void readhd_n(short ind_ata, short drv, void* vetti,
		unsigned int primo, unsigned char quanti, char &errore);
extern "C" void writehd_n(short ind_ata, short drv, void* vetto,
		unsigned int primo, unsigned char quanti, char &errore);
// utile per il debug: invia al log un messaggio relativo all'errore che e' 
// stato riscontrato
void hd_print_error(int i, int d, int sect, short errore);
// cerca la partizione specificata
partizione* hd_find_partition(short ind_ata, short drv, int p);


// super blocco (vedi sopra)
struct superblock_t {
	char		magic[4];
	unsigned int	bm_start;
	int		blocks;
	unsigned int	directory;
	int		(*user_entry)(int);
	addr		user_end;
	int		(*io_entry)(int);
	addr		io_end;
	unsigned int	checksum;
};

// descrittore di swap (vedi sopra)
struct des_swap {
	short channel;		// canale: 0 = primario, 1 = secondario
	short drive;		// dispositivo: 0 = master, 1 = slave
	partizione* part;	// partizione all'interno del dispositivo
	bm_t free;		// bitmap dei blocchi liberi
	superblock_t sb;	// contenuto del superblocco 
} swap; 	// c'e' un unico oggetto swap


// legge dallo swap il blocco il cui indice e' passato come primo parametro, 
// copiandone il contenuto a partire dall'indirizzo "dest"
bool leggi_blocco(unsigned int blocco, void* dest)
{
	char errore;

	// ogni blocco (4096 byte) e' grande 8 settori (512 byte)
	// calcoliamo l'indice del primo settore da leggere
	unsigned int sector = blocco * 8 + swap.part->first;

	// il seguente controllo e' di fondamentale importanza: l'hardware non 
	// sa niente dell'esistenza delle partizioni, quindi niente ci 
	// impedisce di leggere o scrivere, per sbaglio, in un'altra partizione
	if (blocco < 0 || sector + 8 > (swap.part->first + swap.part->dim)) {
		flog(LOG_ERR, "Accesso al di fuori della partizione");
		// fermiamo tutto
		panic("Errore interno");
	}

	readhd_n(swap.channel, swap.drive, dest, sector, 8, errore);

	if (errore != 0) { 
		flog(LOG_ERR, "Impossibile leggere il blocco %d", blocco);
		hd_print_error(swap.channel, swap.drive, blocco * 8, errore);
		return false;
	}
	return true;
}


// scrive nello swap il blocco il cui indice e' passato come primo parametro, 
// copiandone il contenuto a partire dall'indirizzo "dest"
bool scrivi_blocco(unsigned int blocco, void* dest)
{
	char errore;
	unsigned int sector = blocco * 8 + swap.part->first;

	if (blocco < 0 || sector + 8 > (swap.part->first + swap.part->dim)) {
		flog(LOG_ERR, "Accesso al di fuori della partizione");
		// come sopra
		panic("Errore interno");
		return false;
	}
	
	writehd_n(swap.channel, swap.drive, dest, sector, 8, errore);

	if (errore != 0) { 
		flog(LOG_ERR, "Impossibile scrivere il blocco %d", blocco);
		hd_print_error(swap.channel, swap.drive, blocco * 8, errore);
		return false;
	}
	return true;
}

// lettura dallo swap (da utilizzare nella fase di inizializzazione)
bool leggi_swap(void* buf, unsigned int first, unsigned int bytes, const char* msg)
{
	char errore;
	unsigned int sector = first + swap.part->first;

	if (first < 0 || first + bytes > swap.part->dim) {
		flog(LOG_ERR, "Accesso al di fuori della partizione: %d+%d", first, bytes);
		return false;
	}
	
	readhd_n(swap.channel, swap.drive, buf, sector, ceild(bytes, 512), errore);

	if (errore != 0) { 
		flog(LOG_ERR, "\nImpossibile leggere %s", msg);
		hd_print_error(swap.channel, swap.drive, sector, errore);
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

	swap.channel = swap_ch;
	swap.drive   = swap_drv;
	swap.part    = part;

	// lettura del superblocco
	flog(LOG_DEBUG, "lettura del superblocco dall'area di swap...");
	if (!leggi_swap(read_buf, 1, sizeof(superblock_t), "il superblocco"))
		return false;

	swap.sb = *reinterpret_cast<superblock_t*>(read_buf);

	// controlliamo che il superblocco contenga la firma di riconoscimento
	if (swap.sb.magic[0] != 'C' ||
	    swap.sb.magic[1] != 'E' ||
	    swap.sb.magic[2] != 'S' ||
	    swap.sb.magic[3] != 'W')
	{
		flog(LOG_ERR, "Firma errata nel superblocco");
		return false;
	}

	flog(LOG_DEBUG, "lettura della bitmap dei blocchi...");

	// calcoliamo la dimensione della mappa di bit in pagine/blocchi
	unsigned int pages = ceild(swap.sb.blocks, SIZE_PAGINA * 8);

	// quindi allochiamo in memoria un buffer che possa contenerla
	unsigned int* buf = new unsigned int[(pages * SIZE_PAGINA) / sizeof(unsigned int)];
	if (buf == 0) {
		flog(LOG_ERR, "Impossibile allocare la bitmap dei blocchi");
		return false;
	}

	// inizializziamo l'allocatore a mappa di bit
	bm_create(&swap.free, buf, swap.sb.blocks);

	// infine, leggiamo la mappa di bit dalla partizione di swap
	return leggi_swap(buf, swap.sb.bm_start * 8, pages * SIZE_PAGINA, "la bitmap dei blocchi");
}

// ROUTINE DEL TIMER
//
// aggiorna_statistiche viene chiamata dalla routine del timer e si preccupa di 
// campionare tutti i bit A delle pagine e tabelle presenti e rimpiazzabili.  
// Per far cio', scorre tutto l'array di descrittori di pagina fisica, alla 
// ricerca di pagine che contengano o una TABELLA_PRIVATA o una 
// TABELLA_CONDIVISA o un DIRETTORIO (il caso TABELLA_RESIDENTE viene saltato, 
// in quanto una tabella residente punta a pagine non rimpiazzabili, di cui e' 
// quindi inutile aggiornare il contatore).
// Quindi, scorre tutte le entrate di ogni tabella (o direttorio) e, per ogni 
// pagina (o tabella) puntata presente, aggiorna il campo "contatore" nel 
// rispettivo descrittore di pagina fisica.
// NOTA 1: operando in questo modo, il contatore associato ad ogni tabella 
// condivisa verra' aggiornato piu' di una volta, possibilmente in modo 
// scorretto, in quanto ogni tabella condivisa e' puntata da ogni direttorio.  
// Cio' non causa problemi, in quanto le tabelle condivise non sono comunque 
// rimpiazzabili, quindi i loro contatori non sono significativi
// NOTA 2: i direttori e le tabelle vengono gestiti dallo stesso codice, pur 
// avendo tipi diversi. Cio' e' reso possibile dal fatto che il bit A si trova 
// nella stessa posizione sia nel descrittore di pagina che nel descrittore di 
// tabella
// NOTA 3: una piccola ottimizzazione si e' resa necessaria: prima di scorrere 
// tutte le entrate di una tabella, si controlla il campo "quante" del relativo 
// descrittore di pagina fisica. Tale campo contiene il numero di entrate con 
// bit P uguale a 1 nella tabella e viene aggiornato ogni volta che si 
// aggiunge/rimuove una pagina alla tabella [vedi collega_pagina e 
// scollega_pagina].  Il controllo delle entrate viene eseguito se e solo se 
// "quante" e' maggiore di zero. In questo modo, e' possibile evitare di 
// controllare le 1024 entrate di una tabella che si sa essere vuota. Senza 
// questa ottimizzazione, la routine del timer puo' quasi saturare la CPU 
// (soprattutto negli emulatori), visto che questo sistema contiene molte 
// tabelle vuote (si pensi che tutte le 256 tabelle dello spazio utente 
// condiviso vengono preallocate, anche se non utilizzate)
// NOTA 4: l'aggiornamento di tipo LRU e' in realta' una approssimazione, per 
// una serie di motivi: (i) nel vero LRU, due pagine non possono avere lo 
// stesso contatore (gli accessi sono sequenziali, quindi necessariamente una 
// sara' stata acceduta prima dell'altra). In questo sistema, pero', gli 
// accessi che intervengono tra due campionamenti sono considerati tutti uguali 
// (avranno tutti un "1" nella stessa posizione del contatore). (ii) Nel vero 
// LRU conta solo l'ordinamento dato dall'ultimo accesso ad ogni pagina. In 
// questo sistema, pero', la pagina da rimpiazzare viene scelta in base al 
// valore dell'intero contatore. Per quanto detto al punto (i), pero', piu' 
// pagine potranno avere il primo bit a "1" contando da sinistra 
// (corrispondente all'ultimo accesso) nella stessa posizione, quindi la scelta 
// sara' influenzata dagli altri bit a "1" nel contatore, cioe' dagli accessi 
// precedenti l'ultimo. (iii) Infine, nel vero LRU, tutte le pagine presenti in 
// memoria sono ordinate. Pero', il contatore ha una dimensione necessariamente 
// finita (32 bit in questo sistema). Tutte le pagine che non sono accedute da 
// piu' di 32 intervalli di timer (1.6 secondi in questo sistema) avranno il 
// contatore a 0 e saranno quindi indifferenti dal punto di vista del 
// rimpiazzamento.
void aggiorna_statistiche()
{
	des_pf *ppf1, *ppf2;
	tabella* ptab;
	descrittore_pagina* pp;

	for (int i = 0; i < num_pagine_fisiche; i++) {
		ppf1 = &pagine_fisiche[i];

		// la variabile check ci dice se dobbiamo o non dobbiamo 
		// scorrere le entrate della pagina fisica i-esima
		bool check;
		switch (ppf1->contenuto) {
		case TABELLA:
			// vedi NOTA 3
			check = ppf1->tab.quante > 0;
			break;
		case DIRETTORIO:
			check = true;
			break;
		default:
			check = false;
			break;
		}
		if (check) {
			// trattiamo tutto come se fosse una tabella, anche se 
			// si tratta di un direttorio (vedi NOTA 2)
			ptab = totab(indirizzo_pf(i));
			for (int j = 0; j < 1024; j++) {
				pp = &ptab->entrate[j];
				if (pp->P == 1) {
					// ricaviamo un puntatore al 
					// descrittore di pagina fisica 
					// associato alla pagina fisica puntata 
					// da "pp"
					ppf2 = &pagine_fisiche[indice_pf(pagina_puntata(pp))];

					// aggiornamento di tipo LRU
					ppf2->contatore >>= 1;	// shift in ogni caso
					ppf2->contatore |= pp->A << 31U;

					// se volessimo, invece, usare 
					// l'aggiornamento di tipo LFU, 
					// dovremmo scrivere:
					// ppf2->contatore++

					// azzeramento del bit A appena 
					// campionato. Tornera' ad 1 se la 
					// pagina verra' nuovamente acceduta
					pp->A = 0;
				}
			}
		}
	}
	// svuotiamo il TLB, eseguendo un caricamento fittizio di cr3
	// (se non lo facciamo, le pagine il cui descrittore si trova nel TLB 
	// non avranno i loro bit A riportati ad 1 in caso di nuovo accesso)
	carica_cr3(leggi_cr3());
}

// ROUTINE DI TRASFERIMENTO
//
// trasferisce la pagina di indirizzo virtuale "indirizzo_virtuale"
// in memoria fisica
// NOTA: il page fault non puo' essere ragionevolmente eseguito come le 
// primitive di sistema (ovvero, salvataggio dello stato solo all'inizio, 
// interrupt disabilitati, eventuale schedulazione solo alla fine). Infatti, e' 
// nessario eseguire fino a 4 trasferimenti dalla memoria di massa 
// (rimpiazzammento e lettura per la tabella, se mancante, e rimpiazzamento e 
// lettura per la pagina). Ogni trasferimento che coinvolge la memoria di massa 
// (se questa e' realizzata con un normale hard disk, sia in DMA che a 
// controllo di programma) puo' richiedere fino a 10/20 millisecondi (in quanto 
// sono coinvoli tempi meccanici, come il posizionamento della testina sulla 
// traccia richiesta e l'attesa della rotazione del disco fino a quando il 
// settore richiesto non passa sotto la testina). Un microprocessore moderno 
// (con pipeline e frequenza di clock da 400 MHz a 3 GHz) puo' esegurie, in 
// questo tempo, da 4 a 60 milioni di istruzioni (o piu', nel caso di 
// processori superscalari). E' allora ragionevole che, prima di eseguire ogni 
// trasferimento da memoria di massa, il processo P1, che ha subito page fault 
// e che sta eseguendo la routine di trasferimento, venga bloccato in quel 
// punto e ne venga schedulato un altro, chiamiamolo P2. Il processo P1 verra' 
// risvegliato alla fine del trasferimento, per proseguire con la gestione del 
// page fault dal punto successivo all trasferimento appena conclusosi. Poiche' 
// il processo P2 potrebbe a sua volta generare un page fault e, quindi, 
// tentare di eseguire la stessa routine di trasferimento, questa deve essere 
// rientrante.  Purtroppo, alcune struttre dati utilizzate dalla routine sono 
// per forza di cose condivise: le pagine fisiche (con i loro descrittori) e le 
// tabelle delle pagine condivise (per lo spazio utente condiviso). Per 
// proteggere l'accesso a tali strutture, e' necessario che la routine di 
// trasferimento venga eseguita in mutua esclusione (da cui il semaforo 
// pf_mutex).  Inoltre, le tabelle e le pagine condivise vengono accedute anche 
// al di fuori della mutua esclusione (ad esempio, P2 potrebbe accedere ad una 
// pagina condivisa, presente): per evitare problemi anche in questi casi, 
// bisogna fare attenzione all'ordine con cui si manipolano i descrittori di 
// pagina in tali tabelle (vedi commenti nella routine). Infine, tali strutture 
// dati vengono accedute (lette e scritte) anche dalla routine del timer. La 
// mutua esclusione, in questo caso, e' garantita dal fatto che, mentre la 
// routine di trasferimento accede a tali strutture, le interruzioni sono 
// ancora disabilitate.

// semaforo di mutua esclusione per la gestione dei page fault
int pf_mutex;
// primitive di nucleo usate dal nucleo stesso
extern "C" void sem_wait(int);
extern "C" void sem_signal(int);

// funzioni usate dalla routine di trasferimento
tabella* rimpiazzamento_tabella(bool residente = false);
pagina*  rimpiazzamento_pagina_virtuale(tabella* escludi, bool residente = false);
bool	 carica_pagina_virtuale(descrittore_pagina* pdes_pag, pagina* pag);
bool	 carica_tabella(descrittore_tabella* pdes_tab, tabella* ptab);

// indirizzo_virtale e' l'indirizzo non tradotto. scrittura ci dice se 
// l'accesso che ha causato il fault era in scrittura
pagina* trasferimento(direttorio* pdir, addr indirizzo_virtuale)
{
	descrittore_pagina* pdes_pag;
	pagina* pag = 0;
	tabella* ptab;
	
	// in questa realizzazione, si accede all direttorio e alle tabelle 
	// tramite un indirizzo virtuale uguale al loro indirizzo fisico (in 
	// virtu' del fatto che la memoria fisica e' stata mappata in memoria 
	// virtuale)
	
	// ricaviamo per prima cosa il descrittore di tabella interessato dal 
	// fault
	descrittore_tabella* pdes_tab = &pdir->entrate[indice_direttorio(indirizzo_virtuale)];

	if (pdes_tab->P == 0) {
		// la tabella e' assente
		
		// proviamo ad allocare una pagina fisica per la tabella. Se 
		// non ve ne sono, dobbiamo invocare la routine di 
		// rimpiazzamento per creare lo spazio
		ptab = alloca_tabella();
		if (ptab == 0) {
			ptab = rimpiazzamento_tabella();
			if (ptab == 0)
				goto error1;
		}

		// ora possiamo caricare la tabella (operazione bloccante: 
		// verra' schedulato un altro processo e, quindi, gli interrupt 
		// verrano riabilitati)
		if (! carica_tabella(pdes_tab, ptab) )
			goto error3;
		
		// e collegarla al direttorio
		collega_tabella(pdir, ptab, indice_direttorio(indirizzo_virtuale));
	} else {
		// la tabella e' presente
		ptab = tabella_puntata(pdes_tab);
	}
	// ora ptab punta alla tabella delle pagine (sia se era gia' presente, 
	// sia se e' stata appena caricata)
	

	// ricaviamo il descrittore di pagina interessato dal fault
	pdes_pag = &ptab->entrate[indice_tabella(indirizzo_virtuale)];
	
	// dobbiamo controllare che la pagina sia effettivamente assente
	// (scenario: un processo P1 causa un page fault su una pagina p, si
	// blocca per caricarla e viene schedulato P2, che causa un page fault 
	// sulla stessa pagina p e si blocca sul mutex. Quanto P1 finisce di 
	// caricare p, rilascia il mutex e sveglia P2, che ora trova la pagina 
	// p presente)
	if (pdes_pag->P == 0) {

		// proviamo ad allocare una pagina fisica per la pagina. Se non 
		// ve ne sono, dobbiamo invocare la routine di rimpiazzamento 
		// per creare lo spazio
		pag = alloca_pagina_virtuale();
		if (pag == 0) {
			// passiamo l'indirizzo di ptab alla routine di 
			// rimpiazzamento. In nessun caso la routine deve 
			// scegliere ptab come pagina da rimpiazzare.
			pag = rimpiazzamento_pagina_virtuale(ptab);
			if (pag == 0)
				goto error2;
		}
		
		
		// proviamo a caricare la pagina (operazione bloccante: verra' 
		// schedulato un altro processo e, quindi, gli interrupt 
		// verrano riabilitati)
		if (! carica_pagina_virtuale(pdes_pag, pag) ) 
			goto error2;

		// infine colleghiamo la pagina
		collega_pagina_virtuale(ptab, pag, indirizzo_virtuale);
	} else {
		pag = pagina_puntata(pdes_pag);
	}
	return pag;

error3:	rilascia(ptab);
error2:	if (pag != 0) rilascia(pag);
error1: flog(LOG_WARN, "page fault non risolubile");
	return 0;
}

// carica la pagina descritta da pdes_pag in memoria fisica,
// all'indirizzo pag
bool carica_pagina_virtuale(descrittore_pagina* pdes_pag, pagina* pag)
{
	// come detto precedentemente, trattiamo in modo speciale il caso in 
	// cui la pagina da caricare abbia address == 0: in questo caso, la 
	// pagina non si trova gia' nello swap, ma va allocata e azzerata.  
	// Poiche', dopo questa operazione, al campo address viene assegnato il 
	// numero del blocco (allocato tramite bm_alloc e sicuramente diverso 
	// da 0, in quanto il blocco 0 e' sicuramente occupato dal 
	// superblocco), da questo momento in poi e' come se la pagina fosse 
	// sempre stata presente nello swap
	if (pdes_pag->address == 0) {
		unsigned int blocco;
		if (! bm_alloc(&swap.free, blocco) ) {
			flog(LOG_WARN, "spazio nello swap insufficiente");
			return false;
		}
		pdes_pag->address = blocco;
		memset(pag, 0, SIZE_PAGINA);
		return true;
	} else 
		return leggi_blocco(pdes_pag->address, pag);
}

// carica la tabella descritta da pdes_tab in memoria fisica, all'indirizzo
// ptab
bool carica_tabella(descrittore_tabella* pdes_tab, tabella* ptab)
{
	// anche per le tabelle vale il caso speciale in cui address == 0: la 
	// tabella non si trova nello swap, ma va creata. Per crearla, facciamo 
	// in modo che ogni entrata della nuova tabella erediti le proprieta' 
	// della tabella stessa (in particolare, i bit RW e US) e punti a 
	// pagine da creare (ovvero, con address = 0).
	if (pdes_tab->address == 0) {
		unsigned int blocco;
		if (! bm_alloc(&swap.free, blocco)) {
			flog(LOG_WARN, "spazio nello swap insufficiente");
			return false;
		}
		pdes_tab->address = blocco;

		for (int i = 0; i < 1024; i++) {
			descrittore_pagina* pdes_pag = &ptab->entrate[i];
			pdes_pag->address	= 0;
			pdes_pag->D		= 0;
			pdes_pag->A		= 0;
			pdes_pag->pgsz		= 0;
			pdes_pag->RW		= pdes_tab->RW;
			pdes_pag->US		= pdes_tab->US;
			pdes_pag->P		= 0;
		}
		return true;
	} else 
		return leggi_blocco(pdes_tab->address, ptab);
}


// ROUTINE DI RIMPIAZZAMENTO
//
// forward
int rimpiazzamento();

// funzioni di comodo per il rimpiazzamento, specializzate per le
// tabelle e per le pagine
direttorio* rimpiazzamento_direttorio()
{
	int i = rimpiazzamento();
	if (i == -1) return 0;
	des_pf* ppf = &pagine_fisiche[i];

	ppf->contenuto = DIRETTORIO;
	ppf->residente = true;
	return todir(indirizzo_pf(i));
}

tabella* rimpiazzamento_tabella(bool residente)
{
	int i = rimpiazzamento();
	if (i == -1) return 0;
	des_pf* ppf = &pagine_fisiche[i];

	ppf->contenuto = TABELLA;
	ppf->residente = residente;
	return totab(indirizzo_pf(i));
}

pagina* rimpiazzamento_pagina_virtuale(tabella* escludi, bool residente) 
{
	// Per impedire che venga scelta la tabella "escludi", la rendiamo 
	// temporaneamente residente
	des_pf *p = &pagine_fisiche[indice_pf(escludi)];
	bool save = p->residente;
	p->residente = true;
	int i = rimpiazzamento();
	p->residente = save;
	if (i == -1) return 0;

	des_pf* ppf = &pagine_fisiche[i];
	ppf->contenuto = PAGINA_VIRTUALE;
	ppf->residente = residente;

	return topag(indirizzo_pf(i));
}

// funzioni usate da rimpiazzamento
extern "C" void invalida_entrata_TLB(addr ind_virtuale);

// routine di rimpiazzamento vera e propria.
// libera una pagina fisica rimuovendo e (eventualmente) copiando nello swap 
// una tabella privata o una pagina virtuale
int rimpiazzamento()
{
	
	des_pf *ppf, *vittima = 0;
	addr ind_virtuale;
	unsigned int blocco;
	
	// scorriamo tutti i descrittori di pagina fisica alla ricerca della 
	// prima pagina che contiene una pagina virtuale o una tabella privata
	int i = 0, j = -1;
	while (i < num_pagine_fisiche && pagine_fisiche[i].residente)
		i++;

	// se non ce ne sono, la situazione e' critica
	if (i >= num_pagine_fisiche)
		goto error;

	// partendo da quella appena trovata, cerchiamo la pagina con il valore 
	// minimo per il contatore delle statistiche (gli interrupt sono 
	// disabilitati, garantendo la mutua esclusione con la routine del 
	// timer)
	j = i;
	vittima = &pagine_fisiche[j];
	for (i++; i < num_pagine_fisiche; i++) {
		ppf = &pagine_fisiche[i];
		if (ppf->residente)
			continue;
		switch (ppf->contenuto) {
		case PAGINA_VIRTUALE:
			// se e' una pagina virtuale, dobbiamo preferirla a una 
			// tabella, in caso di uguaglianza nei contatori
			if (ppf->contatore < vittima->contatore ||
			    ppf->contatore == vittima->contatore && vittima->contenuto == TABELLA) {
				j = i;
				vittima = &pagine_fisiche[j];
			}
			break;
		case TABELLA:
			if (ppf->contatore < vittima->contatore) {
				j = i;
				vittima = &pagine_fisiche[j];
			}
			break;
		default:
			// in tutti gli altri casi la pagina non e' 
			// rimpiazzabile
			break;
		}
	}

	// assert (vittima != 0);

	if (vittima->contenuto == TABELLA) {
		// usiamo le informazioni nel mapping inverso per ricavare 
		// subito l'entrata nel direttorio da modificare
		descrittore_tabella *pdes_tab =
			scollega_tabella(vittima->tab.pdir, vittima->tab.indice);

		// "dovremmo" cancellarla e basta, ma contiene i blocchi delle
		// pagine allocate dinamicamente
		if (!scrivi_blocco(pdes_tab->address, topag(indirizzo_pf(j))))
			goto error;
	} else {
		// usiamo le informazioni nel mapping inverso per ricavare 
		// subito l'entrata nel direttorio da modificare
		tabella* ptab = vittima->pag.ptab;

		// dobbiamo rendere la pagina non presente prima di cominciare 
		// a salvarla nello swap (altrimenti, qualche altro processo 
		// potrebbe cercare di scrivervi mentre e' ancora in corso 
		// l'operazione di salvataggio)
		descrittore_pagina* pdes_pag = scollega_pagina_virtuale(ptab, vittima->pag.ind_virtuale);
		invalida_entrata_TLB(vittima->pag.ind_virtuale);

		if (pdes_pag->D == 1) {
			// pagina modificata, va salvata nello swap (operazione 
			// bloccante)
			if (!scrivi_blocco(pdes_pag->address, topag(indirizzo_pf(j))))
				goto error;
		} 
	}
	return j;

error:
	flog(LOG_WARN, "Impossibile rimpiazzare pagine");
	return 0;
}

// primtiva che permette di rendere alcune pagine residenti (il programmatore 
// deve usarla, ad esempio, quando un certo buffer di I/O deve trovarsi 
// permanentemente in memoria fisica).
// La primitiva carica tutte le pagine (e le relative tabelle) che contengono i 
// byte nell'intervallo [start, start + quanti) e le marca come residenti (non 
// rimpiazzabili). Restituisce il numero di byte che e' riuscita a rendere 
// residenti (potrebbero essere meno di "quanti", in caso di errore)
extern "C" int c_resident(addr start, int quanti)
{
	int da_fare;
	addr vero_start, last, indirizzo_virtuale;
	direttorio *pdir;
	des_pf *ppf;

	if (quanti <= 0)
		return 0;

	// calcoliamo l'indirizzo della pagina che contiene "start"
	vero_start = start & ~(SIZE_PAGINA - 1);
	// quindi il numero di pagine da rendere residenti
	da_fare = ceild(start - vero_start + quanti, SIZE_PAGINA);
	// e l'indirizzo del primo byte che non sara' reso residente
	last = vero_start + da_fare * SIZE_PAGINA;

	// accesso in mutua esclusione con la routine di trasferimento
	// (entrambe manipolano le stesse strutture dati)
	sem_wait(pf_mutex);

	pdir = leggi_cr3();

	indirizzo_virtuale = vero_start;
	while(indirizzo_virtuale < last)
	{
		descrittore_tabella *pdes_tab = &pdir->entrate[indice_direttorio(indirizzo_virtuale)];
		tabella *ptab;

		if (pdes_tab->P == 1) {
			ptab = tabella_puntata(pdes_tab);
		} else {
			ptab = alloca_tabella();
			if (ptab == 0) 
				ptab = rimpiazzamento_tabella();
			if (! carica_tabella(pdes_tab, ptab) ) {
				rilascia(ptab);
				goto out;
			}
			collega_tabella(pdir, ptab, indice_direttorio(indirizzo_virtuale));
		} 
			
		for (int i = indice_tabella(indirizzo_virtuale); i < 1024 && indirizzo_virtuale < last; i++)
		{
			descrittore_pagina *pdes_pag =
				&ptab->entrate[i];
			pagina *pag;

			if (pdes_pag->P == 0) {
				pag = alloca_pagina_virtuale();
				if (pag == 0)  
					pag = rimpiazzamento_pagina_virtuale(ptab);
				if (! carica_pagina_virtuale(pdes_pag, pag) ) {
					rilascia(pag);
					goto out;
				}
				collega_pagina_virtuale(ptab, pag, indirizzo_virtuale);
				pdes_pag->D = 0;
			} else {
				pag = pagina_puntata(pdes_pag);
			}
			// marchiamo la pagina come residente
			ppf = &pagine_fisiche[indice_pf(pag)];
			ppf->residente = true;

			// prossimo indirizzo virtuale da considerare
			indirizzo_virtuale = indirizzo_virtuale + SIZE_PAGINA;
		}
	}

out:
	// qualunque cosa sia accaduta, rilasciamo il semaforo di mutua 
	// esclusione
	sem_signal(pf_mutex);

	if (indirizzo_virtuale > start + quanti)
		indirizzo_virtuale = start + quanti;

	return indirizzo_virtuale - start;
}


//////////////////////////////////////////////////////////////////////////////////
// CREAZIONE PROCESSI                                                           //
//////////////////////////////////////////////////////////////////////////////////

// descrittore di processo
struct des_proc {			// offset:
//	short id;
	unsigned int link;		// 0
//	int punt_nucleo;
	addr        esp0;		// 4
	unsigned int ss0;		// 8
	addr        esp1;		// 12
	unsigned int ss1;		// 16
	addr        esp2;		// 20
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

// numero di processi attivi
int processi = 0;

// elemento di una coda di processi
struct proc_elem {
	short identifier;
	int priority;
	proc_elem *puntatore;
};

// manipolazione delle code di processi
void inserimento_coda(proc_elem *&p_coda, proc_elem *p_elem);
void rimozione_coda(proc_elem *&p_coda, proc_elem *&p_elem);

// processo in esecuzione
extern proc_elem *esecuzione;

// coda dei processi pronti
extern proc_elem *pronti;

// funzioni usate da crea_processo
pagina* crea_pila_utente(direttorio* pdir);
pagina* crea_pila_sistema(direttorio* pdir);
void rilascia_tutto(direttorio* pdir, addr start, int ntab);
extern "C" int alloca_tss(des_proc* p);
extern "C" void rilascia_tss(int indice);

// ritorna il descrittore del processo id
extern "C" des_proc *des_p(short id);

// elemento di coda e descrittore del primo processo (quello che esegue il 
// codice di inizializzazione e la funzione main)
proc_elem init;
des_proc des_main;

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

	pdes_proc = new des_proc;
	if (pdes_proc == 0) goto errore2;
	memset(pdes_proc, 0, sizeof(des_proc));

	identifier = alloca_tss(pdes_proc);
	if (identifier == 0) goto errore3;

        p->identifier = identifier;
        p->priority = prio;

	// pagina fisica per il direttorio del processo
	pdirettorio = alloca_direttorio();
	if (pdirettorio == 0) {
		pdirettorio = rimpiazzamento_direttorio();
		if (pdirettorio == 0)
			goto errore4;
	}

	// il direttorio viene inizialmente copiato dal direttorio principale
	// (in questo modo, il nuovo processo acquisisce automaticamente gli
	// spazi virtuali condivisi, sia a livello utente che a livello
	// sistema, tramite condivisione delle relative tabelle delle pagine)
	*pdirettorio = *direttorio_principale;
	
	pila_sistema = crea_pila_sistema(pdirettorio);
	if (pila_sistema == 0) goto errore5;

	if (liv == LIV_UTENTE) {

		pila_sistema->parole_lunghe[1019] = (addr)f;	// EIP (codice utente)
		pila_sistema->parole_lunghe[1020] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pila_sistema->parole_lunghe[1021] = 0x00000200;	// EFLAG
		pila_sistema->parole_lunghe[1022] = fine_utente_privato - 2 * sizeof(int);
									// ESP (pila utente)
		pila_sistema->parole_lunghe[1023] = SEL_DATI_UTENTE;	// SS (pila utente)
		// eseguendo una IRET da questa situazione, il processo
		// passera' ad eseguire la prima istruzione della funzione f,
		// usando come pila la pila utente (al suo indirizzo virtuale)

		pila_utente = crea_pila_utente(pdirettorio);
		if (pila_utente == 0) goto errore6;

		// dobbiamo ora fare in modo che la pila utente si trovi nella
		// situazione in cui si troverebbe dopo una CALL alla funzione
		// f, con parametro a:
		pila_utente->parole_lunghe[1022] = 0xffffffff;	// ind. di ritorno non sign.
		pila_utente->parole_lunghe[1023] = a;		// parametro del proc.

		// infine, inizializziamo il descrittore di processo

		// indirizzo del bottom della pila sistema, che verra' usato
		// dal meccanismo delle interruzioni
		pdes_proc->esp0 = fine_sistema_privato;
		pdes_proc->ss0  = SEL_DATI_SISTEMA;

		// inizialmente, il processo si trova a livello sistema, come
		// se avesse eseguito una istruzione INT, con la pila sistema
		// che contiene le 5 parole lunghe preparate precedentemente
		pdes_proc->esp = fine_sistema_privato - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_UTENTE;
		pdes_proc->es  = SEL_DATI_UTENTE;

		pdes_proc->cr3 = pdirettorio;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->liv = LIV_UTENTE;
		// tutti gli altri campi valgono 0
	} else {
		pila_sistema->parole_lunghe[1019] = (addr)f;	  // EIP (codice sistema)
		pila_sistema->parole_lunghe[1020] = SEL_CODICE_SISTEMA;	  // CS (codice sistema)
		pila_sistema->parole_lunghe[1021] = 0x00000200;  	  // EFLAG
		pila_sistema->parole_lunghe[1022] = 0xffffffff;		  // indirizzo ritorno?
		pila_sistema->parole_lunghe[1023] = a;			  // parametro
		// i processi esterni lavorano esclusivamente a livello
		// sistema. Per questo motivo, prepariamo una sola pila (la
		// pila sistema)

		pdes_proc->esp = fine_sistema_privato - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_SISTEMA;
		pdes_proc->es  = SEL_DATI_SISTEMA;

		pdes_proc->cr3 = pdirettorio;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->liv = LIV_SISTEMA;
	}

	return p;

errore6:	rilascia_tutto(pdirettorio, inizio_sistema_privato, ntab_sistema_privato);
errore5:	rilascia(pdirettorio);
errore4:	rilascia_tss(identifier);
errore3:	delete pdes_proc;
errore2:	delete p;
errore1:	return 0;
}

// esegue lo shutdown del sistema
extern "C" void shutdown();

// corpo del processo dummy
void dd(int i)
{
	while (processi > 0)
		;
	shutdown();

}

// creazione del processo dummy (usata in fase di inizializzazione del sistema)
bool crea_dummy()
{
	proc_elem* dummy = crea_processo(dd, 0, -1, LIV_SISTEMA);
	if (dummy == 0) {
		flog(LOG_ERR, "Impossibile creare il processo dummy");
		return false;
	}
	inserimento_coda(pronti, dummy);
	return true;
}
void main_proc(int n);
bool crea_main()
{
	proc_elem* m = crea_processo(main_proc, 0, MAX_PRIORITY, LIV_SISTEMA);
	if (m == 0) {
		flog(LOG_ERR, "Impossibile creare il processo main");
		return false;
	}
	processi = 1;
	inserimento_coda(pronti, m);
	return true;
}

// creazione della pila utente
pagina* crea_pila_utente(direttorio* pdir)
{
	addr ind_virtuale; 
	pagina *ind_fisico;
	descrittore_tabella* pdes_tab;
	descrittore_pagina* pdes_pag;

	ind_virtuale = inizio_utente_privato;
	// prepariamo i descrittori di tabella per tutto lo spazio 
	// utente/privato. Viene usata l'ottimizzazione address = 0
	// (le tabelle verranno create solo se effettivamente utilizzate)
	for (int i = 0; i < ntab_utente_privato; i++) {

		pdes_tab = &pdir->entrate[indice_direttorio(ind_virtuale)];
		pdes_tab->US	  = 1;
		pdes_tab->RW	  = 1;
		pdes_tab->address = 0;
		pdes_tab->PCD	  = 0;
		pdes_tab->PWT     = 0;
		pdes_tab->D	  = 0;
		pdes_tab->A	  = 0;
		pdes_tab->global  = 0;
		pdes_tab->P	  = 0;

		ind_virtuale = ind_virtuale + SIZE_SUPERPAGINA;
	}

	// l'ultima pagina della pila va preallocata e precaricata, in quanto 
	// la crea processo vi dovra' scrivere le parole lunghe di 
	// inizializzazione
	ind_virtuale = fine_utente_privato - SIZE_PAGINA;
	ind_fisico  = reinterpret_cast<pagina*>(trasferimento(pdir, ind_virtuale));
	if (ind_fisico != 0) {
		pdes_tab = &pdir->entrate[indice_direttorio(ind_virtuale)];
		pdes_pag = &tabella_puntata(pdes_tab)->entrate[indice_tabella(ind_virtuale)];
		pdes_pag->D = 1;
	}
	return ind_fisico;
}

// crea la pila sistema di un processo
// la pila sistema e' grande una sola pagina ed e' sempre residente
pagina* crea_pila_sistema(direttorio* pdir)
{
	pagina* ind_fisico;
	tabella* ptab;
	descrittore_tabella* pdes_tab;
	descrittore_pagina*  pdes_pag;
	des_pf*	     ppf;

	// l'indirizzo della cima della pila e' una pagina sopra alla fine 
	// dello spazio sistema/privato
	addr ind = fine_sistema_privato - SIZE_PAGINA;

	ptab = alloca_tabella(true);
	if (ptab == 0) {
		ptab = rimpiazzamento_tabella(true);
		if (ptab == 0)
			goto errore1;
	}

	pdes_tab = &pdir->entrate[indice_direttorio(ind)];
	pdes_tab->address = toaddr(ptab) >> 12;
	pdes_tab->global  = 0;
	pdes_tab->US	  = 0; // livello sistema
	pdes_tab->RW	  = 1; // scrivibile
	pdes_tab->D	  = 0;
	pdes_tab->pgsz    = 0;
	pdes_tab->P	  = 1;

	ind_fisico = alloca_pagina_virtuale(true);
	if (ind_fisico == 0) {
		ind_fisico = rimpiazzamento_pagina_virtuale(ptab, true);
		if (ind_fisico == 0)
			goto errore2;
	}

	memset(ptab, 0, SIZE_PAGINA);
	pdes_pag = &ptab->entrate[indice_tabella(ind)];
	pdes_pag->address = toaddr(ind_fisico) >> 12;
	pdes_pag->pgsz	  = 0;
	pdes_pag->US	  = 0;
	pdes_pag->RW	  = 1;
	pdes_pag->P	  = 1;

	return ind_fisico;

errore2: 	rilascia(ptab);
errore1:	return 0;
}
	
// rilascia tutte le strutture dati private associate al processo puntato da 
// "p" (tranne il proc_elem puntato da "p" stesso)
void distruggi_processo(proc_elem* p)
{
	des_proc* pdes_proc = des_p(p->identifier);

	direttorio* pdirettorio = pdes_proc->cr3;
	rilascia_tutto(pdirettorio, inizio_sistema_privato, ntab_sistema_privato);
	rilascia_tutto(pdirettorio, inizio_utente_privato,  ntab_utente_privato);
	rilascia(pdirettorio);
	rilascia_tss(p->identifier);
	delete pdes_proc;
}

// rilascia ntab tabelle (con tutte le pagine da esse puntate) a partire da 
// quella che mappa l'indirizzo start, nel direttorio pdir
// (start deve essere un multiplo di 4 MiB)
void rilascia_tutto(direttorio* pdir, addr start, int ntab)
{
	int j = indice_direttorio(start);
	for (int i = j; i < j + ntab; i++)
	{
		descrittore_tabella* pdes_tab = &pdir->entrate[i];
		if (pdes_tab->P == 1) {
			tabella* ptab = tabella_puntata(pdes_tab);
			for (int k = 0; k < 1024; k++) {
				descrittore_pagina* pdes_pag = &ptab->entrate[k];
				if (pdes_pag->P == 1)
					rilascia(pagina_puntata(pdes_pag));
				else if (pdes_pag->address != 0)
					bm_free(&swap.free, pdes_pag->address);
			}
			rilascia(ptab);
		}
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

// verifica i diritti del processo in esecuzione sull' area di memoria
// specificata dai parametri
extern "C" bool verifica_area(void *area, unsigned int dim, bool write);


// resetta controllore interruzioni
extern "C" void reset_8259();

extern "C" short
c_activate_p(void f(int), int a, int prio, char liv)
{
	proc_elem	*p;			// proc_elem per il nuovo processo
	short id = 0;

	sem_wait(pf_mutex);

	p = crea_processo(f, a, prio, liv);

	if (p != 0) {
		inserimento_coda(pronti, p);
		processi++;
		id = p->identifier;
		flog(LOG_INFO, "proc=%d entry=%x(%d) prio=%d liv=%d", id, f, a, prio, liv);
	}

	sem_signal(pf_mutex);

	return id;
}

//Disabilito le interruzioni, ripristino l'ebp della multiboot_entry in sistema.d e ritorno
//al chiamante (bldr32)
void shutdown()
{
	flog(LOG_INFO, "Tutti i processi sono terminati!");
		reset_8259();   //Ripristino i registri dei PIC
asm("cli;movl %0, %%eax;movl %%eax, %%ebp;movl %%ebp, %%esp;leave;ret;": :"r"(return_ebp):);
}

extern "C" void c_terminate_p()
{
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;
	flog(LOG_INFO, "Processo %d terminato", p->identifier);
	delete p;
	schedulatore();
}


// come la terminate_p, ma invia anche un warning al log (da invocare quando si 
// vuole terminare un processo segnalando che c'e' stato un errore)
extern "C" void c_abort_p()
{
	proc_elem *p = esecuzione;
	distruggi_processo(p);
	processi--;
	flog(LOG_WARN, "Processo %d abortito", p->identifier);
	delete p;
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
	delete a_p_save[irq];
	a_p_save[irq] = 0;
	return true;

}


extern "C" short c_activate_pe(void f(int), int a, int prio, char liv, int irq)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	sem_wait(pf_mutex);

	p = crea_processo(f, a, prio, liv);
	if (p == 0)
		goto error1;
		
	if (!aggiungi_pe(p, irq) ) 
		goto error2;

	flog(LOG_INFO, "estern=%d entry=%x(%d) prio=%d liv=%d irq=%d", p->identifier, f, a, prio, liv, irq);

	sem_signal(pf_mutex);

	return p->identifier;

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
	    (nuovo = new des_heap) != 0)
	{
		// inseriamo nuovo nella lista, dopo scorri
		nuovo->start = scorri->start + dim; // rispettiamo (2)
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
extern "C" void c_mem_free(void *pv)
{
	if (pv == 0) return;
	addr p = toaddr(pv);

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


extern "C" int c_give_num()
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
unsigned int sem_buf[MAX_SEMAFORI / (sizeof(int) * 8) + 1];
bm_t sem_bm = { sem_buf, MAX_SEMAFORI };

// alloca un nuovo semaforo e ne restiutisce l'indice (da usare con sem_wait e 
// sem_signal). Se non ci sono piu' semafori disponibili, restituisce l'indice 
// 0 (NOTA: la firma della funzione e' diversa da quella del testo, in quanto 
// sono stati eliminati i parametri di ritorno. Il testo verra' aggiornato alla 
// prossima edizione).
extern "C" int c_sem_ini(int val)
{
	unsigned int pos;
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
		inserimento_coda(s->pointer, esecuzione);
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
		rimozione_coda(s->pointer, lavoro);
		if(lavoro->priority <= esecuzione->priority)
			inserimento_coda(pronti, lavoro);
		else { // preemption
			inserimento_coda(pronti, esecuzione);
			esecuzione = lavoro;
		}
	}
}

// richiesta al timer
struct richiesta {
	int d_attesa;
	richiesta *p_rich;
	proc_elem *pp;
};

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

// verifica i permessi del processo sui parametri passati (problema del
//  Cavallo di Troia)
extern "C" bool c_verifica_area(void* a, unsigned int dim, bool write)
{
	des_proc *pdes_proc;
	direttorio* pdirettorio;
	char liv;
	addr area = toaddr(a);
	
	pdes_proc = des_p(esecuzione->identifier);
	liv = pdes_proc->liv;
	pdirettorio = pdes_proc->cr3;

	for (addr i = area; i < area + dim; i += SIZE_PAGINA) {
		descrittore_tabella *pdes_tab = &pdirettorio->entrate[indice_direttorio(i)];
		if (liv == LIV_UTENTE && pdes_tab->US == 0)
			return false;
		if (write && pdes_tab->RW == 0)
			return false;
		if (pdes_tab->P == 0)
			continue;
		tabella *ptab = tabella_puntata(pdes_tab);
		descrittore_pagina *pdes_pag = &ptab->entrate[indice_tabella(i)];
		if (liv == LIV_UTENTE && pdes_pag->US == 0)
			return false;
		if (write && pdes_pag->RW == 0)
			return false;
	}
	return true;
}

// Il log e' un buffer circolare di messaggi, dove ogni messaggio e' composto 
// da tre campi:
// - sev: la severita' del messaggio (puo' essere un messaggio di debug, 
// informativi, un avviso o un errore)
// - identifier: l'identificatore del processo che era in esecuzione quando il 
// messaggio e' stato inviato
// - msg: il messaggio vero e proprio

// numero di messaggi nella coda circolare
const unsigned int LOG_MSG_NUM = 100;

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
	unsigned int prot  : 1;
	unsigned int write : 1;
	unsigned int user  : 1;
	unsigned int res   : 1;
	unsigned int pad   : 28; // bit non significativi
};


// c_page_fault viene chiamata dal gestore dell'eccezione di page fault (in 
// sistema.S), che provvede a salvare in pila il valore del registro %cr2. Tale 
// valore viene qui letto nel parametro "indirizzo_virtuale". "errore" e' il 
// codice di errore descritto prima, mentre "eip" e' l'indirizzo 
// dell'istruzione che ha causato il fault. Sia "errore" che "eip" vengono 
// salvati in pila dal microprogramma di gestione dell'eccezione
extern "C" void c_page_fault(addr indirizzo_virtuale, page_fault_error errore, addr eip)
{
	if (eip < fine_codice_sistema || errore.res == 1) {
		// il sistema non e' progettato per gestire page fault causati 
		// dalle primitie di nucleo, quindi, se cio' si e' verificato, 
		// si tratta di un bug
		flog(LOG_ERR, "eip: %x, page fault a %x: %s, %s, %s, %s", eip, indirizzo_virtuale,
			errore.prot  ? "protezione"	: "pag/tab assente",
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema",
			errore.res   ? "bit riservato"	: "");
		panic("page fault dal modulo sistema");
	}
	// l'errore di protezione non puo' essere risolto: il processo ha 
	// tentato di accedere ad indirizzi proibiti (cioe', allo spazio 
	// sistema)
	if (errore.prot == 1) {
		flog(LOG_WARN, "errore di protezione: eip=%x, ind=%x, %s, %s", eip, indirizzo_virtuale,
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema");
		abort_p();
	}

	// in tutti gli altri casi, proviamo a trasferire la pagina mancante
	sem_wait(pf_mutex);
	void *v = trasferimento(leggi_cr3(), indirizzo_virtuale);
	sem_signal(pf_mutex);

	if (v == 0) {
		abort_p();
	}
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
void rimozione_coda(proc_elem *&p_coda, proc_elem *&p_elem)
{
	p_elem = p_coda;

	if(p_coda)
		p_coda = p_coda->puntatore;
}

// scheduler a priorita'
extern "C" void schedulatore(void)
{
	rimozione_coda(pronti, esecuzione);
}


// coda delle richieste al timer
richiesta *descrittore_timer;

// inserisce P nella coda delle richieste al timer
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


// la variabile globale ticks conta il numero di interruzioni ricevute dal 
// timer, dall'avvio del sistema
extern unsigned long ticks;

// driver del timer
extern "C" void c_driver_t(void)
{
	richiesta *p;

	// conta il numero di interruzioni di timer ricevute
	ticks++;

	if(descrittore_timer != 0)
		descrittore_timer->d_attesa--;

	while(descrittore_timer != 0 && descrittore_timer->d_attesa == 0) {
		inserimento_coda(pronti, descrittore_timer->pp);
		p = descrittore_timer;
		descrittore_timer = descrittore_timer->p_rich;
		delete p;
	}

	// aggiorna le statitische per la memoria virtuale
	aggiorna_statistiche();

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
void *memset(void *dest, int c, unsigned int n)
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
char *strncpy(char *dest, const char *src, unsigned int l)
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
static void itostr(char *buf, unsigned int len, long l)
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
int vsnprintf(char *str, unsigned int size, const char *fmt, va_list ap)
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
extern "C" int snprintf(char *buf, unsigned int n, const char *fmt, ...)
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
void bm_create(bm_t *bm, unsigned int *buffer, unsigned int size)
{
	bm->vect = buffer;
	bm->size = size;
	bm->vecsize = ceild(size, sizeof(unsigned int) * 8);

	for (int i = 0; i < bm->vecsize; ++i)
		bm->vect[i] = 0xffffffff;
}


// usa l'istruzione macchina BSF (Bit Scan Forward) per trovare in modo
// efficiente il primo bit a 1 in v
extern "C" int trova_bit(unsigned int v);

bool bm_alloc(bm_t *bm, unsigned int& pos)
{

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
		        unsigned int	 eip1,
			unsigned short	 cs,
			unsigned int	 eflags,
			unsigned int 	 eip2)
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

	des_proc* p = des_p(esecuzione->identifier);
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
	log_buf.buf[log_buf.last].identifier = esecuzione->identifier;
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
			rimozione_coda(s->pointer, lavoro);
			inserimento_coda(pronti, lavoro);
		}
	}
}
extern "C" void c_log(log_sev sev, const char* buf, int quanti)
{
	inserimento_coda(pronti, esecuzione);
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
typedef char *ind_b;	// indirizzo di una porta a 8 bit
// ingresso di un byte da una porta di IO
extern "C" void inputb(ind_b reg, char &a);
// uscita di un byte su una porta di IO
extern "C" void outputb(char a, ind_b reg);
// ingresso di una stringa di word da un buffer di IO
extern "C" void inputbw(ind_b reg, unsigned short *a,short n);
// uscita di una stringa di word su un buffer di IO
extern "C" void outputbw(unsigned short *a, ind_b reg,short n);

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
	union {				// Command Block Register
		ind_b iCMD;		// 0
		ind_b iSTS;		// 0
	}; //iCMD_iSTS;
	ind_b iDATA;			// 4
	union {
		ind_b iFEATURES;	// 8
		ind_b iERROR;		// 8
	}; //iFEATURES_iERROR
	ind_b iSTCOUNT;			// 12
	ind_b iSECT_N;			// 16
	ind_b iCYL_L_N;			// 20
	ind_b iCYL_H_N;			// 24
	union {
		ind_b iHD_N;		// 28
		ind_b iDRV_HD;		// 32
	}; //iHD_N_iDRV_HD
	union {				// Control Block Register
		ind_b iALT_STS;		// 36
		ind_b iDEV_CTRL;	// 40
	}; //iALT_STS_iDEV_CTRL
}; // size = 44

struct drive {				// Drive su un canale ATA
	bool presente;
	bool dma;
	unsigned int tot_sett;
	partizione* part;
};

struct des_ata {		// Descrittore operazione per i canali ATA
	interfata_reg indreg;
	drive disco[2];		// Ogni canale ATA puo' contenere 2 drive
	hd_cmd comando;
	char errore;
	char cont;
	unsigned short* punt;	
	int mutex;
	int sincr;
};

const int A = 2;
extern "C" des_ata hd[A];	// 2 canali ATA

extern "C" void go_inouthd(ind_b i_dev_ctl);
extern "C" void halt_inouthd(ind_b i_dev_ctl);
extern "C" bool test_canale(ind_b st,ind_b stc,ind_b stn);
extern "C" int leggi_signature(ind_b stc,ind_b stn,ind_b cyl,ind_b cyh);
extern "C" void umask_hd(ind_b h);
extern "C" void mask_hd(ind_b h);
extern "C" bool hd_software_reset(ind_b dvctrl);
extern "C" void hd_read_device(ind_b i_drv_hd, int& ms);
extern "C" void hd_select_device(short ms,ind_b i_drv_hd);
extern "C" void hd_write_address(des_ata *p, unsigned int primo);	
extern "C" void hd_write_command(hd_cmd cmd, ind_b i_drv_cmd);


// seleziona il dispostivo drv e controlla che la selezione abbia avuto 
// successo
bool hd_sel_drv(des_ata* p_des, int drv)
{
	char stato;
	int curr_drv;

	hd_select_device(drv, p_des->indreg.iDRV_HD);

	do {
		inputb(p_des->indreg.iSTS, stato);
	} while ( (stato & HD_STS_BSY) || (stato & HD_STS_DRQ) );

	hd_read_device(p_des->indreg.iDRV_HD, curr_drv);

	return  (curr_drv == drv);
}

// aspetta finche' non e' possibile leggere dal registro dei dati
// (DRQ deve essere 1, BSY 0 e ERR 0)
bool hd_wait_data(des_ata* p_des)
{
	char stato;

	do {
		inputb(p_des->indreg.iALT_STS, stato);
	} while (stato & HD_STS_BSY);

	return ( (stato & HD_STS_DRQ) && !(stato & HD_STS_ERR) );
}

// invia al log di sistema un messaggio che spiega l'errore passato come 
// argomento
void hd_print_error(int i, int d, int sect, short error)
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

// Avvio delle operazioni di lettura da hard disk
bool gohd_input(des_ata* p_des, int drv, unsigned int primo, unsigned char quanti)
{
	if (!hd_sel_drv(p_des, drv))		// selezione del drive
		return false;
	hd_write_address(p_des, primo);			// parametri: primo settore
	outputb(quanti, p_des->indreg.iSTCOUNT);	// parametri: quanti settori
	go_inouthd(p_des->indreg.iDEV_CTRL);		// abilita interruzioni	
	hd_write_command(READ_SECT, p_des->indreg.iCMD);// invia comando
	return true;
}


// inizializza il descrittore d'operazione e avvia la lettura
void starthd_in(des_ata *p_des, short drv, unsigned short vetti[], unsigned int primo, unsigned char quanti)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->comando = READ_SECT;
	p_des->errore = D_ERR_NONE;
	if (!gohd_input(p_des, drv, primo, quanti))
		goto errore;
	return;

errore:
	panic("Errore nella lettura");
}

// Avvio delle operazioni di scrittura su hard disk
bool gohd_output(des_ata *p_des, int drv, unsigned short vetto[], unsigned int primo, unsigned char quanti)
{
	if (!hd_sel_drv(p_des, drv))		// selezione del drive
		return false;
	hd_write_address(p_des, primo);			// invio parametri (primo settore)
	outputb(quanti, p_des->indreg.iSTCOUNT);	// invio parametri (quanti settori)
	go_inouthd(p_des->indreg.iDEV_CTRL);		// abilita interruzioni
	hd_write_command(WRITE_SECT, p_des->indreg.iCMD);// invia comando
	if (!hd_wait_data(p_des))
		return false;
	outputbw(vetto, p_des->indreg.iDATA, DIM_BLOCK / 2);
	return true;
}

// inizializza il descrittore d'operazione e avvia la scrittura
void starthd_out(des_ata *p_des, short drv, unsigned short vetto[], unsigned int primo, unsigned char quanti)	
{	char stato;				
	bool ris;
	p_des->cont = quanti;
	p_des->punt = vetto + DIM_BLOCK / 2;
	p_des->comando = WRITE_SECT;
	p_des->errore = D_ERR_NONE;
	if (!gohd_output(p_des, drv, vetto, primo, quanti))
		goto errore;
	return;

errore:
	panic("Errore nella scrittura");
}


// Primitiva di lettura da hard disk: legge quanti blocchi (max 256, char!)
//  a partire da primo depositandoli in vetti; il controller usato e ind_ata,
//  il drive e' drv. Riporta un fallimento in errore
extern "C" void c_readhd_n(short ind_ata, short drv, unsigned short vetti[],
		unsigned int primo, unsigned char quanti, char &errore)
{
	des_ata *p_des;

	// Controllo sulla protezione
	if (ind_ata < 0 || ind_ata >= A || drv < 0 || drv >= 2) {
		errore = D_ERR_PRESENCE;
		return;
	}

	p_des = &hd[ind_ata];

	//if (!verifica_area(&quanti,sizeof(int),true) ||
	//!verifica_area(vetti,quanti,true) ||
	//!verifica_area(&errore,sizeof(char),true)) return;

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

// Primitiva di scrittura su hard disk: scrive quanti blocchi (max 256, char!)
//  a partire da primo prelevandoli da vetti; il controller usato e' ind_ata,
//  il drive e' drv. Riporta un fallimento in errore
extern "C" void c_writehd_n(short ind_ata, short drv, unsigned short vetti[], unsigned int primo,
		unsigned char quanti, char &errore)
{
	des_ata *p_des;
	
	// Controllo sulla protezione
	if (ind_ata < 0 || ind_ata >= A || drv < 0 || drv >= 2) {
		errore = D_ERR_PRESENCE;
		return;
	}

	p_des = &hd[ind_ata];

	//if (!verifica_area(&quanti,sizeof(int),true) ||
	//!verifica_area(vetti,quanti,true) ||
	//!verifica_area(&errore,sizeof(char),true)) return;

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
	starthd_out(p_des,drv,vetti,primo,quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}



extern "C" void c_driver_hd(int ata)			// opera su desintb[interf]
{	
	proc_elem* lavoro; des_sem* s; des_ata* p_des;
	char stato;
	p_des = &hd[ata];
	p_des->cont--; 
	p_des->errore = D_ERR_NONE;
	if (p_des->cont == 0) {	
		halt_inouthd(p_des->indreg.iDEV_CTRL);	
		s = &array_dess[p_des->sincr];
		s->counter++;
		if (s->counter <= 0) {
			rimozione_coda(s->pointer, lavoro);
			inserimento_coda(pronti, lavoro);
		}
	}
	inputb(p_des->indreg.iSTS, stato); // ack dell'interrupt
	if (p_des->comando == READ_SECT) { 
		if (!hd_wait_data(p_des)) 
			inputb(p_des->indreg.iERROR, p_des->errore);
		else
			inputbw(p_des->indreg.iDATA, p_des->punt, DIM_BLOCK / 2);
	} else {
		if (p_des->cont != 0) {
			if (!hd_wait_data(p_des)) 
				inputb(p_des->indreg.iERROR, p_des->errore);
			else
				outputbw(p_des->punt, p_des->indreg.iDATA, DIM_BLOCK / 2);
		}
	}
	p_des->punt += DIM_BLOCK / 2;
	schedulatore();
}



// descrittore di partizione. Le uniche informazioni che ci interessano sono 
// "offset" e "sectors"
struct des_part {
	unsigned int active	: 8;
	unsigned int head_start	: 8;
	unsigned int sec_start	: 6;
	unsigned int cyl_start_H: 2;
	unsigned int cyl_start_L: 8;
	unsigned int type	: 8;
	unsigned int head_end	: 8;
	unsigned int sec_end	: 6;
	unsigned int cyl_end_H	: 2;
	unsigned int cyl_end_L	: 8;
	unsigned int offset;
	unsigned int sectors;
};

bool leggi_partizioni(short ind_ata, short drv)
{
	char errore;
	des_part* p;
	partizione *estesa, **ptail;
	des_ata* p_des = &hd[ind_ata];
	static char buf[512];
	unsigned int settore;
	partizione* pp;

	// lettura del Master Boot Record (LBA = 0)
	settore = 0;
	readhd_n(ind_ata, drv, buf, settore, 1, errore);
	if (errore != 0)
		goto errore;
		
	p = reinterpret_cast<des_part*>(buf + 446);
	// interpretiamo i descrittori delle partizioni primarie
	estesa = 0;
	pp = new partizione;
	// la partizione 0 corrisponde all'intero hard disk
	pp->first = 0;
	pp->dim = p_des->disco[drv].tot_sett;
	p_des->disco[drv].part = pp;
	ptail = &pp->next;
	for (int i = 0; i < 4; i++) {
		pp = *ptail = new partizione;

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
		unsigned int offset_estesa = estesa->first;
		unsigned int offset_logica = offset_estesa;
		while (1) {
			settore = offset_logica;
			readhd_n(ind_ata, drv, buf, settore, 1, errore);
			if (errore != 0)
				goto errore;
			p = reinterpret_cast<des_part*>(buf + 446);
			
			*ptail = new partizione;
			
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
	hd_print_error(ind_ata, drv, settore, errore);
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
	hd_select_device(0, p_des->indreg.iDRV_HD);
	// invio del comando di reset
	hd_software_reset(p_des->indreg.iDEV_CTRL);
	// se il drive 0 (master) era stato trovato, aspettiamo che porti i bit 
	// BSY e DRQ a 0
	if (p_des->disco[0].presente) {
		char stato;
		do {
			inputb(p_des->indreg.iSTS, stato);
		} while ( (stato & HD_STS_BSY) || (stato & HD_STS_DRQ) );
	}

	// se il drive 1 (slave) era stato trovato, aspettiamo che permetta 
	// l'accesso ai registri
	if (p_des->disco[1].presente) {
		hd_select_device(1, p_des->indreg.iDRV_HD);
		do {
			char b1, b2;

			inputb(p_des->indreg.iSTCOUNT, b1);
			inputb(p_des->indreg.iSECT_N, b2);
			if (b1 == 0x01 && b2 == 0x01) break;
			// ... timeout usando i ticks
		} while (p_des->disco[1].presente);
		// ora dobbiamo controllare che lo slave abbia messo 
		// busy a 0
		if (p_des->disco[1].presente) {
			char stato;

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

		halt_inouthd(p_des->indreg.iDEV_CTRL);	
		// cerchiamo di capire quali (tra master e slave) sono presenti
		// (inizializziamo i campi "presente" della struttura des_ata)
		for (int d = 0; d < 2; d++) {
			hd_select_device(d, p_des->indreg.iDRV_HD);	//LBA e drive drv
			p_des->disco[d].presente =
				test_canale(p_des->indreg.iSTS,
					    p_des->indreg.iSTCOUNT,
					    p_des->indreg.iSECT_N);
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

			hd_select_device(d, p_des->indreg.iDRV_HD);	//LBA e drive drv
			p_des->disco[d].presente = 
				(leggi_signature(p_des->indreg.iSTCOUNT,
						p_des->indreg.iSECT_N,
						p_des->indreg.iCYL_L_N,
						p_des->indreg.iCYL_H_N) == 0);
		}
		// lo standard dice che, se lo slave e' assente, il master puo' 
		// rispondere alle letture/scritture nei registri al posto 
		// dello slave. Cio' vuol dire che non possiamo essere sicuri 
		// che lo slave ci sia davvero fino a quando non inviamo un 
		// comando. Inviamo quindi il comando IDENTIFY DEVICE
		for (int d = 0; d < 2; d++) {
			unsigned short st_sett[256];
			char serial[41], *ptr = serial;
			char stato;

			if (!p_des->disco[d].presente) 
				continue;
			halt_inouthd(p_des->indreg.iDEV_CTRL);	
			if (!hd_sel_drv(p_des, d))
				goto error;
			hd_write_command(IDENTIFY, p_des->indreg.iCMD);
			if (!hd_wait_data(p_des))
				goto error;
			inputb(p_des->indreg.iSTS, stato); // ack
			inputbw(p_des->indreg.iDATA, st_sett, DIM_BLOCK / 2);
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
extern "C" void attiva_timer(unsigned int count);
extern "C" void disattiva_timer();
extern "C" unsigned long  calibra_tsc();

// inizializzazione del modulo di I/O
typedef int (*entry_t)(int);
entry_t io_entry = 0;
	
// controllore interruzioni
extern "C" void init_8259();

// gestione processi
extern "C" void salta_a_main();
extern "C" void salta_a_utente(entry_t user_entry, addr stack);
void main_proc(int n);

bool crea_spazio_condiviso(addr& last_address)
{
	descrittore_tabella *pdes_tab1, *pdes_tab2, *pdes_tab3;
	tabella* ptab;
	direttorio *tmp, *main_dir;
	const int nspazi = 2;
	addr inizio[2] = { inizio_io_condiviso, inizio_utente_condiviso },
	     fine[2]   = { fine_io_condiviso,   fine_utente_condiviso   };

	main_dir = des_p(esecuzione->identifier)->cr3;

	
	// lettura del direttorio principale dallo swap
	flog(LOG_INFO, "lettura del direttorio principale...");
	tmp = new direttorio;
	if (tmp == 0) {
		flog(LOG_ERR, "memoria insufficiente");
		return false;
	}
	if (!leggi_swap(tmp, swap.sb.directory * 8, sizeof(direttorio), "il direttorio principale"))
		return false;
	

	for (int sp = 0; sp < nspazi; sp++) {
		for (addr ind = inizio[sp]; ind < fine[sp]; ind = ind + SIZE_SUPERPAGINA)
		{
			pdes_tab1 = &tmp->entrate[indice_direttorio(ind)];

			if (pdes_tab1->P == 1) {	  
				pdes_tab2 = &direttorio_principale->entrate[indice_direttorio(ind)];
				pdes_tab3 = &main_dir->entrate[indice_direttorio(ind)];
				*pdes_tab2 = *pdes_tab1;

				last_address = ind + SIZE_SUPERPAGINA;

				ptab = alloca_tabella(true);
				if (ptab == 0) {
					flog(LOG_ERR, "Impossibile allocare tabella condivisa");
					return false;
				}
				if (! carica_tabella(pdes_tab2, ptab) ) {
					flog(LOG_ERR, "Impossibile caricare tabella condivisa");
					return false;
				}
				pdes_tab2->address	= toaddr(ptab) >> 12;
				for (int i = 0; i < 1024; i++) {
					descrittore_pagina* pdes_pag = &ptab->entrate[i];
					if (pdes_pag->P == 1) {
						pagina* pag = alloca_pagina_virtuale(true);
						if (pag == 0) {
							flog(LOG_ERR, "Impossibile allocare pagina residente");
							return false;
						}
						if (! carica_pagina_virtuale(pdes_pag, pag) ) {
							flog(LOG_ERR, "Impossibile caricare pagina residente");
							return false;
						}
						collega_pagina_virtuale(ptab, pag, ind + i * SIZE_PAGINA);
					}
				}
				*pdes_tab3 = *pdes_tab2;
			}
		}
	}
	delete tmp;
	carica_cr3(leggi_cr3());
	return true;
}

short swap_ch = -1, swap_drv = -1, swap_part = -1;
int heap_mem = DEFAULT_HEAP_SIZE;

// routine di inizializzazione. Viene invocata dal bootstrap loader dopo aver 
// caricato in memoria l'immagine del modulo sistema
extern "C" void cmain (unsigned long magic, multiboot_info_t* mbi,int retebp)
{
	des_proc* pdes_proc;
	char *arg, *cont;
	
	//Inizializzo la variabile globale return_ebp con il valore del registro %ebp 
	// passatomi da sistem.s al momento della chiamata a cmain
    return_ebp=retebp;

	// anche se il primo processo non e' completamente inizializzato,
	// gli diamo un identificatore, in modo che compaia nei log
	init.identifier = 0;
	init.priority   = MAX_PRIORITY;
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
	flog(LOG_INFO, "Memoria fisica: %d byte (%d MiB)", max_mem_upper, max_mem_upper >> 20 );

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
	if (max_mem_upper > fine_sistema_privato) {
		max_mem_upper = fine_sistema_privato;
		flog(LOG_WARN, "verranno gestiti solo %d byte di memoria fisica", max_mem_upper);
	}
	
	//Assegna allo heap heap_mem byte a partire dalla fine del modulo sistema
	salta_a(mem_upper + heap_mem);
	flog(LOG_INFO, "Heap di sistema: %d B", heap_mem);


	// il resto della memoria e' per le pagine fisiche
	if (!init_pagine_fisiche())
		goto error;
	flog(LOG_INFO, "Pagine fisiche: %d", num_pagine_fisiche);


	// il direttorio principale viene utilizzato fino a quando non creiamo
	// il primo processo.  Poi, servira' come "modello" da cui creare i
	// direttori dei nuovi processi.
	direttorio_principale = alloca_direttorio();
	if (direttorio_principale == 0) {
		flog(LOG_ERR, "Impossibile allocare il direttorio principale");
		goto error;
	}
		
	memset(direttorio_principale, 0, SIZE_PAGINA);

	// memoria fisica in memoria virtuale
	if (!mappa_mem_fisica(direttorio_principale, max_mem_upper))
		goto error;
	flog(LOG_INFO, "Mappata memoria fisica in memoria virtuale");

	carica_cr3(direttorio_principale);
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
	if (!crea_dummy())
		goto error;
	flog(LOG_INFO, "Creato il processo dummy");

	// processi esterni generici
	if (!init_pe())
		goto error;
	flog(LOG_INFO, "Creati i processi esterni generici");

	// il primo processo utilizza il direttorio principale e,
	// inizialmente, si trova a livello sistema (deve eseguire la parte 
	// rimanente di questa routine, che si trova a livello sistema)
	if (!crea_main())
		goto error;
	flog(LOG_INFO, "Creato il processo main");

	// da qui in poi, e' il processo main che esegue
	schedulatore();
	salta_a_main();

error:
	c_panic("Errore di inizializzazione", 0, 0, 0, 0);
}

void main_proc(int n)
{
	unsigned long clocks_per_sec;
	int errore;
	pagina* pila_utente;
	des_proc *my_des = des_p(esecuzione->identifier);

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
	flog(LOG_INFO, "partizione di swap: %d+%d", swap.part->first, swap.part->dim);
	flog(LOG_INFO, "sb: blocks=%d user=%x/%x io=%x/%x", 
			swap.sb.blocks,
			swap.sb.user_entry,
			swap.sb.user_end,
			swap.sb.io_entry,
			swap.sb.io_end);

	
	// tabelle condivise per lo spazio utente condiviso
	flog(LOG_INFO, "creazione o lettura delle tabelle condivise...");
	addr last_address;
	if (!crea_spazio_condiviso(last_address))
		goto error;

	// inizializzazione dello heap utente
	heap.start = allinea(swap.sb.user_end, sizeof(int));
	heap.dimensione = last_address - heap.start;
	flog(LOG_INFO, "heap utente a %x, dimensione: %d B (%d MiB)",
			heap.start, heap.dimensione, heap.dimensione / (1024 * 1024));

	// semaforo per la mutua esclusione nella gestione dei page fault
	pf_mutex = c_sem_ini(1);
	if (pf_mutex == 0) {
		flog(LOG_ERR, "Impossibile allocare il semaforo per i page fault");
		goto error;
	}

	// inizializzazione del modulo di io
	flog(LOG_INFO, "inizializzazione del modulo di I/O...");
	errore = swap.sb.io_entry(0);
	if (errore < 0) {
		flog(LOG_ERR, "ERRORE dal modulo I/O: %d", -errore);
		goto error;
	}

	// inizializzazione del meccanismo di lettura del log
	if (!log_init_usr())
		goto error;
		
	//Stampo l'indirizzo di ritorno a bldr32 Questa riga pu˜ essere cancellata!
		      flog(LOG_INFO, "Il valore di return_ebp e' %x", return_ebp); /////////////////
		      
	// ora trasformiamo il processo corrente in un processo utente (in modo 
	// che possa passare ad eseguire la routine main)
	flog(LOG_INFO, "salto a livello utente...");
	pila_utente = crea_pila_utente(my_des->cr3);
	if (pila_utente == 0) {
		flog(LOG_ERR, "Impossibile allocare la pila utente per main");
		goto error;
	}
	my_des->esp0 = fine_sistema_privato;
	my_des->ss0 = SEL_DATI_SISTEMA;
	my_des->ds  = SEL_DATI_UTENTE;
	my_des->es  = SEL_DATI_UTENTE;
	my_des->fpu.cr = 0x037f;
	my_des->fpu.tr = 0xffff;
	my_des->liv = LIV_UTENTE;

	attiva_timer(DELAY);
	salta_a_utente(swap.sb.user_entry, fine_utente_privato);
error:
	panic("Errore di inizializzazione");
}
