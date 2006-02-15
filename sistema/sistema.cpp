// sistema.cpp
//
#include "mboot.h"
#include "costanti.h"
#include "elf.h"


///////////////////////////////////////////////////
// FUNZIONI PER LA MANIPOLAZIONE DEGLI INDIRIZZI //
///////////////////////////////////////////////////

// purtroppo, lo standard C++ non prevede tipi predefiniti che possano essere 
// usati come "indirizzi di memoria". I candidati sono "void*", "char*" e 
// "unsigned int", ma "char*" e "unsigned int" costringono a continui 
// reinterpet_cast, mentre "void*" non puo' essere usato in espressioni 
// aritmetiche. Come compromesso, si e' deciso di usare "void*" come
// tipo per gli indirizzi (per ridurre al minimo la necessita' dei cast) e di 
// definire le seguenti funzioni per svolgere le elementari funzioni arimetiche


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

// funzioni di utilita'
// allocazione a mappa di bit
struct bm_t {
	unsigned int *vect;
	unsigned int size;
	unsigned int vecsize;
};
void bm_create(bm_t *bm, unsigned int *buffer, unsigned int size);
bool bm_alloc(bm_t *bm, unsigned int& pos);
void bm_free(bm_t *bm, unsigned int pos);
/////


// funzioni di utilita'

// restituisce il minimo naturale maggiore o uguale a v/q
unsigned int ceild(unsigned int v, unsigned int q) {
	return v / q + (v % q != 0 ? 1 : 0);
}

// se T e' int dobbiamo specializzare il template, perche'
// reinterpret_cast<unsigned int> non e' valido su un int
inline
unsigned int allinea(unsigned int v, unsigned int a) {
	return (v % a == 0 ? v : ((v + a - 1) / a) * a);
}
inline
void* allineav(void* v, unsigned int a) {
	return reinterpret_cast<void*>(allinea(reinterpret_cast<unsigned int>(v), a));
}

// copia n byte da src a dst [vedi avanti]
void *memcpy(void *dest, const void *src, unsigned int n);
// copia c nei primi n byte della zona di memoria puntata da dest
void *memset(void *dest, int c, unsigned int n);
// restituisce true se le due stringe first e second sono uguali
bool str_equal(const char* first, const char* second);
// stampa formattata sulla console (simile a printf della lib. del C)
extern "C" int printk(const char *fmt, ...);
// stampa mgs su console, quindi blocca il sistema
extern "C" void panic(const char *msg);

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
// 2) di seguito al sistema, il boostrap loader ha caricato i moduli, e ha
// passato la lista dei descrittori di moduli tramite il registro %ebx.
// Scorrendo tale lista, facciamo avanzare il puntatore mem_upper oltre lo
// spazio ooccupato dai moduli
// 3) il modulo di io deve essere ricopiato all'indirizzo a cui e' stato
// collegato (molto probabilmente diverso dall'indirizzo a cui il bootloader lo
// ha caricato, in quanto il bootloader non interpreta il contenuto dei
// moduli). Nel ricopiarlo, viene occupata ulteriore memoria fisica.  4) di 
// seguito al modulo io (ricopiato), viene allocato l'array di descrittori di 
// pagine fisiche [vedi avanti], la cui dimensione e' calcolata dinamicamente,
// in base al numero di pagine fisiche rimanenti dopo le operazioni precedenti.
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

// forward declaration
void free_interna(void* indirizzo, unsigned int quanti);

// allocazione linerare, da usare durante la fase di inizializzazione.  Si
// mantiene un puntatore (mem_upper) all'ultimo byte non occupato.  Tale
// puntatore puo' solo avanzare, tramite le funzioni 'occupa' e 'salta_a', e
// non puo' superare la massima memoria fisica contenuta nel sistema
// (max_mem_upper) Se il puntatore viene fatto avanzare tramite la funzione
// 'salta_a', i byte "saltati" vengono dati all'allocatore a lista
// tramite la funzione "free_interna"
void* occupa(unsigned int quanti) {
	void* p = 0;
	void* appoggio = add(mem_upper, quanti);
	if (appoggio <= max_mem_upper) {
		p = mem_upper;
		mem_upper = appoggio;
	}
	return p; // se e' zero, non e' stato possibile spostare mem_upper di
		  // "quanti" byte in avanti
}

