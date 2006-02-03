// io.s
//
#include "costanti.h"

////////////////////////////////////////////////////////////////////////////////
//                                 COSTANTI                                   //
////////////////////////////////////////////////////////////////////////////////

// Tipi delle interruzioni delle chiamate di sistema usate
// Devono coincidere con quelli usati in sistema.s e utente.s
//
.set tipo_t, TIPO_T
.set tipo_si, TIPO_SI
.set tipo_w, TIPO_W
.set tipo_s, TIPO_S
.set tipo_nd, TIPO_ND

// Tipi delle interruzioni usate per interfacciarsi al modulo
//  sistema
// Devono coincidere con quelli usati in sistema.s
//
.set tipo_ae, TIPO_AE
.set tipo_nwfi, TIPO_NWFI
.set tipo_va, TIPO_VA
.set tipo_p, TIPO_P
.set tipo_fg, TIPO_FG
.set tipo_r, TIPO_R
.set tipo_cr, TIPO_CR
.set tipo_cw, TIPO_CW
.set tipo_cs, TIPO_CS
.set tipo_cl, TIPO_CL
.set tipo_cu, TIPO_CU
.set tipo_ci, TIPO_CI


// Tipi delle interruzioni usate dalle primitive di IO
// Devono coincidere con quelli usati in utente.s
//
.set io_tipo_rsen, IO_TIPO_RSEN
.set io_tipo_rseln, IO_TIPO_RSELN
.set io_tipo_wsen, IO_TIPO_WSEN
.set io_tipo_wse0, IO_TIPO_WSE0
.set io_tipo_tr, IO_TIPO_TR
.set io_tipo_tw, IO_TIPO_TW
.set io_tipo_geom, IO_TIPO_GEOM
.set io_tipo_rhdn, IO_TIPO_RHDN
.set io_tipo_whdn, IO_TIPO_WHDN
.set io_tipo_rfdn, IO_TIPO_RFDN
.set io_tipo_wfdn, IO_TIPO_WFDN


////////////////////////////////////////////////////////////////////////////////
//                  MACRO PER LA MANIPOLAZIONE DEI PARAMETRI                  //
////////////////////////////////////////////////////////////////////////////////

// Copia dei parametri di una chiamata di sistema dalla pila utente
//  alla pila sistema
// Replicato in sistema.s
//
.macro copia_param n_long offset
        movl $\offset, %ecx
        movl 4(%esp, %ecx, 4), %eax     // cs in eax
        testl $3, %eax			// verifica del livello di privilegio
					//  del chiamante
        jz 1f                           // copia da pila sistema

        movl 12(%esp, %ecx, 4), %eax    // vecchio esp (della pila utente)
					//  in eax
        leal 4(%eax), %esi		// indirizzo del primo parametro in
					//  esi
        jmp 2f
1:
        leal 16(%esp, %ecx, 4), %esi	// indirizzo del primo parametro in esi
2:
        movl $\n_long, %eax		// creazione in pila dello spazio per
        shll $2, %eax			//  la copia dei parametri
        subl %eax, %esp
        leal (%esp), %edi		// indirizzo della destinazione del
					//  primo parametro in edi

        movl $\n_long, %ecx
        cld
        rep
           movsl			// copia dei parametri
.endm

// Salvataggio dei registri in pila
// Replicato in sistema.s
//
.macro salva_registri
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp
.endm

// Caricamento dei registri dalla pila (duale rispetto a salva_registri)
// Replicato in sistema.s
//
.macro carica_registri
	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx
	popl %eax
.endm

.global start, _start
start:
_start:	jmp _cmain

////////////////////////////////////////////////////////////////////////////////
//                             SEZIONE DATI                                   //
////////////////////////////////////////////////////////////////////////////////

	.data

// Descrittori delle interfacce seriali
//
	.global _com		// non comi, como
