#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "costanti.h"
#include "elf.h"
#include "dos.h"
#include "coff.h"

using namespace std;

typedef unsigned int block_t;
typedef unsigned int uint;

const uint UPB = SIZE_PAGINA / sizeof(uint);


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

	unsigned int avail:	2;	// non usato
	unsigned int preload:	1;	// la pag. deve essere precaricata

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

struct superblock_t {
	char	magic[4];
	block_t	bm_start;
	int	blocks;
	block_t	directory;
	void*   entry_point;
	void*   end;
	unsigned int	checksum;
} superblock;

// interfaccia generica ai tipi di file eseguibile
class Segmento {
public:
	virtual bool scrivibile() const = 0;
	virtual uint ind_virtuale() const = 0;
	virtual uint dimensione() const = 0;
	virtual bool finito() const = 0;
	virtual bool copia_prossima_pagina(pagina* dest) = 0;
	virtual ~Segmento() {}
};

class Eseguibile {
public:
	virtual Segmento* prossimo_segmento() = 0;
	virtual void* entry_point() const = 0;
	virtual bool prossima_resident(uint& start, uint& dim) = 0;
	virtual ~Eseguibile() {}
};

class ListaInterpreti;

class Interprete {
public:
	Interprete();
	virtual Eseguibile* interpreta(FILE* exe) = 0;
	virtual ~Interprete() {}
};

class ListaInterpreti {
	
public:
	static ListaInterpreti* instance();
	void aggiungi(Interprete* in) { testa = new Elem(in, testa); }
	void rewind() { curr = testa; }
	bool ancora() { return curr != NULL; }
	Interprete* prossimo();
private:
	static ListaInterpreti* instance_;
	ListaInterpreti() : testa(NULL), curr(NULL) {}
	
	struct Elem {
		Interprete* in;
		Elem* next;

		Elem(Interprete* in_, Elem* next_ = NULL):
			in(in_), next(next_)
		{}
	} *testa, *curr;


};

ListaInterpreti* ListaInterpreti::instance_ = NULL;

ListaInterpreti* ListaInterpreti::instance()
{
	if (!instance_) {
		instance_ = new ListaInterpreti();
	}
	return instance_;
}

Interprete* ListaInterpreti::prossimo()
{
	Interprete *in = NULL;
	if (curr) {
		in = curr->in;
		curr = curr->next;
	}
	return in;
}

Interprete::Interprete()
{
	ListaInterpreti::instance()->aggiungi(this);
}


// interprete per formato elf
class InterpreteElf32: public Interprete {
public:
	InterpreteElf32();
	~InterpreteElf32() {}
	virtual Eseguibile* interpreta(FILE* pexe);
};

InterpreteElf32::InterpreteElf32()
{}

class EseguibileElf32: public Eseguibile {
	FILE *pexe;
	Elf32_Ehdr h;
	char *seg_buf;
	char *sec_buf;
	char *strtab;
	uint resident_start;
	uint resident_size;
	int curr_seg;
	int curr_resident;

	class SegmentoElf32: public Segmento {
		EseguibileElf32 *padre;
		Elf32_Phdr* ph;
		uint curr_offset;
		uint da_leggere;
	public:
		SegmentoElf32(EseguibileElf32 *padre_, Elf32_Phdr* ph_);
		virtual bool scrivibile() const;
		virtual uint ind_virtuale() const;
		virtual uint dimensione() const;
		virtual bool finito() const;
		virtual bool copia_prossima_pagina(pagina* dest);
		~SegmentoElf32() {}
	};

	friend class SegmentoElf32;
public:
	EseguibileElf32(FILE* pexe_);
	bool init();
	virtual Segmento* prossimo_segmento();
	virtual void* entry_point() const;
	virtual bool prossima_resident(uint& start, uint& dim);
	~EseguibileElf32();
};

EseguibileElf32::EseguibileElf32(FILE* pexe_)
	: pexe(pexe_), curr_seg(0), curr_resident(0),
	  seg_buf(NULL), sec_buf(NULL), strtab(NULL)
{}

	

