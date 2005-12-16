#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "costanti.h"
#include "elf.h"

using namespace std;

typedef unsigned int block_t;
typedef unsigned int uint;

const uint UPB = SIZE_PAGINA / sizeof(uint);

inline
void* add(const void* a, unsigned int v) {
	v += reinterpret_cast<unsigned int>(a);
	return reinterpret_cast<void*>(v);
};

// sottrae v byte all'indirizzo a
inline
void* sub(const void* a, unsigned int v) {
	unsigned int a1 = reinterpret_cast<unsigned int>(a) - v;
	return reinterpret_cast<void*>(a1);
}

// calcola il numero di byte tra a1 (incluso)
// e a2 (escluso)
inline
int distance(const void* a1, const void* a2) {
	return reinterpret_cast<unsigned int>(a1) - 
	       reinterpret_cast<unsigned int>(a2);
}

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
	unsigned int P:		1;	// bit di presenza
	unsigned int RW:	1;	// Read/Write
	unsigned int US:	1;	// User/Supervisor
	unsigned int PWT:	1;	// Page Write Through
	unsigned int PCD:	1;	// Page Cache Disable
	unsigned int A:		1;	// Accessed
	unsigned int D:		1;	// Dirty
	unsigned int pgsz:	1;	// non visto a lezione
	unsigned int global:	1;	// non visto a lezione
	unsigned int avail:	1;	// non usati
	unsigned int preload:   1;	// pag. da precaricare
	unsigned int azzera:    1;	// ottimizzazione pagine iniz. vuote
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
	unsigned int vecsize = size / sizeof(int) + (size % sizeof(int) ? 1 : 0);

	for(int i = 0; i < vecsize; ++i)
		bm->vect[i] = 0xffffffff;
}


