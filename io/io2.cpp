// io.cpp
//
#include "costanti.h"
#define BOCHS
////////////////////////////////////////////////////////////////////////////////
//COSTANTI                                   //
////////////////////////////////////////////////////////////////////////////////


// Priorita' dei processi esterni
const int PRIO_ESTERN = 0x400;


typedef unsigned char* ind_b;		// indirizzo di un byte (una porta)
typedef unsigned short* ind_w;		// indirizzo di una parola
typedef unsigned int* ind_l;		// indirizzo di una parola lunga

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

extern "C" bool verifica_area(void *area, unsigned int dim, bool write);
extern "C" void fill_gate(int gate, void (*f)(void), int tipo, int dpl);
extern "C" void abort_p();

extern "C" void writevid(char pc);
extern "C" void log(log_sev sev, const char* buf, int quanti);

////////////////////////////////////////////////////////////////////////////////
//                      FUNZIONI GENERICHE DI SUPPORTO                        //
////////////////////////////////////////////////////////////////////////////////

// ingresso di un byte da una porta di IO
extern "C" void inputb(ind_b reg, unsigned char &a);

// uscita di un byte su una porta di IO
extern "C" void outputb(char a, ind_b reg);

// ingresso di una word da una porta di IO
extern "C" void inputw(ind_b reg, unsigned short &a);

// uscita di una word su una porta di IO
extern "C" void outputw(short a, ind_b reg);

// ingresso di una stringa di word da un buffer di IO
extern "C" void inputbuffw(ind_b reg, unsigned short *a, short n);

// uscita di una stringa di word su un buffer di IO
extern "C" void outputbuffw(short *a, ind_b reg,short n);

void flog(log_sev sev, const char* fmt, ...);
void *memset(void *dest, int c, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);

////////////////////////////////////////////////////////////////////////////////
//                    GESTIONE DELLE INTERFACCE SERIALI                       //
////////////////////////////////////////////////////////////////////////////////

const int S = 2;

extern "C" void go_inputse(ind_b i_ctr);
extern "C" void halt_inputse(ind_b i_ctr);
extern "C" void go_outputse(ind_b i_ctr);
extern "C" void halt_outputse(ind_b i_ctr);

enum funz { input_n, input_ln, output_n, output_0 };

struct interfse_reg {
	ind_b iRBR;
	ind_b iTHR;
	ind_b iLSR;
	ind_b iIER;
	ind_b iIIR;
};

struct des_se {
	interfse_reg indreg;
	int mutex;
	int sincr;
	int cont;
	ind_b punt;
	funz funzione;
	char stato;
};

extern "C" des_se com[S];

void startse_in(des_se *p_des, unsigned char vetti[], int quanti, funz op)
{
	p_des->cont = quanti;
	p_des->punt = vetti;
	p_des->funzione = op;
	go_inputse(p_des->indreg.iIER);
}

extern "C" void c_readse_n(int serial, unsigned char vetti[], int quanti, char &errore)
{
	des_se *p_des;

	if(serial < 0 || serial >= S || !verifica_area(vetti, quanti, true) ||
			!verifica_area(&errore, sizeof(char), true))
		abort_p();

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_in(p_des, vetti, quanti, input_n);
	sem_wait(p_des->sincr);
	errore = p_des->stato;
	sem_signal(p_des->mutex);
}

extern "C" void c_readse_ln(int serial, unsigned char vetti[], int &quanti, char &errore)
{
	des_se *p_des;

	if(serial < 0 || serial >= S ||
			!verifica_area(&quanti, sizeof(int), true) ||
			!verifica_area(vetti, quanti, true) ||
 			!verifica_area(&errore, sizeof(char), true))
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
	char c = *p_des->punt;
	bool fine = false;

	p_des->punt++;

	if(p_des->funzione == output_n) {
		p_des->cont--;
		if(p_des->cont == 0) {
			halt_outputse(p_des->indreg.iIER);
			fine = true;
		}

		outputb(c, p_des->indreg.iTHR);
	} else if(p_des->funzione == output_0) {
		if(c == 0) {
			fine = true;
			halt_outputse(p_des->indreg.iIER);
		} else {
			p_des->cont++;
			outputb(c, p_des->indreg.iTHR);
		}
	}

	if(fine == true)
		sem_signal(p_des->sincr);

}

void input_com(des_se *p_des)
{
	unsigned char s, c;
	bool fine = false;

	halt_inputse(p_des->indreg.iIER);

	inputb(p_des->indreg.iLSR, s);

	p_des->stato = s&0x1e;
	if(p_des->stato != 0)
		fine = true;
	else {
		inputb(p_des->indreg.iRBR, c);
		if(p_des->funzione == input_n) {
			*p_des->punt = c;
			p_des->punt++;
			p_des->cont--;
			if(p_des->cont == 0)
				fine = true;
		} else if(p_des->funzione == input_ln)
			if(c == '\r' || c == '\n') {
				fine = true;
				p_des->cont = 80 - p_des->cont;
			} else {
				*p_des->punt = c;
				p_des->punt++;
				p_des->cont--;
				if(p_des->cont == 0) {
					fine = true;
					p_des->cont = 80;
				}
			}
		}

	if(fine == true) {
		*p_des->punt = 0;	// manca *
		sem_signal(p_des->sincr);
	} else
		go_inputse(p_des->indreg.iIER);
}

