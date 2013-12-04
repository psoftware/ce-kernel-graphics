// sistema.s


#define ASM 1
#include "mboot.h"
#include "costanti.h"

////////////////////////////////////////////////////////////////
// MACRO                                                      //
////////////////////////////////////////////////////////////////
.macro salva_registri
	
	pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rbx
	pushq %rsi
	pushq %rdi
	pushq %rbp
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

.endm

.macro carica_registri
	
	popq %rax
	popq %rcx
	popq %rdx
	popq %rbx
	popq %rsi
	popq %rdi
	popq %rbp
	popq %r8
	popq %r9
	popq %r10
	popq %r11
	popq %r12
	popq %r13
	popq %r14
	popq %r15

.endm

//////////////////////////////////////////////////////////////////////////
// AVVIO                                                                  //
//////////////////////////////////////////////////////////////////////////

#define STACK_SIZE                      0x8000




.globl  _start, start
_start:				// entry point
start:
	movq    $(stack + STACK_SIZE), %rsp
	call init_idt
	call    cmain			// il resto dell'inizializzazione
	hlt				// e' scritto in C++
					
					
					
	
	
	
// Carica un gate della IDT
// num: indice (a partire da 0) in IDT del gate da caricare
// routine: indirizzo della routine da associare al gate
// dpl: dpl del gate (LIV_SISTEMA o LIV_UTENTE)
// NOTA: la macro si limita a chiamare la routine init_gate
//       con gli stessi parametri. Verra' utilizzata per
//       motivi puramente estetici
.macro carica_gate num routine dpl

	movq $\num, %rdi
	movq $\routine, %rsi
	movq $\dpl, %rdx
	call init_gate
.endm
					

/ carica la idt
// le prime 20 entrate sono definite dall'Intel, e corrispondono
// alle possibili eccezioni. 
.global init_idt
init_idt:
	//		indice		routine			dpl
	// gestori eccezioni:
	carica_gate	0 		divide_error 	LIV_SISTEMA
	carica_gate	1 		debug 		LIV_SISTEMA
	carica_gate	2 		nmi 		LIV_SISTEMA
	carica_gate	3 		breakpoint 	LIV_SISTEMA
	carica_gate	4 		overflow 	LIV_SISTEMA
	carica_gate	5 		bound_re 	LIV_SISTEMA
	carica_gate	6 		invalid_opcode	LIV_SISTEMA
	carica_gate	7 		dev_na 		LIV_SISTEMA
	carica_gate	8 		double_fault 	LIV_SISTEMA
	carica_gate	9 		coproc_so 	LIV_SISTEMA
	carica_gate	10 		invalid_tss 	LIV_SISTEMA
	carica_gate	11 		segm_fault 	LIV_SISTEMA
	carica_gate	12 		stack_fault 	LIV_SISTEMA
	carica_gate	13 		prot_fault 	LIV_SISTEMA
	carica_gate	14 		int_tipo_pf 	LIV_SISTEMA
	carica_gate	16 		fp_exc 		LIV_SISTEMA
	carica_gate	17 		ac_exc 		LIV_SISTEMA
	carica_gate	18 		mc_exc 		LIV_SISTEMA
	carica_gate	19 		simd_exc 	LIV_SISTEMA

	// driver/handler
	carica_gate	VETT_0 		driver_td	LIV_SISTEMA
	carica_gate	VETT_1 		handler_1	LIV_SISTEMA
	carica_gate	VETT_2 		driver_td	LIV_SISTEMA
	carica_gate	VETT_3 		handler_3	LIV_SISTEMA
	carica_gate	VETT_4 		handler_4	LIV_SISTEMA
	carica_gate	VETT_5 		handler_5	LIV_SISTEMA
	carica_gate	VETT_6 		handler_6	LIV_SISTEMA
	carica_gate	VETT_7 		handler_7	LIV_SISTEMA
	carica_gate	VETT_8 		handler_8	LIV_SISTEMA
	carica_gate	VETT_9 		handler_9	LIV_SISTEMA
	carica_gate	VETT_10		handler_10	LIV_SISTEMA
	carica_gate	VETT_11		handler_11	LIV_SISTEMA
	carica_gate	VETT_12		handler_12	LIV_SISTEMA
	carica_gate	VETT_13		handler_13	LIV_SISTEMA
	carica_gate	VETT_14 	handler_14 	LIV_SISTEMA
	carica_gate	VETT_15 	handler_15 	LIV_SISTEMA
	carica_gate	VETT_16		handler_16	LIV_SISTEMA
	carica_gate	VETT_17		handler_17	LIV_SISTEMA
	carica_gate	VETT_18		handler_18	LIV_SISTEMA
	carica_gate	VETT_19		handler_19	LIV_SISTEMA
	carica_gate	VETT_20		handler_20	LIV_SISTEMA
	carica_gate	VETT_21		handler_21	LIV_SISTEMA
	carica_gate	VETT_22		handler_22	LIV_SISTEMA
	carica_gate	VETT_23		handler_23	LIV_SISTEMA
	carica_gate	VETT_S		handler_24	LIV_SISTEMA

	lidt idt_pointer
	ret

