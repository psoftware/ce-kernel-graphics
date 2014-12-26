#include <stdint.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "costanti.h"
#include "interp.h"
#include "swap.h"

class verbose {
	bool v;
public:
	void set_verbose(bool v_) {
		v = v_;
	}

	template<class T> verbose& operator << (const T& t) {
		if (v) {
			std::cout << t;
		}
		return *this;
	}
};

verbose log;

const uint32_t UPB = DIM_PAGINA / sizeof(uint32_t);
const uint32_t BPU = sizeof(uint32_t) * 8;

const uint32_t fine_utente_condiviso = (NTAB_SIS_C + NTAB_SIS_P + NTAB_MIO_C + NTAB_USR_C) * DIM_MACROPAGINA;

union entrata {
	// caso di pagina presente
	struct {
		// byte di accesso
		uint64_t P:		1;	// bit di presenza
		uint64_t RW:	1;	// Read/Write
		uint64_t US:	1;	// User/Supervisor
		uint64_t PWT:	1;	// Page Write Through
		uint64_t PCD:	1;	// Page Cache Disable
		uint64_t A:		1;	// Accessed
		uint64_t D:		1;	// Dirty
		uint64_t PAT:	1;	// non visto a lezione
		// fine byte di accesso
		
		uint64_t global:	1;	// non visto a lezione
		uint64_t avail:	3;	// non usati

		uint64_t address:	40;	// indirizzo fisico

		uint64_t avail2:	11;
		uint64_t NX:	1;
	} p;
	// caso di pagina assente
	struct {
		// informazioni sul tipo di pagina
		uint64_t P:	1;
		uint64_t RW:	1;
		uint64_t US:	1;
		uint64_t PWT:	1;
		uint64_t PCD:	1;
		uint64_t resvd:	7;

		uint64_t block:	51;

		uint64_t NX:	1;
	} a;	
};

struct tabella {
	entrata e[512];
};

struct pagina {
	union {
		uint8_t byte[DIM_PAGINA];
		uint64_t parole_lunghe[DIM_PAGINA / sizeof(uint64_t)];
	};
};

int i_tabella(uint64_t ind_virt, int liv)
{
	int shift = 12 + (liv - 1) * 9;
	uint64_t mask = 0x1ffUL << shift;
	return (ind_virt & mask) >> shift;
}


tabella *tabella_puntata(entrata *e)
{
	return reinterpret_cast<tabella*>(e->p.address << 12);
}

pagina* pagina_puntata(entrata* e) {
	return reinterpret_cast<pagina*>(e->p.address << 12);
}

struct bm_t {
	uint64_t *vect;
	uint64_t size;
};

inline uint64_t bm_isset(bm_t *bm, uint64_t pos)
{
	return !(bm->vect[pos / 64] & (1UL << (pos % 64)));
}

inline void bm_set(bm_t *bm, uint64_t pos)
{
	bm->vect[pos / 64] &= ~(1UL << (pos % 64));
}

inline void bm_clear(bm_t *bm, uint64_t pos)
{
	bm->vect[pos / 64] |= (1UL << (pos % 64));
}

void bm_create(bm_t *bm, uint64_t *buffer, uint64_t size)
{
	bm->vect = buffer;
	bm->size = size;
	uint64_t vecsize = size / BPU + (size % BPU ? 1 : 0);

	for(int i = 0; i < vecsize; ++i)
		bm->vect[i] = 0;
}


bool bm_alloc(bm_t *bm, uint64_t& pos)
{
	int i, l;

	i = 0;
	while(i < bm->size && bm_isset(bm, i)) i++;

	if (i == bm->size)
		return false;

	bm_set(bm, i);
	pos = i;
	//log << "allocated block " << pos << "\n";
	return true;
}

void bm_free(bm_t *bm, uint64_t pos)
{
	bm_clear(bm, pos);
}

#define CHECKSW(f, b, d) \
	do { \
		if (!swap->f(b, d)) { \
			std::cerr << "blocco " << b << ": " #f " fallita\n";\
			exit(EXIT_FAILURE);\
		}\
	} while(0)

superblock_t superblock;
tabella tab[5];
bm_t blocks;
pagina pag;
pagina zero_pag;
Swap* swap = NULL;

class TabCache {
	bool dirty[3];
	bool valid[3];
	block_t block[3];
public:
	
	TabCache() {
		for (int i = 0; i < 3; i++)
			valid[i] = false;
	}