_com:	.long	0x03f8		// com[0].indreg.iRBR
	.long	0x03f8		// com[0].indreg.iTHR
	.long	0x03fd		// com[0].indreg.iLSR
	.long	0x03f9		// com[0].indreg.iIER
	.long	0x03fa		// com[0].indreg.iIIR
	.long	0		// com[0].mutex
	.long	0		// com[0].sincr
	.long	0		// com[0].cont
	.long	0		// com[0].punt
	.long	0		// com[0].funzione
	.long	0		// com[0].stato
	.long	0x02f8		// com[1].indreg.iRBR
	.long	0x02f8		// com[1].indreg.iTHR
	.long	0x03fd		// com[1].indreg.iLSR
	.long	0x02f9		// com[1].indreg.iIER
	.long	0x02fa		// com[1].indreg.iIIR
	.long	0		// com[1].mutex
	.long	0		// com[1].sincr
	.long	0		// com[1].cont
	.long	0		// com[1].punt
	.long	0		// com[1].funzione
	.long	0		// com[1].stato


// Descrittore Hard Disk
//

	.global _hd
_hd:	.long	0x01f7		//hd[0].indreg.iCMD_iSTS
	.long	0x01f0		//hd[0].indreg.iDATA
	.long	0x01f1		//hd[0].indreg.iFEATURES_iERROR
	.long 	0x01f2		//hd[0].indreg.iSTCONT
	.long	0x01f3		//hd[0].indreg.iSECT_N
	.long	0x01f4		//hd[0].indreg.iCYL_L_N
	.long	0x01f5		//hd[0].indreg.iCYL_H_N
	.long	0x01f6		//hd[0].indreg.iHD_N_iDRV_HD
	.long	0x03f6		//hd[0].indreg.iALT_STS_iDEV_CTRL
	.long	0		//hd[0].disco[0].{presente,dma} (sono byte!)
	.word	0		//hd[0].disco[0].geom.cil
	.word	0		//hd[0].disco[0].geom.test
	.long	0		//hd[0].disco[0].geom.sett
	.long	0		//hd[0].disco[0].geom.tot_sett
	.long	0		//hd[0].disco[1].{presente,dma} (sono byte!)
	.word	0		//hd[0].disco[1].geom.cil
	.word	0		//hd[0].disco[1].geom.test
	.long	0		//hd[0].disco[1].geom.sett
	.long	0		//hd[0].disco[1].geom.tot_sett
	.long	0		//hd[0].comando
				//hd[0].errore (byte!)
	.long	0		//hd[0].cont (byte!)
	.long	0		//hd[0].punt
	.long	0		//hd[0].mutex
	.long	0		//hd[0].sincr
	.long	0x0177		//hd[1].indreg.iCMD_iSTS
	.long	0x0170		//hd[1].indreg.iDATA
	.long	0x0171		//hd[1].indreg.iFEATURES_iERROR
	.long 	0x0172		//hd[1].indreg.iSTCONT
	.long	0x0173		//hd[1].indreg.iSECT_N
	.long	0x0174		//hd[1].indreg.iCYL_L_N
	.long	0x0175		//hd[1].indreg.iCYL_H_N
	.long	0x0176		//hd[1].indreg.iHD_N_iDRV_HD
	.long	0x0376		//hd[1].indreg.iALT_STS_iDEV_CTRL
	.long	0		//hd[1].disco[0].{presente,dma} (sono byte!)
	.word	0		//hd[1].disco[0].geom.cil
	.word	0		//hd[1].disco[0].geom.test
	.long	0		//hd[1].disco[0].geom.sett
	.long	0		//hd[1].disco[0].geom.tot_sett
	.long	0		//hd[1].disco[1].{presente,dma} (sono byte!)
	.word	0		//hd[1].disco[1].geom.cil
	.word	0		//hd[1].disco[1].geom.test
	.long	0		//hd[1].disco[1].geom.sett
	.long	0		//hd[1].disco[1].geom.tot_sett
	.long	0		//hd[1].comando
				//hd[1].errore (byte!)
	.long	0		//hd[1].cont (byte!)
	.long	0		//hd[1].punt
	.long	0		//hd[1].mutex
	.long	0		//hd[1].sincr

