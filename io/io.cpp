// io.cpp
//
#include "costanti.h"
#include "tipo.h"
#define BOCHS
////////////////////////////////////////////////////////////////////////////////
//COSTANTI                                   //
////////////////////////////////////////////////////////////////////////////////


// Priorita' dei processi esterni
const int PRIO_ESTERN = 0x400;


////////////////////////////////////////////////////////////////////////////////
//                        CHIAMATE DI SISTEMA USATE                           //
////////////////////////////////////////////////////////////////////////////////

extern "C" void terminate_p(void);
extern "C" int sem_ini(int val);
extern "C" void sem_wait(int sem);
extern "C" void sem_signal(int sem);
extern "C" short activate_p(void f(int), int a, int prio, char liv);

////////////////////////////////////////////////////////////////////////////////
//               INTERFACCIA OFFERTA DAL NUCLEO AL MODULO DI IO               //
////////////////////////////////////////////////////////////////////////////////

extern "C" short activate_pe(void f(int), int a, int prio, char liv, int irq);

enum controllore { master=0, slave=1 };
extern "C" void nwfi(controllore c);

extern "C" void fill_gate(int gate, void (*f)(void), int tipo, int dpl);
extern "C" void abort_p();

extern "C" void writevid(char pc);
extern "C" void log(log_sev sev, const char* buf, int quanti);

////////////////////////////////////////////////////////////////////////////////
//                      FUNZIONI GENERICHE DI SUPPORTO                        //
////////////////////////////////////////////////////////////////////////////////

// ingresso di un byte da una porta di IO
extern "C" void inputb(ioaddr reg, natb &a);

// uscita di un byte su una porta di IO
extern "C" void outputb(natb a, ioaddr reg);

// ingresso di una word da una porta di IO
extern "C" void inputw(ioaddr reg, natw &a);

// uscita di una word su una porta di IO
extern "C" void outputw(natw a, ioaddr reg);

extern "C" void flog(log_sev sev, const char* fmt, ...);
void *memset(void *dest, int c, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);

////////////////////////////////////////////////////////////////////////////////
//                    GESTIONE DELLE INTERFACCE SERIALI                       //
////////////////////////////////////////////////////////////////////////////////

const int S = 2;

extern "C" void go_inputse(ioaddr i_ctr);
extern "C" void halt_inputse(ioaddr i_ctr);
extern "C" void go_outputse(ioaddr i_ctr);
extern "C" void halt_outputse(ioaddr i_ctr);

enum funz { input_n, input_ln, output_n, output_0 };

struct interfse_reg {
	ioaddr iRBR, iTHR, iLSR, iIER, iIIR;
};

struct des_se {
	interfse_reg indreg;
	int mutex;
	int sincr;
	int cont;
	addr punt;
	funz funzione;
	natb stato;
};

extern "C" des_se com[S];

void startse_in(des_se *p_des, natb vetti[], int quanti, funz op)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->funzione = op;
	go_inputse(p_des->indreg.iIER);
}

extern "C" void c_readse_n(int serial, natb vetti[], int quanti, natb& errore)
{
	des_se *p_des;

	if (serial < 0 || serial >= S)
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, quanti, input_n);
	sem_wait(p_des->sincr);
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

extern "C" void c_readse_ln(int serial, natb vetti[], int& quanti, natb& errore)
{
	des_se *p_des;

	if (serial < 0 || serial >= S)
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, 80, input_ln);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

void output_com(des_se *p_des)
{
	natb c; bool fine;
        fine = false;

	if(p_des->funzione == output_n) {
		p_des->cont--;
		if(p_des->cont == 0) {
			halt_outputse(p_des->indreg.iIER);
			fine = true;
		}
		c = *static_cast<natb*>(p_des->punt); //prelievo
		outputb(c, p_des->indreg.iTHR);
		p_des->punt = static_cast<natb*>(p_des->punt) + 1; 
	} else if(p_des->funzione == output_0) {
		c = *static_cast<natb*>(p_des->punt); //prelievo
		if(c == 0) {
			fine = true;
			halt_outputse(p_des->indreg.iIER);
		} else {
			p_des->cont++;
			outputb(c, p_des->indreg.iTHR);
			p_des->punt = static_cast<natb*>(p_des->punt) + 1; 
		}
	}

	if(fine == true)
		sem_signal(p_des->sincr);

}

