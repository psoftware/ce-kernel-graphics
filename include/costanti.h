#define MAX_SEM			4096

#define SEL_CODICE_SISTEMA	8
#define SEL_DATI_SISTEMA 	16
#define SEL_CODICE_UTENTE	27
#define SEL_DATI_UTENTE 	35
#define	PRES			0b10000000
#define NON_P			0b00000000
#define	SEG_CODICE		0b00011010
#define SEG_DATI		0b00010010
#define	SEG_TSS			0b00001001
#define G_PAGINA		0b10000000
#define G_BYTE			0b00000000
#define LIV_UTENTE		3
#define LIV_SISTEMA		0

#define DIM_PAGINA		4096
#define DIM_SUPERPAGINA		(DIM_PAGINA * 1024)
#define DIM_DESP		216
#define DIM_DESS		8
#define BYTE_SEM		(DIM_DESS * MAX_SEM)
#define DELAY			59659		
#define DEFAULT_HEAP_SIZE	(1024*1024)

#define MAX_PRIORITY		0xfffffff
#define MIN_PRIORITY		0x0000001
#define DUMMY_PRIORITY		0x0000000

#define TIPO_A			0x42
#define TIPO_T			0x43
#define TIPO_SI			0x44
#define TIPO_W			0x45
#define TIPO_S			0x46
#define TIPO_MA			0x47
#define TIPO_MF			0x48
#define TIPO_D			0x49
#define TIPO_RL			0x4a
#define TIPO_RE			0x4b
#define TIPO_EP			0x4c

#define TIPO_APE		0x52
#define TIPO_NWFI		0x53
#define TIPO_FG			0x54
#define TIPO_P			0x55
#define TIPO_AB			0x56
#define TIPO_L			0x57

#define IO_TIPO_HDR		0x62
#define IO_TIPO_HDW		0x63

#define IO_TIPO_RSEN		0x72
#define IO_TIPO_RSELN		0x73
#define IO_TIPO_WSEN		0x74
#define IO_TIPO_WSE0		0x75
#define IO_TIPO_RCON		0x76
#define IO_TIPO_WCON		0x77
#define IO_TIPO_INIC		0x78

#define DIM_BLOCK 512			// Dimensione del blocco (HD)

#define D_ERR_NONE 0x00
#define D_ERR_BOUNDS 0xFF
#define D_ERR_PRESENCE 0xFE
#define D_ERR_GENERIC  0xFD

#define NTAB_SIS_C		256	// 1GiB
#define NTAB_SIS_P		1	// 4MiB
#define NTAB_MIO_C		255	// 1GiB - 4MiB
#define NTAB_USR_C		256	// 1GiB
#define NTAB_USR_P		256	// 1GiB

#ifndef ASM
const unsigned int T_STAT = 4;

const unsigned int LOG_MSG_SIZE = 80;
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR };

struct log_msg {
	log_sev sev;
	short	identifier;
	char msg[LOG_MSG_SIZE + 1];
};

#ifdef WIN
	typedef long unsigned int size_t;
#else
	typedef unsigned int size_t;
#endif

#endif
