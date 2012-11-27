// ( costanti usate in sistema.cpp e sistema.S
//   SEL = selettori (segmentazione con modello flat)
//   LIV = livelli di privilegio
#define SEL_CODICE_SISTEMA	8
#define SEL_DATI_SISTEMA 	16
#define SEL_CODICE_UTENTE	27
#define SEL_DATI_UTENTE 	35
#define LIV_UTENTE		3
#define LIV_SISTEMA		0
// )

// ( varie dimensioni
#define MAX_SEM			4096
#define DIM_PAGINA		4096U
#define DIM_MACROPAGINA		(DIM_PAGINA * 1024U)
#define DIM_DESP		216 	// descrittore di processo
#define DIM_DESS		8	// descrittore di semaforo
#define BYTE_SEM		(DIM_DESS * MAX_SEM)
#define MAX_PRD			16
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
#define TIPO_NWFI		0x53	// nwfi
#define TIPO_FG			0x54	// *fill_gate
#define TIPO_P			0x55	// *panic
#define TIPO_AB			0x56	// *abort_p
#define TIPO_L			0x57	// *log
#define TIPO_TRA		0x58	// trasforma

#define IO_TIPO_HDR		0x62	// readhd_n
#define IO_TIPO_HDW		0x63	// writehd_n
#define IO_TIPO_RSEN		0x72	// readse_n
#define IO_TIPO_RSELN		0x73	// readse_ln
#define IO_TIPO_WSEN		0x74	// writese_n
#define IO_TIPO_WSE0		0x75	// writese_0
#define IO_TIPO_RCON		0x76	// readconsole
#define IO_TIPO_WCON		0x77	// writeconsole
#define IO_TIPO_INIC		0x78	// iniconsole
// * in piu' rispetto al libro
// )

#define IO_TIPO_PCIF		0x64	// pci_find
#define IO_TIPO_PCIR		0x65	// pci_read
#define IO_TIPO_PCIW		0x66	// pci_write
#define IO_TIPO_DMAHDR		0x67	// dmareadhd_n
#define IO_TIPO_DMAHDW		0x68	// dmawritehd_n
// ( suddivisione della memoria virtuale
//   NTAB = Numero di Tabelle delle pagine
//   SIS  = SIStema
//   MIO  = Modulo IO
//   USR  = utente (USeR)
//   C    = condiviso
//   P    = privato
#define NTAB_SIS_C		256	// 1GiB
#define NTAB_SIS_P		1	// 4MiB
#define NTAB_MIO_C		250	// 1GiB - 4MiB - 20MiB
#define NTAB_PCI_C		5	// 20MiB
#define NTAB_USR_C		256	// 1GiB
#define NTAB_USR_P		256	// 1GiB
// )
