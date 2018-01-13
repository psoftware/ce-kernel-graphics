// sistema.s

#include "costanti.h"

//////////////////////////////////////////////////////////////////////////
// AVVIO                                                                  //
//////////////////////////////////////////////////////////////////////////

#define STACK_SIZE                      0x1000

.globl  _start, start
_start:				// entry point
start:
	// usiamo la pila che abbiamo messo da parte
	movq $(stack + STACK_SIZE), %rsp
	// inizializziamo la IDT
	call init_idt
	// Il C++ prevede che si possa eseguire del codice prima di main (per
	// es. nei costruttori degli oggetti globali). gcc organizza questo
	// codice in una serie di funzioni di cui raccoglie i puntatori
	// nell'array __init_array_start. Il C++ run time deve poi chiamare
	// tutte queste funzioni prima di saltare a main.  Poiche' abbiamo
	// compilato il modulo con -nostdlib, dobbiamo provvedere da soli a
	// chiamare queste funzioni, altrimenti gli oggetti globali non saranno
	// inizializzati correttamente.
	movabs $__init_array_start, %rax
	movq %rax, %rbx
1:	cmpq $__init_array_end, %rbx
	je 2f
	call *(%rbx)
	addq $8, %rbx
	jmp 1b
	// il resto dell'inizializzazione e' scritto in C++
2:	call cmain
	// se arrivamo qui c'e' stato un errore, fermiamo la macchina
	hlt			

// macro per estrarre la base da un descrittore di TSS
// si aspetta l'indirizzo del segmento in %rax e
// lascia il risultato in %rbx
.macro estrai_base
	movl 8(%eax), %ebx
	shlq $32, %rbx      	// bit 63:32 nella parte alta di %rbx
	movb 7(%eax), %bh	// bit 31:24 della base in %bh
	movb 4(%eax), %bl	// bit 23:16 della base in %bl
	shll $16, %ebx		// bit 31:16 nella parte alta di %ebx
	movw 2(%eax), %bx	// bit 15:0 nella parte basse di %ebx
.endm

// offset dei vari registri all'interno di des_proc
.set CR3,104
.set RAX,CR3+8
.set RCX,CR3+16
.set RDX,CR3+24
.set RBX,CR3+32
.set RSP,CR3+40
.set RBP,CR3+48
.set RSI,CR3+56
.set RDI,CR3+64
.set R8, CR3+72
.set R9, CR3+80
.set R10,CR3+88
.set R11,CR3+96
.set R12,CR3+104
.set R13,CR3+112
.set R14,CR3+120
.set R15,CR3+128

salva_stato:
	pushq %rbx
	pushq %rax

	movq esecuzione,%rax
	movq $0, %rbx
	movw (%rax),%bx		// campo id in proc_elem
	leaq gdt(%rbx), %rax	// indirizzo della corrispondente
				// entrata nella GDT -> %rax
	estrai_base		// base del des_proc -> %rbx

	popq %rax

	movq %rax, RAX(%rbx)
	movq %rcx, RCX(%rbx)
	movq %rdx, RDX(%rbx)
	popq %rax
	movq %rax, RBX(%rbx)
	movq %rsp, %rax
	addq $8, %rax   	//non includiamo l'indirizzo di ritorno
	movq %rax, RSP(%rbx)
	movq %rbp, RBP(%rbx)
	movq %rsi, RSI(%rbx)
	movq %rdi, RDI(%rbx)
	movq %r8,  R8 (%rbx)
	movq %r9,  R9 (%rbx)
	movq %r10, R10(%rbx)
	movq %r11, R11(%rbx)
	movq %r12, R12(%rbx)
	movq %r13, R13(%rbx)
	movq %r14, R14(%rbx)
	movq %r15, R15(%rbx)

	ret


