// sys.h
// header per l' interfaccia offerta dal nucleo e dal modulo di IO
//

#ifndef SYS_H_
#define SYS_H_

#include "tipo.h"

extern "C" natl activate_p(void f(int), int a, natl prio, natl liv);
extern "C" void terminate_p();
extern "C" natl give_num();
extern "C" void end_program();
extern "C" natl sem_ini(int val);
extern "C" void sem_wait(natl sem);
extern "C" void sem_signal(natl sem);
extern "C" void *mem_alloc(natl dim);
extern "C" void mem_free(void *pv);
extern "C" void delay(natl n);
extern "C" void readse_n(natl serial, natb vetti[], natl quanti, natb &errore);
extern "C" void readse_ln(natl serial, natb vetti[], natl &quanti, natb &errore);
extern "C" void writese_0(natl serial, natb vetto[], natl &quanti);
extern "C" void writese_n(natl serial, natb vetto[], natl quanti);


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

extern "C" void iniconsole(natb cc);
extern "C" void readconsole(char* buff, int& quanti);
extern "C" void writeconsole(const char* buff);

// Costanti
//
const int LIV_SISTEMA = 0, LIV_UTENTE = 3;

#endif