bool EseguibileElf32::init()
{
	if (fseek(pexe, 0, SEEK_SET) != 0) 
		return false;

	if (fread(&h, sizeof(Elf32_Ehdr), 1, pexe) < 1)
		return false;

	// i primi 4 byte devono contenere un valore prestabilito
	if (!(h.e_ident[EI_MAG0] == ELFMAG0 &&
	      h.e_ident[EI_MAG1] == ELFMAG1 &&
	      h.e_ident[EI_MAG2] == ELFMAG2 &&
	      h.e_ident[EI_MAG2] == ELFMAG2))
		return false;

	if (!(h.e_ident[EI_CLASS] == ELFCLASS32  &&  // 32 bit
	      h.e_ident[EI_DATA]  == ELFDATA2LSB &&  // little endian
	      h.e_type            == ET_EXEC     &&  // eseguibile
	      h.e_machine         == EM_386))        // per Intel x86
		return false;

	// leggiamo la tabella dei segmenti
	seg_buf = new char[h.e_phnum * h.e_phentsize];
	if ( fseek(pexe, h.e_phoff, SEEK_SET) != 0 ||
	     fread(seg_buf, h.e_phentsize, h.e_phnum, pexe) < h.e_phnum)
	{
		fprintf(stderr, "Fine prematura del file ELF\n");
		exit(EXIT_FAILURE);
	}
	
	// dall'intestazione, calcoliamo l'inizio della tabella delle sezioni
	sec_buf = new char[h.e_shnum * h.e_shentsize];
	if ( fseek(pexe, h.e_shoff, SEEK_SET) != 0 ||
	     fread(sec_buf, h.e_shentsize, h.e_shnum, pexe) < h.e_shnum)
	{
		fprintf(stderr, "Fine prematura del file ELF\n");
		exit(EXIT_FAILURE);
	}
	// dobbiamo cariare anche la sezione contenente la tabella delle 
	// stringhe
	Elf32_Shdr* elf_strtab = (Elf32_Shdr*)(sec_buf + h.e_shentsize * h.e_shstrndx);
	strtab = new char[elf_strtab->sh_size];
	if ( fseek(pexe, elf_strtab->sh_offset, SEEK_SET) != 0 ||
	     fread(strtab, elf_strtab->sh_size, 1, pexe) < 1)
	{
		fprintf(stderr, "Errore nella lettura della tabella delle stringhe\n");
		exit(EXIT_FAILURE);
	}

	return true;
}

Segmento* EseguibileElf32::prossimo_segmento()
{
	while (curr_seg < h.e_phnum) {
		Elf32_Phdr* ph = (Elf32_Phdr*)(seg_buf + h.e_phentsize * curr_seg);
		curr_seg++;
		
		if (ph->p_type != PT_LOAD)
			continue;

		return new SegmentoElf32(this, ph);
	}
	return NULL;
}

void* EseguibileElf32::entry_point() const
{
	return h.e_entry;
}

bool EseguibileElf32::prossima_resident(uint& start, uint& dim)
{
	while (curr_resident < h.e_shnum) {
		Elf32_Shdr* sh = (Elf32_Shdr*)(sec_buf + h.e_shentsize * curr_resident);

		curr_resident++;

		if (strcmp("RESIDENT", strtab + sh->sh_name) != 0)
			continue;

		start = a2i(sh->sh_addr);
		dim = sh->sh_size;
		return true;
	}
	return false;
}

EseguibileElf32::~EseguibileElf32()
{
	delete[] seg_buf;
	delete[] sec_buf;
	delete[] strtab;
}

EseguibileElf32::SegmentoElf32::SegmentoElf32(EseguibileElf32* padre_, Elf32_Phdr* ph_)
	: padre(padre_), ph(ph_),
	  curr_offset(ph->p_offset & ~(SIZE_PAGINA - 1)),
	  da_leggere(ph->p_filesz + (ph->p_offset - curr_offset))
{}

bool EseguibileElf32::SegmentoElf32::scrivibile() const
{
	return (ph->p_flags & PF_W);
}

uint EseguibileElf32::SegmentoElf32::ind_virtuale() const
{
	return a2i(ph->p_vaddr);
}

uint EseguibileElf32::SegmentoElf32::dimensione() const
{
	return ph->p_memsz;
}

bool EseguibileElf32::SegmentoElf32::finito() const
{
	return (da_leggere <= 0);
}