carica_stato:
	
	// otteniamo la base del des_proc del processo in esecuzione
	// (come per salva_stato)
	movq esecuzione, %rdx
	movq $0, %rbx
	movw (%rdx), %bx
	movq %rbx, %rcx
	leaq gdt(%rbx), %rax
	estrai_base

	// carichiamo TR con l'id del nuovo processo
	// (in modo che il meccanismo delle interruzioni usi la
	// pila sistema del nuovo processo)
	andb $0b11111101, 5(%rax)	// reset del bit BUSY
					// (richiesto dal processore
					// per compatibilita' con il modo
					// a 32 bit)
	ltr %cx

	popq %rcx   //ind di ritorno, va messo nella nuova pila

	// nuovo valore per cr3
	movq CR3(%rbx), %rax
	movq %rax, %cr3		// il TLB viene invalidato

	// anche se abbiamo cambiato cr3 siamo sicuri che
	// l'esecuzione prosegue da qui, perche' ci troviamo dentro
	// la finestra FM che e' comune a tutti i processi
	movq RSP(%rbx), %rsp    //cambiamo pila
	pushq %rcx              //rimettiamo l'indirizzo di ritorno

	movq RCX(%rbx), %rcx
	movq RDI(%rbx), %rdi
	movq RSI(%rbx), %rsi
	movq RBP(%rbx), %rbp
	movq RDX(%rbx), %rdx
	movq RAX(%rbx), %rax
	movq R8(%rbx), %r8
	movq R9(%rbx), %r9
	movq R10(%rbx), %r10
	movq R11(%rbx), %r11
	movq R12(%rbx), %r12
	movq R13(%rbx), %r13
	movq R14(%rbx), %r14
	movq R15(%rbx), %r15
	movq RBX(%rbx), %rbx

	retq


// alloca_tss: usata alla creazione di un processo
// cerca un descrittore di TSS libero, lo inizializza
// ponendo come base l'indirizzo del des_proc passato
// come argomento. Restituisce l'offset del descrittore
// TSS allocato, che funge da identificatore del processo
.set p_dpl_type, 0b10001001 //p=1,dpl=00,type=1001=tss ready
.set pres_bit,   0b10000000
	.global alloca_tss
alloca_tss:
	movq last_tss, %rdx
iter_tss:
	// usiamo il bit di presenza nel descrittore per
	// distiunguere i descrittori liberi da quelli allocati
	testb $pres_bit, 5(%rdx)
	jz set_entry_tss	// libero, saltiamo all'inizializzazione
	addq $16, %rdx		// occupato, passiamo al prossimo
	cmpq $end_gdt, %rdx
	jne advance_tss
	movq $des_tss, %rdx
advance_tss:
	cmpq last_tss, %rdx
	jne iter_tss
	movq $0, %rax		// terminati, restituiamo 0
	jmp end_tss
set_entry_tss:
	movq %rdx, last_tss
	movw $DIM_DESP, (%rdx) 	//[15:0] = limit[15:0]
	decw (%rdx)
	movq %rdi, %rax
	movw %ax, 2(%rdx)	//[31:16] = base[15:0]
	shrq $16,%rax
	movb %al, 4(%rdx)	//[39:32] = base[24:16]
	movb $p_dpl_type, 5(%rdx)	//[47:40] = p_dpl_type
	movb $0, 6(%rdx)	//[55:48] = 0
	movb %ah, 7(%rdx)	//[63:56] = base[31:24]
	shrq $16, %rax
	movl %eax, 8(%rdx) 	//[95:64] = base[63:32]
	movl $0, 12(%rdx)	//[127:96] = 0

	movq %rdx,%rax
	subq $gdt, %rax
end_tss:
	retq

// rilascia_tss: usata alla terminazione di un processo
// rende nuovamente libero il descrittore TSS associato al processo
// il cui identificatore e' passato come argomento
	.global rilascia_tss
rilascia_tss:
	movq $0, gdt(%rdi)
	addq $8, %rdi
	movq $0, gdt(%rdi)
	retq

// dato l'identificatore di un processo,
// ne restituisce il puntatore al descrittore
// (0 se non allocato)
	.global des_p
des_p:
	pushq %rbx
	xorq %rbx, %rbx
	leaq gdt(%rdi), %rax
	testb $pres_bit, 5(%rax)
	jz 1f
	estrai_base
1:	movq %rbx, %rax
	popq %rbx
	retq

// dato un indirizzo virtuale (come parametro) usa l'istruzione invlpg per
// eliminare la corrispondente traduzione dal TLB
	.global invalida_entrata_TLB //