bool bm_alloc(bm_t *bm, unsigned int& pos)
{
	int i, l;

	i = 0;
	while(i <= bm->size && bm_isset(bm, i)) i++;

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

struct superblock_t {
	char	bootstrap[512];
	block_t	bm_start;
	int	blocks;
	block_t	directory;
	void*   entry_point;
} superblock;

bool elf32_check(Elf32_Ehdr* elf_h) {

	// i primi 4 byte devono contenere un valore prestabilito
	if (!(elf_h->e_ident[EI_MAG0] == ELFMAG0 &&
	      elf_h->e_ident[EI_MAG1] == ELFMAG1 &&
	      elf_h->e_ident[EI_MAG2] == ELFMAG2 &&
	      elf_h->e_ident[EI_MAG2] == ELFMAG2))
		return false;

	if (!(elf_h->e_ident[EI_CLASS] == ELFCLASS32  &&  // 32 bit
	      elf_h->e_ident[EI_DATA]  == ELFDATA2LSB &&  // little endian
	      elf_h->e_type	       == ET_EXEC     &&  // eseguibile
	      elf_h->e_machine 	       == EM_386))	  // per Intel x86
		return false;

	return true;
}

void scrivi_blocco(FILE* img, block_t b, void* buf)
{
	if ( fseek(img, b * SIZE_PAGINA, SEEK_SET) < 0 ||
	     fwrite(buf, sizeof(pagina), 1, img) < 1)
	{
		fprintf(stderr, "errore nella scrittura dello swap\n");
		exit(EXIT_FAILURE);
	}
}

void leggi_blocco(FILE* img, block_t b, void* buf)
{
	if ( fseek(img, b * SIZE_PAGINA, SEEK_SET) < 0 ||
	     fread(buf, sizeof(pagina), 1, img) < 1)
	{
		fprintf(stderr, "errore nella lettura dallo swap\n");
		exit(EXIT_FAILURE);
	}
}
	
FILE *exe, *img;
bool image_created = false;


void rmimage()
{
	if (!image_created) 
		remove("swap");
}

int main(int argc, char* argv[]) {

	bm_t blocks;
	direttorio main_dir;
	tabella_pagine tab;
	pagina pag;

	if (argc < 2) {
		fprintf(stderr, "Utilizzo: %s <dimensione> <eseguibile> ...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int  dim = atoi(argv[1]);

	int nlong = dim / sizeof(uint) + (dim % sizeof(uint) ? 1 : 0);
	bm_create(&blocks, new uint[nlong], dim);

	int nbmblocks = nlong / UPB + (nlong % UPB ? 1 : 0);
	for (int i = 0; i <= nbmblocks + 1; i++)
		bm_set(&blocks, i);

	superblock.bm_start = 1;
	superblock.blocks = dim;
	superblock.directory = nbmblocks + 1;


	if ( !(exe = fopen(argv[2], "r")) ) {
		perror(argv[2]);
		exit(EXIT_FAILURE);
	}

	if ( !(img = fopen("swap", "w+")) ) {
		perror("swap");
		exit(EXIT_FAILURE);
	}

	atexit(rmimage);

	if ( fseek(img, dim * SIZE_PAGINA - 1, SEEK_SET) < 0 ||
	     fwrite("\0", 1, 1, img) < 1) 
	{
		fprintf(stderr, "errore nella creazione dello swap\n");
		exit(EXIT_FAILURE);
	}

	descrittore_tabella* pdes_tab;
	tabella_pagine* ptabella;
	descrittore_pagina* pdes_pag;

	Elf32_Ehdr* elf_h = new Elf32_Ehdr;

	if (fread(elf_h, sizeof(Elf32_Ehdr), 1, exe) < 1 || !elf32_check(elf_h)) {
		fprintf(stderr, "%s non ELF\n", argv[2]);
		exit(EXIT_FAILURE);
	}
	superblock.entry_point = elf_h->e_entry;

	memset(&main_dir, 0, sizeof(direttorio));
	
	// dall'intestazione, calcoliamo l'inizio della tabella dei segmenti di programma
	char* buf = new char[elf_h->e_phnum * elf_h->e_phentsize];
	if ( fseek(exe, elf_h->e_phoff, SEEK_SET) < 0 ||
	     fread(buf, elf_h->e_phentsize, elf_h->e_phnum, exe) < elf_h->e_phnum)
	{
		fprintf(stderr, "Fine prematura del file ELF\n");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < elf_h->e_phnum; i++) {
		Elf32_Phdr* elf_ph = (Elf32_Phdr*)(buf + elf_h->e_phentsize * i);


		// ci interessano solo i segmenti di tipo PT_LOAD
		// (corrispondenti alle sezioni .text e .data)
		if (elf_ph->p_type != PT_LOAD)
			continue;
		
		// un flag del descrittore ci dice se il segmento deve essere
		// scrivibile
		unsigned int scrivibile = (elf_ph->p_flags & PF_W ? 1 : 0);

		uint start_off = elf_ph->p_offset & ~(SIZE_PAGINA - 1);
		if ( fseek(exe, start_off, SEEK_SET) < 0) {
			fprintf(stderr, "errore nel file ELF\n");
			exit(EXIT_FAILURE);
		}
		uint da_leggere = elf_ph->p_filesz + (elf_ph->p_offset - start_off);
		int preload = 0;
		for (uint  ind_virtuale = uint(elf_ph->p_vaddr);
		           ind_virtuale < uint(elf_ph->p_vaddr) + elf_ph->p_memsz;
	                   ind_virtuale += SIZE_PAGINA)
		{
			
			block_t b;
			pdes_tab = &main_dir.entrate[indice_direttorio(ind_virtuale)];
			if (pdes_tab->address == 0) {
				memset(&tab, 0, sizeof(tabella_pagine));
			
				if (! bm_alloc(&blocks, b) ) {
					fprintf(stderr, "spazio insufficiente nello swap\n");
					exit(EXIT_FAILURE);
				}

				pdes_tab->address = b;
				pdes_tab->RW	  = 1;
				pdes_tab->US	  = 1;
			} else {
				leggi_blocco(img, pdes_tab->address, &tab);
			}

			pdes_pag = &tab.entrate[indice_tabella(ind_virtuale)];
			if (da_leggere > 0) {
				if (! bm_alloc(&blocks, b) ) {
					fprintf(stderr, "spazio insufficiente nello swap\n");
					exit(EXIT_FAILURE);
				}
				memset(&pag, 0, sizeof(pagina));
				int curr = (da_leggere > sizeof(pagina) ? sizeof(pagina) : da_leggere);
				if ( fread(&pag, 1, curr, exe) < curr ) {
					fprintf(stderr, "errore nella lettura dal file ELF\n");
					exit(EXIT_FAILURE);
				}
				da_leggere -= curr;
				pdes_pag->address = b;
				scrivi_blocco(img, pdes_pag->address, &pag);
			} else {
				pdes_pag->azzera = 1;
			}
			pdes_pag->RW = scrivibile;
			pdes_pag->US = 1;
			if (scrivibile && preload < 2) {
				pdes_pag->preload = 1;
				preload++;
			}
			scrivi_blocco(img, pdes_tab->address, &tab);
		}

	}
	if ( fseek(img, 0, SEEK_SET) < 0 ||
	     fwrite(&superblock, sizeof(superblock), 1, img) < 0 ||
	     fseek(img, SIZE_PAGINA, SEEK_SET) < 0 ||
	     fwrite(blocks.vect, SIZE_PAGINA, nbmblocks, img) < nbmblocks ||
	     fwrite(&main_dir, sizeof(direttorio), 1, img) < 1 )
	{
		fprintf(stderr, "errore nella creazione dello swap\n");
		exit(EXIT_FAILURE);
	}
	fclose(img);
	fclose(exe);
	image_created = true;
	return EXIT_SUCCESS;
}


	
	
