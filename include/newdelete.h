#ifndef NEWDELETE_H_
#define NEWDELETE_H_

void* mem_alloc(natl dim);
void mem_free(void* p);

void * operator new(long unsigned int n)
{
	void * res = mem_alloc(n);
	if(res == 0)
		flog(LOG_ERR, "HEAP: allocazione fallita");
	return res;
}

void* operator new[](long unsigned int n)
{
	void * res = mem_alloc(n);
        if(res == 0)
                flog(LOG_ERR, "HEAP: allocazione fallita");
	return res;
}

void operator delete(void* obj)
{
	mem_free(obj);
}

void operator delete[](void* obj)
{
	mem_free(obj);
}

void operator delete(void *obj, unsigned long)
{
	mem_free(obj);
}

#endif
