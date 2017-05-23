// abilita output flog per grafico
//#define DO_GRAPH

// ( costanti usate in sistema.cpp e sistema.S
//   SEL = selettori (segmentazione con modello flat)
//   LIV = livelli di privilegio
#define SEL_CODICE_SISTEMA	0x8
#define SEL_CODICE_UTENTE	0x13
#define SEL_DATI_UTENTE 	0x1b
#define LIV_UTENTE		3
#define LIV_SISTEMA		0
#define NUM_TSS 		1024
#define MIN_PROC_ID		32
#define MAX_PROC_ID		(NUM_TSS*16)
// )

// ( varie dimensioni
#define KiB			1024UL
#define MiB			(1024*KiB)
#define GiB			(1024*MiB)

// dimensione in byte della parte hardware del descrittore di processo
#define DIM_DESP                104
#define MAX_SEM			4096
#define DIM_PAGINA		4096UL
#define MAX_PRD			16

#define MEM_TOT			(32*MiB)
#define DIM_USR_HEAP		(1*MiB)
#define DIM_USR_STACK		(10*MiB)
#define DIM_IO_HEAP		(16*MiB)
#define DIM_SYS_STACK		(4*KiB)
#define DIM_SWAP		(80*MiB)
#define DIM_BLOCK		512
// )

// ( tipi interruzioni esterne
#define VETT_0			0xF0
#define VETT_1                  0xD0
#define VETT_2                  0xC0
#define VETT_3                  0xB0
#define VETT_4                  0xA0
#define VETT_5                  0x90
#define VETT_6                  0x80
#define VETT_7                  0x70
#define VETT_8                  0x60
#define VETT_9                  0x50
#define VETT_10                 0x40
#define VETT_11                 0x30
#define VETT_12                 0x20
#define VETT_13                 0xD1
#define VETT_14                 0xE0
#define VETT_15                 0xE1
#define VETT_16                 0xC1
#define VETT_17                 0xB1
#define VETT_18                 0xA1
#define VETT_19                 0x91
#define VETT_20                 0x81
#define VETT_21                 0x71
#define VETT_22                 0x61
#define VETT_23                 0x51
#define VETT_S			0x4F
// )
// ( tipi delle primitive
#define TIPO_A			0x42	// activate_p
#define TIPO_T			0x43	// terminate_p
#define TIPO_SI			0x44	// sem_ini
#define TIPO_W			0x45	// sem_wait
#define TIPO_S			0x46	// sem_signal
#define TIPO_D			0x49	// delay
#define TIPO_RE			0x4b	// resident
#define TIPO_EP			0x4c	// end_program
#define TIPO_APE		0x52	// activate_pe
#define TIPO_WFI		0x53	// wfi
#define TIPO_FG			0x54	// *fill_gate
#define TIPO_P			0x55	// *panic
#define TIPO_AB			0x56	// *abort_p
#define TIPO_L			0x57	// *log
#define TIPO_TRA		0x58	// trasforma

#define IO_TIPO_HDR		0x62	// readhd_n
#define IO_TIPO_HDW		0x63	// writehd_n
#define IO_TIPO_DMAHDR		0x64	// dmareadhd_n
#define IO_TIPO_DMAHDW		0x65	// dmawritehd_n
#define IO_TIPO_RSEN		0x72	// readse_n
#define IO_TIPO_RSELN		0x73	// readse_ln
#define IO_TIPO_WSEN		0x74	// writese_n
#define IO_TIPO_WSE0		0x75	// writese_0

#define IO_TIPO_WINDC		0x79	// crea_finestra
#define IO_TIPO_WINDV		0x80	// visualizza_finestra
#define IO_TIPO_WINDOC		0x81	// crea_oggetto
#define IO_TIPO_WINDUC		0x82	// aggiorna_oggetto
#define IO_TIPO_WINDPE		0x83	// preleva_evento
#define IO_TIPO_WINDTK		0x84	// winman_time_tick
// * in piu' rispetto al libro
// )

// ( suddivisione della memoria virtuale
//   N    = Numero di entrate in tab4
//   I	  = Indice della prima entrata in tab4
//   SIS  = SIStema
//   MIO  = Modulo IO
//   UTN  = modulo UTeNte
//   C    = Condiviso
//   P    = Privato
#define I_SIS_C		0
#define I_SIS_P		1
#define I_MIO_C		2
#define I_UTN_C       256
#define I_UTN_P	      384

#define N_SIS_C		1
#define N_SIS_P		1
#define N_MIO_C		1
#define N_UTN_C	      128
#define N_UTN_P	      127
// )