////////////////////////////////////////////////////////////////////////////////
//                            SEZIONE TESTO                                   //
////////////////////////////////////////////////////////////////////////////////

	.text

////////////////////////////////////////////////////////////////////////////////
//                          CHIAMATE DI SISTEMA                               //
////////////////////////////////////////////////////////////////////////////////

	.global _terminate_p
_terminate_p:
	int $tipo_t
	ret

	.global _sem_ini
_sem_ini:
	int $tipo_si
	ret

	.global _sem_wait
_sem_wait:
	int $tipo_w
	ret

	.global _sem_signal
_sem_signal:
	int $tipo_s
	ret

////////////////////////////////////////////////////////////////////////////////
//                     INTERFACCIA VERSO IL MODULO SISTEMA                    //
////////////////////////////////////////////////////////////////////////////////

	.global _activate_pe
_activate_pe:
	int $tipo_ae
	ret

	.global _nwfi
_nwfi:
	int $tipo_nwfi
	ret

	.global _verifica_area
_verifica_area:
	int $tipo_va
	ret

	.global _panic
_panic:
	int $tipo_p
	ret

	.global _fill_gate
_fill_gate:
	int $tipo_fg
	ret

	.global _reboot
_reboot:
	int $tipo_r
	ret

	.global _con_write
_con_write:
	int $tipo_cw
	ret

	.global _con_read
_con_read:
	int $tipo_cr
	ret

	.global _con_save
_con_save:
	int $tipo_cs
	ret

	.global _con_load
_con_load:
	int $tipo_cl
	ret

	.global _con_update
_con_update:
	int $tipo_cu
	ret

	.global _con_init
_con_init:
	int $tipo_ci
	ret

	.global _ndelay
_ndelay:
	int $tipo_nd
	ret
 
////////////////////////////////////////////////////////////////////////////////
//                         FUNZIONI DI SUPPORTO                               //
////////////////////////////////////////////////////////////////////////////////

// Ingresso di un byte da una porta di IO
// Replicato in sistema.s
//
	.global _inputb
_inputb:
	pushl %eax
	pushl %edx
	movl 12(%esp), %edx
	inb %dx, %al
	movl 16(%esp), %edx
	movb %al, (%edx)
	popl %edx
	popl %eax
	ret

// Uscita di un byte su una porta di IO
// Replicato in sistema.s
//
	.global _outputb
_outputb:
	pushl %eax
	pushl %edx
	movb 12(%esp), %al
	movl 16(%esp), %edx
	outb %al, %dx
	popl %edx
	popl %eax
	ret

// Ingresso di una word da una porta di IO
// Replicato in sistema.s
//
	.global _inputw
_inputw:
	pushl %eax
	pushl %edx
	movl 12(%esp), %edx
	inw %dx, %ax
	movl 16(%esp), %edx
	movw %ax, (%edx)
	popl %edx
	popl %eax
	ret

// Uscita di una word su una porta di IO
// Replicato in sistema.s
//
	.global _outputw
_outputw:
	pushl %eax
	pushl %edx
	movw 12(%esp), %ax
	movl 16(%esp), %edx
	outw %ax, %dx
	popl %edx
	popl %eax
	ret

// Ingresso di un buffer continuo da un buffer di IO di word
// Replicato in sistema.s
//
	.global _inputbuffw
_inputbuffw:
	pushl %eax
	pushl %edx
	pushl %edi
	pushl %ecx
	
	movl 20(%esp), %edx
	movl 24(%esp), %edi
	movl 28(%esp),%ecx
	cld
	rep
	insw
	
	popl %ecx
	popl %edi
	popl %edx
	popl %eax
	ret

// Uscita di un buffer continuo su una buffer di IO di word
// Replicato in sistema.s
//
	.global _outputbuffw