invalida_entrata_TLB:
	invlpg (%rdi)
	ret

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

	//primitive
	carica_gate	TIPO_A		a_activate_p	LIV_UTENTE
	carica_gate	TIPO_T		a_terminate_p	LIV_UTENTE
	carica_gate	TIPO_SI		a_sem_ini	LIV_UTENTE
	carica_gate	TIPO_W		a_sem_wait	LIV_UTENTE
	carica_gate	TIPO_S		a_sem_signal	LIV_UTENTE
	carica_gate	TIPO_D		a_delay		LIV_UTENTE
	carica_gate	TIPO_L		a_log		LIV_UTENTE
	carica_gate	TIPO_EP		a_end_program	LIV_SISTEMA

	// primitive per il livello I/O
	carica_gate	TIPO_APE	a_activate_pe	LIV_SISTEMA
	carica_gate	TIPO_WFI	a_wfi		LIV_SISTEMA
	carica_gate	TIPO_FG		a_fill_gate	LIV_SISTEMA
	carica_gate	TIPO_P		a_panic		LIV_SISTEMA
	carica_gate	TIPO_AB		a_abort_p	LIV_SISTEMA
	carica_gate	TIPO_TRA	a_trasforma	LIV_SISTEMA

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



	.global init_gdt
init_gdt:
	lgdt gdt_pointer

	retq

////////////////////////////////////////////////////////
// a_primitive                                        //
////////////////////////////////////////////////////////
        .extern c_activate_p
a_activate_p:	// routine int $tipo_a
	cavallo_di_troia %rdi
        call c_activate_p
        iretq

        .extern c_terminate_p
a_terminate_p:	// routine int $tipo_t
	movq $terminate_stack_end, %rsp
        call c_terminate_p
	call carica_stato
	iretq

	.extern c_sem_ini
a_sem_ini:	// routine int $tipo_si
	call c_sem_ini
	iretq

	.extern c_sem_wait
a_sem_wait:	// routine int $tipo_w
	call salva_stato
	call c_sem_wait
	call carica_stato
	iretq

	.extern c_sem_signal
a_sem_signal:	// routine int $tipo_s
	call salva_stato
	call c_sem_signal
	call carica_stato
	iretq

	.extern c_delay
a_delay:	// routine int $tipo_d
	call salva_stato
	call c_delay
	call carica_stato
	iretq


//
// Interfaccia offerta al modulo di IO, inaccessibile dal livello utente
//

	.extern c_activate_pe
a_activate_pe:
	cavallo_di_troia %rdi
        call c_activate_pe
	iretq


a_wfi:		// routine int $tipo_wfi
	call salva_stato
	call apic_send_EOI
	call schedulatore
	call carica_stato
	iretq

a_fill_gate:
	call init_gate
	iretq

	.extern c_panic
a_panic:	// routine int $tipo_p
	call salva_stato
	//cavallo_di_troia 1
	movq %rsp, %rsi
	call c_panic
1:	nop
	jmp 1b

	.extern c_abort_p
a_abort_p:
	movq $terminate_stack_end, %rsp
        call c_abort_p
	call carica_stato
	iretq

	.extern c_trasforma
a_trasforma:
	call c_trasforma
	iretq
	.extern c_log
a_log:
	call salva_stato
	//cavallo_di_troia 1
	//cavallo_di_troia2 1 2
	call c_log
	call carica_stato
	iretq

a_end_program:
	lidt triple_fault_idt
	int $1

////////////////////////////////////////////////////////////////
// gestori delle eccezioni				       //
////////////////////////////////////////////////////////////////
// alcune eccezioni lasciano in pila un ulteriore quad
// (il cui significato dipende dal tipo di eccezione)
// Per uniforimita', facciamo eseguire una pushq $0 come
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
	call c_nmi

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
	pushq %rdi
	pushq %rsi
	movq 16(%rsp), %rdi // errore
	movq 24(%rsp), %rsi // eip
	// salviamo gli altri registri scratch non salvati dal C++
	pushq %rax
	pushq %rdx
	pushq %rcx
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	call c_pre_routine_pf
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rcx
	pop %rdx
	pop %rax
	popq %rsi
	popq %rdi
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
	movq (%rsp), %rdi //tipo
	movq 8(%rsp), %rsi //errore
	leaq 16(%rsp), %rdx
	call gestore_eccezioni
	jmp a_abort_p


////////////////////////////////////////////////////////
// handler/driver                                     //
////////////////////////////////////////////////////////
//


// driver del timer
	.extern c_driver_td
driver_td:
	call salva_stato
	call c_driver_td
	call apic_send_EOI
	call carica_stato

	iretq