bool EseguibileElf32::SegmentoElf32::copia_prossima_pagina(pagina* dest)
{
	if (finito())
		return false;

	if (fseek(padre->pexe, curr_offset, SEEK_SET) != 0) {
		fprintf(stderr, "errore nel file ELF\n");
		exit(EXIT_FAILURE);
	}

	memset(dest, 0, sizeof(pagina));
	int curr = (da_leggere > sizeof(pagina) ? sizeof(pagina) : da_leggere);
	if (fread(dest, 1, curr, padre->pexe) < curr) {
		fprintf(stderr, "errore nella lettura dal file ELF\n");
		exit(EXIT_FAILURE);
	}
	da_leggere -= curr;
	curr_offset += curr;
	return true;
}

Eseguibile* InterpreteElf32::interpreta(FILE* pexe)
{
	EseguibileElf32 *pe = new EseguibileElf32(pexe);

	if (pe->init())
		return pe;

	delete pe;
	return NULL;
}

void scrivi_blocco(FILE* img, block_t b, void* buf)
{
	if ( fseek(img, b * SIZE_PAGINA, SEEK_SET) != 0 ||
	     fwrite(buf, sizeof(pagina), 1, img) < 1)
	{
		fprintf(stderr, "errore nella scrittura dello swap\n");
		exit(EXIT_FAILURE);
	}
}

void leggi_blocco(FILE* img, block_t b, void* buf)
{
	if ( fseek(img, b * SIZE_PAGINA, SEEK_SET) != 0 ||
	     fread(buf, sizeof(pagina), 1, img) < 1)
	{
		fprintf(stderr, "errore nella lettura dallo swap\n");
		exit(EXIT_FAILURE);
	}
}

// interprete per formato coff-go32-exe
class InterpreteCoff_go32: public Interprete {
public:
	InterpreteCoff_go32();
	~InterpreteCoff_go32() {}
	virtual Eseguibile* interpreta(FILE* pexe);
};

InterpreteCoff_go32::InterpreteCoff_go32()
{}

class EseguibileCoff_go32: public Eseguibile {
	FILE *pexe;
	FILHDR h;
	AOUTHDR ah;
	unsigned int soff;
	char *seg_buf;
	uint resident_start;
	uint resident_size;
	int curr_seg;
	int curr_resident;

	class SegmentoCoff_go32: public Segmento {
		EseguibileCoff_go32 *padre;
		SCNHDR* ph;
		uint curr_offset;
		uint curr_vaddr;
		uint da_leggere;
	public:
		SegmentoCoff_go32(EseguibileCoff_go32 *padre_, SCNHDR* ph_);
		virtual bool scrivibile() const;
		virtual uint ind_virtuale() const;
		virtual uint dimensione() const;
		virtual bool finito() const;
		virtual bool copia_prossima_pagina(pagina* dest);
		~SegmentoCoff_go32() {}
	};

	friend class SegmentoCoff_go32;
public:
	EseguibileCoff_go32(FILE* pexe_);
	bool init();
	virtual Segmento* prossimo_segmento();
	virtual void* entry_point() const;
	virtual bool prossima_resident(uint& start, uint& dim);
	~EseguibileCoff_go32();
};

EseguibileCoff_go32::EseguibileCoff_go32(FILE* pexe_)
	: pexe(pexe_), curr_seg(0), curr_resident(0), seg_buf(NULL)
{}

	

bool EseguibileCoff_go32::init()
{
	DOS_EXE dos;

	if (fseek(pexe, 0, SEEK_SET) != 0)
		return false;

	if (fread(&dos, sizeof(DOS_EXE), 1, pexe) < 1)
		return false;

	if (dos.signature != DOS_MAGIC)
		return false;

	soff = dos.blocks_in_file * 512L;
	if (dos.bytes_in_last_block)
		  soff -= (512 - dos.bytes_in_last_block);

	if (fseek(pexe, soff, SEEK_SET) != 0) 
		return false;

	if (fread(&h, FILHSZ, 1, pexe) < 1)
		return false;

	// i primi 2 byte devono contenere un valore prestabilito
	if (h.f_magic != I386MAGIC)
		return false;

	if (!(h.f_flags & F_EXEC)) 
		return false;

	// leggiamo l'a.out header
	if (fread(&ah, 1, h.f_opthdr, pexe) < h.f_opthdr)
	{
		fprintf(stderr, "Fine prematura del file COFF-go32\n");
		exit(EXIT_FAILURE);
	}

	// controlliamo che sia consistente
	if (ah.magic != ZMAGIC)
		return false;

	// leggiamo la tabella delle sezioni
	seg_buf = new char[h.f_nscns * SCNHSZ];
	if (fread(seg_buf, SCNHSZ, h.f_nscns, pexe) < h.f_nscns)
	{
		fprintf(stderr, "Fine prematura del file COFF-go32\n");
		exit(EXIT_FAILURE);
	}
	
	return true;
}