	~TabCache() {
		for (int i = 0; i < 3; i++) {
			if (valid[i] && dirty[i]) {
				CHECKSW(scrivi_blocco, block[i], &tab[i + 1]);
			}
		}
	}

	block_t nuova(int liv) {
		block_t b;
		int i = liv - 1;

		if (valid[i] && dirty[i]) {
			CHECKSW(scrivi_blocco, block[i], &tab[liv]);
		}
		memset(&tab[liv], 0, sizeof(tabella));
	
		if (! bm_alloc(&blocks, b) ) {
			fprintf(stderr, "spazio insufficiente nello swap\n");
			exit(EXIT_FAILURE);
		}
		valid[i] = true;
		dirty[i] = true;
		block[i] = b;
		return b;
	}

	void leggi(int liv, block_t blocco) {
		int i = liv - 1;
		if (valid[i]) {
			if (blocco == block[i])
				return;
			if (dirty[i])
				CHECKSW(scrivi_blocco, block[i], &tab[liv]);
		}
		CHECKSW(leggi_blocco, blocco, &tab[liv]);
		block[i] = blocco;
		valid[i] = true;
		dirty[i] = false;
	}
	void scrivi(int liv) {
		dirty[liv - 1] = true;
	}
};


void do_map(char* fname, int liv, uint64_t& entry_point, uint64_t& last_address)
{
	FILE* file;
	TabCache c;
	entrata *e[5];

	if ( !(file = fopen(fname, "rb")) ) {
		perror(fname);
		exit(EXIT_FAILURE);
	}



	Eseguibile *exe = NULL;
	ListaInterpreti* interpreti = ListaInterpreti::instance();
	interpreti->rewind();
	while (interpreti->ancora()) {
		exe = interpreti->prossimo()->interpreta(file);
		if (exe) break;
	}
	if (!exe) {
		fprintf(stderr, "Formato del file '%s' non riconosciuto\n", fname);
		exit(EXIT_FAILURE);
	}

	entry_point = exe->entry_point();

	
	// dall'intestazione, calcoliamo l'inizio della tabella dei segmenti di programma
	last_address = 0;
	Segmento *s = NULL;
	while (s = exe->prossimo_segmento()) {
		uint64_t ind_virtuale = s->ind_virtuale();
		uint64_t dimensione = s->dimensione();
		uint64_t end_addr = ind_virtuale + dimensione;

		log << "==> seg dim " << std::hex << dimensione << " addr " <<
			ind_virtuale << "\n";

		if (end_addr > last_address)
			last_address = end_addr;

		ind_virtuale &= 0xfffffffffffff000;
		end_addr = (end_addr + 0x0000000000000fff) & 0xfffffffffffff000;
		for (; ind_virtuale < end_addr; ind_virtuale += sizeof(pagina))
		{
			log << "    addr " << std::hex << ind_virtuale << std::dec << "\n";
			block_t b;
			for (int l = 4; l > 1; l--) {
				int i = i_tabella(ind_virtuale, l);
				e[l] = &tab[l].e[i];
				log << "       T" << l << "[" << i << "] ->";
				if (e[l]->a.block == 0) {
					b = c.nuova(l - 1);
					log << " NEW";
					e[l]->a.block = b;
					e[l]->a.PWT   = 0;
					e[l]->a.PCD   = 0;
					e[l]->a.RW    = 1;
					e[l]->a.US    = liv;
					e[l]->a.P     = 0;
				} else {
					c.leggi(l - 1, e[l]->a.block);
				}
				log << " T" << (l - 1) << " at " << e[l]->a.block << "\n";
			}

			int i = i_tabella(ind_virtuale, 1);
			e[1] = &tab[1].e[i];
			log << "       T1[" << i << "] ->";
			if (e[1]->a.block == 0) {
				if (! bm_alloc(&blocks, b) ) {
					fprintf(stderr, "%s: spazio insufficiente nello swap\n", fname);
					exit(EXIT_FAILURE);
				}
				e[1]->a.block = b;
				log << " NEW";
			} else {
				CHECKSW(leggi_blocco, e[1]->a.block, &pag);
			}
			if (s->pagina_di_zeri()) {
				CHECKSW(scrivi_blocco, e[1]->a.block, &zero_pag);
				log << " zero";
			} else {
				s->copia_pagina(&pag);
				CHECKSW(scrivi_blocco, e[1]->a.block, &pag);
			}
			log << " page at " << e[1]->a.block << "\n";
			e[1]->a.PWT = 0;
			e[1]->a.PCD = 0;
			e[1]->a.RW |= s->scrivibile();
			e[1]->a.US |= liv;
			c.scrivi(1);
			s->prossima_pagina();
		}

	}
	fclose(file);
}