void input_com(des_se *p_des)
{
	natb c; bool fine;
	fine = false;

	halt_inputse(p_des->indreg.iIER);

	inputb(p_des->indreg.iLSR, c);

	p_des->stato = c & 0x1e;
	if (p_des->stato != 0)
		fine = true;
	else {
		inputb(p_des->indreg.iRBR, c);
		if (p_des->funzione == input_n) {
			*static_cast<natb*>(p_des->punt) = c; // memorizzazione
			p_des->punt = static_cast<natb*>(p_des->punt) + 1;
			p_des->cont--;
			if(p_des->cont == 0)
				fine = true;
		} else if (p_des->funzione == input_ln)
			if(c == '\r' || c == '\n') {
				fine = true;
				p_des->cont = 80 - p_des->cont;
			} else {
				*static_cast<natb*>(p_des->punt) = c; // memorizzazione
				p_des->punt = static_cast<natb*>(p_des->punt) + 1;
				p_des->cont--;
				if (p_des->cont == 0) {
					fine = true;
					p_des->cont = 80;
				}
			}
		}

	if(fine == true) {
		*static_cast<natb*>(p_des->punt) = 0;	// carattere nullo
		sem_signal(p_des->sincr);
	} else
		go_inputse(p_des->indreg.iIER);
}

void estern_com(int i)
{
	natb r;
	des_se *p_des;
	p_des = &com[i];
	for(;;) {
		inputb(p_des->indreg.iIIR, r);
		if ((r&0x06) == 0x04) 
			input_com(p_des);
		else if ((r&0x06) == 0x02)
			output_com(p_des);
		nwfi(master); // sia com1 che com2 sono sul master
	}
}

void startse_out(des_se *p_des, natb vetto[], int quanti, funz op)
{
	p_des->cont = quanti;
	p_des->punt = vetto;
	p_des->funzione = op;
	go_outputse(p_des->indreg.iIER);
	output_com(p_des);
}

extern "C" void c_writese_n(int serial, natb vetto[], int quanti)
{
	des_se *p_des;

	if (serial < 0 || serial >= S)
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, quanti, output_n);
	sem_wait(p_des->sincr);
	sem_signal(p_des->mutex);
}

extern "C" void c_writese_0(int serial, natb vetto[], int &quanti)
{
	des_se *p_des;

	if (serial < 0 || serial >= S)
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, 0, output_0);
	sem_wait(p_des->sincr);
	quanti = p_des->cont;
	sem_signal(p_des->mutex);
}


extern "C" void com_setup(void);

// Interruzioni hardware delle interfacce seriali
//
int com_irq[S] = { 4, 3 };

