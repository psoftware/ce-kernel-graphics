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

#define TIPO_A			0x30
#define TIPO_T			0x31
#define TIPO_SI			0x34
#define TIPO_W			0x35
#define TIPO_S			0x36
#define TIPO_MA			0x37
#define TIPO_MF			0x38
#define TIPO_D			0x39
#define TIPO_RL			0x3a
#define TIPO_R			0x3b
#define TIPO_EP			0x3c

#define TIPO_APE		0x40
#define TIPO_NWFI		0x41
#define TIPO_FG			0x44
#define TIPO_P			0x45
#define TIPO_AB			0x48
#define TIPO_L			0x49

#define IO_TIPO_RHDN		0x50
#define IO_TIPO_WHDN		0x51

#define IO_TIPO_RSEN		0x60
#define IO_TIPO_RSELN		0x61
#define IO_TIPO_WSEN		0x62
#define IO_TIPO_WSE0		0x63
#define IO_TIPO_RKBD		0x64
#define IO_TIPO_IKBD		0x65
#define IO_TIPO_WFIKBD		0x66
#define IO_TIPO_SKBD		0x67
#define IO_TIPO_SMON		0x68
#define IO_TIPO_WMON		0x69
#define IO_TIPO_WVID		IO_TIPO_WMON
#define IO_TIPO_CMON		0x6a
#define IO_TIPO_GMON		0x6b
#define IO_TIPO_LKBD		0x6c
#define IO_TIPO_KMON		0x6d
#define IO_TIPO_PKBD		0x6e
#define IO_TIPO_ICON		0x6f

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