// carica un gate nella IDT
// parametri: (vedere la macro carica_gate)
.global init_gate
init_gate: //rdi = indice nella idt; rsi = offset della routine; rdx = dpl 
	movq $idt, %r11
	movq %rsi, %rax		// offset della routine

	shlq $4, %rdi	//indice moltiplicato la grandezza del gate (16)
	movw %ax, (%r11, %rdi)  	// primi 16 bit dell'offset
	movw $SEL_CODICE_SISTEMA, 2(%r11, %rdi)

	movw $0, %ax
	movb $0b10001110, %ah 	        // byte di accesso
					// (presente, 32bit, tipo interrupt)
	movb %dl, %al		// DPL
	shlb $5, %al			// posizione del DPL nel byte di accesso
	orb  %al, %ah			// byte di accesso con DPL in %ah
	movb $0, %al			// la parte bassa deve essere 0
	movl %eax, 4(%r11, %rdi)	// 16 bit piu' sign. dell'offset
					// e byte di accesso
	shrq $32, %rax			//estensione a 64 bit dell'offset
	movl %eax, 8(%r11,%rdi)
	movl $0, 12(%r11,%rdi) 	//riservato

	ret


// carica un descrittore di segmento in GDT
// parametri: (vedere la macro carica_descr)
.global init_descrittore
init_descrittore:
	pushq %rbp
	movq %rsp, %rbp

	pushq %rbx
	pushq %rcx
	pushq %rax
	pushq %rdx

	movl $gdt, %ebx
	movl 8(%ebp), %ecx		// indice GDT -> %ecx
	movl 16(%ebp), %edx		// limite -> %edx
	movw %dx,  (%ebx, %ecx, 8)	// bit 15:00 limite -> 1a parola descr.
	movw 12(%ebp), %ax		// bit 15:00 base -> %ax
	movw %ax, 2(%ebx, %ecx, 8)	// -> 2a parola descr.
	movb 14(%ebp), %al		// bit 23:16 base -> %al
	orb  24(%ebp), %ah		// DPL
	shlb $5, %ah			// posizione del DPL nel byte di accesso
	orb  20(%ebp), %ah		// bit di presenza
	orb  28(%ebp), %ah		// tipo
	movw %ax, 4(%ebx, %ecx, 8)	// -> 3a parola descr.
	movb 15(%ebp), %dh		// bit 31:24 base -> %dh
	shrl $16, %edx			// bit 19:16 limite -> low nibble %dl
	orb  $0b01000000, %dl		// operandi su 32 bit
	orb  32(%ebp), %dl		// granularita'
	movw %dx, 6(%ebx, %ecx, 8)	// -> 4a parola descr.
	
	popq %rdx
	popq %rax
	popq %rcx
	popq %rbx

	leave
	ret
	
	
.set tss_limit, tss_end - tss 
.set p_dpl_type, 0b10001001
.set dpl_mask, 0b01100000
	.global init_gdt
init_gdt:
	lgdt gdt_pointer
	movq $des_tss, %rdi
	movw $tss_limit, (%rdi) //[15:0] = limit[15:0]
	movq $tss, %rax	
	movw %ax, 2(%rdi)	//[31:16] = base[15:0]
	sarq $16,%rax
	movb %al, 4(%rdi)	//[39:32] = base[24:16]
	movb $p_dpl_type, 5(%rdi)	//[47:40] = p_dpl_type
	movb $0, 6(%rdi)	//[55:48] = 0
	movb %ah, 7(%rdi)	//[63:56] = base[31:24]
	sarq $16, %rax
	movl %eax, 8(%rdi) //[95:64] = base[63:32]
	movl $0, 12(%rdi)	//[127:96] = 0
	movq $(des_tss - gdt), %rax
	ltr %ax

