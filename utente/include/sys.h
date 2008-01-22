// sys.h
// header per l' interfaccia offerta dal nucleo e dal modulo di IO
//

#ifndef SYS_H_
#define SYS_H_

#include "tipo.h"

extern "C" int activate_p(void f(int), int a, natl prio, natb liv);
extern "C" void terminate_p();
extern "C" natl give_num();
extern "C" void end_program();
extern "C" int sem_ini(int val);
extern "C" void sem_wait(int sem);
extern "C" void sem_signal(int sem);
extern "C" void *mem_alloc(natl dim);
extern "C" void mem_free(void *pv);
extern "C" void delay(natl n);
extern "C" void readse_n(int serial, char vetti[], int quanti, char &errore);
extern "C" void readse_ln(int serial, char vetti[], int &quanti, char &errore);
extern "C" void writese_0(int serial, char vetto[], int &quanti);
extern "C" void writese_n(int serial, char vetto[], int quanti);


const unsigned int LOG_MSG_SIZE = 72;
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR };

struct log_msg {
	log_sev sev;
	short  identifier;
	char msg[LOG_MSG_SIZE];
};
extern "C" void readlog(log_msg& m);
extern "C" void log(log_sev sev, const char* msg, int quanti);

extern "C" int resident(void* start, int quanti);

extern "C" void readconsole(char* buff, int& quanti);
extern "C" void writeconsole(char* buff);

// Costanti
//
const int LIV_SISTEMA = 0, LIV_UTENTE = 3;

#endif