_outputbuffw:
	pushl %eax
	pushl %edx
	pushl %esi
	pushl %ecx
	
	movl 24(%esp), %edx
	movl 20(%esp), %esi
	movl 28(%esp),%ecx
	cld
	rep
	outsw
	
	popl %ecx
	popl %esi
	popl %edx
	popl %eax
	ret

// Indirizzi delle porte del controllore delle interruzioni
//
.set OCW1M, 0x21
.set OCW2M, 0x20
.set OCW1S, 0xa1
.set OCW2S, 0xa0
.set EOI, 0x20

// Inizio dell' ingresso da una interfaccia seriale
//
	.global _go_inputse
_go_inputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		// ind. di IER in edx
	inb %dx, %al
	orb $0x01, %al			// abilitazione dell' interfaccia a
					//  generare interruzioni in ingresso
	outb %al, %dx

	cmpw $0x03f9, %dx
	jne 1f
	movb $0b11101111, %ah
	jmp 2f

1:
	movb $0b11110111, %ah
2:
	cli
	inb $OCW1M, %al
	andb %ah, %al
	outb %al, $OCW1M		// abilitazione del controllore ad
					//  accettare richieste di interruzione
					//  da parte dell' interfaccia
	sti

	popl %edx
	popl %eax
	ret

// Fine dell' ingresso da un' interfaccia seriale
//
	.global _halt_inputse
_halt_inputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		// ind. di IER in edx
	inb %dx, %al
	and $0xfe, %al
	outb %al, %dx			// disabilitazione della generazione
					//  di interruzioni

	cmpw $0x03f9, %dx
	jne 1f
	movb $0b00010000, %ah
	jmp 2f

1:
	movb $0b00001000, %ah
2:
	cli
	inb $OCW1M, %al
	andb %ah, %al
	outb %al, $OCW1M		// disabilitazione dell 'interruzione al
					//  controllore
	sti

	popl %edx
	popl %eax
	ret

// Inizio dell' uscita su interfaccia seriale
//
	.global _go_outputse
_go_outputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		// ind. di IER in edx
	inb %dx, %al
	orb $0x02, %al
	outb %al, %dx

	cmpw $0x03f9, %dx
	jne 1f
	movb $0b11101111, %ah
	jmp 2f

1:
	movb $0b11110111, %ah
2:
	cli
	inb $OCW1M, %al
	andb %ah, %al
	outb %al, $OCW1M
	sti

	popl %edx
	popl %eax
	ret

// Fine dell' uscita su interfaccia seriale
//
	.global _halt_outputse
_halt_outputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		// ind. di IER in edx
	inb %dx, %al
	and $0xfd, %al
	outb %al, %dx

	cmpw $0x03f9, %dx
	jne 1f
	movb $0b00010000, %ah
	jmp 2f

1:
	movb $0b00001000, %ah
2:
	cli
	inb $OCW1M, %al
	andb %ah, %al
	outb %al, $OCW1M
	sti

	popl %edx
	popl %eax
	ret

// Indirizzi delle porte delle interfacce seriali
//
.set LCR1, 0x03fb
.set LCR2, 0x02fb
.set DLR_LSB1, 0x03f8
.set DLR_LSB2, 0x02f8
.set DLR_MSB1, 0x03f9
.set DLR_MSB2, 0x02f9
.set IER1, 0x03f9
.set IER2, 0x02f9
.set RBR1, 0x03f8
.set RBR2, 0x02f8
.set MCR1, 0x03fc
.set MCR2, 0x02fc


// Inizializzazione delle interfacce seriali
// 
	.global _com_setup
