#include "costanti.h"

param_err:
	.asciz "indirizzo non valido: %p"

// Chiama fill_gate con i parametri specificati
.macro fill_io_gate gate off
	movq $\gate, %rdi
	movabs $\off, %rax
	movq %rax, %rsi
	movq $LIV_UTENTE, %rdx
	call fill_gate
.endm

violazione:
	movq $2, %rdi
	movabs $param_err, %rsi
	movq %rax, %rdx
	xorq %rax, %rax
	call flog
	int $TIPO_AB

// controlla che l'indirizzo virtuale op sia accessibile dal
// livello di privilegio del chiamante della INT. Abortisce il
// processo in caso contrario.
.macro cavallo_di_troia reg

	cmpq $SEL_CODICE_SISTEMA, 8(%rsp)
	je 1f
	movabs $0xffff000000000000, %rax
	testq \reg, %rax
	jnz 1f
	movq \reg, %rax
	jmp violazione
1:	
.endm

// controlla che base+dim non causi un wrap-around
.macro cavallo_di_troia2 base dim

	movq \base, %rax
	addq \dim, %rax
	jc violazione
.endm

// come sopra, ma la dimensione e' in settori
.macro cavallo_di_troia3 base sec

	movq \base, %rax
	shlq $9, %rax
	addq \sec, %rax
	jc violazione
.endm

	.text
.global _start, start
_start:
start:	jmp cmain

////////////////////////////////////////////////////////////////////////////////
//                          CHIAMATE DI SISTEMA                               //
////////////////////////////////////////////////////////////////////////////////

	.global activate_p
activate_p:
	int $TIPO_A
	ret

	.global terminate_p //
terminate_p:
	int $TIPO_T
	ret

	.global sem_ini
sem_ini:
	int $TIPO_SI
	ret

	.global sem_wait	//
sem_wait:
	int $TIPO_W
	ret

	.global sem_signal	//
sem_signal:
	int $TIPO_S
	ret

	.global trasforma
trasforma:
	int $TIPO_TRA
	ret

////////////////////////////////////////////////////////////////////////////////
//                     INTERFACCIA VERSO IL MODULO SISTEMA                    //
////////////////////////////////////////////////////////////////////////////////

	.global activate_pe
activate_pe:	//
	int $TIPO_APE
	ret

	.global wfi	//
wfi:
	int $TIPO_WFI
	ret

	.global panic
panic:
	int $TIPO_P
	ret

	.global abort_p
abort_p:
	int $TIPO_AB
	ret

	.global fill_gate
fill_gate:
	int $TIPO_FG
	ret

	.global delay
delay:
	int $TIPO_D
	ret

	.global do_log
do_log:
	int $TIPO_L
	ret

	.global writevid
writevid:
	int $IO_TIPO_WCON
	ret

////////////////////////////////////////////////////////////////////////////////
//                         FUNZIONI DI SUPPORTO                               //
////////////////////////////////////////////////////////////////////////////////

// Inizio dell' ingresso da una interfaccia seriale
	.global go_inputse
go_inputse:
	movw %di, %dx		// ind. di IER in edx
	inb %dx, %al
	orb $0x01, %al		// abilitazione dell' interfaccia a
				//  generare interruzioni in ingresso
	outb %al, %dx
	ret

// Fine dell' ingresso da un' interfaccia seriale
	.global halt_inputse
halt_inputse:
	movw %di, %dx		// ind. di IER in edx
	inb %dx, %al
	and $0xfe, %al
	outb %al, %dx		// disabilitazione della generazione
				//  di interruzioni
	ret

// Inizio dell' uscita su interfaccia seriale
	.global go_outputse
go_outputse:
	movw %di, %dx		// ind. di IER in edx
	inb %dx, %al
	orb $0x02, %al
	outb %al, %dx
	ret

// Fine dell' uscita su interfaccia seriale
	.global halt_outputse
halt_outputse:
	movw %di, %dx		// ind. di IER in edx
	inb %dx, %al
	and $0xfd, %al
	outb %al, %dx
	ret

// Indirizzi delle porte delle interfacce seriali
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
	.global com_setup