int main(int argc, char* argv[])
{
	if (argc >= 2 && std::string(argv[1]) == "-v") {
		log.set_verbose(true);
		argc--;
		argv++;
	}

	if (argc < 3) {
		fprintf(stderr, "Utilizzo: %s [-v] <swap> <modulo io> <modulo utente>\n", argv[0]);
		exit(EXIT_FAILURE);
	}


	ListaTipiSwap* tipiswap = ListaTipiSwap::instance();
	tipiswap->rewind();
	while (tipiswap->ancora()) {
		swap = tipiswap->prossimo()->apri(argv[1]);
		if (swap) break;
	}
	if (!swap) {
		fprintf(stderr, "Dispositivo swap '%s' non riconosciuto\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	long dim = swap->dimensione() / DIM_PAGINA;
	int nlong = dim / BPU + (dim % BPU ? 1 : 0);
	int nbmblocks = nlong / UPB + (nlong % UPB ? 1 : 0);
	
	bm_create(&blocks, new uint64_t[nbmblocks * UPB], dim);

	for (int i = 0; i < dim; i++)
		bm_free(&blocks, i);

	for (int i = 0; i <= nbmblocks + 1; i++)
		bm_set(&blocks, i);

	superblock.bm_start = 1;
	superblock.blocks = dim;
	superblock.directory = nbmblocks + 1;

	memset(&tab[4], 0, sizeof(tabella));

	log << "Loading " << argv[2] << "\n";
	uint64_t last_address;
	do_map(argv[2], 0, superblock.io_entry, last_address);
	superblock.io_end = last_address;

	log << "Loading " << argv[3] << "\n";
	do_map(argv[3], 1, superblock.user_entry, last_address);
	superblock.user_end = last_address;

	// le tabelle condivise per lo heap:
	log << "==> HEAP dim " << std::hex << DIM_USR_HEAP << " addr " <<
			last_address << "\n";
	TabCache c;
	for (uint64_t addr = last_address; addr < last_address + DIM_USR_HEAP; addr += sizeof(pagina)) {
		entrata *e[5];
		block_t b;
		log << "    addr " << std::hex << addr << std::dec << "\n";

		for (int l = 4; l > 1; l--) {
			block_t b;
			int i = i_tabella(addr, l);
			e[l] = &tab[l].e[i];
			log << "       T" << l << "[" << i << "] ->";
			if (e[l]->a.block == 0) {
				b = c.nuova(l - 1);
				log << " NEW";
				e[l]->a.block = b;
				e[l]->a.PWT   = 0;
				e[l]->a.PCD   = 0;
				e[l]->a.RW    = 1;
				e[l]->a.US    = 1;
				e[l]->a.P     = 0;
			} else {
				c.leggi(l - 1, e[l]->a.block);
			}
			log << " T" << (l - 1) << " at " << e[l]->a.block << "\n";
		}

		int i = i_tabella(addr, 1);
		e[1] = &tab[1].e[i];
		log << "       T1[" << i << "] ->";
		if (e[1]->a.block == 0) {
			if (! bm_alloc(&blocks, b) ) {
				std::cerr << argv[1] << ": spazio insufficiente nello swap\n";
				exit(EXIT_FAILURE);
			}
			e[1]->a.block = b;
			CHECKSW(scrivi_blocco, e[1]->a.block, &zero_pag);
			log << " NEW zero";
		} 
		log << " page at " << e[1]->a.block << "\n";
		e[1]->a.PWT = 0;
		e[1]->a.PCD = 0;
		e[1]->a.RW |= 1;
		e[1]->a.US |= 1;
		c.scrivi(1);

	}
		
	superblock.magic[0] = 'C';
	superblock.magic[1] = 'E';
	superblock.magic[2] = '6';
	superblock.magic[3] = '4';
	superblock.magic[4] = 'S';
	superblock.magic[5] = 'W';
	superblock.magic[6] = 'A';
	superblock.magic[7] = 'P';

	int *w = (int*)&superblock, sum = 0;
	for (int i = 0; i < sizeof(superblock) / sizeof(int) - 1; i++)
		sum += w[i];
	superblock.checksum = -sum;

	if ( !swap->scrivi_superblocco(superblock) ||
	     !swap->scrivi_bitmap(blocks.vect, nbmblocks) ||
	     !swap->scrivi_blocco(superblock.directory, &tab[4]))
	{
		fprintf(stderr, "errore nella creazione dello swap\n");
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}


	
	