Segmento* EseguibileCoff_go32::prossimo_segmento()
{
	while (curr_seg < h.f_nscns) {
		SCNHDR* ph = (SCNHDR*)(seg_buf + SCNHSZ * curr_seg);
		curr_seg++;

		if (ph->s_vaddr == 0)
			continue;
		
		return new SegmentoCoff_go32(this, ph);
	}
	return NULL;
}

void* EseguibileCoff_go32::entry_point() const
{
	return (void*)ah.entry;
}

bool EseguibileCoff_go32::prossima_resident(uint& start, uint& dim)
{
	while (curr_resident < h.f_nscns) {
		SCNHDR* sh = (SCNHDR*)(seg_buf + SCNHSZ * curr_resident);

		curr_resident++;

		if (strncmp("RESIDENT", sh->s_name, 8) != 0)
			continue;

		start = sh->s_vaddr;
		dim = sh->s_size;
		return true;
	}
	return false;
}

EseguibileCoff_go32::~EseguibileCoff_go32()
{
	delete[] seg_buf;
}

EseguibileCoff_go32::SegmentoCoff_go32::SegmentoCoff_go32(EseguibileCoff_go32* padre_, SCNHDR* ph_)
	: padre(padre_), ph(ph_),
	  curr_offset(ph->s_scnptr),
	  curr_vaddr(ph->s_vaddr),
	  da_leggere(ph->s_size)
{
}

bool EseguibileCoff_go32::SegmentoCoff_go32::scrivibile() const
{
	return (ph->s_flags & (STYP_DATA | STYP_BSS));
}

uint EseguibileCoff_go32::SegmentoCoff_go32::ind_virtuale() const
{
	return (ph->s_vaddr);
}

uint EseguibileCoff_go32::SegmentoCoff_go32::dimensione() const
{
	return ph->s_size;
}

bool EseguibileCoff_go32::SegmentoCoff_go32::finito() const
{
	return (da_leggere <= 0);
}


bool EseguibileCoff_go32::SegmentoCoff_go32::copia_prossima_pagina(pagina* dest)
{
	if (finito())
		return false;

	if (fseek(padre->pexe, padre->soff + curr_offset, SEEK_SET) != 0) {
		fprintf(stderr, "errore nel file COFF-go32\n");
		exit(EXIT_FAILURE);
	}

	uint line = curr_vaddr & 0x00000fff;

	size_t curr = (da_leggere > sizeof(pagina) - line ? sizeof(pagina) - line: da_leggere);
	if (ph->s_flags & STYP_BSS) {
		memset(dest->byte + line, 0, curr);
	} else {
		if (fread(dest->byte + line, 1, curr, padre->pexe) < curr) {
			fprintf(stderr, "errore nella lettura dal file COFF-go32\n");
			exit(EXIT_FAILURE);
		}
	}
	da_leggere -= curr;
	curr_offset += curr;
	curr_vaddr += curr;
	return true;
}

Eseguibile* InterpreteCoff_go32::interpreta(FILE* pexe)
{
	EseguibileCoff_go32 *pe = new EseguibileCoff_go32(pexe);

	if (pe->init())
		return pe;

	delete pe;
	return NULL;
}


InterpreteElf32		int_elf32;
InterpreteCoff_go32	int_coff_go32;
	

