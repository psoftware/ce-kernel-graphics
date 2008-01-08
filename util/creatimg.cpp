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
#include "nucleo.h"
#include "swap.h"


const natl UPB = DIM_PAGINA / sizeof(natl);
const natl BPU = sizeof(natl) * 8;


// converte v in un indirizzo
inline
void* toaddr(natl v) {
	return reinterpret_cast<void*>(v);
}

// converte un indirizzo in un natl
inline
natl a2i(void* v) {
	return reinterpret_cast<natl>(v);
}


typedef descrittore_pagina descrittore_tabella;

struct direttorio {
	descrittore_tabella entrate[1024];
}; 

struct tabella_pagine {
	descrittore_pagina  entrate[1024];
};

struct pagina {
	union {
		unsigned char byte[DIM_PAGINA];
		natl  parole_lunghe[DIM_PAGINA / sizeof(natl)];
	};
};

short indice_direttorio(natl indirizzo) {
	return (indirizzo & 0xffc00000) >> 22;
}

short indice_tabella(natl indirizzo) {
	return (indirizzo & 0x003ff000) >> 12;
}

tabella_pagine* tabella_puntata(descrittore_tabella* pdes_tab) {
	return reinterpret_cast<tabella_pagine*>(pdes_tab->p.address << 12);
}

pagina* pagina_puntata(descrittore_pagina* pdes_pag) {
	return reinterpret_cast<pagina*>(pdes_pag->p.address << 12);
}

struct bm_t {
	natl *vect;
	natl size;
};

inline natl bm_isset(bm_t *bm, natl pos)
{
	return !(bm->vect[pos / 32] & (1UL << (pos % 32)));
}

inline void bm_set(bm_t *bm, natl pos)
{
	bm->vect[pos / 32] &= ~(1UL << (pos % 32));
}

inline void bm_clear(bm_t *bm, natl pos)
{
	bm->vect[pos / 32] |= (1UL << (pos % 32));
}

void bm_create(bm_t *bm, natl *buffer, natl size)
{
	bm->vect = buffer;
	bm->size = size;
	natl vecsize = size / BPU + (size % BPU ? 1 : 0);

	for(int i = 0; i < vecsize; ++i)
		bm->vect[i] = 0;
}


bool bm_alloc(bm_t *bm, block_t& pos)
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

void bm_free(bm_t *bm, natl pos)
{
	bm_clear(bm, pos);
}

#define CHECKSW(f, b, d)	do { if (!swap->f(b, d)) { fprintf(stderr, "blocco %d: " #f " fallita\n", b); exit(EXIT_FAILURE); } } while(0)

superblock_t superblock;
bm_t blocks;
direttorio dir;
pagina pag;
tabella_pagine tab;
Swap* swap = NULL;

class TabCache {
	bool dirty;
	bool valid;
	block_t block;
public:
	
	TabCache() {
		valid = false;
	}

	~TabCache() {
		if (valid && dirty) {
			CHECKSW(scrivi_blocco, block, &tab);
		}
	}

	block_t nuova() {
		block_t b;

		if (valid && dirty) {
			CHECKSW(scrivi_blocco, block, &tab);
		}
		memset(&tab, 0, sizeof(tabella_pagine));
	
		if (! bm_alloc(&blocks, b) ) {
			fprintf(stderr, "spazio insufficiente nello swap\n");
			exit(EXIT_FAILURE);
		}
		valid = true;
		dirty = true;
		block = b;
		return b;
	}

	void leggi(block_t blocco) {
		if (valid) {
			if (blocco == block)
				return;
			if (dirty)
				CHECKSW(scrivi_blocco, block, &tab);
		}
		CHECKSW(leggi_blocco, blocco, &tab);
		block = blocco;
		valid = true;
		dirty = false;
	}
	void scrivi() {
		dirty = true;
	}
};

