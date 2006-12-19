#if __GNUC__ >= 3 && !defined(WIN)
	#include <cstdio>
	#include <cstdlib>
	#include <cstring>

	using namespace std;
#else
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

#include "costanti.h"
#include "interp.h"
#include "swap.h"

typedef unsigned int uint;

const uint UPB = SIZE_PAGINA / sizeof(uint);
const uint BPU = sizeof(uint) * 8;


// converte v in un indirizzo
inline
void* addr(unsigned int v) {
	return reinterpret_cast<void*>(v);
}

// converte un indirizzo in un unsigned int
inline
unsigned int a2i(void* v) {
	return reinterpret_cast<unsigned int>(v);
}

struct descrittore_pagina {
	// byte di accesso
	unsigned int P:		1;	// bit di presenza
	unsigned int RW:	1;	// Read/Write
	unsigned int US:	1;	// User/Supervisor
	unsigned int PWT:	1;	// Page Write Through
	unsigned int PCD:	1;	// Page Cache Disable
	unsigned int A:		1;	// Accessed
	unsigned int D:		1;	// Dirty
	unsigned int pgsz:	1;	// non visto a lezione
	unsigned int global:	1;	// non visto a lezione
	// fine byte di accesso

	unsigned int avail:	3;	// non usato

	unsigned int address:	20;	// indirizzo fisico/blocco
};


typedef descrittore_pagina descrittore_tabella;

struct direttorio {
	descrittore_tabella entrate[1024];
}; 

struct tabella_pagine {
	descrittore_pagina  entrate[1024];
};

struct pagina {
	union {
		unsigned char byte[SIZE_PAGINA];
		unsigned int  parole_lunghe[SIZE_PAGINA / sizeof(unsigned int)];
	};
};

short indice_direttorio(uint indirizzo) {
	return (indirizzo & 0xffc00000) >> 22;
}

short indice_tabella(uint indirizzo) {
	return (indirizzo & 0x003ff000) >> 12;
}

tabella_pagine* tabella_puntata(descrittore_tabella* pdes_tab) {
	return reinterpret_cast<tabella_pagine*>(pdes_tab->address << 12);
}

pagina* pagina_puntata(descrittore_pagina* pdes_pag) {
	return reinterpret_cast<pagina*>(pdes_pag->address << 12);
}

struct bm_t {
	unsigned int *vect;
	unsigned int size;
};

inline unsigned int bm_isset(bm_t *bm, unsigned int pos)
{
	return !(bm->vect[pos / 32] & (1UL << (pos % 32)));
}

inline void bm_set(bm_t *bm, unsigned int pos)
{
	bm->vect[pos / 32] &= ~(1UL << (pos % 32));
}

inline void bm_clear(bm_t *bm, unsigned int pos)
{
	bm->vect[pos / 32] |= (1UL << (pos % 32));
}

void bm_create(bm_t *bm, unsigned int *buffer, unsigned int size)
{
	bm->vect = buffer;
	bm->size = size;
	unsigned int vecsize = size / BPU + (size % BPU ? 1 : 0);

	for(int i = 0; i < vecsize; ++i)
		bm->vect[i] = 0;
}


bool bm_alloc(bm_t *bm, unsigned int& pos)
{
	int i, l;

	i = 0;
	while(i < bm->size && bm_isset(bm, i)) i++;

	if (i == bm->size)
		return false;

	bm_set(bm, i);
	pos = i;
	return true;
}

void bm_free(bm_t *bm, unsigned int pos)
{
	bm_clear(bm, pos);
}

#define CHECKSW(f, b, d)	do { if (!swap->f(b, d)) { fprintf(stderr, "blocco %d: " #f " fallita\n", b); exit(EXIT_FAILURE); } } while(0)

superblock_t superblock;
direttorio main_dir;
bm_t blocks;
tabella_pagine tab;
pagina pag;