com_setup:
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
	ret

.set KBD_WCB,	0x60
.set KBD_RCB,	0x20

.macro wait_kbd_out
1:	inb $0x64, %al	  // leggi stato
	testb $0x02, %al  // busy?
	jnz 1b
.endm

	.global go_inputkbd
go_inputkbd:
	pushq %rbp
	movq %rsp, %rbp

	movw 6(%rdi), %dx
	movb $KBD_WCB, %al
	outb %al, %dx
	movw 2(%rdi), %dx
	movb $0x61, %al
	outb %al, %dx
	movw 6(%rdi), %dx
1:	inb %dx, %al
	testb $0x02, %al
	jnz 1b

	leave
	ret


	ret

	.global halt_inputkbd
halt_inputkbd:
	pushq %rbp
	movq %rsp, %rbp

	movw 6(%rdi), %dx
	movb $KBD_WCB, %al
	outb %al, %dx
	movw 2(%rdi), %dx
	movb $0x60, %al
	outb %al, %dx
	movw 6(%rdi), %dx
1:	inb %dx, %al
	testb $0x02, %al
	jnz 1b

	leave
	ret

// indirizzi delle porte relative alla gestione del cursore, nel controllore
// della scheda video
.set CUR_HIGH, 0x0e
.set CUR_LOW, 0x0f
.set DAT, 0x03d5


// visualizza il cursore nella posizione passata come parametro
	.global cursore
cursore:
	pushq %rbp
	movq %rsp, %rbp
	movb  $80, %al
	mulb  %cl
	addb  %dl, %al
	adcb  $0, %ah
	movw  %ax, %cx
	movw  %di, %dx
	movb  $CUR_HIGH, %al
	outb  %al, %dx
	movw  %si, %dx
	movb  %ch, %al
	outb  %al, %dx
	movw  %di, %dx
	movb  $CUR_LOW, %al
	outb  %al, %dx
	movw  %si, %dx
	movb  %cl, %al
	outb  %al, %dx
	leave
	retq

// Inizializzazione dei gate per le primitive di IO
	.global fill_io_gates
fill_io_gates:
	pushq %rbp
	movq %rsp, %rbp

	fill_io_gate	IO_TIPO_RSEN	a_readse_n
	fill_io_gate	IO_TIPO_RSELN	a_readse_ln
	fill_io_gate	IO_TIPO_WSEN	a_writese_n
	fill_io_gate	IO_TIPO_WSE0	a_writese_0
	fill_io_gate	IO_TIPO_RCON	a_readconsole
	fill_io_gate	IO_TIPO_WCON	a_writeconsole
	fill_io_gate	IO_TIPO_INIC	a_iniconsole
	fill_io_gate	IO_TIPO_HDR	a_readhd_n
	fill_io_gate	IO_TIPO_HDW	a_writehd_n
	fill_io_gate	IO_TIPO_DMAHDR	a_dmareadhd_n
	fill_io_gate	IO_TIPO_DMAHDW	a_dmawritehd_n

	leave
	ret

////////////////////////////////////////////////////////////////////////////////
//                              PRIMITIVE DI IO                               //
////////////////////////////////////////////////////////////////////////////////

	.extern c_readse_n
a_readse_n:
	cavallo_di_troia %rsi
	cavallo_di_troia2 %rsi %rdx
	cavallo_di_troia %rcx
	call c_readse_n
	iretq

	.extern c_readse_ln
a_readse_ln:
	cavallo_di_troia %rsi
	cavallo_di_troia %rdx
	cavallo_di_troia2 %rdx $4
	cavallo_di_troia2 %rsi (%rdx)
	cavallo_di_troia %rcx
	call c_readse_ln
	iretq

	.extern c_writese_n
a_writese_n:
	cavallo_di_troia %rdi
	cavallo_di_troia2 %rdi %rdx
	call c_writese_n
	iretq

	.extern c_writese_0	// non c_writese_ln, che va lo stesso
