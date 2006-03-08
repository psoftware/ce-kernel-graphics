#define MAX_SEMAFORI		100

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

#define SIZE_PAGINA		4096
#define SIZE_SUPERPAGINA	(SIZE_PAGINA * 1024)
#define SIZE_DESP		216
#define SIZE_DESS		64
#define DELAY			59659		


#define TIPO_A			0x30
#define TIPO_T			0x31
#define TIPO_B			0x32
#define TIPO_G			0x33
#define TIPO_SI			0x34
#define TIPO_W			0x35
#define TIPO_S			0x36
#define TIPO_MA			0x37
#define TIPO_MF			0x38
#define TIPO_D			0x39
#define TIPO_AE			0x40
#define TIPO_NWFI		0x41
#define TIPO_VA			0x42
#define TIPO_P			0x43
#define TIPO_FG			0x44
#define TIPO_R			0x45
#define TIPO_RL			0x46
#define TIPO_WV			0x56
#define TIPO_AV			0x57
#define IO_TIPO_RSEN		0x60
#define IO_TIPO_RSELN		0x61
#define IO_TIPO_WSEN		0x62
#define IO_TIPO_WSE0		0x63
#define IO_TIPO_TR		0x64
#define IO_TIPO_TW		0x65
#define IO_TIPO_GEOM		0x66
#define IO_TIPO_RHDN		0x67
#define IO_TIPO_WHDN		0x68

#define BLK_SIZE 512			// Dimensione del blocco (HD e FD)
#define H_BLK_SIZE (BLK_SIZE / 2)	// Meta' dimensione blocco (short int)

#define D_TIMEOUT 0x7FFF		// Timeout per l'accesso ai dischi
#define D_ERR_NONE 0x00
#define D_ERR_BOUNDS 0xFF
#define D_ERR_PRESENCE 0xFE
#define D_ERR_GENERIC  0xFD

#ifndef ASM
const unsigned int ntab_sistema_condiviso = 256;
const unsigned int ntab_sistema_privato   = 256;
const unsigned int ntab_utente_condiviso  = 256;
const unsigned int ntab_utente_privato    = 255;

const unsigned int dim_sistema_condiviso = ntab_sistema_condiviso * SIZE_SUPERPAGINA;
const unsigned int dim_sistema_privato   = ntab_sistema_privato   * SIZE_SUPERPAGINA;
const unsigned int dim_utente_condiviso  = ntab_utente_condiviso  * SIZE_SUPERPAGINA;
const unsigned int dim_utente_privato    = ntab_utente_privato    * SIZE_SUPERPAGINA;

void* const inizio_sistema_condiviso = (void*)0x00000000;
void* const fine_sistema_condiviso   = (void*)((unsigned int)inizio_sistema_condiviso + dim_sistema_condiviso);
void* const inizio_sistema_privato   = fine_sistema_condiviso;
void* const fine_sistema_privato     = (void*)((unsigned int)inizio_sistema_privato + dim_sistema_privato);
void* const inizio_utente_condiviso  = fine_sistema_privato;
void* const fine_utente_condiviso    = (void*)((unsigned int)inizio_utente_condiviso + dim_utente_condiviso);
void* const inizio_utente_privato    = fine_utente_condiviso;
void* const fine_utente_privato      = (void*)((unsigned int)inizio_utente_privato + dim_utente_privato);

const unsigned int LOG_MSG_SIZE = 72;
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR };

struct log_msg {
	log_sev sev;
	short	identifier;
	char msg[LOG_MSG_SIZE];
};

#endif