int salta_a(void* indirizzo) {
	int saltati = -1;
	if (indirizzo >= mem_upper && indirizzo < max_mem_upper) {
		saltati = distance(indirizzo, mem_upper);
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
void* malloc(unsigned int quanti) {

	// per motivi di efficienza, conviene allocare sempre multipli di 4 byte
	// (in modo che le strutture dati restino allineate alla linea)
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
			des_mem* nuovo = static_cast<des_mem*>(add(p, dim));

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
void free(void* p) {

	// e' normalmente ammesso invocare "free" su un puntatore nullo.
	// In questo caso, la funzione non deve fare niente.
	if (p == 0) return;
	
	// recuperiamo l'indirizzo del descrittore della zona
	des_mem* des = static_cast<des_mem*>(p) - 1;

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
// riunificazione e' importante, per evitare il problema della "falsa
// frammentazione", in cui si vengono a creare tante zone libere, contigue, ma
// non utilizzabili per allocazioni piu' grandi della dimensione di ciascuna di
// esse.
// free_interna puo' essere usata anche in fase di inizializzazione, per
// definire le zone di memoria fisica che verranno utilizzate dall'allocatore
void free_interna(void* indirizzo, unsigned int quanti) {

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
		printk("indirizzo = 0x%x\n", (void*)indirizzo);
		panic("double free\n");
	}
	// assert(scorri == 0 || scorri > indirizzo)

	// verifichiamo che la zona possa essere unificata alla zona che la
	// precede.  Cio' e' possibile se tale zona esiste e il suo ultimo byte
	// e' contiguo al primo byte della nuova zona
	if (prec != 0 && add(prec + 1, prec->dimensione) == indirizzo) {

		// controlliamo se la zona e' unificabile anche alla eventuale
		// zona che la segue
		if (scorri != 0 && add(indirizzo, quanti) == scorri) {
			
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

	} else if (scorri != 0 && add(indirizzo, quanti) == scorri) {

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
		des_mem* nuovo = static_cast<des_mem*>(indirizzo);

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

		des_mem* nuovo = static_cast<des_mem*>(indirizzo);
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

////////////////////////////////////////////////////////////////////////////
// INTRODUZIONE GENERALE ALLA MEMORIA VIRTUALE                            //
////////////////////////////////////////////////////////////////////////////
//
// SPAZIO VIRTUALE DI UN PROCESSO:
// Ogni processo possiede un proprio spazio di indirizzamento, suddiviso in 
// quattro parti (di 1 Giga Byte ciascuna):
// - sistema/condivisa: contiene l'immagine di tutta la memoria fisica
// installata nel sistema
// - sistema/privata: contiene la pila sistema del processo
// - utente/condivisa: contiene l'immagine dei corpi di tutti i processi, dei 
// dati globali e dello heap
// - utente/privata: contiene la pila utente del processo
// Le parti sistema non sono gestite tramite memoria virtuale (i bit P sono 
// permanentemente a 1 o a 0) e sono accessibili solo da livello di privilegio 
// sistema. Le parti utete sono gestite tramite memoria virtuale e sono 
// accessibili da livello utente.
// Le parti condivise sono comuni a tutti i processi. La condivisione viene 
// realizzata esclusivamente condividendo le tabelle delle pagine: i direttori 
// di tutti i processi puntano alle stesse tabelle nelle entrate numero 0-255 
// (relative alla parte sistema/condivisa) e nelle entrate numero 512-767 
// (relative alla parte utente/condivisa). In questo modo, ogni pagina (sia 
// essa appartenende ad uno spazio privato che ad uno spazio condiviso) e' 
// puntata sempre da una sola tabella. Cio' semplifica la gestione della 
// memoria virtuale: per rimuovere o aggiungere una pagina e' necessario 
// modificare una sola entrata (altrimenti, per rimuovere o aggiungere una 
// pagina di uno spazio condiviso, sarebbe necessario modificare le entrate 
// relative alla pagina in tutte le tabelle, di tutti i processi, che la 
// puntano). Inoltre, le tabelle condivise sono sempre preallocate e non 
// rimpiazzabili (altrimenti, per aggiungere o rimuovere una tabella condivisa, 
// sarebbe necessario modificare l'entrata relativa nel direttorio di ogni 
// processo).
//
// MEMORIA FISICA:
// La memoria fisica e' suddivisa in due parti: la prima parte contiene il 
// codice e le strutture dati, statiche e dinamiche, del nucleo e del modulo di 
// io, mentre la seconda parte e' suddivisa in pagine fisiche, destinate a 
// contenere i direttori, le tabelle delle pagine e le pagine virtuali dei 
// processi. Poiche' l'intera memoria fisica (sia la prima che la seconda 
// parte) e' mappata nello spazio sistema/condiviso di ogni processo, il nucleo 
// e il modulo di io (sia il codice che le strutture dati) sono mappati, agli 
// stessi indirizzi, nello spazio virtuale di ogni processo. In questo modo, 
// ogni volta che un processo passa ad eseguire una routine di nucleo (o di io) 
// (per effetto di una interruzione software, esterna o di una eccezione), la
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
// caricata in memoria fisica (in particolare, i valori da assegnare ai bit RW, 
// PCD e PWT). Quando una pagina V, non presente, viene resa presente, 
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
// utente/condivisa, uguale per tutti i processi. Lo swap contiene, infatti, un 
// solo direttorio, di cui sono significative solo le entrate numero 512-767, 
// le relative tabelle e le pagine puntate da tali tabelle.
//
// All'avvio del sistema, viene creato un direttorio principale, nel seguente 
// modo:
// - le entrate da 0 a 255 (massimo), corrispondenti allo spazio 
// sistema/condiviso, veongono fatte puntare a tabelle, opportunamente create, 
// che mappano tutta la memoria fisica in memoria virtuale
// - le entrate da 512 a 768 (massimo), corrispondenti allo spazio 
// utente/condiviso, vengono copiate dalle corrispondenti entrate che si 
// trovano nel direttorio nello swap. Le tabelle puntate da tali entrate 
// vengono tutte caricate in memoria fisica
// - le rimanenti entrate sono non significative
//
// Ogni volta che viene creato un nuovo processo, il suo direttorio viene prima 
// copiato dal direttorio principale (in questo modo, il nuovo processo 
// condividera' con tutti gli altri processi le tabelle, e quindi le pagine, 
// degli spazio sistema e utente condivisi). Gli spazi privati (sistema e 
// utente), che sono inizialmente vuoti (fatta eccezione per alcune parole 
// lunghe) vengono creati dinamicamente, allocando nuove tabelle e lo spazio 
// nello swap per le corrispondenti pagine. Se non si facesse cosi', lo swap 
// dovrebbe essere preparato conoscendo a priori il numero di processi che 
// verranno creati al momento dell'esecuzione di main.
//


/////////////////////////////////////////////////////////////////////////
// PAGINAZIONE                                                         //
/////////////////////////////////////////////////////////////////////////
// Il descrittore di pagina (o di tabella) prevede 3 bit che sono lasciati a 
// disposizione del programmatore di sistema. In questo sistema, ne e' stato 
// usato uno, denominato "preload" (precarica). Il suo significato e' il 
// seguente: una pagina virtuale (o le tabella) il cui descrittore contiene il 
// bit "preload" pari a 1 e' esclusa dal meccanismo del rimpiazzamento: la 
// pagina viene precaricata dallo swap al momento della creazione del processo, 
// e poi non puo' essere rimpiazzata.  Tale meccanismo serve a realizzare le 
// pagine residenti, che si rendono necessarie soprattutto per il processo che 
// esegue la funzione "main" (tale processo puo' invocare le primitive 
// activate_p e sem_ini, che prevedono dei parametri formali di ritorno; i 
// parametri attuali sono scritti dal nucleo e devono trovarsi in pagine non 
// rimpiazzabili e precaricate, altrimenti il nucleo stesso potrebbe causare 
// dei page fault)
//
// una pagina virtuale non presente, il cui descrittore contiene il campo 
// address pari a 0, non possiede inizialmente un blocco in memoria di massa.  
// Se, e quando, tale pagina verra' realmente acceduta, un nuovo blocco verra' 
// allocato in quel momento, e la pagina riempita con zeri. Tale ottimizzazione 
// si rende necessaria, perche' molte pagine sono potenzialmente utilizzabili 
// dal processo, ma non sono realmente utilizzate quasi mai (ad esempio, la 
// maggior parte delle pagine che compongono lo heap e lo stack). Senza questa 
// ottimizzazione, bisognerebbe prevedere uno dispositivo di swap molto grande 
// (ad es., 1 Giga Byte di spazio privato per ogni processo, piu' 1 Giga Byte 
// di spazio utente condiviso) oppure limitare ulteriormente la dimensione 
// massima dello heap o dello stack. Tale ottimizzazione e' analoga a quella 
// che si trova nei sistemi con file system, in cui i dati globali 
// inizializzati a zero non sono realmente memorizzati nel file eseguibile, ma 
// il file eseguibile ne specifica solo la dimensione totale.
// Se il descrittore di una tabella delle pagine possiede il campo address pari 
// a 0, e' come se tutte le pagine puntate dalla tabella avessero il campo 
// address pari a 0

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
	unsigned int global:	1;	// non visto a lezione
	// fine byte di accesso

	unsigned int avail:	2;	// non usato
	unsigned int preload:	1;	// la pag. deve essere precaricata

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
struct tabella_pagine {
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
// contiene (poiche' gli indirizzi di partenza coincidono, si tratta solo di 
// eseguire una conversione di tipo)
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
// (i dieci bit piu' significativi dell'indirizzo)
short indice_direttorio(void* indirizzo) {
	return (reinterpret_cast<unsigned int>(indirizzo) & 0xffc00000) >> 22;
}

// dato un indirizzo virtuale, ne restituisce l'indice nella tabella delle
// pagine (i bit 12:21 dell'indirizzo)
short indice_tabella(void* indirizzo) {
	return (reinterpret_cast<unsigned int>(indirizzo) & 0x003ff000) >> 12;
}

// dato un puntatore ad un descrittore di tabella, restituisce
// un puntatore alla tabella puntata
// (campo address del descrittore esteso a 32 bit aggiungendo 12 bit a 0)
tabella_pagine* tabella_puntata(descrittore_tabella* pdes_tab) {
	return reinterpret_cast<tabella_pagine*>(pdes_tab->address << 12);
}

// dato un puntatore ad un descrittore di pagina, restituisce
// un puntatore alla pagina puntata
// (campo address del descrittore esteso a 32 bit aggiungendo 12 bit a 0)
pagina* pagina_puntata(descrittore_pagina* pdes_pag) {
	return reinterpret_cast<pagina*>(pdes_pag->address << 12);
}

// il direttorio principale contiene i puntatori a tutte le tabelle condivise 
// (sia dello spazio sistema che dello spazio utente). Viene usato nella fase 
// inziale, quando ancora non e' stato creato alcun processo. Inoltre, ogni 
// volta che viene creato un nuovo processo, il direttorio del processo viene 
// inzialmente copiato dal direttorio principale (in modo che il nuovo processo 
// condivida tutte le tabelle condivise)
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
	TABELLA_PRIVATA,	// tabella appartenente a un solo processo
	TABELLA_CONDIVISA,	// tabella appartenente a piu' processi
				// (non rimpiazzabile, ma che punta a pagine
				// rimpiazzabili)
	TABELLA_RESIDENTE,	// tabella non rimpiazzabile, che punta a 
				// a pagine non rimpiazzabili
	PAGINA,			// pagina rimpiazzabile
	PAGINA_RESIDENTE };	// pagina non rimpiazzabile

// descrittore di pagina fisica
struct des_pf {
	cont_pf		contenuto;	// uno dei valori precedenti
	unsigned int	contatore;	// contatore per le statistiche
	union {
		struct { // informazioni relative a una pagina
			unsigned int	blocco;		// blocco in memoria di massa
			tabella_pagine*	tabella;	// tabella che punta alla pagina
			void*		ind_virtuale;   // indirizzo virtuale della pagina
		} pag;
		struct { // informazioni relative a una tabella
			unsigned int	blocco;		// blocco in memoria di massa
			direttorio*     dir;		// direttorio che punta alla tabella
			short		indice;		// indice della tab nel direttorio
			short		quante;		// numero di pagine puntate
		} tab;
		struct  { // informazioni relative a una pagina libera
			des_pf*		prossima_libera;
		} avl;
		// non ci sono informazioni aggiuntive per una pagina 
		// contenente un direttorio
	} u;
};


// puntatore al primo descrittore di pagina fisica
des_pf* pagine_fisiche;

// numero di pagine fisiche
unsigned long num_pagine_fisiche;

// testa della lista di pagine fisiche libere
des_pf* pagine_libere = 0;

// indirizzo fisico della prima pagina fisica
// (necessario per calcolare l'indirizzo del descrittore associato ad una data
// pagina fisica e viceversa)
pagina_fisica* prima_pagina;

// restituisce l'indirizzo fisico della pagina fisica associata al descrittore
// passato come argomento
pagina_fisica* indirizzoPF(des_pf* p) {
	return static_cast<pagina_fisica*>(add(prima_pagina, (p - pagine_fisiche) * SIZE_PAGINA));
}

// dato l'indirizzo di una pagina fisica, restituisce un puntatore al
// descrittore associato
des_pf* strutturaPF(pagina_fisica* pagina) {
	return &pagine_fisiche[distance(pagina, prima_pagina) / SIZE_PAGINA];
}

// init_pagine_fisiche viene chiamata in fase di inizalizzazione.  Tutta la
// memoria non ancora occupata viene usata per le pagine fisiche.  La funzione
// si preoccupa anche di allocare lo spazio per i descrittori di pagina fisica,
// e di inizializzarli in modo che tutte le pagine fisiche risultino libere
void init_pagine_fisiche() {

	// allineamo mem_upper alla linea, per motivi di efficienza
	salta_a(allineav(mem_upper, sizeof(int)));

	// calcoliamo quanta memoria principale rimane
	int dimensione = distance(max_mem_upper, mem_upper);

	if (dimensione <= 0)
		panic("Non ci sono pagine libere");

	// calcoliamo quante pagine fisiche possiamo definire (tenendo conto
	// del fatto che ogni pagina fisica avra' bisogno di un descrittore)
	unsigned int quante = dimensione / (SIZE_PAGINA + sizeof(des_pf));

	// allochiamo i corrispondenti descrittori di pagina fisica
	pagine_fisiche = static_cast<des_pf*>(occupa(sizeof(des_pf) * quante));

	// riallineamo mem_upper a un multiplo di pagina
	salta_a(allineav(mem_upper, SIZE_PAGINA));

	// ricalcoliamo quante col nuovo mem_upper, per sicurezza
	// (sara' minore o uguale al precedente)
	quante = distance(max_mem_upper, mem_upper) / SIZE_PAGINA;

	// occupiamo il resto della memoria principale con le pagine fisiche;
	// ricordiamo l'indirizzo della prima pagina fisica e il loro numero
	prima_pagina = static_cast<pagina_fisica*>(occupa(quante * SIZE_PAGINA));
	num_pagine_fisiche = quante;

	// se resta qualcosa (improbabile), lo diamo all'allocatore a lista
	salta_a(max_mem_upper);

	// costruiamo la lista delle pagine fisiche libere
	des_pf* p = 0;
	for (int i = quante - 1; i >= 0; i--) {
		pagine_fisiche[i].contenuto = LIBERA;
		pagine_fisiche[i].u.avl.prossima_libera = p;
		p = &pagine_fisiche[i];
	}
	pagine_libere = &pagine_fisiche[0];
}

// funzione di allocazione generica di una pagina
// Nota: restituisce un puntatore al descrittore della pagina, 
// e non alla pagina
des_pf* alloca_pagina() {
	des_pf* p = pagine_libere;
	if (p != 0)
		pagine_libere = pagine_libere->u.avl.prossima_libera;
	return p;
}

// perche' la struttura dati composta dai descrittori di pagina fisica sia
// consistente, e' necessario che, ogni volta che si alloca una pagina, si
// aggiorni il campo "contenuto" del corrispondente descrittore in base all'uso
// che si deve fare della pagina.  Per semplificare questa operazione
// ripetitiva, definiamo una funzione di allocazione specifica per ogni
// possibile uso della pagina fisica da allocare (in questo modo, otteniamo
// anche il vantaggio di poter restituire un puntatore alla pagina, del tipo
// corretto)

direttorio* alloca_direttorio() {
	des_pf* p = alloca_pagina();
	if (p == 0) return 0;
	p->contenuto = DIRETTORIO;

	// tramite il descrittore puntato da "p", calcoliamo l'indirizzo della 
	// pagina descritta (funzione "indirizzo"), che puntera' ad una "union 
	// pagina_fisica". Quindi, restituiamo un puntatore al campo di tipo 
	// "direttorio" all'interno della union
	return &indirizzoPF(p)->dir;
}

// le tre funzioni per l'allocazione di una tabella differiscono solo per il 
// valore da scrivere nel campo tipo. Poiche' e' buona norma di programmazione 
// evitare il copia e incolla, ne definiamo una sola, parametrizzata sul tipo.  
// Le altre si limitano a richiamare questa con il tipo corretto
tabella_pagine* alloca_tabella(cont_pf tipo = TABELLA_PRIVATA) {
	des_pf* p = alloca_pagina();
	if (p == 0) return 0;
	p->contenuto = tipo;
	return &indirizzoPF(p)->tab;
}

tabella_pagine* alloca_tabella_condivisa() {
	return alloca_tabella(TABELLA_CONDIVISA);
}

tabella_pagine* alloca_tabella_residente() {
	return alloca_tabella(TABELLA_RESIDENTE);
}


// analogo discorso per le pagine
pagina* alloca_pagina_virtuale(cont_pf tipo = PAGINA) {
	des_pf* p = alloca_pagina();
	if (p == 0) return 0;
	p->contenuto = tipo;
	return &indirizzoPF(p)->pag;
}

pagina* alloca_pagina_residente() {
	return alloca_pagina_virtuale(PAGINA_RESIDENTE);
}

// rende libera la pagina associata al descrittore di pagina fisica puntato da
// "p"
void rilascia_pagina(des_pf* p) {
	p->contenuto = LIBERA;
	p->u.avl.prossima_libera = pagine_libere;
	pagine_libere = p;
}

// funzioni di comodo per il rilascio di una pagina fisica
// dato un puntatore a un direttorio, a una tabella o ad una pagina, ne 
// calcolano prima l'indirizzo della pagina fisica che le contiene (semplice 
// conversione di tipo) tramite la funzione "pfis", quindi l'indirizzo del 
// descrittore di pagina fisica associato (funzione "struttura") e, infine, 
// invocano la funzione "rilascia_pagina" generica.
inline
void rilascia(direttorio* d) {
	rilascia_pagina(strutturaPF(pfis(d)));
}

inline
void rilascia(tabella_pagine* d) {
	rilascia_pagina(strutturaPF(pfis(d)));
}

inline
void rilascia(pagina* d) {
	rilascia_pagina(strutturaPF(pfis(d)));
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
	des_pf* ppf  = strutturaPF(pfis(ptab));
	ppf->u.tab.dir    = pdir;
	ppf->u.tab.indice = indice;
	ppf->u.tab.quante = 0; // inizialmente, nessuna pagina e' presente
	// il contatore deve essere inizializzato come se fosse appena stato 
	// effettuato un accesso (cosa che, comunque, avverra' al termine della 
	// gestione del page fault). Diversamente, la pagina o tabella appena 
	// caricata potrebbe essere subito scelta per essere rimpiazzata, al 
	// prossimo page fault, pur essendo stata acceduta di recente (infatti, 
	// non c'e' alcuna garanzia che la routine del timer riesca ad andare 
	// in esecuzione e aggiornare il contatore prima del prossimo page 
	// fault)
	ppf->contatore       = 0x80000000;
	ppf->u.tab.blocco    = pdes_tab->address; // vedi NOTA prec.
	
	// mapping diretto
	pdes_tab->address    = uint(ptab) >> 12;
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
	des_pf* ppf = strutturaPF(pfis(tabella_puntata(pdes_tab)));
	pdes_tab->address = ppf->u.tab.blocco;

	return pdes_tab;
}

// collega la pagina puntata da "pag" alla tabella puntata da "ptab", 
// all'indirizzo virtuale "ind_virtuale". Aggiorna anche il descrittore di 
// pagina fisica corrispondente alla pagina fisica che contiene la pagina.
// NOTA: si suppone che la pagina non fosse precedentemente collegata, e quindi 
// che il corrispondente descrittore di pagina nella tabella contenga
// il numero del blocco della pagina nello swap
descrittore_pagina* collega_pagina(tabella_pagine* ptab, pagina* pag, void* ind_virtuale) {
	
	descrittore_pagina* pdes_pag = &ptab->entrate[indice_tabella(ind_virtuale)];

	// mapping inverso
	des_pf* ppf = strutturaPF(pfis(pag));
	ppf->u.pag.tabella	= ptab;
	ppf->u.pag.ind_virtuale = ind_virtuale;
	// per il campo contatore, vale lo stesso discorso fatto per le tabelle 
	// delle pagine
	ppf->contatore          = 0x80000000;
	ppf->u.pag.blocco	= pdes_pag->address; // vedi NOTA prec

	// incremento del contatore nella tabella
	ppf = strutturaPF(pfis(ptab));
	ppf->u.tab.quante++;
	ppf->contatore		|= 0x80000000;

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
	des_pf* ppf = strutturaPF(pfis(pagina_puntata(pdes_pag)));
	pdes_pag->address = ppf->u.pag.blocco;

	// infine, decrementiamo il numero di pagine puntate dalla tabella
	ppf = strutturaPF(pfis(ptab));
	ppf->u.tab.quante--;

	return pdes_pag;
}
	



/////////////////////////////////////////////////////////////////////////////
// MEMORIA VIRTUALE                                                        //
/////////////////////////////////////////////////////////////////////////////
extern "C" void readhd_n(short ind_ata, short drv, void* vetti,
		unsigned int primo, unsigned char quanti, char &errore);
extern "C" void writehd_n(short ind_ata, short drv, void* vetti,
		unsigned int primo, unsigned char quanti, char &errore);
void hd_print_error(int i, int d, int sect, char errore);

bm_t block_bm;

#define SWAP_PS  1
#define SWAP_MS  0

bool leggi_blocco(unsigned int blocco, void* dest) {
	char errore;

	readhd_n(SWAP_PS, SWAP_MS, dest, blocco * 8, 8, errore);
	if (errore != 0) { 
		printk("Impossibile leggere il blocco %d\n", blocco);
		hd_print_error(SWAP_PS, SWAP_MS, blocco * 8, errore);
		return false;
	}
	return true;
}


bool scrivi_blocco(unsigned int blocco, void* dest) {
	char errore;

	writehd_n(SWAP_PS, SWAP_MS, dest, blocco * 8, 8, errore);
	if (errore != 0) { 
		printk("Impossibile scrivere il blocco %d\n", blocco);
		hd_print_error(SWAP_PS, SWAP_MS, blocco * 8, errore);
		return false;
	}
	return true;
}

// ROUTINE DEL TIMER
// aggiorna_statistiche viene chiamata dalla routine del timer e si preccupa di 
// campionare tutti i bit A delle pagine e tabelle presenti e rimpiazzabili.  
// Per far cio', scorre tutto l'array di descrittori di pagina fisica, alla 
// ricerca di pagine che contengano o una TABELLA_PRIVATA o una 
// TABELLA_CONDIVISA o un DIRETTORIO (il caso TABELLA_RESIDENTE viene saltato, 
// in quanto una tabella residente punta a pagine non rimpiazzabili, di cui e' 
// quindi inutile aggiornare il contatore).
// Quindi, di ogni tabella o direttorio, ne scorre tutte le entrate, e, per 
// ogni pagina o tabella puntata presente, ne aggiorna il campo "contatore" nel 
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
// bit P uguale a 1 nella tabella, e viene aggiornato ogni volta che si 
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
// piu' di 32 intervalli di timer (1.6 sec in questo sistema) avranno il 
// contatore a 0, e saranno quindi indifferenti dal punto di vista del 
// rimpiazzamento.
void aggiorna_statistiche()
{
	des_pf *ppf1, *ppf2, *ppf3, *save = 0;
	tabella_pagine* ptab;
	descrittore_pagina* pp, *save_pp;

	for (int i = 0; i < num_pagine_fisiche; i++) {
		ppf1 = &pagine_fisiche[i];

		// la variabile check ci dice se dobbiamo o non dobbiamo 
		// scorrere le entrate della pagina fisica i-esima
		bool check = false;
		switch (ppf1->contenuto) {
		case PAGINA:
		case LIBERA:
			// queste non vanno controllate perche' non ha senso
		case TABELLA_RESIDENTE:
			// queste non vanno controllate perche' puntano a 
			// pagine non rimpiazzabili
			break;
		case TABELLA_PRIVATA:
		case TABELLA_CONDIVISA:
			// vedi NOTA 3
			check = ppf1->u.tab.quante > 0;
			break;
		case DIRETTORIO:
			check = true;
			break;
		}
		if (check) {
			// trattiamo tutto come se fosse una tabella, anche se 
			// si tratta di un direttorio (vedi NOTA 2)
			ptab = &indirizzoPF(ppf1)->tab;
			for (int j = 0; j < 1024; j++) {
				pp = &ptab->entrate[j];
				if (pp->P == 1) {
					// ricaviamo un puntatore al 
					// descrittore di pagina fisica 
					// associato alla pagina fisica puntata 
					// da "pp"
					ppf2 = strutturaPF(pfis(pagina_puntata(pp)));

					
					// aggiornamento di tipo LRU
					ppf2->contatore >>= 1;	// shift in ogni caso
					ppf2->contatore |= (unsigned long)pp->A << 31;

					// aggiornamento di tipo LFU
					// ppf2->contatore++

					pp->A = 0;
				}
			}
		}
	}
	// svuotiamo il TLB, eseguendo un caricamento fittizio di cr3
	carica_cr3(leggi_cr3());
}

// ROUTINE DI TRASFERIMENTO
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
// punto e ne venga schedulato un altro, P2.  Il processo P1 verra' risvegliato 
// alla fine del trasferimento, per proseguire con la gestione del page fault 
// dal punto successivo all trasferimento appena conclusosi. Poiche' il 
// processo P2 potrebbe a sua volta generare un page fault, e quindi tentare di 
// eseguire la stessa routine di trasferimento, questa deve essere rientrante.  
// Purtroppo, alcune struttre dati utilizzate dalla routine sono per forza di 
// cose condivise: le pagine fisiche (con i loro descrittori) e le tabelle 
// delle pagine condivise (per lo spazio utente condiviso). Per proteggere 
// l'accesso a tali strutture, e' necessario che la routine di trasferimento 
// venga eseguita in mutua esclusione (semaforo pf_mutex). Inoltre, le tabelle 
// e le pagine condivise vengono accedute anche al di fuori della mutua 
// esclusione (ad esempio, P2 potrebbe accedere ad una pagina condivisa, 
// presente): per evitare problemi anche in questi casi, bisogna fare 
// attenzione all'ordine con cui si manipolano i descrittori di pagina in tali 
// tabelle (vedi commenti nella routine). Infine, tali strutture dati vengono 
// accedute (lette e scritte) anche dalla routine del timer. La mutua 
// esclusione, in questo caso, e' garantita dal fatto che, mentre la routine di 
// trasferimento accede a tali strutture, le interruzioni sono ancora 
// disabilitate.

// semaforo di mutua esclusione per la gestione dei page fault
int pf_mutex;
extern "C" void sem_wait(int);
extern "C" void sem_signal(int);



// funzioni usate dalla routine di trasferimento
tabella_pagine* rimpiazzamento_tabella();
pagina* 	rimpiazzamento_pagina(tabella_pagine* escludi);
bool carica_pagina(descrittore_pagina* pdes_pag, pagina* pag);
bool carica_tabella(descrittore_tabella* pdes_tab, tabella_pagine* ptab);
extern "C" void abort_p();

void trasferimento(void* indirizzo_virtuale, bool scrittura)
{
	descrittore_pagina* pdes_pag;
	pagina* pag;
	
	// mutua esclusione
	sem_wait(pf_mutex);

	// in questa realizzazione, si accede all direttorio e alle tabelle 
	// tramite un indirizzo virtuale uguale al loro indirizzo fisico (in 
	// virtu' del fatto che la memoria fisica e' stata mappata in memoria 
	// virtuale)
	direttorio* pdir = leggi_cr3(); // direttorio del processo

	// ricaviamo per prima cosa il descrittore di pagina interessato dal 
	// fault
	descrittore_tabella* pdes_tab = &pdir->entrate[indice_direttorio(indirizzo_virtuale)];

	// se l'accesso era in scrittura e (tutte le pagine puntate da) la 
	// tabella e' di sola lettura, il processo ha commesso un errore e va 
	// interrotto
	if (scrittura && pdes_tab->RW == 0) {
		printk("Errore di accesso in scrittura\n");
		goto error; // Dijkstra se ne faccia una ragione
	}
	
	tabella_pagine* ptab;
	if (pdes_tab->P == 1) {
		// la tabella e' presente
		ptab = tabella_puntata(pdes_tab);
	} else {
		// la tabella e' assente
		
		// proviamo ad allocare una pagina fisica per la tabella. Se 
		// non ve ne sono, dobbiamo invocare la routine di 
		// rimpiazzamento per creare lo spazio
		ptab = alloca_tabella();
		if (ptab == 0) 
			ptab = rimpiazzamento_tabella();

		// ora possiamo caricare la tabella (operazione bloccante: 
		// verra' schedulato un altro processo e, quindi, gli interrupt 
		// verrano riabilitati)
		if (! carica_tabella(pdes_tab, ptab) )
			goto error;
		
		// e collegarla al direttorio
		collega_tabella(pdir, ptab, indice_direttorio(indirizzo_virtuale));
	} 
	// ora ptab punta alla tabella delle pagine (sia che fosse gia' 
	// presente, sia che sia stata appena caricata)
	

	// ricaviamo il descrittore di pagina interessato dal fault
	pdes_pag = &ptab->entrate[indice_tabella(indirizzo_virtuale)];
	

	// se l'accesso era in scrittura e la pagina e' di sola lettura, il 
	// processo utente ha commesso un errore e va interrotto
	if (scrittura && pdes_pag->RW == 0) {
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
			// passiamo l'indirizzo di ptab alla routine di 
			// rimpiazzamento. In nessun caso la routine deve 
			// scegliere ptab come pagina da rimpiazzare.
			pag = rimpiazzamento_pagina(ptab);
		
		
		// proviamo a caricare la pagina (operazione bloccante: verra' 
		// schedulato un altro processo e, quindi, gli interrupt 
		// verrano riabilitati)
		if (! carica_pagina(pdes_pag, pag) ) 
			goto error;

		// infine colleghiamo la pagina
		collega_pagina(ptab, pag, indirizzo_virtuale);
		pdes_pag->D = 0; // appena caricata


	}
	sem_signal(pf_mutex);
	return;

error:
	printk("page fault non risolubile\n");
	// anche in caso di errore dobbiamo rilasciare il semaforo di mutua 
	// esclusione, pena il blocco di tutta la memoria virtuale
	sem_signal(pf_mutex);
	abort_p();
}

// carica la pagina descritta da pdes_pag in memoria fisica,
// all'indirizzo pag
bool carica_pagina(descrittore_pagina* pdes_pag, pagina* pag)
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
		if (! bm_alloc(&block_bm, blocco) ) {
			printk("spazio nello swap insufficiente\n");
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
bool carica_tabella(descrittore_tabella* pdes_tab, tabella_pagine* ptab)
{
	// anche per le tabelle vale il caso speciale in cui address == 0: la 
	// tabella non si trova nello swap, ma va creata. Per crearla, facciamo 
	// in modo che ogni entrata della nuova tabella erediti le proprieta' 
	// della tabella stessa (in particolare, i bit RW e US) e punti a 
	// pagine da creare (ovvero, con address = 0).
	if (pdes_tab->address == 0) {
		unsigned int blocco;
		if (! bm_alloc(&block_bm, blocco)) {
			printk("spazio nello swap insufficiente\n");
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



// forward
des_pf* rimpiazzamento();

// funzioni di comodo per il rimpiazzamento, specializzate per le
// tabelle e per le pagine
tabella_pagine* rimpiazzamento_tabella()
{
	des_pf *ppf = rimpiazzamento();

	if (ppf == 0) return 0;

	ppf->contenuto = TABELLA_PRIVATA;
	return &indirizzoPF(ppf)->tab;
}

pagina* rimpiazzamento_pagina(tabella_pagine* escludi) 
{
	// Per impedire che venga scelta la tabella "escludi", la rendiamo 
	// temporaneamente residente
	des_pf *p = strutturaPF(pfis(escludi));
	cont_pf save = p->contenuto;
	p->contenuto = TABELLA_RESIDENTE;

	des_pf *ppf = rimpiazzamento();
	
	// ripristiniamo il vecchio contenuto del decrittore di "escludi" 
	// (potra' essere TABELLA_PRIVATA o TABELLA_CONDIVISA)
	p->contenuto = save;

	if (ppf == 0) return 0;

	ppf->contenuto = PAGINA;

	return &indirizzoPF(ppf)->pag;
}

// funzioni usate da rimpiazzamento
extern "C" void invalida_entrata_TLB(void* ind_virtuale);

// routine di rimpiazzamento vera e propria.
// libera una pagina fisica rimuovendo e (eventualmente) copiando nello swap 
// una tabella privata o una pagina virtuale
des_pf* rimpiazzamento() {
	
	des_pf *ppf, *vittima = 0;
	void* ind_virtuale;
	unsigned int blocco;
	
	// scorriamo tutti i descrittori di pagina fisica alla ricerca della 
	// prima pagina che contiene una pagina virtuale o una tabella privata
	int i = 0;
	while (i < num_pagine_fisiche &&
	       pagine_fisiche[i].contenuto != PAGINA &&
	       pagine_fisiche[i].contenuto != TABELLA_PRIVATA)
		i++;

	// se non ce ne sono, la situazione e' critica
	if (i >= num_pagine_fisiche)
		goto error;

	// partendo da quella appena trovata, cerchiamo la pagina con il valore 
	// minimo per il contatore delle statistiche (gli interrupt sono 
	// disabilitati, garantendo la mutua esclusione con la routine del 
	// timer)
	vittima = &pagine_fisiche[i];
	for (i++; i < num_pagine_fisiche; i++) {
		ppf = &pagine_fisiche[i];
		switch (ppf->contenuto) {
		case PAGINA:
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

	// assert (vittima != 0);

	if (vittima->contenuto == TABELLA_PRIVATA) {
		// usiamo le informazioni nel mapping inverso per ricavare 
		// subito l'entrata nel direttorio da modificare
		descrittore_tabella *pdes_tab =
			scollega_tabella(vittima->u.tab.dir, vittima->u.tab.indice);

		// "dovremmo" cancellarla e basta, ma contiene i blocchi delle
		// pagine allocate dinamicamente
		if (!scrivi_blocco(pdes_tab->address, indirizzoPF(vittima)))
			goto error;
	} else {
		// usiamo le informazioni nel mapping inverso per ricavare 
		// subito l'entrata nel direttorio da modificare
		tabella_pagine* ptab = vittima->u.pag.tabella;

		// dobbiamo rendere la pagina non presente prima di cominciare 
		// a salvarla nello swap (altrimenti, qualche altro processo 
		// potrebbe cercare di scrivervi mentre e' ancora in corso 
		// l'operazione di salvataggio)
		descrittore_pagina* pdes_pag = scollega_pagina(ptab, vittima->u.pag.ind_virtuale);
		invalida_entrata_TLB(vittima->u.pag.ind_virtuale);

		if (pdes_pag->D == 1) {
			// pagina modificata, va salvata nello swap (operazione 
			// bloccante)
			if (!scrivi_blocco(pdes_pag->address, indirizzoPF(vittima)))
				goto error;
		} 
	}
	return vittima;

error:
	panic("Impossibile rimpiazzare pagine");
}

//////////////////////////////////////////////////////////////////////////////////
// CREAZIONE PROCESSI                                                           //
//////////////////////////////////////////////////////////////////////////////////

// descrittore di processo
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
pagina* crea_pila_utente(direttorio* pdir, void* ind_virtuale, int num_pagine);
pagina* crea_pila_sistema(direttorio* pdir, void* ind_virtuale);
void rilascia_tutto(direttorio* pdir, void* start, void* end);
extern "C" int alloca_tss(des_proc* p);
extern "C" void rilascia_tss(int indice);

// ritorna il descrittore del processo id
extern "C" des_proc *des_p(short id);

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
	if (pdirettorio == 0) goto errore4;

	// il direttorio viene inizialmente copiato dal direttorio principale
	// (in questo modo, il nuovo processo acquisisce automaticamente gli
	// spazi virtuali condivisi, sia a livello utente che a livello
	// sistema, tramite condivisione delle relative tabelle delle pagine)
	*pdirettorio = *direttorio_principale;
	
	pila_sistema = crea_pila_sistema(pdirettorio, fine_sistema_privato);
	if (pila_sistema == 0) goto errore5;

	if (liv == LIV_UTENTE) {

		pila_sistema->parole_lunghe[1019] = uint((void*)f);	// EIP (codice utente)
		pila_sistema->parole_lunghe[1020] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pila_sistema->parole_lunghe[1021] = 0x00000200;	// EFLAG
		pila_sistema->parole_lunghe[1022] = uint(fine_utente_privato) - 2 * sizeof(int);
									// ESP (pila utente)
		pila_sistema->parole_lunghe[1023] = SEL_DATI_UTENTE;	// SS (pila utente)
		// eseguendo una IRET da questa situazione, il processo
		// passera' ad eseguire la prima istruzione della funzione f,
		// usando come pila la pila utente (al suo indirizzo virtuale)

		pila_utente = crea_pila_utente(pdirettorio, fine_utente_privato, 64);
		if (pila_utente == 0) goto errore6;

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

errore6:	rilascia_tutto(pdirettorio,
		  sub(fine_sistema_privato, SIZE_PAGINA),
		  fine_sistema_privato);
errore5:	rilascia(pdirettorio);
errore4:	rilascia_tss(identifier);
errore3:	delete pdes_proc;
errore2:	delete p;
errore1:	return 0;
}

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

	pdes_tab = collega_tabella(pdir, ptab, indice_direttorio(ind));

	ind_fisico = alloca_pagina_virtuale();
	if (ind_fisico == 0) goto errore2;
	pdes_pag = &ptab->entrate[indice_tabella(ind)];
	if (! carica_pagina(pdes_pag, ind_fisico) )
		goto errore3;
	collega_pagina(ptab, ind_fisico, ind);

	pdes_pag->D		= 1; // verra' modificata
	pdes_pag->A		= 1; // e acceduta
	pdes_tab->A		= 1;

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


//////////////////////////////////////////////////////////////////////////////////
// PRIMITIVE                                                                    //
//////////////////////////////////////////////////////////////////////////////////

typedef int (*entry_t)(int);
entry_t io_entry = 0;
volatile short id_main = 0;

// schedulatore
extern "C" void schedulatore(void);

// verifica i diritti del processo in esecuzione sull' area di memoria
// specificata dai parametri
extern "C" bool verifica_area(void *area, unsigned int dim, bool write);


extern "C" void
c_activate_p(void f(int), int a, int prio, char liv, short &id, bool &risu)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	if (id_main != 0 && esecuzione->identifier != id_main) {
		printk("activate_p non chiamata da main");
		abort_p();
	}

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

void shutdown() {
	printk("Tutti i processi sono terminati!\n");
	asm("1: sti; nop; jmp 1b" : : );
}
extern "C" void terminate_p();

extern "C" void c_terminate_p()
{
	des_proc* pdes_proc = des_p(esecuzione->identifier);

	direttorio* pdirettorio = pdes_proc->cr3;
	rilascia_tutto(pdirettorio, inizio_sistema_privato, fine_sistema_privato);
	rilascia_tutto(pdirettorio, inizio_utente_privato,  fine_utente_privato);
	rilascia(pdirettorio);
	rilascia_tss(esecuzione->identifier);
	delete esecuzione;
	delete pdes_proc;
	processi--;
	schedulatore();
}

// Registrazione processi esterni

const int S = 2;
const int A = 2;

proc_elem *pe_tast;
proc_elem *in_com[S], *out_com[S];
proc_elem *ata[A];

// Valori usati in io.cpp
enum estern_id {
	tastiera,
	com1_in,
	com1_out,
	com2_in,
	com2_out,
	ata0,
	ata1
};

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

extern "C" void c_activate_pe(void f(int), int a, int prio, char liv,
	short &identifier, estern_id interf, bool &risu)
{
	proc_elem	*p;			// proc_elem per il nuovo processo

	if (id_main != 0 && esecuzione->identifier != id_main) {
		printk("activate_pe non chiamata da main");
		abort_p();
	}

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

extern "C" void attiva_timer(unsigned long delay);

extern "C" void c_begin_p()
{
	if (esecuzione->identifier != id_main) {
		printk("begin_p() non chiamata da main!\n");
		abort_p();
	}
	processi--;
        schedulatore();
}

extern "C" void c_give_num(int &lav)
{
        lav = processi;
}


// descrittore di semaforo
struct des_sem {
	int counter;
	proc_elem *pointer;
};


// vettore dei descrittori di semaforo
extern des_sem array_dess[MAX_SEMAFORI];

// bitmap per l'allocazione dei semafori
unsigned int sem_buf[MAX_SEMAFORI / sizeof(int) + 1];
bm_t sem_bm = { sem_buf, MAX_SEMAFORI };

extern "C" void c_sem_ini(int &index_des_s, int val, bool &risu)
{
	unsigned int pos;

	if (id_main != 0 && esecuzione->identifier != id_main) {
		printk("sem_ini non chiamata da main");
		abort_p();
	}

	if(!bm_alloc(&sem_bm, pos)) {
		risu = false;
		return;
	}

	index_des_s = pos;
	array_dess[index_des_s].counter = val;
	risu = true;
}

// nel testo non si controlla il parametro sem (se > MAX_SEM crea problemi)
extern "C" void c_sem_wait(int sem)
{
	des_sem *s;

	if(sem < 0 || sem >= MAX_SEMAFORI) {
		printk("semaforo errato: %d\n", sem);
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
extern "C" void c_sem_signal(int sem)
{
	des_sem *s;
	proc_elem *lavoro;

	if(sem < 0 || sem >= MAX_SEMAFORI) {
		printk("semaforo errato: %d\n", sem);
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
struct page_fault_error {
	unsigned int prot  : 1;
	unsigned int write : 1;
	unsigned int user  : 1;
	unsigned int res   : 1;
	unsigned int pad   : 28; // bit non significativi
};

extern "C" void c_page_fault(void* indirizzo_virtuale, page_fault_error errore, void* eip)
{
	if (eip < fine_codice_sistema) {
		printk("eip: %x, page fault a %x:", eip, indirizzo_virtuale);
		printk("%s, %s, %s, %s\n",
			errore.prot  ? "protezione"	: "pag/tab assente",
			errore.write ? "scrittura"	: "lettura",
			errore.user  ? "da utente"	: "da sistema",
			errore.res   ? "bit riservato"	: "");
		panic("page fault dal modulo sistema");
	}
	if (errore.res == 1)
		panic("descrittore scorretto");

	if (errore.prot == 1) {
		printk("Errore di protezione, il processo verra' terminato\n");
		terminate_p();
	}

	trasferimento(indirizzo_virtuale, errore.write);
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
		}
	}
	// una volta copiati i segmenti di programma all'indirizzo per cui
	// erano stati collegati, il modulo non ci serve piu'
	free_interna(mod->mod_start, distance(mod->mod_end, mod->mod_start));
	return elf_h->e_entry;
}



extern "C" void gestore_eccezioni(int tipo, unsigned errore,
		unsigned eip, unsigned cs, short eflag)
{
	printk("Eccezione %d, errore %x\n", tipo, errore);
	printk("eflag = %x, eip = %x, cs = %x\n", eflag, eip, cs);
	abort_p();
}

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

extern "C" void c_delay(int n)
{
	richiesta *p;

	p = new richiesta;
	p->d_attesa = n;
	p->pp = esecuzione;

	inserimento_coda_timer(p);
	schedulatore();
}

extern unsigned long ticks;

// driver del timer
//
extern "C" void c_driver_t(void)
{
	richiesta *p;

	ticks++;

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

extern "C" bool c_verifica_area(void *area, unsigned int dim, bool write)
{
	des_proc *pdes_proc;
	direttorio* pdirettorio;
	char liv;
	
	pdes_proc = des_p(esecuzione->identifier);
	liv = pdes_proc->liv;

	for (void* i = area; i < add(area, dim); i = add(i, SIZE_PAGINA)) {
		descrittore_tabella *pdes_tab = &pdirettorio->entrate[indice_direttorio(i)];
		if (liv == LIV_UTENTE && pdes_tab->US == 0)
			return false;
		if (write && pdes_tab->RW == 0)
			return false;
		if (pdes_tab->P == 0)
			continue;
		tabella_pagine *ptab = tabella_puntata(pdes_tab);
		descrittore_pagina *pdes_pag = &ptab->entrate[indice_tabella(i)];
		if (liv == LIV_UTENTE && pdes_pag->US == 0)
			return false;
		if (write && pdes_pag->RW == 0)
			return false;
	}
	return true;
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

	buf[0] = '0';
	buf[1] = 'x';

	for (int i = 9; i > 1; --i) {
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
int snprintf(char *buf, unsigned int n, const char *fmt, ...)
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
//
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

///////////////////////////////////////////////////////////////////////////////////////
// DRIVER SWAP                                                                      
///////////////////////////////////////////////////////////////////////////////////////
// ingresso di una stringa di word da un buffer di IO
extern "C" void inputbuffw(ind_b reg, short *a,short n);

// uscita di una stringa di word su un buffer di IO
extern "C" void outputbuffw(short *a, ind_b reg,short n);

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
	NONE=0x00,
	IDENTIFY=0xEC,
	SET_FEATURES=0xEF,
	SET_MULT_MODE=0xC6,
	WRITE_SECT=0x30,
	READ_SECT=0x20,
	HD_SEEK=0x70,
	DIAGN=0x90
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

struct geometria {			// Struttura 3-D di un disco
	unsigned short cil;	
	unsigned short test;
	unsigned short sett;
	unsigned int tot_sett;		// Utile in LBA
};

struct drive {				// Drive su un canale ATA
	bool presente;
	bool dma;
	geometria geom;
};

struct des_ata {		// Descrittore operazione per i canali ATA
	interfata_reg indreg;
	drive disco[2];		// Ogni canale ATA puo' contenere 2 drive
	hd_cmd comando;
	char errore;
	char cont;
	short* punt;	
	int mutex;
	int sincr;
};

extern "C" des_ata hd[A];	// 2 canali ATA

const estern_id hd_id[2]= { ata0, ata1 };

//Funzioni per l'ingresso/uscita dati e processo estern0
//
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



bool hd_check_status(des_ata* p_des, int drv)
{
	char stato;
	int curr_drv;
	for (int i = 0; i < 2; i++) {
		do {
			inputb(p_des->indreg.iSTS, stato);
		} while ( (stato & HD_STS_BSY) || (stato & HD_STS_DRQ) );

		hd_read_device(p_des->indreg.iDRV_HD, curr_drv);

		if  (curr_drv == drv) 
			return true;

		hd_select_device(drv, p_des->indreg.iDRV_HD);
	}
	return false;
}

bool hd_wait_data(des_ata* p_des)
{
	char stato;

	do {
		inputb(p_des->indreg.iALT_STS, stato);
	} while (stato & HD_STS_BSY);

	return ( (stato & HD_STS_DRQ) && !(stato & HD_STS_ERR) );
}

void hd_print_error(int i, int d, int sect, char error) {
	if (error == D_ERR_NONE)
		return;

	printk("Errore su hard disk:\n");
	if (i < 0 || i > A || d < 0 || d > 2) {
		printk("valori errati (%d, %d)\n");
	} else {
		printk("%s/%s: ", (i ? "secondario" : "primario"), (d ? "slave"      : "master"));
		switch (error) {
		case D_ERR_PRESENCE:
			printk("assente o non rilevato\n");
			break;
		case D_ERR_BOUNDS:
			printk("accesso al settore %s fuori dal range\n");
			break;
		case D_ERR_GENERIC:
			printk("errore generico (DRQ=0)\n");
			break;
		default:
			if (error & 4) 
				printk("comando abortito\n");
			else
				printk("error register = %d\n", error);
			break;
		}
	}
}


// Avvio delle operazioni di lettura da hard disk
// 
void starthd_in(des_ata *p_des, short drv, short vetti[], unsigned int primo, unsigned char quanti)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->comando = READ_SECT;
	p_des->errore = D_ERR_NONE;
	if (!hd_check_status(p_des, drv))		// selezione del drive
		goto errore; 	
	hd_write_address(p_des, primo);			// parametri: primo settore
	outputb(quanti, p_des->indreg.iSTCOUNT);	// parametri: quanti settori
	go_inouthd(p_des->indreg.iDEV_CTRL);		// abilita interruzioni	
	umask_hd(p_des->indreg.iSTS);			// abilita int. nel PIC
	hd_write_command(READ_SECT, p_des->indreg.iCMD);// invia comando
	return;

errore:
	panic("Errore nella lettura");
}

// Avvio delle operazioni di scrittura su hard disk
//
void starthd_out(des_ata *p_des, short drv, short vetti[], unsigned int primo, unsigned char quanti)	
{	char stato;				
	bool ris;
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->comando = WRITE_SECT;
	p_des->errore = D_ERR_NONE;

	if (!hd_check_status(p_des, drv))		// selezione del drive
		goto errore;
	hd_write_address(p_des, primo);			// invio parametri (primo settore)
	outputb(quanti, p_des->indreg.iSTCOUNT);	// invio parametri (quanti settori)
	go_inouthd(p_des->indreg.iDEV_CTRL);		// abilita interruzioni
	hd_write_command(WRITE_SECT, p_des->indreg.iCMD);// invia comando
	if (!hd_wait_data(p_des))
		goto errore;
	outputbuffw(vetti, p_des->indreg.iDATA, H_BLK_SIZE);
	p_des->punt += H_BLK_SIZE;
	p_des->cont--;
	umask_hd(p_des->indreg.iSTS);
	return;

errore:
	panic("Errore nella scrittura");
}

// Primitiva di lettura da hard disk: legge quanti blocchi (max 256, char!)
//  a partire da primo depositandoli in vetti; il controller usato e ind_ata,
//  il drive e' drv. Riporta un fallimento in errore
//  
extern "C" void c_readhd_n(short ind_ata, short drv, short vetti[],
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
	if (primo + quanti >= p_des->disco[drv].geom.tot_sett) {
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
//  
extern "C" void c_writehd_n(short ind_ata, short drv, short vetti[], unsigned int primo,
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
	if (primo + quanti >= p_des->disco[drv].geom.tot_sett) {
		errore=D_ERR_BOUNDS;
		return;
	}

	sem_wait(p_des->mutex);
	starthd_out(p_des,drv,vetti,primo,quanti);
	sem_wait(p_des->sincr);
	errore = p_des->errore;
	sem_signal(p_des->mutex);
}

// driver per l'hard disk, valido sia per l'ingresso che per l'uscita
//
extern "C" void c_driver_hd(int ind_ata)
{
	des_ata *p_des;
	int drv;
	hd_cmd curr_cmd;
	proc_elem* lavoro;
	bool fine;
	char stato;

	p_des = &hd[ind_ata];	

	inputb(p_des->indreg.iSTS, stato); // ack dell'interrupt

	curr_cmd = p_des->comando;	
	p_des->comando = NONE;	
	fine = false;
	switch (curr_cmd) {
	case WRITE_SECT:
		if (p_des->cont == 0)
			fine = true;
		else {
			if (!hd_wait_data(p_des)) {
				inputb(p_des->indreg.iERROR, p_des->errore);
				fine = true;
			}
			outputbuffw(p_des->punt, p_des->indreg.iDATA, H_BLK_SIZE);
			p_des->comando = WRITE_SECT;
			p_des->punt += H_BLK_SIZE;
			p_des->cont--;
		}
		break;
	case READ_SECT:
		if (!hd_wait_data(p_des)) {
			inputb(p_des->indreg.iERROR, p_des->errore);
			fine = true;
		}
		inputbuffw(p_des->indreg.iDATA, p_des->punt, H_BLK_SIZE);
		p_des->cont--;
		if (p_des->cont == 0)
			fine = true;
		else {
			p_des->comando = READ_SECT;
			p_des->punt += H_BLK_SIZE;
		}
		break;
	default:
		printk("Comando sconosciuto: %d\n", curr_cmd);
		fine = true;
		break;
	}
	if (fine == true) {
		mask_hd(p_des->indreg.iSTS);
		halt_inouthd(p_des->indreg.iDEV_CTRL);	
		des_sem* s = &array_dess[p_des->sincr];
		s->counter++;
		if (s->counter <= 0) {
			rimozione_coda(s->pointer, lavoro);
			inserimento_coda(pronti, lavoro);
		}
	}	
	schedulatore();
}

// esegue un software reset di entrambi gli hard disk collegati al canale 
// descritto da p_des
void hd_reset(des_ata* p_des) {
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
void hd_init() {
	des_ata *p_des;
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
		// comando. Inviamo, quindi il comando IDENTIFY DEVICE
		for (int d = 0; d < 2; d++) {
			short st_sett[256];
			char serial[41], *ptr = serial;
			char stato;

			if (!p_des->disco[d].presente) 
				continue;
			halt_inouthd(p_des->indreg.iDEV_CTRL);	
			if (!hd_check_status(p_des, d))
				goto error;
			hd_write_command(IDENTIFY, p_des->indreg.iCMD);
			if (!hd_wait_data(p_des))
				goto error;
			inputb(p_des->indreg.iSTS, stato); // ack
			inputbuffw(p_des->indreg.iDATA, st_sett, H_BLK_SIZE);
			// Inizializzazione della geometria
			p_des->disco[d].geom.cil      = st_sett[1];		
			p_des->disco[d].geom.test     = st_sett[3];
			p_des->disco[d].geom.sett     = st_sett[6];
			p_des->disco[d].geom.tot_sett = (st_sett[61] << 16) + st_sett[60];
			for (int j = 27; j <= 46; j++) {
				*ptr++ = (char)(st_sett[j] >> 8);
				*ptr++ = (char)(st_sett[j]);
			}
			*ptr = 0;
			printk("  - %s %s: %s - %d/%d/%d (%d)\n",
				(i ? "sec" : "pri"),
				(d ? "slave "   : "master"),
				serial,
				p_des->disco[d].geom.cil,
				p_des->disco[d].geom.test,
				p_des->disco[d].geom.sett,
				p_des->disco[d].geom.tot_sett);
			continue;

		error:
			p_des->disco[d].presente = false;
			continue;
		}

	}
	mask_hd(p_des->indreg.iSTS);
	bool r1, r2;
	c_sem_ini(p_des->mutex, 1, r1);
	c_sem_ini(p_des->sincr, 0, r2);
	if(!r1 || !r2)
		panic("\nImpossibile allocare i semafori per l' IO");
}


// Funzione di utilita' perche' l'utente possa conoscere se interessato,
//  le dimensioni e la struttura dei suoi hard disk
//  
extern "C" void c_geometria(int interf,short drv,unsigned short &c,unsigned short &t,unsigned short &s,int &ts,char &err) {
	if (hd[interf].disco[drv].presente==true) {
		c=hd[interf].disco[drv].geom.cil;
		t=hd[interf].disco[drv].geom.test;
		s=hd[interf].disco[drv].geom.sett;
		ts=hd[interf].disco[drv].geom.tot_sett;
		err=D_ERR_NONE;
	}
	else
		err=D_ERR_PRESENCE;
}

///////////////////////////////////////////////////////////////////////////////////
// INIZIALIZZAZIONE                                                              //
///////////////////////////////////////////////////////////////////////////////////
//


struct superblock_t {
	char		bootstrap[512];
	unsigned int	bm_start;
	int		blocks;
	unsigned int	directory;
	int		(*entry_point)(int);
	void*		end;
};

char read_buf[SIZE_PAGINA];

void leggi_swap(void* buf, unsigned int block, unsigned int bytes, const char* msg) {
	char errore;
	des_sem* s;
	
	readhd_n(SWAP_PS, SWAP_MS, buf, block * 8, ceild(bytes, 512), errore);
	if (errore != 0) { 
		printk("\nImpossibile leggere %s\n", msg);
		hd_print_error(SWAP_PS, SWAP_MS, block * 8, errore);
		panic("Fatal error");
	}
}

// timer
extern unsigned long ticks;
extern unsigned long clocks_per_usec;

	
// inizializzazione della console
// controllore interruzioni
extern "C" void init_8259();
extern "C" unsigned long  calibra_tsc();
extern "C" void trasforma_in_processo();
extern "C" void salta_a_main(entry_t user_entry, void* stack);
extern void* stack;

void dd(int i)
{
	while (processi > 0)
		;
	shutdown();

}

proc_elem init;

extern "C" void
cmain (unsigned long magic, multiboot_info_t* mbi)
{
	des_proc* pdes_proc;
	proc_elem* dummy;
	unsigned long clocks_per_sec;
	superblock_t *superblock;
	unsigned int pages, *buf;
	void* last_address;
	bool risu;
	direttorio* tmp, *main_dir;
	pagina* pila_utente;
	int errore;

	// inizializziamo per prima cosa la console, in modo da poter scrivere
	// messaggi sullo schermo
	con_init();

	printk("Nucleo di Calcolatori Elettronici, v1.0\n");

	// controlliamo di essere stati caricati
	// da un bootloader che rispetti lo standard multiboot
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printk ("Numero magico non valido: 0x%x\n", magic);
		goto error;
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
		module_t* mod = mbi->mods_addr;
		for (int i = 0; i < mbi->mods_count; i++) {
			if (salta_a(mod->mod_start) < 0 ||
			    occupa(distance(mod->mod_end, mod->mod_start)) == 0) {
				printk("Errore nel caricamento del modulo (lo spazio risulta occupato)");
				goto error;
			}
			mod++;
		}

		// quindi, ne interpretiamo il contenuto
		mod = mbi->mods_addr;
		for (int i = 0; i < mbi->mods_count; i++) {
			if (str_equal(mod->string, "/io")) {
				io_entry = (entry_t)carica_modulo_io(mod);
				if (io_entry == 0) {
					printk("Impossibile caricare il modulo di IO");
					goto error;
				}
				printk("Caricato il modulo di IO\n");
			} else {
				free_interna(mod->mod_start, distance(mod->mod_end, mod->mod_start));
				printk("Modulo '%s' non riconosciuto (eliminato)\n");
			}
			mod++;
		}

	}


	// il resto della memoria e' per le pagine fisiche
	init_pagine_fisiche();
	printk("Pagine fisiche: %d\n", num_pagine_fisiche);

	// inizializziamo la mappa di bit che serve a tenere traccia dei
	// semafori allocati
	bm_create(&sem_bm, sem_buf, MAX_SEMAFORI);
	printk("Semafori: %d\n", MAX_SEMAFORI);

	// il direttorio principale viene utilizzato fino a quando non creiamo
	// il primo processo.  Poi, servira' come "modello" da cui creare i
	// direttori dei nuovi processi.
	direttorio_principale = alloca_direttorio();
	if (direttorio_principale == 0) {
		printk("Impossibile allocare il direttorio principale");
		goto error;
	}
		
	memset(direttorio_principale, 0, SIZE_PAGINA);

	// memoria fisica in memoria virtuale
	for (void* ind = addr(0); ind < max_mem_upper; ind = add(ind, SIZE_PAGINA)) {
		descrittore_tabella* pdes_tab =
			&direttorio_principale->entrate[indice_direttorio(ind)];
		tabella_pagine* ptab;
		if (pdes_tab->P == 0) {
			ptab = alloca_tabella_residente();
			if (ptab == 0) {
				printk("Impossibile allocare le tabelle condivise");
				goto error;
			}
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
	printk("Mappata memoria fisica in memoria virtuale\n");

	carica_cr3(direttorio_principale);
	// avendo predisposto il direttorio in modo che tutta la memoria fisica
	// si trovi gli stessi indirizzi in memoria virtuale, possiamo attivare
	// la paginazione, sicuri che avremo continuita' di indirizzamento
	attiva_paginazione();
	printk("Paginazione attivata\n");

	// quando abbiamo finito di usare la struttura dati passataci dal boot
	// loader, possiamo assegnare allo heap la memoria fisica di indirizzo
	// < 1MB. Lasciamo inutilizzata la prima pagina, in modo che
	// l'indirizzo 0 non venga mai allocato e possa essere utilizzato per
	// specificare un puntatore non valido
	free_interna(addr(SIZE_PAGINA), distance(max_mem_lower, addr(SIZE_PAGINA)));


	// inizializziamo il controllore delle interruzioni [vedi sistema.S]
	init_8259();
	printk("Controllore delle interruzioni inizializzato\n");
	
	// processo dummy
	dummy = crea_processo(dd, 0, -1, LIV_SISTEMA);
	if (dummy == 0) {
		printk("Impossibile creare il processo dummy");
		goto error;
	}
	inserimento_coda(pronti, dummy);
	printk("Creato il processo dummy\n");

	// creazione del primo processo
	pdes_proc = new des_proc;
	if (pdes_proc == 0) {
		printk("Impossibile creare il processo main (mem. insufficiente)");
		goto error;
	}
	id_main = alloca_tss(pdes_proc);
	if (id_main == 0) {
		printk("Impossibile creare il processo main (gdt insufficiente)");
		goto error;
	}
	init.identifier = id_main;
	init.priority = 0;

	// il primo processo utilizza il direttorio principale e,
	// inizialmente, si trova a livello sistema (deve eseguire la parte 
	// rimanente di questa routine, che si trova a livello sistema)
	memset(pdes_proc, 0, sizeof(des_proc));
	pdes_proc->cr3 = direttorio_principale;
	pdes_proc->liv = LIV_SISTEMA;
	esecuzione = &init;
	processi++;
	trasforma_in_processo();

	// da qui in poi, e' il processo init che esegue
	printk("Processo main:\n");

	// attiviamo il timer e calibriamo il contatore per i microdelay
	// (necessari nella corretta realizzazione del driver dell'hard disk)
	printk("- calibrazione del tsc...");
	attiva_timer(DELAY);
	clocks_per_sec = calibra_tsc();
	clocks_per_usec = ceild(clocks_per_sec, 1000000UL);
	printk("%d clocks/usec\n", clocks_per_usec);

	// inizializziamo il driver dell'hard disk, in modo da poter leggere lo 
	// swap
	printk("- inizializzazione e riconoscimento hard disk:\n");
	hd_init();

	// lettura del superblocco
	printk("- lettura del superblocco dall'area di swap:\n");
	leggi_swap(read_buf, 0, sizeof(superblock_t), "il superblocco");
	superblock = reinterpret_cast<superblock_t*>(read_buf);
	printk("  - bm: %d, blocks: %d, dir: %d, entry: %x, end: %x\n", 
			superblock->bm_start,
			superblock->blocks,
			superblock->directory,
			superblock->entry_point,
			superblock->end);

	// lettura della bitmap dei blocchi
	printk("- lettura della bitmap dei blocchi...");
	pages = ceild(superblock->blocks, SIZE_PAGINA * 8);
	buf = new unsigned int[(pages * SIZE_PAGINA) / sizeof(unsigned int)];
	if (buf == 0) {
		printk("Impossibile allocare la bitmap dei blocchi");
		goto error;
	}
	bm_create(&block_bm, buf, superblock->blocks);
	leggi_swap(buf, superblock->bm_start, pages * SIZE_PAGINA, "la bitmap dei blocchi");
	printk("ok\n");

	// lettura del direttorio principale dallo swap
	printk("- lettura del direttorio principale...");
	tmp = new direttorio;
	if (tmp == 0) {
		printk("memoria insufficiente");
		goto error;
	}
	leggi_swap(tmp, superblock->directory, sizeof(direttorio), "il direttorio principale");
	printk("ok\n");

	// tabelle condivise per lo spazio utente condiviso
	printk("- creazione o lettura delle tabelle condivise...");
	for (void* ind = inizio_utente_condiviso;
	     	   ind < fine_utente_condiviso;
	           ind = add(ind, SIZE_PAGINA * 1024))
	{
		descrittore_tabella
			*pdes_tab1 = &tmp->entrate[indice_direttorio(ind)],
			*pdes_tab2 = &direttorio_principale->entrate[indice_direttorio(ind)];
		tabella_pagine* ptab;
		
		*pdes_tab2 = *pdes_tab1;

		last_address = add(ind, SIZE_PAGINA * 1024);

		ptab = alloca_tabella_condivisa();
		if (ptab == 0) {
			printk("Impossibile allocare tabella condivisa\n");
			goto error;
		}
		if (! carica_tabella(pdes_tab2, ptab) ) {
			printk("Impossibile caricare tabella condivisa");
			goto error;
		}
		pdes_tab2->address	= uint(ptab) >> 12;
		pdes_tab2->P		= 1;
		for (int i = 0; i < 1024; i++) {
			descrittore_pagina* pdes_pag = &ptab->entrate[i];
			if (pdes_pag->preload == 1) {
				pagina* pag = alloca_pagina_residente();
				if (pag == 0) {
					printk("Impossibile allocare pagina residente");
					goto error;
				}
				if (! carica_pagina(pdes_pag, pag) ) {
					printk("Impossibile caricare pagina residente");
					goto error;
				}
				collega_pagina(ptab, pag, add(ind, i * SIZE_PAGINA));
			}
		}

	}
	delete tmp;
	printk("ok\n");



	// inizializzazione dello heap utente
	heap.start = allineav(superblock->end, sizeof(int));
	heap.dimensione = distance(last_address, heap.start);
	printk("- heap utente a %x, dimensione: %d B (%d MiB)\n",
			heap.start, heap.dimensione, heap.dimensione / (1024 * 1024));

	// semaforo per la mutua esclusione nella gestione dei page fault
	c_sem_ini(pf_mutex, 1, risu);
	if (!risu) {
		printk("Impossibile allocare il semaforo per i page fault");
		goto error;
	}


	// inizializzazione del modulo di io
	printk("- inizializzazione del modulo di I/O...");
	errore = io_entry(0);
	if (errore < 0) {
		printk("ERRORE: %d\n", -errore);
		goto error;
	}
	printk("ok\n");
	
	// ora trasformiamo il processo corrente in un processo utente (in modo 
	// che possa passare ad eseguire la routine main)
	printk("- salto a livello utente\n");
	pila_utente = crea_pila_utente(direttorio_principale, fine_utente_privato, 1);
	if (pila_utente == 0) {
		printk("Impossibile allocare la pila utente per main");
		goto error;
	}
	pdes_proc->esp0 = add(&stack, SIZE_PAGINA);
	pdes_proc->ss0 = SEL_DATI_SISTEMA;
	pdes_proc->ds  = SEL_DATI_UTENTE;
	pdes_proc->es  = SEL_DATI_UTENTE;
	pdes_proc->fpu.cr = 0x037f;
	pdes_proc->fpu.tr = 0xffff;
	pdes_proc->liv = LIV_UTENTE;
	salta_a_main(superblock->entry_point, fine_utente_privato);
error:
	panic("Fatal error");
}
