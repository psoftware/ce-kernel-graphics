#ifndef _LIB_H_
#define _LIB_H_
#include <sys.h>

typedef char *va_list;

// Macro per funzioni variadiche: versione semplificata che funziona
//  solo se in pila ci sono oggetti di dimensione multipla di 4.
// Basta per interi e puntatori, tutto quello di cui c' e' bisogno in
//  questo sistema; se ci fosse bisogno di usare tipi di lunghezza diversa
//  e' necessario sostituire i sizeof con una macro o una funzione che
//  resitituisca la dimensione in pila del tipo specificato.
//
#define va_start(ap, last_req) (ap = (char *)&(last_req) + sizeof(last_req))
#define va_arg(ap, type) ((ap) += sizeof(type), *(type *)((ap) - sizeof(type)))
#define va_end(ap)

typedef unsigned int size_t;

int strlen(const char *s);
char *strncpy(char *dest, const char *src, size_t l);

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int printf(int term, const char *fmt, ...);
//
// copia n byte da src a dst
extern "C" void *memcpy(void *dest, const void *src, unsigned int n);
// copia c nei primi n byte della zona di memoria puntata da dest
extern "C" void *memset(void *dest, int c, unsigned int n);

extern bool lib_init();
extern void flog(log_sev sev, const char* fmt, ...);
extern void writevterm_n(int vterm, const char* vetto, int quanti);
extern void vterm_setcolor(int vterm, int fgcol, int bgcol, bool blink = false);
extern bool vterm_setresident(int vterm);
extern void vterm_clear(int vterm);

struct vterm_edit_status {
	int validi;
	int cursore;
	int posizione;
};

#define VTERM_NOECHO ((vterm_edit_status*)1)

extern int readvterm_n(int vterm, char* vetti, int quanti, vterm_edit_status* stat = 0);
extern int readvterm_ln(int vterm, char* vetti, int quanti, vterm_edit_status* stat = 0);

#endif