_com_setup:
	pushl %eax
	pushl %edx

	movb $0x80, %al
	movw $LCR1, %dx
	outb %al, %dx
	movw $0x000c, %ax
	movw $DLR_LSB1, %dx
	outb %al, %dx
	movb %ah, %al
	movw $DLR_MSB1, %dx
	outb %al, %dx
	movb $0x03, %al
	movw $LCR1, %dx
	outb %al, %dx
	movb $0x00, %al
	movw $IER1, %dx
	outb %al, %dx
	movw $RBR1, %dx
	movw $MCR1, %dx			// abilitazione porta 3-state
	movb $0b00001000, %al
	outb %al, %dx
	inb %dx, %al

	movb $0x80, %al
	movw $LCR2, %dx
	outb %al, %dx
	movw $0x000c, %ax
	movw $DLR_LSB2, %dx
	outb %al, %dx
	movb %ah, %al
	movw $DLR_MSB2, %dx
	outb %al, %dx
	movb $0x03, %al
	movw $LCR2, %dx
	outb %al, %dx
	movb $0x00, %al
	movw $IER2, %dx
	outb %al, %dx
	movw $RBR2, %dx
	movw $MCR2, %dx
	movb $0b00001000, %al
	outb %al, %dx

	inb %dx, %al

	popl %edx
	popl %eax

	ret

// Abilitazione del controllore delle interruzioni ad inoltrare le richieste
//  provenienti dalla tastera
//
	.global _abilita_tastiera
_abilita_tastiera:
	inb $OCW1M, %al
	andb $0b11111101, %al
	outb %al, $OCW1M
	ret


// Funzioni di utilita' per la gestione degli hard disk
//

// Esegue un probing del canale ATA specificato tentando di capire se
//  ad esso sono collegati nessuno, uno o piu` dispositivi
//
	.global _test_canale
_test_canale:
	pushl %ebx
	pushl %edx
	movw 12(%esp),%dx
	movw 0xFFFF,%bx
chcyc1:	inb %dx,%al
	cmpb $0xFF,%al
	je chfl1
	inb %dx,%al
	testb $0x80,%al
	jz chprb1
	decw %bx
	jnz chcyc1
chprb1:	movb $0x55,%al
	movw 16(%esp),%dx 
	outb %al,%dx
	movb $0xAA,%al
	movw 20(%esp),%dx
	outb %al,%dx
	movw 16(%esp),%dx
	outb %al,%dx
	movb $0x55,%al
	movw 20(%esp),%dx
	outb %al,%dx
	movw 16(%esp),%dx
	outb %al,%dx
	movb $0xAA,%al
	movw 20(%esp),%dx
	outb %al,%dx
	movw 16(%esp),%dx
	inb %dx,%al
	movb %al,%ah
	movw 20(%esp),%dx
	inb %dx,%al
	cmpw $0x55AA,%ax
	jne chfl1
chnfl1:	
	movl $1,%eax
	jmp chret1
chfl1:	
	movl $0,%eax
chret1:	popl %edx
	popl %ebx
	ret

// Assmendo che il canale sia "attivo" cerca di capire se il dispositivo
//  indirizzato e` un ATA (hard disk) o ATAPI (chrom)
//
	.global _leggi_signature
_leggi_signature:
	pushl %edx
	movw 8(%esp),%dx
	inb %dx,%al
	cmpb $0x01,%al
	jne sgerr
	movw 12(%esp),%dx
	inb %dx,%al
	cmpb $0x01,%al
	jne sgerr
	movw 16(%esp),%dx
	inb %dx,%al
	cmpb $0x00,%al
	je sgata1
	cmpb $0x14,%al
	je sgpi1
	jmp sgerr
sgata1:	movw 20(%esp),%dx
	inb %dx,%al
	cmpb $0x00,%al
	jne sgerr
	jmp sgata
sgpi1:	movw 20(%esp),%dx
	inb %dx,%al
	cmpb $0xEB,%al
	jne sgerr
	jmp sgpi
sgata:	
	movl $0,%eax
	jmp sgret
sgpi:	
	movl $1,%eax
	jmp sgret
sgerr:	
	movl $-1,%eax
	jmp sgret
sgret:	
	popl %edx
	ret

	.global _drive_reset
_drive_reset:
	pushl %edx
	movw 8(%esp),%dx
	movb $0x0E,%al
	outb %al,%dx
	movw 12(%esp),%dx
	inb %dx,%al
	testb $0x80,%al
	jz rserr
	movw 8(%esp),%dx
	movb $0x02,%al
	outb %al,%dx
	movl $1,%eax
	jmp rsret
