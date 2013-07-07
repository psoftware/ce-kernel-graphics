// Versione semplificata delle macro per manipolare le liste di parametri
//  di lunghezza variabile; funziona solo se gli argomenti sono di
//  dimensione multipla di 4, ma e' sufficiente per le esigenze di printk.
//
typedef char *va_list;
#define va_start(ap, last_req) (ap = (char *)&(last_req) + sizeof(last_req))
#define va_arg(ap, type) ((ap) += sizeof(type), *(type *)((ap) - sizeof(type)))
#define va_end(ap)

extern "C" void *memcpy(str dest, cstr src, natl n);
extern "C" void *memset(str dest, int c, natl n);
natl strlen(cstr s);
natb *strncpy(str dest, cstr src, natl l);
int strtoi(char* buf);
int vsnprintf(str vstr, natl size, cstr vfmt, va_list ap);
extern "C" int snprintf(str buf, natl n, cstr fmt, ...);
natl allinea(natl v, natl a);
addr allineav(addr v, natl a);
void* alloca(natl dim);
void free_interna(addr indirizzo, natl quanti);
void dealloca(void* p);
extern "C" void flog(log_sev sev, const char* fmt, ...);
