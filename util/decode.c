#include <stdio.h>

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

int main()
{
	struct descrittore_pagina p;
	int record = 0;

	while (fread(&p, sizeof(p), 1, stdin)) {
		printf("%4d: P=%d RW=%d US=%d preload=%d address=%d\n",
			record++, p.P, p.RW, p.US, p.preload, p.address);
	}
	return 0;
}