descrittore_pagina* crea_pila_utente(direttorio* pdir)
{
	natl j = indice_direttorio(inizio_utente_privato),
	     js = j + ntab_utente_privato - 1;
	block_t b, b1;
	descrittore_tabella *pdes_tab;
	tabella_pagine tab;
	for (pdes_tab = &pdir->entrate[j]; pdes_tab < &pdir->entrate[js]; pdes_tab++) {
		pdes_tab->a.block = 0;
		pdes_tab->a.PWT   = 0;
		pdes_tab->a.PCD   = 0;
		pdes_tab->a.RW	  = 1;
		pdes_tab->a.US	  = 1;
		pdes_tab->a.P	  = 0;
	}
	if (! bm_alloc(&blocks, b) ) {
		fprintf(stderr, "pila utente: spazio insufficiente nello swap\n");
		exit(EXIT_FAILURE);
	}
	pdes_tab->a.block = b;
	pdes_tab->a.PWT   = 0;
	pdes_tab->a.PCD   = 0;
	pdes_tab->a.RW	  = 1;
	pdes_tab->a.US	  = 1;
	pdes_tab->a.P	  = 0;
	memset(&tab, 0, sizeof(tabella_pagine));
	j = indice_tabella(fine_utente_privato - DIM_PAGINA);
	descrittore_pagina *pdes_pag = &tab.entrate[j];
	if (! bm_alloc(&blocks, b1) ) {
		fprintf(stderr, "pila utente: spazio insufficiente nello swap\n");
		exit(EXIT_FAILURE);
	}
	pdes_pag->a.block = b1;
	pdes_pag->a.PWT   = 0;
	pdes_pag->a.PCD   = 0;
	pdes_pag->a.RW	  = 1;
	pdes_pag->a.US	  = 1;
	pdes_pag->a.P	  = 0;
	CHECKSW(scrivi_blocco, b, &tab);
	return pdes_pag;
}

descrittore_pagina* crea_pila_sistema(direttorio *pdir)
{
	natl j = indice_direttorio(inizio_sistema_privato),
	     js = j + ntab_sistema_privato - 1;
	block_t b, b1;
	descrittore_tabella *pdes_tab;
	tabella_pagine tab;
	for (pdes_tab = &pdir->entrate[j]; pdes_tab < &pdir->entrate[js]; pdes_tab++) {
		pdes_tab->a.block = 0;
		pdes_tab->a.PWT   = 0;
		pdes_tab->a.PCD   = 0;
		pdes_tab->a.RW	  = 1;
		pdes_tab->a.US	  = 0;
		pdes_tab->a.P	  = 0;
	}
	if (! bm_alloc(&blocks, b) ) {
		fprintf(stderr, "pila utente: spazio insufficiente nello swap\n");
		exit(EXIT_FAILURE);
	}
	pdes_tab->a.block = b;
	pdes_tab->a.PWT   = 0;
	pdes_tab->a.PCD   = 0;
	pdes_tab->a.RW	  = 1;
	pdes_tab->a.US	  = 0;
	pdes_tab->a.P	  = 1;
	memset(&tab, 0, sizeof(tabella_pagine));
	j = indice_tabella(fine_sistema_privato - DIM_PAGINA);
	descrittore_pagina *pdes_pag = &tab.entrate[j];
	if (! bm_alloc(&blocks, b1) ) {
		fprintf(stderr, "pila utente: spazio insufficiente nello swap\n");
		exit(EXIT_FAILURE);
	}
	pdes_pag->a.block = b1;
	pdes_pag->a.PWT   = 0;
	pdes_pag->a.PCD   = 0;
	pdes_pag->a.RW	  = 1;
	pdes_pag->a.US	  = 0;
	pdes_pag->a.P	  = 1;
	CHECKSW(scrivi_blocco, b, &tab);
	return pdes_pag;
}

