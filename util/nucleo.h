#ifndef NUCLEO_H_
#define NUCLEO_H_

#include "tipo.h"

union descrittore_pagina {
	// caso di pagina presente
	struct {
		// byte di accesso
		natl P:		1;	// bit di presenza
		natl RW:	1;	// Read/Write
		natl US:	1;	// User/Supervisor
		natl PWT:	1;	// Page Write Through
		natl PCD:	1;	// Page Cache Disable
		natl A:		1;	// Accessed
		natl D:		1;	// Dirty
		natl pgsz:	1;	// non visto a lezione
		// fine byte di accesso
		
		natl global:	1;	// non visto a lezione
		natl avail:	3;	// non usati

		natl address:	20;	// indirizzo fisico
	} p;
	// caso di pagina assente
	struct {
		// informazioni sul tipo di pagina
		natl P:		1;
		natl RW:	1;
		natl US:	1;
		natl PWT:	1;
		natl PCD:	1;

		natl block:	27;
	} a;	
};

struct des_proc {			// offset:
//	int nome;
	natl link;		// 0
//	addr punt_nucleo;
	addr        esp0;		// 4
//	addr riservato;
	natl ss0;		// 8
	addr        esp1;		// 12
	natl ss1;		// 16
	addr        esp2;		// 20
	natl ss2;		// 24

//	addr cr3;
	addr cr3;		// 28

//	natl contesto[N_REG];
	natl eip;		// 32, non utilizzato
	natl eflags;		// 36, non utilizzato

	natl eax;		// 40
	natl ecx;		// 44
	natl edx;		// 48
	natl ebx;		// 52
	natl esp;		// 56
	natl ebp;		// 60
	natl esi;		// 64
	natl edi;		// 68

	natl es;		// 72
	natl cs; 		// 76
	natl ss;		// 80
	natl ds;		// 84
	natl fs;		// 86
	natl gs;		// 90
	natl ldt;		// 94, non utilizzato

	natl io_map;		// 100, non utilizzato

	struct {
		natl cr;	// 104
		natl sr;	// 108
		natl tr;	// 112
		natl ip;	// 116
		natl is;	// 120
		natl op;	// 124
		natl os;	// 128
		char regs[80];		// 132
	} fpu;

	natb cpl;			// 212
}; // dimensione 212 + 1 + 3(allineamento) = 216

typedef unsigned int block_t;

struct superblock_t {
	char	magic[4];
	block_t	bm_start;
	int	blocks;
	des_proc io;
	void*   io_end;
	des_proc utente;
	void*   utente_end;
	unsigned int	checksum;
};

#endif
