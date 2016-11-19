void* mem_alloc(natl dim);
void mem_free(void* p);

void * operator new(long unsigned int n)
{
	return mem_alloc(n);
}

void* operator new[](long unsigned int n)
{	
	return mem_alloc(n);
}

void operator delete(void* obj)
{
	mem_free(obj);
}

void operator delete[](void* obj)
{
	mem_free(obj);
}