rserr:	movl $0,%eax
rsret:	popl %edx
	ret

// Abilita il controller ATA specificato a generare richieste di interruzione
//  utili sia per l'ingresso che per l'uscita
//
	.global _go_inouthd
_go_inouthd:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		// ind. di DEV_CTL in edx
	movb $0x08,%al
	outb %al, %dx			// abilitazione dell' interfaccia a
					// generare interruzioni
	popl %edx
	popl %eax
	ret

// Disabilita il controller ATA specificato a generare richeste di interruzione
//  NB. Su un PC (non su Bochs) questa stassa procedura produrra` una richiesta
//  di interruzione!
//
	.global _halt_inouthd
_halt_inouthd:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		// ind. di DEV_CTL in edx
	movb $0x0A,%al
	outb %al, %dx			// disabilitazione della generazione
					// di interruzioni
	popl %edx
	popl %eax
	ret

// Indica al controllore quale drive deve essere selezionato
//
	.global _set_drive
_set_drive:
	pushl %eax
	pushl %edx

	movl 12(%esp),%eax
	cmpl $0,%eax
	je shd_ms
shd_sl:	movb $0xF0,%al
	jmp ms_out
shd_ms:	movb $0xE0,%al
ms_out:	movl 16(%esp),%edx
	outb %al,%dx

	pushl $400
	call _ndelay
	addl $4, %esp

	popl %edx
	popl %eax
	ret

// Legge lo stato attuale del controllore per capire quale drive e` attivo
//
	.global _get_drive
_get_drive:
	pushl %eax
	pushl %edx

	movl 12(%esp),%edx
	inb %dx,%al
	
	testb $0x10,%al
	jz ghd_ms
ghd_sl:	movl $1,%eax
	jmp ms_ret
ghd_ms:	movl $0,%eax
ms_ret:	movl 16(%esp),%edx
	movl %eax,(%edx)

	popl %edx
	popl %eax
	ret
	
// Indirizzi dei registri del controller ata
//

.set HD_STS_1, 0x01F7
.set HD_STS_2, 0x0177
.set HD_SECT_N, 0x01f3
.set HD_CYL_L_N, 0x01f4
.set HD_CYL_H_N, 0x01f5
.set HD_N, 0x01f6

// Maschere dei bit fndamentali
//
.set HD_STS_DRDY, 0x40

// Disabilita il controllore di interruzioni ad inoltrare richieste provenienti
//  da uno dei due controller ATA, Slave PIC, linee 14 o 15
//
	.global _mask_hd
_mask_hd:
	pushl %eax
	movl 8(%esp),%eax
	cmpl $0x01F7,%eax
	je msk0
	cmpl $0x0177,%eax
	je msk1			// capisce qual'e` il canale
	jmp m_end
msk0:	inb $OCW1S,%al
	orb $0b01000000,%al	// linea 14
	outb %al,$OCW1S	
	jmp m_end
msk1:	inb $OCW1S,%al
	orb $0b10000000,%al	// linea 15
	outb %al,$OCW1S
m_end:	popl %eax
	ret

// Abilita il controllore di interruzione ad inoltrere le richieste provenienti
//  da uno dei due controller ATA, Slave PIC, linee 14 o 15
//
	.global _umask_hd
_umask_hd:
	pushl %eax
	movl 8(%esp),%eax
	cmpl $0x01F7,%eax
	je umsk0
	cmpl $0x0177,%eax
	je umsk1		// Capisce qual'e` il canale
	jmp um_end
umsk0:	inb $OCW1S,%al
	andb $0b10111111,%al	// linea 14
	outb %al,$OCW1S	
	jmp um_end
umsk1:	inb $OCW1S,%al
	andb $0b01111111,%al	// linea 15
	outb %al,$OCW1S
um_end:	popl %eax
	ret
	
