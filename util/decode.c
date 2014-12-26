#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

union entrata {
	// caso di pagina presente
	struct {
		// byte di accesso
		uint64_t P:		1;	// bit di presenza
		uint64_t RW:		1;	// Read/Write
		uint64_t US:		1;	// User/Supervisor
		uint64_t PWT:		1;	// Page Write Through
		uint64_t PCD:		1;	// Page Cache Disable
		uint64_t A:		1;	// Accessed
		uint64_t D:		1;	// Dirty
		uint64_t PAT:		1;	// non visto a lezione
		// fine byte di accesso
		
		uint64_t global:	1;	// non visto a lezione
		uint64_t avail:		3;	// non usati

		uint64_t address:	40;	// indirizzo fisico

		uint64_t avail2:	11;
		uint64_t NX:		1;
	} p;
	// caso di pagina assente
	struct {
		// informazioni sul tipo di pagina
		uint64_t P:		1;
		uint64_t RW:		1;
		uint64_t US:		1;
		uint64_t PWT:		1;
		uint64_t PCD:		1;
		uint64_t resvd:		7;

		uint64_t block:		51;

		uint64_t NX:		1;
	} a;	
};

typedef uint64_t block_t;

struct superblock_t {
	int8_t		magic[8];
	block_t		bm_start;
	uint64_t	blocks;
	block_t		directory;
	uint64_t	user_entry;
	uint64_t	user_end;
	uint64_t	io_entry;
	uint64_t	io_end;
	uint64_t	checksum;
};


int get_bit() {
	static int nbit = 0;
	static uint64_t bitmap = 0;
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

int main(int argc, char* argv[])
{
	union entrata *p;
	uint64_t tmp;
	struct superblock_t sb;
	char buf[4096];
	int record = 0;
	int count[2];
	int bit, last_bit;
	int i, *w, sum;

	if (argc != 2) {
		fprintf(stderr, "Uso: %s [p|s|b]\n", argv[0]);
		exit(1);
	}
	switch (argv[1][0]) {
	case 'p':
	case 'P':
		for (record = 0; fread(&tmp, sizeof(tmp), 1, stdin); record++) {
			if (argv[1][0] == 'P' && !tmp)
				continue;
			p = (union entrata*)&tmp;
			printf("%4d: P=%d RW=%d US=%d block=%lu",
				record, p->a.P, p->a.RW, p->a.US, (unsigned long)p->a.block);
			printf(p->a.PWT? " PWT": "    ");
			printf(p->a.PCD? " PCD": "    ");
			printf(p->a.NX?  " NX" : "    ");
			printf("\n");
		}
		break;
	case 's':
		fread(buf, 512, 1, stdin);
		fread(&sb, sizeof(sb), 1, stdin);
		printf("magic:     %c%c%c%c%c%c%c%c\n",
				sb.magic[0], sb.magic[1], sb.magic[2], sb.magic[3],
				sb.magic[4], sb.magic[5], sb.magic[6], sb.magic[7]);
		printf("bm_start:  %ld\n", sb.bm_start);
		printf("blocks:    %ld\n", sb.blocks);
		printf("tab4:      %ld\n", sb.directory);
		printf("usr_entry: %016lx\n", sb.user_entry);
		printf("usr_end:   %016lx\n", sb.user_end);
		printf("io_entry:  %016lx\n", sb.io_entry);
		printf("io_end:    %016lx\n", sb.io_end);
		printf("checksum:  %lu", sb.checksum);
		w = (int*)&sb;
		sum = 0;
		for (i = 0; i < sizeof(sb) / sizeof(int); i++)
			sum += w[i];
		printf(" (%s)\n", sum? "BAD" : "OK");
		break;
	case 'b':
		record = 0;
		last_bit = -1;
		while(record <= 4096*64) {
			if (record < 4096*64 && (bit = get_bit()) == last_bit)
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
