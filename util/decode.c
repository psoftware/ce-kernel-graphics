#include <stdio.h>
#include <stdlib.h>

#include "costanti.h"
#include "nucleo.h"

int get_bit() {
	static int nbit = 0;
	static unsigned int bitmap = 0;
	int bit;
	if (nbit == 0) {
		fread(&bitmap, sizeof(bitmap), 1, stdin);
		nbit = sizeof(bitmap) * 8;
	}
	bit = bitmap & 1;
	bitmap >>= 1;
	nbit--;
	return bit;
}

const char* seg_des(unsigned int seg)
{
	switch (seg) {
		case SEL_CODICE_SISTEMA:
			return "CODICE_SISTEMA";
		case SEL_DATI_SISTEMA:
			return "DATI_SISTEMA";
		case SEL_CODICE_UTENTE:
			return "CODICE_UTENTE";
		case SEL_DATI_UTENTE:
			return "DATI_UTENTE";
		default:
			return "UNKNOWN";
	}
}

const char* priv(natl pl)
{
	switch (pl) {
		case LIV_SISTEMA:
			return "LIV_SISTEMA";
		case LIV_UTENTE:
			return "LIV_UTENTE";
		default:
			return "UNKNOWN";
	}
}

void print_desp(des_proc* desp)
{
	printf("\tesp0 ss0: %08x %s\n", desp->esp0, seg_des(desp->ss0));
	printf("\tcr3: %d\n", desp->cr3);
	printf("\teax: %08x ebx: %08x ecx: %08x edx: %08x\n", desp->eax, desp->ebx, desp->ecx, desp->edx);
	printf("\tesp: %08x ebp: %08x esi: %08x edi: %08x\n", desp->esp, desp->ebp, desp->esi, desp->edi);
	printf("\tss: %s\n", seg_des(desp->ss));
	printf("\tds: %s\n", seg_des(desp->ds));
	printf("\tcpl: %s\n", priv(desp->cpl));
}


int main(int argc, char* argv[])
{
	union descrittore_pagina p;
	struct superblock_t sb;
	char buf[512];
	int record = 0;
	int count[2];
	int bit, last_bit;

	if (argc != 2) {
		fprintf(stderr, "Uso: %s [p|s|b]\n", argv[0]);
		exit(1);
	}
	switch (argv[1][0]) {
	case 'p':
		while (fread(&p, sizeof(p), 1, stdin)) {
			printf("%4d: P=%d RW=%d US=%d block=%d\n",
				record++, p.a.P, p.a.RW, p.a.US, p.a.block);
		}
		break;
	case 's':
		fread(buf, 512, 1, stdin);
		fread(&sb, sizeof(sb), 1, stdin);
		printf("magic: %c%c%c%c\n", sb.magic[0], sb.magic[1], sb.magic[2], sb.magic[3]);
		printf("bm_start: %d\n", sb.bm_start);
		printf("blocks: %d\n", sb.blocks);
		printf("io:\n");
		print_desp(&sb.io);
		printf("io_end: %x\n", sb.io_end);
		printf("utente:\n");
		print_desp(&sb.utente);
		printf("utente_end: %x\n", sb.utente_end);
		printf("checksum: %u\n", sb.checksum);
		break;
	case 'b':
		record = 0;
		last_bit = -1;
		while(record <= 4096*32) {
			if (record < 4096*32 && (bit = get_bit()) == last_bit)
				count[bit]++;
			else {
				if (last_bit >= 0) {
					printf("%6d+%6d: %s\n", record-count[last_bit], count[last_bit],
							last_bit ? "liberi" : "occupati");
					count[last_bit] = 0;
				}
				last_bit = bit;
				count[last_bit] = 1;
			}
			record++;
		}
		break;
	default:
		fprintf(stderr, "Tipo non riconosciuto: '%s'\n", argv[1]);
		exit(1);
	}
	return 0;
}
