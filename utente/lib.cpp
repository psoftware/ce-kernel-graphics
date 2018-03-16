#include <lib.h>
#include <sys.h>

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
	//writeconsole/readconsole sono state eliminate.
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

extern "C" natl end;

extern "C" void lib_init()
{
	unsigned long long end_ = (unsigned long long)&end;
	mem_mutex = sem_ini(1);
	end_ = (end_ + DIM_PAGINA - 1) & ~(DIM_PAGINA - 1);
	heap_init((void *)end_, DIM_USR_HEAP);
}