// Scompone correttamente il blocco iniziale di una operazione su hard disk
// e lo scrive nei registri opportuni
//
	.global _setup_addr_hd
_setup_addr_hd:
	pushl %eax
	pushl %edx
	pushl %edi
	
	movl 20(%esp),%eax
	movl 16(%esp),%edi	// accede al descrittore per comodita`
	movl 16(%edi),%edx
	outb %al,%dx		// caricato iSECT_N
	movl 20(%edi),%edx
	movb %ah,%al
	outb %al,%dx		// caricato cyl_LSB
	shrl $16,%eax
	movl 24(%edi),%edx
	outb %al,%dx		// caricato cyl_MSB
	movl 28(%edi),%edx
	inb %dx,%al		// iDRV_HD in %al
	andb $0xF0,%al		// maschera per l'indirizzo in DRV_HD
	andb $0x0F,%ah		// maschera per i 4 bit +sing di primo
	orb %ah,%al
	outb %al,%dx		// caricato iDRV_HD
	
	popl %edi
	popl %edx
	popl %eax
	ret

// Chiama _fill_gate con i parametri specificati
//
.macro fill_io_gate gate off
	pushl $LIV_UTENTE
	pushl $\off
	pushl $\gate
	call _fill_gate
	addl $12, %esp
.endm

// Inizializzazione dei gate per le primitive di IO
//
	.global _fill_io_gates
_fill_io_gates:
	pushl %ebp
	movl %esp, %ebp

	fill_io_gate io_tipo_rsen a_readse_n
	fill_io_gate io_tipo_rseln a_readse_ln
	fill_io_gate io_tipo_wsen a_writese_n
	fill_io_gate io_tipo_wse0 a_writese_0
	fill_io_gate io_tipo_tr a_term_read_n
	fill_io_gate io_tipo_tw a_term_write_n
	
	leave
	ret

////////////////////////////////////////////////////////////////////////////////
//                              PRIMITIVE DI IO                               //
////////////////////////////////////////////////////////////////////////////////

	.global a_readse_n
	.extern _c_readse_n
a_readse_n:
	salva_registri
	copia_param 4 7
	call _c_readse_n
	addl $16, %esp
	carica_registri
	iret

	.global a_readse_ln
	.extern _c_readse_ln
a_readse_ln:
	salva_registri
	copia_param 4 7
	call _c_readse_ln
	addl $16, %esp
	carica_registri
	iret

	.global a_writese_n
	.extern _c_writese_n
a_writese_n:
	salva_registri
	copia_param 3 7
	call _c_writese_n
	addl $12, %esp
	carica_registri
	iret

	.global a_writese_0
	.extern _c_writese_0	// non _c_writese_ln, che va lo stesso
a_writese_0:
	salva_registri
	copia_param 3 7
	call _c_writese_0
	addl $12, %esp
	carica_registri
	iret

	.global a_term_read_n
	.extern _c_term_read_n
a_term_read_n:	// routine int $io_tipo_tr
	salva_registri
	copia_param 3 7
	call _c_term_read_n
	addl $12, %esp
	carica_registri
	iret

	.global a_term_write_n
	.extern _c_term_write_n
a_term_write_n: // routine int $io_tipo_tw
	salva_registri
	copia_param 3 7
 	call _c_term_write_n
 	addl $12, %esp
 	carica_registri
	iret
	
	.global a_geometria
	.extern _c_geometria
a_geometria:	// routine int $io_tipo_geom
	salva_registri
	copia_param 7 7
	call _c_geometria
	addl $28,%esp
	carica_registri
	iret

	.global a_readhd_n
	.extern _c_readhd_n
a_readhd_n:
	salva_registri
	copia_param 6 7
	call _c_readhd_n
	addl $24, %esp
	carica_registri
	iret

	.global a_writehd_n
	.extern _c_writehd_n
a_writehd_n:
	salva_registri
	copia_param 6 7
	call _c_writehd_n
	addl $24, %esp
	carica_registri
	iret

