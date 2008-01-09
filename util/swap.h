#ifndef SWAP_H__
#define SWAP_H__
#if __GNUC__ >= 3 && !defined(WIN)
	#include <cstdlib>

	using namespace std;
#else
	#include <stdlib.h>
#endif

typedef unsigned int block_t;

struct superblock_t {
	char	magic[4];
	block_t	bm_start;
	int	blocks;
	block_t	directory;
	void*   user_entry;
	void*   user_end;
	void*	io_entry;
	void*   io_end;
	unsigned int	checksum;
};

// interfaccia generica agli swap
class Swap {
public:
	virtual unsigned int dimensione() const = 0;
	bool scrivi_superblocco(const superblock_t& sb);
	bool scrivi_bitmap(const void* vec, int nb);
	bool scrivi_blocco(block_t b, const void* blk);
	bool leggi_blocco(block_t b, void* blk);
	virtual ~Swap() {}
protected:
	virtual bool leggi(unsigned int off, void* buff, unsigned int size) = 0;
	virtual bool scrivi(unsigned int off, const void* buff, unsigned int size) = 0;
};

class ListaTipiSwap;

class TipoSwap {
public:
	TipoSwap();
	virtual Swap* apri(const char* nome) = 0;
	virtual ~TipoSwap() {};
};

class ListaTipiSwap {
	
public:
	static ListaTipiSwap* instance();
	void aggiungi(TipoSwap* in) { testa = new Elem(in, testa); }
	void rewind() { curr = testa; }
	bool ancora() { return curr != NULL; }
	TipoSwap* prossimo();
private:
	static ListaTipiSwap* instance_;
	ListaTipiSwap() : testa(NULL), curr(NULL) {}
	
	struct Elem {
		TipoSwap* in;
		Elem* next;

		Elem(TipoSwap* in_, Elem* next_ = NULL):
			in(in_), next(next_)
		{}
	} *testa, *curr;


};

#endif
