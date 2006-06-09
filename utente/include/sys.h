// sys.h
// header per l' interfaccia offerta dal nucleo e dal modulo di IO
//

#ifndef _SYS_H_
#define _SYS_H_

// Prototipi delle chiamate di sistema disponibili
//

// Crea un nuovo processo, avente corpo F. A e` il parametro di F, PRIO la
//  priorita`, LIV specifica lo stato (utente o sistema), in ID viene ritornato
//  l' identificatore del nuovo processo e RISU vale FALSE in caso di errore,
//  true in caso di successo
//
extern "C" short activate_p(void f(int), int a, int prio, char liv);

// Termina il processo in esecuzione
//
extern "C" void terminate_p();

// Ritorna il valore della variabile di nucleo processi, contenente il numero di
//  processi creati mediante activate_p e non terminati con terminate_p
//
extern "C" void give_num(int &lav);

// Cerca un descrittore di semaforo non utilizzato e lo inizializza a VAL.
//  L' indice del semaforo e` ritornato in INDEX_DES_S
//
extern "C" int sem_ini(int val);

// Decrementa il valore del semaforo di cui SEM e` l' indice; se il nuovo
//  valore e` negativo blocca il processo su SEM
//
extern "C" void sem_wait(int sem);

// Incrementa il valore del semaforo il cui indice e` SEM; se il nuovo valore
//  non e` positivo sblocca un processo bloccato su SEM
//  
extern "C" void sem_signal(int sem);

// Alloca DIM bytes nella memoria dinamica
//
extern "C" void *mem_alloc(int dim);

// Rilascia una zona di memoria indirizzata da PV precedentemente allocata
//  con mem_alloc()
//
extern "C" void mem_free(void *pv);

// Attende N interruzioni del timer di sistema
//
extern "C" void delay(int n);

// Legge QUANTI caratteri dalla interfaccia seriale SERIAL, scrivendoli in
//  QUANTI, riportando eventuali errori in ERRORE
//
extern "C" void readse_n(int serial, char vetti[], int quanti, char &errore);

// Legge al massimo QUANTI caratteri dalla interfaccia seriale SERIAL,
//  smette quando ne trova uno nullo. Mette il risultato in vetti e
//  lascia il numero di caratteri letti in QUANTI ed un eventuale codice
//  di errore in ERRORE
//
extern "C" void readse_ln(int serial, char vetti[], int &quanti, char &errore);

// Preleva al massimo QUANTI caratteri da VETTO fino ad incontrarne uno
//  nullo, scrivendoli sull' interfaccia seriale SERIAL, lasciando il
//  numero di caratteri letti in QUANTI.
//
extern "C" void writese_0(int serial, char vetto[], int &quanti);

// Preleva QUANTI caratteri da VETTO scrivendoli sull' interfaccia
//  seriale SERIAL
//
extern "C" void writese_n(int serial, char vetto[], int quanti);

// Legge QUANTI caratteri dal terminale TERM e li mette in VETTI
//
extern "C" void readvterm_n(int term, char vetti[], int quanti, bool echo = true);
extern "C" void readvterm_ln(int term, char vetti[], int &quanti, bool echo = true);

// Scrive QUANTI caratteri sul terminale TERM, prelevandoli da VETTI
//
extern "C" void writevterm_n(int term, char vetti[], int quanti, bool echo = true);

const unsigned int LOG_MSG_SIZE = 72;
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR };

struct log_msg {
	log_sev sev;
	short  identifier;
	char msg[LOG_MSG_SIZE];
};
extern "C" void readlog(log_msg& m);

extern "C" int resident(void* start, int quanti);

// Costanti
//
const int LIV_SISTEMA = 0, LIV_UTENTE = 3;

#endif