a_writese_0:
	cavallo_di_troia %rsi
	cavallo_di_troia %rdx
	cavallo_di_troia2 %rdx $4
	movslq (%rdx), %r8
	cavallo_di_troia2 %rsi %r8
	call c_writese_0
	iretq

	.extern c_readconsole
a_readconsole:
	cavallo_di_troia %rdi
	cavallo_di_troia %rsi
	cavallo_di_troia2 %rsi $4
	movslq (%rsi), %r8
	cavallo_di_troia2 %rdi %r8
	call c_readconsole
	iretq

	.extern c_writeconsole
a_writeconsole:
	cavallo_di_troia %rdi
	call c_writeconsole
	iretq

	.extern c_iniconsole
a_iniconsole:
	call c_iniconsole
	iretq


# interface ATA

.global			hd_write_address
hd_write_address:
	pushq %rbp
	movq %rsp, %rbp

	movl %esi,%eax
	movw 6(%rdi),%dx
	outb %al,%dx		// caricato SNR
	movw 2(%rdi),%dx
	movb %ah,%al
	outb %al,%dx		// caricato CNL
	shrl $16,%eax
	movw 4(%rdi),%dx
	outb %al,%dx		// caricato CNH
	movw 8(%rdi),%dx
	inb %dx,%al		// HND in %al
	andb $0xf0,%al		// maschera per l'indirizzo in HND
	andb $0x0f,%ah		// maschera per i 4 bit +sign di primo
	orb  $0xe0,%ah		// seleziona LBA
	orb %ah,%al
	outb %al,%dx		// caricato HND

	leave
	ret

.global			hd_write_command
hd_write_command:
	pushq %rbp
	movq %rsp, %rbp

	movl %edi, %eax
	movw %si, %dx
	outb %al, %dx

	leave
	ret


.global		hd_go_inout
hd_go_inout:		#...
	movw %di, %dx		// ind. di DEV_CTL in edx
	movb $0x08,%al
	outb %al, %dx			// abilitazione dell' interfaccia a
					// generare interruzioni
	ret
.global			hd_halt_inout
hd_halt_inout:
	movw %di, %dx		// ind. di DEV_CTL in edx
	movb $0x0A,%al
	outb %al, %dx			// disabilitazione della generazione
					// di interruzioni
	ret

// Seleziona uno dei due drive di un canale ATA
	.global hd_select_device
hd_select_device:
	pushq %rbp
	movq %rsp, %rbp

	movl %edi,%eax
	cmpl $0,%eax
	je shd_ms
shd_sl:	movb $0xf0,%al
	jmp ms_out
shd_ms:	movb $0xe0,%al
ms_out:	movw %si,%dx
	outb %al,%dx

	leave
	ret

.global		readhd_n
readhd_n:	int		$IO_TIPO_HDR
		ret
.global		writehd_n
writehd_n:	int		$IO_TIPO_HDW
		ret
.global		dmareadhd_n
dmareadhd_n:	int		$IO_TIPO_DMAHDR
		ret
.global		dmawritehd_n
dmawritehd_n:	int		$IO_TIPO_DMAHDW
		ret
.extern		c_readhd_n
a_readhd_n:	# routine INT $io_tipo_hdr
		cavallo_di_troia %rdi
		cavallo_di_troia3 %rdi %rdx
		cavallo_di_troia %rcx
		call	c_readhd_n
		iretq

.EXTERN		c_writehd_n
a_writehd_n:	# routine INT $io_tipo_hdw
		cavallo_di_troia %rdi
		cavallo_di_troia3 %rdi %rdx
		cavallo_di_troia %rcx
		call	c_writehd_n
		iretq

.EXTERN		c_dmareadhd_n
a_dmareadhd_n: 	# routine INT $dma_tipob_r
		cavallo_di_troia %rdi
		cavallo_di_troia3 %rdi %rdx
		cavallo_di_troia %rcx
		call	c_dmareadhd_n
		iretq

.EXTERN		c_dmawritehd_n
a_dmawritehd_n:	# routine INT $dma_tipob_w
		cavallo_di_troia %rdi
		cavallo_di_troia3 %rdi %rdx
		cavallo_di_troia %rcx
		call	c_dmawritehd_n
		iretq

