// sys.h
// header per l' interfaccia offerta dal nucleo e dal modulo di IO
//

#ifndef SYS_H_
#define SYS_H_

#include <tipo.h>
#include <costanti.h>

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

extern "C" void readlog(log_msg& m);
extern "C" void log(log_sev sev, cstr msg, natl quanti);

extern "C" int resident(void* start, natl quanti);

extern "C" void iniconsole(natb cc);
extern "C" void readconsole(str buff, natl& quanti);
extern "C" void writeconsole(cstr buff);


#endif