bool com_init()
{
	des_se *p_des;
	short id;
	int i, com_base_prio = PRIO_ESTERN;

	com_setup();

	for(i = 0; i < S; ++i) {
		p_des = &com[i];

		if ( (p_des->mutex = sem_ini(1)) == 0) {
			flog(LOG_ERR, "com: impossibile creare mutex");
			return false;
		}
		if ( (p_des->sincr = sem_ini(0)) == 0) {
			flog(LOG_ERR, "com: impossibile creare sincr");
			return false;
		}

		id = activate_pe(estern_com, i, com_base_prio - i, LIV_SISTEMA, com_irq[i]);
		if (id == 0) {
			flog(LOG_ERR, "com: impossibile creare proc. esterno");
			return false;
		}

	}
	flog(LOG_INFO, "com: inizializzate %d seriali", S);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
//                         GESTIONE DELLA CONSOLE                            //
////////////////////////////////////////////////////////////////////////////////

const int MAX_CODE = 29; 

struct interfkbd_reg {
	ioaddr iRBR, iTBR, iCMR, iSTR;
};

struct des_kbd {
	interfkbd_reg indreg;
	addr punt;
	int cont;
	bool shift;
	natb tab[MAX_CODE];
	natb tabmin[MAX_CODE];
	natb tabmai[MAX_CODE];
};

const int COLS = 80;
const int ROWS = 25;
const int VIDEO_SIZE = COLS * ROWS;

struct interfvid_reg {
	ioaddr iIND, iDAT;
};

struct des_vid {
	interfvid_reg indreg;
	natw* video;
	int x, y;
	natb attr;
};

struct des_console {
	int mutex;
	int sincr;
	des_kbd kbd;
	des_vid vid;
};

extern "C" des_console console;

extern "C" void abilita_tastiera(void);
extern "C" void halt_inputkbd(interfkbd_reg indreg);
extern "C" void go_inputkbd(interfkbd_reg indreg);
void writeelem(natb c);
void writeseq(cstr buff);

void startkbd_in(des_kbd* p_des, addr buff)
{
	p_des->punt = buff;
	p_des->cont = 80;
	go_inputkbd(p_des->indreg);
}

extern "C" void c_readconsole(str buff, int& quanti)
{
	des_console *p_des;

	p_des = &console;
	sem_wait(p_des->mutex);
	startkbd_in(&p_des->kbd, buff);
	sem_wait(p_des->sincr);
	quanti = p_des->kbd.cont;
	sem_signal(p_des->mutex);
}

natb converti(des_kbd* p_des, natb c) {
	natb cc;
	int pos = 0;
	while (pos < MAX_CODE && p_des->tab[pos] != c)
		pos++;
	if (pos == MAX_CODE)
		return 0;
	if (p_des->shift)
		cc = p_des->tabmai[pos];
	else
		cc = p_des->tabmin[pos];
	return cc;
}


void estern_kbd(int h)
{
	des_console *p_des = &console;
	natb a, c;
	bool fine;

	for(;;) {
		halt_inputkbd(p_des->kbd.indreg);

		inputb(p_des->kbd.indreg.iRBR, c);
		
		fine = false;
		switch (c) {
		case 0x2a: // left shift make code
			p_des->kbd.shift = true;
			break;
		case 0xaa: // left shift break code
			p_des->kbd.shift = false;
			break;
		default:
			if (c < 0x80) {
				a = converti(&p_des->kbd, c);
				if (a == 0)
					break;
				if (a == '\b') {
					if (p_des->kbd.cont < 80) {
						p_des->kbd.punt = static_cast<natb*>(p_des->kbd.punt) - 1;
						p_des->kbd.cont++;
						writeseq("\b \b");
					}
				} else if (a == '\r' || a == '\n') {
					fine = true;
					p_des->kbd.cont = 80 - p_des->kbd.cont;
					*static_cast<natb*>(p_des->kbd.punt) = 0;
					writeseq("\r\n");
				} else {
					*static_cast<natb*>(p_des->kbd.punt) = a;
					p_des->kbd.punt = static_cast<natb*>(p_des->kbd.punt) + 1;
					p_des->kbd.cont--;
					writeelem(a);
					if (p_des->kbd.cont == 0) {
						fine = true;
						p_des->kbd.cont = 80;
					}
				}
			}
			break;
		}
		if (fine == true) 
			sem_signal(p_des->sincr);
		else
			go_inputkbd(p_des->kbd.indreg);
		nwfi(master);
	}
}
// Interruzione hardware della tastiera
const int KBD_IRQ = 1;

extern "C" void kbd_enable();

bool kbd_init()
{
	des_kbd *p_des = &console.kbd;
	if (activate_pe(estern_kbd, 0, PRIO_ESTERN, LIV_SISTEMA, KBD_IRQ) == 0) {
		flog(LOG_ERR, "kbd: impossibile creare estern_kbd");
		return false;
	}

	kbd_enable();

	return true;
}

extern "C" des_vid vid;

extern "C" void cursore(ioaddr iIND, ioaddr iDAT, int x, int y);

bool vid_init()
{
	des_vid *p_des = &console.vid;
	for (int i = 0; i < VIDEO_SIZE; i++) 
		p_des->video[i] = 0 | p_des->attr << 8;
	cursore(p_des->indreg.iIND, p_des->indreg.iDAT,
		p_des->x, p_des->y);
	flog(LOG_INFO, "vid: video inizializzato");
	return true;
}

bool console_init() {
	des_console *p_des = &console;

	if ( (p_des->mutex = sem_ini(1)) == 0) {
		flog(LOG_ERR, "kbd: impossibile creare mutex");
		return false;
	}
	if ( (p_des->sincr = sem_ini(0)) == 0) {
		flog(LOG_ERR, "kbd: impossibile creare sincr");
		return false;
	}
	return kbd_init() && vid_init();
}

void scroll(des_vid *p_des)
{
	for (int i = 0; i < VIDEO_SIZE - COLS; i++) 
		p_des->video[i] = p_des->video[i + COLS];
	for (int i = 0; i < COLS; i++)
		p_des->video[VIDEO_SIZE - COLS + i] = 0 | p_des->attr << 8;
	p_des->y--;
}

void writeelem(natb c) {
	des_vid* p_des = &console.vid;
	switch (c) {
	case '\r':
		p_des->x = 0;
		break;
	case '\n':
		p_des->y++;
		if (p_des->y >= ROWS)
			scroll(p_des);
		break;
	case '\b':
		if (p_des->x > 0 || p_des->y > 0) {
			p_des->x--;
			if (p_des->x < 0) {
				p_des->x = COLS - 1;
				p_des->y--;
			}
		}
		break;
	default:
		p_des->video[p_des->y * COLS + p_des->x] = c | p_des->attr << 8;
		p_des->x++;
		if (p_des->x >= COLS) {
			p_des->x = 0;
			p_des->y++;
		}
		if (p_des->y >= ROWS) 
			scroll(p_des);
		break;
	}
	cursore(p_des->indreg.iIND, p_des->indreg.iDAT,
		p_des->x, p_des->y);
}

void writeseq(cstr seq)
{
	const natb* pn = static_cast<const natb*>(seq);
	while (*pn != 0) {
		writeelem(*pn);
		pn++;
	}
}

extern "C" void c_writeconsole(cstr buff)
{
	des_console *p_des = &console;
	sem_wait(p_des->mutex);
	writeseq(buff);
	writeseq("\r\n");
	sem_signal(p_des->mutex);
}



//////////////////////////////////////////////////////////////////////////////////
//                  FUNZIONI DI LIBRERIA                                       
/////////////////////////////////////////////////////////////////////////////////
typedef char *va_list;

// Versione semplificata delle macro per manipolare le liste di parametri
//  di lunghezza variabile; funziona solo se gli argomenti sono di
//  dimensione multipla di 4, ma e' sufficiente per le esigenze di printk.
//
#define va_start(ap, last_req) (ap = (char *)&(last_req) + sizeof(last_req))
#define va_arg(ap, type) ((ap) += sizeof(type), *(type *)((ap) - sizeof(type)))
#define va_end(ap)

int strlen(const char *s)
{
	int l = 0;

	while(*s++)
		++l;

	return l;
}

char *strncpy(char *dest, const char *src, unsigned long l)
{
	unsigned long i;

	for(i = 0; i < l && src[i]; ++i)
		dest[i] = src[i];

	return dest;
}

static const char hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

static void htostr(char *buf, unsigned long l)
{
	int i;

	buf[0] = '0';
	buf[1] = 'x';

	for(i = 9; i > 1; --i) {
		buf[i] = hex_map[l % 16];
		l /= 16;
	}
}

static void itostr(char *buf, unsigned int len, long l)
{
	int i, div = 1000000000, v, w = 0;

	if(l == (-2147483647 - 1)) {
		strncpy(buf, "-2147483648", 12);
		return;
	} else if(l < 0) {
		buf[0] = '-';
		l = -l;
		i = 1;
	} else if(l == 0) {
		buf[0] = '0';
		buf[1] = 0;
		return;
	} else
		i = 0;

	while(i < len - 1 && div != 0) {
		if((v = l / div) || w) {
			buf[i++] = '0' + (char)v;
			w = 1;
		}

		l %= div;
		div /= 10;
	}

	buf[i] = 0;
}

#define DEC_BUFSIZE 12

int vsnprintf(char *str, unsigned long size, const char *fmt, va_list ap)
{
	int in = 0, out = 0, tmp;
	char *aux, buf[DEC_BUFSIZE];

	while(out < size - 1 && fmt[in]) {
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

int snprintf(char *buf, unsigned long n, const char *fmt, ...)
{
	va_list ap;
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return l;
}

// copia n byte da src a dest
void *memcpy(void *dest, const void *src, unsigned int n)
{
	char       *dest_ptr = static_cast<char*>(dest);
	const char *src_ptr  = static_cast<const char*>(src);

	if (src_ptr < dest_ptr && src_ptr + n > dest_ptr)
		for (int i = n - 1; i >= 0; i--)
			dest_ptr[i] = src_ptr[i];
	else
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

// log formattato
void flog(log_sev sev, const char *fmt, ...)
{
	va_list ap;
	char buf[LOG_MSG_SIZE];

	va_start(ap, fmt);
	int l = vsnprintf(buf, LOG_MSG_SIZE, fmt, ap);
	va_end(ap);

	if (l > 1)
		log(sev, buf, l - 1);
}

////////////////////////////////////////////////////////////////////////////////
//                 INIZIALIZZAZIONE DEL SOTTOSISTEMA DI I/O                   //
////////////////////////////////////////////////////////////////////////////////

// inizializza i gate usati per le chiamate di IO
//
extern "C" void fill_io_gates(void);

// eseguita in fase di inizializzazione
//
extern "C" void cmain(int)
{
	int error;

	fill_io_gates();
	if (!console_init())
		abort_p();
	if (!com_init())
		abort_p();
	terminate_p();
}