void crea_processo(natl f, int liv, des_proc* pdes_proc, direttorio* pdir)
{
	memset(pdes_proc, 0, sizeof(des_proc));

	descrittore_pagina *pila_sistema = crea_pila_sistema(pdir);

	if (liv == LIV_UTENTE) {
		memset(&pag, 0, sizeof(pagina));
		pag.parole_lunghe[1019] = f;			// EIP (codice utente)
		pag.parole_lunghe[1020] = SEL_CODICE_UTENTE;	// CS (codice utente)
		pag.parole_lunghe[1021] = 0x00000200;		// EFLAG
		pag.parole_lunghe[1022] = fine_utente_privato - 2 * sizeof(int); // ESP (pila utente)
		pag.parole_lunghe[1023] = SEL_DATI_UTENTE;	// SS (pila utente)
		CHECKSW(scrivi_blocco, pila_sistema->a.block, &pag);
		descrittore_pagina *pila_utente = crea_pila_utente(pdir);
		memset(&pag, 0, sizeof(pagina));
		pag.parole_lunghe[1022] = 0xffffffff;	// ind. di ritorno non sign.
		pag.parole_lunghe[1023] = 0;		// parametro del proc.
		CHECKSW(scrivi_blocco, pila_utente->a.block, &pag);
		pdes_proc->esp0 = toaddr(fine_sistema_privato);
		pdes_proc->ss0  = SEL_DATI_SISTEMA;
		pdes_proc->esp = fine_sistema_privato - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_UTENTE;
		pdes_proc->es  = SEL_DATI_UTENTE;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->cpl = LIV_UTENTE;
	} else {
		memset(&pag, 0, sizeof(pagina));
		pag.parole_lunghe[1019] = f;		  	// EIP (codice sistema)
		pag.parole_lunghe[1020] = SEL_CODICE_SISTEMA;  // CS (codice sistema)
		pag.parole_lunghe[1021] = 0x00000200;  	// EFLAG
		pag.parole_lunghe[1022] = 0xffffffff;		// indirizzo ritorno?
		pag.parole_lunghe[1023] = 0;			// parametro
		CHECKSW(scrivi_blocco, pila_sistema->a.block, &pag);
		pdes_proc->esp = fine_sistema_privato - 5 * sizeof(int);
		pdes_proc->ss  = SEL_DATI_SISTEMA;

		pdes_proc->ds  = SEL_DATI_SISTEMA;
		pdes_proc->es  = SEL_DATI_SISTEMA;

		pdes_proc->fpu.cr = 0x037f;
		pdes_proc->fpu.tr = 0xffff;
		pdes_proc->cpl = LIV_SISTEMA;
	}
}

natl do_map(char* fname, int liv, direttorio* pdir, natl& last_address)
{
	FILE* exe;
	TabCache tabc;

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

	// dall'intestazione, calcoliamo l'inizio della tabella dei segmenti di programma
	last_address = 0;
	Segmento *s = NULL;
	while (s = e->prossimo_segmento()) {
		natl ind_virtuale = s->ind_virtuale();
		natl dimensione = s->dimensione();
		natl end_addr = ind_virtuale + dimensione;

		if (end_addr > last_address) 
			last_address = end_addr;

		ind_virtuale &= 0xfffff000;
		end_addr = (end_addr + 0x00000fff) & 0xfffff000;
		for (; ind_virtuale < end_addr; ind_virtuale += sizeof(pagina))
		{
			block_t b;
			pdes_tab = &pdir->entrate[indice_direttorio(ind_virtuale)];
			if (pdes_tab->a.block == 0) {
				b = tabc.nuova();
				pdes_tab->a.block = b;
				pdes_tab->a.PWT   = 0;
				pdes_tab->a.PCD   = 0;
				pdes_tab->a.RW	  = 1;
				pdes_tab->a.US	  = liv;
				pdes_tab->a.P	  = 1;
			} else {
				tabc.leggi(pdes_tab->a.block);
			}

			pdes_pag = &tab.entrate[indice_tabella(ind_virtuale)];
			if (!s->pagina_di_zeri()) {
				if (pdes_pag->a.block == 0) {
					if (! bm_alloc(&blocks, b) ) {
						fprintf(stderr, "%s: spazio insufficiente nello swap\n", fname);
						exit(EXIT_FAILURE);
					}
					pdes_pag->a.block = b;
				} else {
					CHECKSW(leggi_blocco, pdes_pag->a.block, &pag);
				}
				s->copia_pagina(&pag);
				CHECKSW(scrivi_blocco, pdes_pag->a.block, &pag);
			} 
			pdes_pag->a.PWT = 0;
			pdes_pag->a.PCD = 0;
			pdes_pag->a.RW |= s->scrivibile();
			pdes_pag->a.US |= liv;
			pdes_pag->a.P  |= (1 - liv);
			tabc.scrivi();
			s->prossima_pagina();
		}

	}
	fclose(exe);
	return (natl)e->entry_point();
}