void estern_com(int h)
{
	unsigned char r;
	des_se *p_des;

	p_des = &com[h];

	for(;;) {
		inputb(p_des->indreg.iIIR, r);

		if ((r&0x06) == 0x02) 
			output_com(p_des);
		else
			input_com(p_des);

		nwfi(master); // sia com1 che com2 sono sul master
	}
}

void startse_out(des_se *p_des, unsigned char vetto[], int quanti, funz op)
{
	p_des->cont = quanti;
	p_des->punt = vetto;
	p_des->funzione = op;
	go_outputse(p_des->indreg.iIER);
	output_com(p_des);
}

extern "C" void c_writese_n(int serial, unsigned char vetto[], int quanti)
{
	des_se *p_des;

	if(serial < 0 || serial >= S || !verifica_area(vetto, quanti, false))
		return;

	p_des = &com[serial];
	sem_wait(p_des->mutex);
	startse_out(p_des, vetto, quanti, output_n);
	sem_wait(p_des->sincr);
	sem_signal(p_des->mutex);
}

extern "C" void c_writese_0(int serial, unsigned char vetto[], int &quanti)
{
	des_se *p_des;

	if(serial < 0 || serial >= S ||
			!verifica_area(&quanti, sizeof(int), true) ||
			!verifica_area(vetto, quanti, false))
		return;

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
//                         GESTIONE DELLA TASTIERA                            //
////////////////////////////////////////////////////////////////////////////////

struct interfkbd_reg {
	ind_b iRBR;
};

const int MAX_CODE = 57; 

struct des_kbd {
	interfkbd_reg indreg;
	int mutex;
	int sincr;
	char* punt;
	int shift;
	char ascii[2][MAX_CODE];
};

extern "C" des_kbd kbd;

extern "C" void abilita_tastiera(void);
extern "C" void halt_inputkbd();
extern "C" void go_inputkbd();

void startkbd_in(des_kbd* p_des, char* pc)
{
	p_des->punt = pc;
	go_inputkbd();
}

extern "C" void c_readkbd(char* pc)
{
	des_kbd *p_des;

	if(!verifica_area(pc, 1, true))
		abort_p();

	p_des = &kbd;
	sem_wait(p_des->mutex);
	startkbd_in(p_des, pc);
	sem_wait(p_des->sincr);
	sem_signal(p_des->mutex);
}

void estern_kbd(int h)
{
	des_kbd *p_des = &kbd;
	unsigned char c;
	bool bcode;

	for(;;) {
		halt_inputkbd();

		inputb(p_des->indreg.iRBR, c);
		bcode = c & 0x80;
		c &= ~0x80;
		
		if (!bcode && c > 0 && c <= MAX_CODE || c == 0x2a) {
			if (c == 0x2a) 
				p_des->shift = bcode ? 0 : 1;
			else {
				c = p_des->ascii[p_des->shift][c];
				if (c == '\n')
					writevid('\r');
				writevid(c);
				*p_des->punt = c;
				sem_signal(p_des->sincr);
			}
		} else {
			go_inputkbd();
		}
		nwfi(master);
	}
}
// Interruzione hardware della tastiera
const int KBD_IRQ = 1;

extern "C" void kbd_enable();

bool kbd_init()
{
	des_kbd *p_des = &kbd;
	if ( (p_des->mutex = sem_ini(1)) == 0) {
		flog(LOG_ERR, "kbd: impossibile creare mutex");
		return false;
	}
	if ( (p_des->sincr = sem_ini(0)) == 0) {
		flog(LOG_ERR, "kbd: impossibile creare sincr");
		return false;
	}
	if (activate_pe(estern_kbd, 0, PRIO_ESTERN, LIV_SISTEMA, KBD_IRQ) == 0) {
		flog(LOG_ERR, "kbd: impossibile creare estern_kbd");
		return false;
	}

	kbd_enable();

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//  MONITOR 								     //
///////////////////////////////////////////////////////////////////////////////

const int COLS = 80;
const int ROWS = 25;
const int VIDEO_SIZE = COLS * ROWS;

// descrittore di monitor
struct des_vid {
	unsigned short* video;
	int x, y;
	int mutex;
	unsigned char attr;
};

extern "C" des_vid vid;

extern "C" void show_cursor(int offset);

bool vid_init()
{
	des_vid *p_des = &vid;
	if ( (p_des->mutex = sem_ini(1)) == 0) {
		flog(LOG_ERR, "vid: impossibile creare mutex");
		return false;
	}
	for (int i = 0; i < VIDEO_SIZE; i++) 
		p_des->video[i] = ' ' | p_des->attr << 8;
	show_cursor(p_des->y * COLS + p_des->x);
	flog(LOG_INFO, "vid: video inizializzato");
	return true;
}

void scroll(des_vid *p_des)
{
	for (int i = 0; i < VIDEO_SIZE - COLS; i++) 
		p_des->video[i] = p_des->video[i + COLS];
	for (int i = 0; i < COLS; i++)
		p_des->video[VIDEO_SIZE - COLS + i] = ' ' | p_des->attr << 8;
	p_des->y--;
}


extern "C" void c_writevid(char c)
{
	des_vid *p_des = &vid;
	sem_wait(p_des->mutex);
	switch (c) {
	case '\r':
		p_des->x = 0;
		break;
	case '\n':
		p_des->y++;
		if (p_des->y >= ROWS)
			scroll(p_des);
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
	show_cursor(p_des->y * COLS + p_des->x);
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
extern "C" int cmain(void)
{
	int error;

	fill_io_gates();
	if (!kbd_init())
		return -1;
	if (!com_init())
		return -3;
	if (!vid_init())
		return -4;

	return 0;
}