int main(int argc, char* argv[]) {

	bm_t blocks;
	direttorio main_dir;
	tabella_pagine tab;
	pagina pag;
	FILE *exe, *img;

	if (argc < 2) {
		fprintf(stderr, "Utilizzo: %s <swap> <eseguibile>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if ( !(img = fopen(argv[1], "rb+")) || 
	      (fseek(img, 0L, SEEK_END) != 0) )
	{
		perror(argv[1]);
		exit(EXIT_FAILURE);
	}

	long dim = ftell(img) / SIZE_PAGINA;

	int nlong = dim / sizeof(uint) + (dim % sizeof(uint) ? 1 : 0);
	bm_create(&blocks, new uint[nlong], dim);

	for (int i = 0; i < dim; i++)
		bm_free(&blocks, i);

	int nbmblocks = nlong / UPB + (nlong % UPB ? 1 : 0);
	for (int i = 0; i <= nbmblocks + 1; i++)
		bm_set(&blocks, i);

	superblock.bm_start = 1;
	superblock.blocks = dim;
	superblock.directory = nbmblocks + 1;


	if ( !(exe = fopen(argv[2], "rb")) ) {
		perror(argv[2]);
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
		fprintf(stderr, "Formato del file eseguibile non riconosciuto\n");
		exit(EXIT_FAILURE);
	}

	superblock.entry_point = e->entry_point();

	memset(&main_dir, 0, sizeof(direttorio));
	
	// dall'intestazione, calcoliamo l'inizio della tabella dei segmenti di programma
	uint last_address = 0;
	Segmento *s = NULL;
	while (s = e->prossimo_segmento()) {
		uint ind_virtuale = s->ind_virtuale();
		uint dimensione = s->dimensione();
		uint end_addr = ind_virtuale + dimensione;

		if (end_addr > last_address) 
			last_address = end_addr;

		for (; ind_virtuale < end_addr; ind_virtuale += sizeof(pagina))
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
			if (!s->finito()) {
				if (pdes_pag->address == 0) {
					if (! bm_alloc(&blocks, b) ) {
						fprintf(stderr, "spazio insufficiente nello swap\n");
						exit(EXIT_FAILURE);
					}
					pdes_pag->address = b;
				} else {
					leggi_blocco(img, pdes_pag->address, &pag);
				}
				s->copia_prossima_pagina(&pag);
				scrivi_blocco(img, pdes_pag->address, &pag);
			} 
			pdes_pag->RW |= s->scrivibile();
			pdes_pag->US |= 1;
			scrivi_blocco(img, pdes_tab->address, &tab);
		}

	}
	// ora settiamo il bit preload per tutte le pagine che devono contenere
	// la sezione RESIDENT

	// cerchiamo la nostra sezione
	uint start, size;
	while (e->prossima_resident(start, size)) {
		for (uint ind_virtuale = start;
			  ind_virtuale < start + size;
			  ind_virtuale += sizeof(pagina))
		{
			pdes_tab = &main_dir.entrate[indice_direttorio(ind_virtuale)];
			if (pdes_tab->address == 0) {
				fprintf(stderr, "Errore interno\n");
				exit(EXIT_FAILURE);
			} else {
				leggi_blocco(img, pdes_tab->address, &tab);
			}

			pdes_pag = &tab.entrate[indice_tabella(ind_virtuale)];
			pdes_pag->preload = 1;
			scrivi_blocco(img, pdes_tab->address, &tab);
		}
	}			
	
	// infine, le tabelle condivise per lo heap
	for (int i = indice_direttorio(last_address) + 1;
		 i < indice_direttorio(a2i(fine_utente_condiviso));
		 i++)
	{
		descrittore_pagina* pdes_pag = &main_dir.entrate[i];
		pdes_pag->address = 0;
		pdes_pag->US	 = 1;
		pdes_pag->RW	 = 1;
	}
		
	superblock.end = addr(last_address);
	superblock.magic[0] = 'C';
	superblock.magic[1] = 'E';
	superblock.magic[2] = 'S';
	superblock.magic[3] = 'W';
	if ( fseek(img, 512, SEEK_SET) != 0 ||
	     fwrite(&superblock, sizeof(superblock), 1, img) < 0 ||
	     fseek(img, SIZE_PAGINA, SEEK_SET) != 0 ||
	     fwrite(blocks.vect, SIZE_PAGINA, nbmblocks, img) < nbmblocks ||
	     fwrite(&main_dir, sizeof(direttorio), 1, img) < 1 )
	{
		fprintf(stderr, "errore nella creazione dello swap\n");
		exit(EXIT_FAILURE);
	}
	fclose(img);
	fclose(exe);
	return EXIT_SUCCESS;
}


	
	