int main(int argc, char* argv[])
{
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

	long dim = swap->dimensione() / DIM_PAGINA;
	int nlong = dim / BPU + (dim % BPU ? 1 : 0);
	int nbmblocks = nlong / UPB + (nlong % UPB ? 1 : 0);
	
	bm_create(&blocks, new natl[nbmblocks * UPB], dim);

	for (int i = 0; i < dim; i++)
		bm_free(&blocks, i);

	for (int i = 0; i <= nbmblocks; i++)
		bm_set(&blocks, i);

	superblock.bm_start = 1;
	superblock.blocks = dim;


	natl last_address, f;
	block_t b;
	if (! bm_alloc(&blocks, b) ) {
		fprintf(stderr, "%s: spazio insufficiente nello swap (direttorio)\n", argv[2]);
		exit(EXIT_FAILURE);
	}
	memset(&dir, 0, sizeof(direttorio));
	f = do_map(argv[2], LIV_SISTEMA, &dir, last_address);
	superblock.io_end = toaddr(last_address);
	crea_processo(f, LIV_SISTEMA,  &superblock.io, &dir);
	superblock.io.cr3 = (direttorio*)b;
	CHECKSW(scrivi_blocco, b, &dir);

	if (! bm_alloc(&blocks, b) ) {
		fprintf(stderr, "%s: spazio insufficiente nello swap (direttorio)\n", argv[3]);
		exit(EXIT_FAILURE);
	}
	memset(&dir, 0, sizeof(direttorio));
	f = do_map(argv[3], LIV_UTENTE, &dir, last_address);
	superblock.utente_end = toaddr(last_address);
	crea_processo(f, LIV_UTENTE,  &superblock.utente, &dir);
	superblock.utente.cr3 = (direttorio*)b;
	CHECKSW(scrivi_blocco, b, &dir);

	// le tabelle condivise per lo heap:
	// - prima, eventuali descrittori di pagina nell'ultima tabella 
	// utilizzata:
	descrittore_tabella *pdes_tab = &dir.entrate[indice_direttorio(last_address)];
	if (pdes_tab->a.P) {
		tabella_pagine tab;
		CHECKSW(leggi_blocco, pdes_tab->a.block, &tab);
		int primo_indice = indice_tabella(last_address) + (last_address % DIM_PAGINA ? 1 : 0);
		for (int i = primo_indice; i < 1024; i++) {
			descrittore_pagina *pdes_pag = &tab.entrate[i];
			pdes_pag->a.block = 0;
			pdes_pag->a.PWT   = 0;
			pdes_pag->a.PCD   = 0;
			pdes_pag->a.US	  = 1;
			pdes_pag->a.RW	  = 1;
			pdes_pag->a.P	  = 0;
		}
		CHECKSW(scrivi_blocco, pdes_tab->a.block, &tab);
	}
	// - quindi, i rimanenti descrittori di tabella:
	for (int i = indice_direttorio(last_address) + 1;
		 i < indice_direttorio(fine_utente_condiviso);
		 i++)
	{
		descrittore_tabella* pdes_tab = &dir.entrate[i];
		pdes_tab->a.block = 0;
		pdes_tab->a.PWT   = 0;
		pdes_tab->a.PCD   = 0;
		pdes_tab->a.US	  = 1;
		pdes_tab->a.RW	  = 1;
		pdes_tab->a.P     = 1;
	}
	CHECKSW(scrivi_blocco, (natl)superblock.utente.cr3, &dir);
		
	superblock.magic[0] = 'C';
	superblock.magic[1] = 'E';
	superblock.magic[2] = 'S';
	superblock.magic[3] = 'W';
	if ( !swap->scrivi_superblocco(superblock) ||
	     !swap->scrivi_bitmap(blocks.vect, nbmblocks)
	   ) {
		fprintf(stderr, "errore nella creazione dello swap\n");
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}


	
	