//	popq %rsi
//	movq %rsp, %rdi	//carico il nuovo cs
//	movw %ss,%ax
//	pushq %ax
//	pushq %rdi
//	pushfq
//	xorq %rax,%rax
//	movw $8,%ax
//	pushq %ax
//	pushq %rsi
//	iretq
	retq

	.global set_tss_stack
set_tss_stack:
	movq %rdi,tss+4
	retq

	.global set_tss_dpl
set_tss_dpl:
	movb $dpl_mask, %al
	movq $des_tss, %rsi
	cmpl $0,%edi
	je sistema
	orb %al, 5(%rsi)
	jmp end	
sistema:
	notb %al
	andb %al, 5(%rsi)
end:
	retq
	
	.global iretq_to_user
iretq_to_user:
	movq $(des_tss - gdt), %rax
	ltr %ax
	movq %rdi, %rsp
	iretq

.balign 4096
	.global pag_utente
pag_utente:
	jmp pag_utente_virt
.balign 4096
////////////////////////////////////////////////////////////////
// gestori delle eccezioni				       //
////////////////////////////////////////////////////////////////
// alcune eccezioni lasciano in pila un ulteriore parola lunga
// (il cui significato dipende dal tipo di eccezione)
// Per uniforimita', facciamo eseguire una pushl $0 come
// prima istruzione di tutte le eccezioni che non prevedono
// questa ulteriore parola lunga.
// Inoltre, il trattamento di tutte le eccezioni e' simile:
// inviare un messaggio al log e interrompere il processo
// che ha causato l'eccezione. Per questo motivo, ogni gestore
// mette in pila il numero corrispondente al suo tipo di eccezione
// e salta al codice comune per tutti.
divide_error:
	pushq $0
	pushq $0
	jmp comm_exc

debug:
	pushq $0
	pushq $1
	jmp comm_exc

nmi:
	pushq $0
	pushq $2
	jmp comm_exc

breakpoint:
	pushq $0
	pushq $3
	jmp comm_exc

overflow:
	pushq $0
	pushq $4
	jmp comm_exc

bound_re:
	pushq $0
	pushq $5
	jmp comm_exc

invalid_opcode:
	pushq $0
	pushq $6
	jmp comm_exc

dev_na:
	pushq $0
	pushq $7
	jmp comm_exc

double_fault:
	pushq $8
	jmp comm_exc

coproc_so:
	pushq $0
	pushq $9
	jmp comm_exc

invalid_tss:
	pushq $10
	jmp comm_exc

segm_fault:
	pushq $11
	jmp comm_exc

stack_fault:
	pushq $12
	jmp comm_exc

prot_fault:
	pushq $13
	jmp comm_exc

// l'eccezione di page fault la trattiamo a parte. Vogliamo, infatti, gestirla 
// per realizzare la memoria virtuale. Per far cio', invochiamo la routine 
// c_page_fault passandole tre parametri:
// - la coppia (cs, eip), salvata in pila del meccanismo di eccezione. Tale 
// coppia ci permette di individuare l'istruzione che aveva causato il fault e 
// di sapere se il fault si e' verificato mentre il processore era in stato 
// utente o in stato sistema (se era in stato sistema, si tratta probabilmente 
// di un bug nel nucleo, nel qual caso vogliamo fermare tutto)
// - il contenuto del registro speciale %cr2, che contiene l'indirizzo virtuale 
// non tradotto che ha generato il fault
int_tipo_pf:
	salva_registri
	movq 120(%rsp), %rsi // errore
	movq 128(%rsp), %rdi // eip
	//TODO call c_pre_routine_pf
	carica_registri
	addq $8, %rsp
	iretq
.global readCR2
readCR2:
	movq %cr2, %rax
	retq

fp_exc:
	pushq $0
	pushq $16
	jmp comm_exc

ac_exc:
	pushq $17
	jmp comm_exc

mc_exc:
	pushq $0
	pushq $18
	jmp comm_exc

simd_exc:
	pushq $0
	pushq $19
	jmp comm_exc


comm_exc:
	popq %rdi //tipo
	popq %rsi //errore
	popq %rdx //rip
	popq %rcx //cs
	popq %r8  //rflag
	pushq %r8
	pushq %rcx
	pushq %rdx
	call gestore_eccezioni
	//jmp a_abort_p
	hlt


////////////////////////////////////////////////////////
// handler/driver                                     //
////////////////////////////////////////////////////////
//
// driver del timer	[4.16]
	.extern c_driver_td