void do_map(Swap *swap, char* fname, int liv, void*& entry_point, uint& last_address)
{
	FILE* exe;

	if ( !(exe = fopen(fname, "rb")) ) {
		perror(fname);
		exit(EXIT_FAILURE);
	}


	descrittore_tabella* pdes_tab;
	tabella_pagine* ptabella;
	descrittore_pagina* pdes_pag;

	Eseguibile *e = NULL;
	ListaInterpreti* interpreti = ListaInterpreti::instance();
	interpreti->rewind();
	while (interpreti->ancora()) {
		e = interpreti->prossimo()->interpreta(exe);
		if (e) break;
	}
	if (!e) {
		fprintf(stderr, "Formato del file '%s' non riconosciuto\n", fname);
		exit(EXIT_FAILURE);
	}

	entry_point = e->entry_point();

	
	// dall'intestazione, calcoliamo l'inizio della tabella dei segmenti di programma
	last_address = 0;
	Segmento *s = NULL;
	while (s = e->prossimo_segmento()) {
		uint ind_virtuale = s->ind_virtuale();
		uint dimensione = s->dimensione();
		uint end_addr = ind_virtuale + dimensione;

		if (end_addr > last_address) 
			last_address = end_addr;

		ind_virtuale &= 0xfffff000;
		end_addr = (end_addr + 0x00000fff) & 0xfffff000;
		for (; ind_virtuale < end_addr; ind_virtuale += sizeof(pagina))
		{
			block_t b;
			pdes_tab = &main_dir.entrate[indice_direttorio(ind_virtuale)];
			if (pdes_tab->address == 0) {
				memset(&tab, 0, sizeof(tabella_pagine));
			
				if (! bm_alloc(&blocks, b) ) {
					fprintf(stderr, "%s: spazio insufficiente nello swap\n", fname);
					exit(EXIT_FAILURE);
				}

				pdes_tab->address = b;
				pdes_tab->RW	  = 1;
				pdes_tab->US	  = liv;
				pdes_tab->P	   = 1;
			} else {
				CHECKSW(leggi_blocco, pdes_tab->address, &tab);
			}

			pdes_pag = &tab.entrate[indice_tabella(ind_virtuale)];
			if (!s->finito()) {
				if (pdes_pag->address == 0) {
					if (! bm_alloc(&blocks, b) ) {
						fprintf(stderr, "%s: spazio insufficiente nello swap\n", fname);
						exit(EXIT_FAILURE);
					}
					pdes_pag->address = b;
				} else {
					CHECKSW(leggi_blocco, pdes_pag->address, &pag);
				}
				s->copia_prossima_pagina(&pag);
				CHECKSW(scrivi_blocco, pdes_pag->address, &pag);
			} 
			pdes_pag->RW |= s->scrivibile();
			pdes_pag->US |= liv;
			pdes_pag->P  |= (1 - liv);
			CHECKSW(scrivi_blocco, pdes_tab->address, &tab);
		}

	}
	fclose(exe);
}




int main(int argc, char* argv[])
{
	Swap *swap = NULL;

	if (argc < 3) {
		fprintf(stderr, "Utilizzo: %s <swap> <modulo io> <modulo utente>\n", argv[0]);
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

	long dim = swap->dimensione() / SIZE_PAGINA;
	int nlong = dim / BPU + (dim % BPU ? 1 : 0);
	int nbmblocks = nlong / UPB + (nlong % UPB ? 1 : 0);
	
	bm_create(&blocks, new uint[nbmblocks * UPB], dim);

	for (int i = 0; i < dim; i++)
		bm_free(&blocks, i);

	for (int i = 0; i <= nbmblocks + 1; i++)
		bm_set(&blocks, i);

	superblock.bm_start = 1;
	superblock.blocks = dim;
	superblock.directory = nbmblocks + 1;

	memset(&main_dir, 0, sizeof(direttorio));

	uint last_address;
	do_map(swap, argv[2], 0, superblock.io_entry, last_address);
	superblock.io_end = addr(last_address);

	do_map(swap, argv[3], 1, superblock.user_entry, last_address);
	superblock.user_end = addr(last_address);

	// le tabelle condivise per lo heap:
	// - prima, eventuali descrittori di pagina nell'ultima tabella 
	// utilizzata:
	descrittore_tabella *pdes_tab = &main_dir.entrate[indice_direttorio(last_address)];
	if (pdes_tab->P) {
		tabella_pagine tab;
		CHECKSW(leggi_blocco, pdes_tab->address, &tab);
		for (int i = indice_tabella(last_address) + 1; i < 1024; i++) {
			descrittore_pagina *pdes_pag = &tab.entrate[i];
			pdes_pag->address = 0;
			pdes_pag->US	  = 1;
			pdes_pag->RW	  = 1;
			pdes_pag->P	  = 0;
		}
		CHECKSW(scrivi_blocco, pdes_tab->address, &tab);
	}
	// - quindi, i rimanenti descrittori di tabella:
	for (int i = indice_direttorio(last_address) + 1;
		 i < indice_direttorio(a2i(fine_utente_condiviso));
		 i++)
	{
		descrittore_tabella* pdes_tab = &main_dir.entrate[i];
		pdes_tab->address = 0;
		pdes_tab->US	  = 1;
		pdes_tab->RW	  = 1;
		pdes_tab->P       = 1;
	}
		
	superblock.magic[0] = 'C';
	superblock.magic[1] = 'E';
	superblock.magic[2] = 'S';
	superblock.magic[3] = 'W';
	if ( !swap->scrivi_superblocco(superblock) ||
	     !swap->scrivi_bitmap(blocks.vect, nbmblocks) ||
	     !swap->scrivi_blocco(superblock.directory, &main_dir))
	{
		fprintf(stderr, "errore nella creazione dello swap\n");
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}


	
	
