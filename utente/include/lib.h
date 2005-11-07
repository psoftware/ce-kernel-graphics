#ifndef _LIB_H_
#define _LIB_H_

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

#endif