.macro handler n
handler_\n:
	call salva_stato
	call inspronti

	movq $\n, %rcx
	movq a_p(, %rcx, 8), %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
.endm

handler 1
handler 2
handler 3
handler 4
handler 5
handler 6
handler 7
handler 8
handler 9
handler 10
handler 11
handler 12
handler 13
handler 14
handler 15
handler 16
handler 17
handler 18
handler 19
handler 20
handler 21
handler 22
handler 23
handler 24

	.global invalida_TLB  //
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


//TIMER
.set CWR,     0x43
.set CTR_LSB, 0x40
.set CTR_MSB, 0x40

// attiva il timer di sistema
// parametri: il valore da caricare nel registro CTR del timer
.extern apic_set_MIRQ
	.global attiva_timer
attiva_timer:
	movb $0x36, %al
	outb %al, $CWR
	movl %edi, %eax
	outb %al, $CTR_LSB
	movb %ah, %al
	outb %al, $CTR_MSB

	movq $2, %rdi
	movq $0, %rsi
	call apic_set_MIRQ

	retq

//////////////////////////////////////////////////////////
// primitive richiamate dal nucleo stesso	        //
//////////////////////////////////////////////////////////
	.global sem_ini
sem_ini:
	int $TIPO_SI
	ret

	.global sem_wait
sem_wait:
	int $TIPO_W
	ret

	.global activate_p
activate_p:
	int $TIPO_A
	ret

	.global delay
delay:
	int $TIPO_D
	ret

	.global terminate_p
terminate_p:
	int $TIPO_T
	ret

	.global panic
panic:
	int $TIPO_P
	ret

	.global abort_p
abort_p:
	int $TIPO_AB
	ret

	.global wfi
wfi:
	int $TIPO_WFI
	ret

	.global end_program
end_program:
	int $TIPO_EP
	ret

	.global salta_a_main
salta_a_main:
	call carica_stato		// carichiamo tr
	iretq				// torniamo al chiamante "trasformati" in processo

////////////////////////////////////////////////////////////////
// sezione dati: tabelle e stack			      //
////////////////////////////////////////////////////////////////
.data
.global		fine_codice_sistema
fine_codice_sistema:
	.quad etext
	// puntatori alle tabelle GDT e IDT
	// nel formato richiesto dalle istruzioni LGDT e LIDT
gdt_pointer:
	.word end_gdt-gdt		 	// limite della GDT
	.quad gdt			// base della GDT
idt_pointer:
	.word 0xFFF			// limite della IDT (256 entrate)
	.quad idt			// base della IDT
triple_fault_idt:
	.word 0
	.quad 0
param_err:
	.asciz "indirizzo non valido: %p"

.balign 8
.global gdt
gdt:
	.quad 0		//segmento nullo
code_sys_seg:
	.word 0b0           //limit[15:0]   not used
	.word 0b0           //base[15:0]    not used
	.byte 0b0           //base[23:16]   not used
	.byte 0b10011010    //P|DPL|1|1|C|R|A|  DPL=00=sistema
	.byte 0b00100000    //G|D|L|-|-------|  L=1 long mode
	.byte 0b0           //base[31:24]   not used
code_usr_seg:
	.word 0b0           //limit[15:0]   not used
	.word 0b0           //base[15:0]    not used
	.byte 0b0           //base[23:16]   not used
	.byte 0b11111010    //P|DPL|1|1|C|R|A|  DPL=11=utente
	.byte 0b00100000    //G|D|L|-|-------|  L=1 long mode
	.byte 0b0           //base[31:24]   not used
data_usr_seg:
	.word 0b0           //limit[15:0]   not used
	.word 0b0           //base[15:0]    not used
	.byte 0b0           //base[23:16]   not used
	.byte 0b11110010    //P|DPL|1|0|E|W|A|  DPL=11=utente
	.byte 0b00000000    //G|D|-|-|-------|
	.byte 0b0           //base[31:24]   not used
des_tss:
	.space 16*NUM_TSS,0	//segmento tss, riempito a runtime
end_gdt:

last_tss:
.quad	des_tss

.bss
.balign 16
idt:
	// spazio per 256 gate
	// verra' riempita a tempo di esecuzione
	.space 16 * 256, 0
end_idt:

	.global stack
stack:
	.space STACK_SIZE, 0
terminate_stack:
	.space STACK_SIZE, 0
terminate_stack_end:
