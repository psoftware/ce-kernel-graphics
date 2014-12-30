#include <lib.h>
#include <sys.h>

char* copy(const char* src, char* dst) {
       while (*src)
               *dst++ = *src++;
       *dst = '\0';
       return dst;
}

char* convint(int n, char* out)
{
       char buf[12];
       int i = 11;
       bool neg = false;

       if (n == 0) 
               return copy("0", out);

       buf[i--] = '\0';

       if (n < 0) {
               n = -n;
               neg = true;
       }
       while (n > 0) {
               buf[i--] = n % 10 + '0';
               n = n / 10;
       }
       if (neg)
               buf[i--] = '-';
       return copy(buf + i + 1, out);
}


int printf(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, 1024, fmt, ap);
	va_end(ap);

	writeconsole(buf);

	return l;
}

int sprintf(char *buf, natq size, const char *fmt, ...)
{
	va_list ap;
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, size, fmt, ap);
	va_end(ap);

	return l;
}

char pause_buf[1];
natl pause_len = 1;
void pause()
{
#ifndef AUTOCORR
	writeconsole("Premere INVIO per continuare");
	readconsole(pause_buf, pause_len);
#endif
}


extern "C" void panic(const char* msg)
{
	flog(LOG_WARN, "%s", msg);
	terminate_p();
}


natl mem_mutex;

void* mem_alloc(natl dim)
{
	void *p;

	sem_wait(mem_mutex);
	p = alloca(dim);
	sem_signal(mem_mutex);

	return p;
}


void mem_free(void* p)
{
	sem_wait(mem_mutex);
	dealloca(p);
	sem_signal(mem_mutex);
}

extern "C" void do_log(log_sev sev, const char* msg, natl size)
{
	log(sev, msg, size);
}

extern "C" natl end;

extern "C" void lib_init()
{
	mem_mutex = sem_ini(1);
	heap_init(&end, DIM_USR_HEAP);
}