handler_msg:
	.asciz "handler %d"
driver_td:
	salva_registri
	call c_driver_td
	call apic_send_EOI
	carica_registri
	iretq

handler_1:
	salva_registri
	carica_registri
	iretq

handler_2:
	salva_registri
	carica_registri
	iretq

handler_3:
	salva_registri
	carica_registri
	iretq

handler_4:
	salva_registri
	carica_registri
	iretq
	
handler_5:
	salva_registri
	carica_registri
	iretq

handler_6:
	salva_registri
	carica_registri
	iretq

handler_7:
	salva_registri
	carica_registri
	iretq

handler_8:
	salva_registri
	carica_registri
	iretq

handler_9:
	salva_registri
	carica_registri
	iretq

handler_10:
	salva_registri
	carica_registri
	iretq

handler_11:
	salva_registri
	carica_registri
	iretq

handler_12:
	salva_registri
	carica_registri
	iretq

handler_13:
	salva_registri
	carica_registri
	iretq

handler_14:
	salva_registri
	carica_registri
	iretq
	
handler_15:
	salva_registri
	carica_registri
	iretq

handler_16:
	salva_registri
	carica_registri
	iretq

handler_17:
	salva_registri
	carica_registri
	iretq

handler_18:
	salva_registri
	carica_registri
	iretq

handler_19:
	salva_registri
	carica_registri
	iretq

handler_20:
	salva_registri
	carica_registri
	iretq

handler_21:
	salva_registri
	carica_registri
	iretq

handler_22:
	salva_registri
	carica_registri
	iretq

handler_23:
	salva_registri
	carica_registri
	iretq

handler_24:
	salva_registri
	carica_registri
	iretq



	.global invalida_TLB  // [6.6]
invalida_TLB:
	movq %cr3, %rax
	movq %rax, %cr3
	retq

// carica il registro cr3
// parametri: indirizzo fisico del nuovo direttorio
	.global loadCR3
loadCR3:
	movq %rdi, %cr3
	retq

// restituisce in %eax il contenuto di cr3
	.global readCR3
readCR3:
	movq %cr3, %rax
	retq
/

//TIMER
.set CWR,     0x43
.set CTR_LSB, 0x40
.set CTR_MSB, 0x40

// attiva il timer di sistema
// parametri: il valore da caricare nel registro CTR del timer
.extern ioapic_set_MIRQ
	.global attiva_timer
attiva_timer:
	movb $0x36, %al
	outb %al, $CWR
	movl %edi, %eax
	outb %al, $CTR_LSB
	movb %ah, %al
	outb %al, $CTR_MSB

	movq $2, %rdi
	call ioapic_unmask

	retq

.global panic
panic:
	hlt

////////////////////////////////////////////////////////////////
// sezione dati: tabelle e stack			      //
////////////////////////////////////////////////////////////////
.data
.global		fine_codice_sistema
fine_codice_sistema:
		.long etext
end_possibili_pf:
		.long 0
.global		ticks
ticks:		.long 0
.global		clocks_per_usec
clocks_per_usec:
		.long 1
.global ioapic
ioapic:
	.quad 0x00000000FEC00000	//ioapic.IOREGSEL
	.quad 0x00000000FEC00010	//ioapic.IOWIN
pEOI:	.quad 0x00000000FEE000B0	//ioapic.EOI
	.byte 0x10		//ioapic.RTO
	.space 3, 0

	// puntatori alle tabelle GDT e IDT
	// nel formato richiesto dalle istruzioni LGDT e LIDT
gdt_pointer:
	.word end_gdt-gdt		 	// limite della GDT
	.quad gdt			// base della GDT
idt_pointer:
	.word 0xFFF			// limite della IDT (256 entrate)
	.quad idt			// base della IDT

.balign 8
gdt:
	.quad 0		//segmento nullo
	.quad 0x209c0000000000 //segmento codice sistema
	.quad 0x20fc0000000000 //segmento codice utente
des_tss:
	.space 16*4,0	//segmento tss, riempito a runtime
end_gdt:
	
tss:
	.space 108, 0
tss_end:

.balign 16
idt:
	// spazio per 256 gate
	// verra' riempita a tempo di esecuzione
	.space 16 * 256, 0
end_idt:

	.global pag_utente_virt
pag_utente_virt:
	.quad pag_utente
	.global stack
stack:
	.space STACK_SIZE, 0
