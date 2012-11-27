// sistema.s

#define ASM 1
#include "mboot.h"
#include "costanti.h"


////////////////////////////////////////////////////////////////
// MACRO                                                      //
////////////////////////////////////////////////////////////////
// definiamo delle macro che verranno utilizzate piu' avanti

// salvataggio dei registri in pila
.macro salva_registri

	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp

.endm

// caricamento dei registri dalla pila (duale rispetto a salva_registri)
.macro carica_registri

	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx
	popl %eax

.endm

// salvataggio dei registri in pila per chiamate di sistema che ritornano
//  un valore in eax, che non viene salvato
.macro salva_reg_rit

	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp

.endm


// ripristino dei registri (tutti meno eax) dalla pila (duale rispetto a
//  salva_reg_rit)
.macro carica_reg_rit

	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx

.endm

// copia i parametri dalla pila utente (o sistema) 
// alla pila sistema, da usare nelle a_primitive
// n_long: numero di parole lunghe da copiare
// n_reg: numero di registri salvati in cima alla pila
//        (questa informazione e' necessaria, in 
//         quanto copia_param verra' chiamata dopo
//         il salvataggio dei registri in pila, e tale
//         numero varia da primitiva a primitiva)
.macro copia_param n_long n_reg

        movl $\n_reg, %ecx
        movw 4(%esp, %ecx, 4), %dx      // CS in DX
        andb $0b00000011, %dl	        // CPL del chiamante in CL
	cmpb $LIV_SISTEMA, %dl		// se sistema, non c'e' stato cambio pila
        je 1f                           // copia da pila sistema 
        movl 12(%esp, %ecx, 4), %eax    // vecchio ESP (della pila utente) in EAX
        leal 4(%eax), %esi		// indirizzo del primo parametro in ESI
        jmp 2f
1:	leal 16(%esp, %ecx, 4), %esi	// indirizzo del primo parametro in ESI
2:	movl $\n_long, %eax		// creazione in pila dello spazio per
        shll $2, %eax			//  la copia dei parametri
        subl %eax, %esp
        leal (%esp), %edi		// indirizzo della destinazione del
					//  primo parametro in EDI
        movl $\n_long, %ecx
        cld
        rep
	movsl				// copia dei parametri

.endm

// DL livello
.macro check_cavallo
	cmpb $LIV_SISTEMA, %dl
	je 1f
	testl $0x80000000, %eax
	jnz 1f
	pushl %eax
	pushl $param_err
	pushl $2
	call flog
	addl $12, %esp
	int $tipo_ab
1:

.endm

.macro cavallo_di_troia offset

	movl (\offset * 4) (%esp), %eax
	check_cavallo
.endm

.macro cavallo_di_troia2 offset1 offset2

	movl (\offset1 * 4) (%esp), %eax
	addl (\offset2 * 4) (%esp), %eax
	decl %eax
	check_cavallo
.endm


// Carica un gate della IDT
// num: indice (a partire da 0) in IDT del gate da caricare
// routine: indirizzo della routine da associare al gate
// dpl: dpl del gate (LIV_SISTEMA o LIV_UTENTE)
// NOTA: la macro si limita a chiamare la routine init_gate
//       con gli stessi parametri. Verra' utilizzata per
//       motivi puramente estetici
.macro carica_gate num routine dpl

	pushl $\dpl
	pushl $\routine
	pushl $\num
	call init_gate
	addl $12, %esp

.endm

// campi in un descrittore di segmento
.set PRES, 0b10000000
.set NON_P, 0b00000000
.set SEG_CODICE, 0b00011010
.set SEG_DATI, 0b00010010
.set SEG_TSS, 0b00001001
.set G_PAGINA, 0b10000000
.set G_BYTE, 0b00000000

// Carica un descrittore della GDT
// num: indice (a partire da 0) in GDT del descrittore da caricare
// base: base del segmento
// limite: campo limite (su 20 bit)
// pres: bit di presenza (usare le costanti PRES e NON_P)
// dpl: dpl del segmento (usare le costanti LIV_SISTEMA o LIV_UTENTE)
// tipo: tipo del gate (usare le costanti SEG_CODICE, SEG_DATI o SEG_TSS)
// gran: granularita' (usare le costanti G_PAGINA o G_BYTE)
// NOTA: la macro si limita a chiamare la routine init_descrittore
//       con gli stessi parametri. Verra' utilizzata per
//       motivi puramente estetici
.macro carica_descr num base limite pres dpl tipo gran

	pushl $\gran
	pushl $\tipo
	pushl $\dpl
	pushl $\pres
	pushl $\limite
	pushl $\base
	pushl $\num
	call  init_descrittore
	addl $28, %esp

.endm

// Estrae la base del segmento da un descrittore.
// Si aspetta l'indirizzo del descrittore in %eax,
// lascia la base del segmento in %ebx
// NOTA: il formato dei descrittori di segmento dei 
//       processori Intel x86, per motivi di compatibilita'
//       con i processori Intel 286 (che erano a 16 bit),
//       e' piu' complicato di quello visto a lezione.
//       In particolare, i byte che compongono il campo base
//       non sono consecutivi
.macro estrai_base 

        movb 7(%eax), %bh	// bit 31:24 della base in %bh
        movb 4(%eax), %bl	// bit 23:16 della base in %bl
        shll $16, %ebx		// bit 31:16 nella parte alta di %ebx
	movw 2(%eax), %bx	// bit 15:0 nella parte basse di %ebx
	
.endm


//////////////////////////////////////////////////////////////////////////
// AVVIO                                                                  //
//////////////////////////////////////////////////////////////////////////
// Il bootstrap loader attiva il modo protetto (per poter accedere agli
// indirizzi di memoria principale superiori a 1MiB) e carica il sistema
// in memoria, quindi salta alla prima
// istruzione del sistema. Il bootstrap loader puo' anche passare
// delle informazioni al sistema (tramite i registri e la memoria).
//
// In questo sistema usiamo lo standard multiboot, che definisce il formato che 
// il file contentente il sistema deve rispettare e
// il formato delle informazioni passate dal bootstrap loader al sistema.
// Il formato del file contenente il sistema deve essere quello di un
// normale file eseguibile, ma, nei primi 2*4KiB, deve contenere 
// la struttura multiboot_header, definita piu' avanti. 
// Il boot loader, prima di saltare alla prima istruzione del sistema
// (l'entry point specificato nel file eseguibile), lascia nel registro
// %eax un valore di riconoscimento e in %ebx l'indirizzo di una struttura
// dati, contentente varie informazioni (in particolare, la quantita'
// di memoria principale installata nel sistema, il dispositivo da cui
// e' stato eseguito il bootstrap e l'indirizzo di memoria in cui sono
// stati caricati gli eventuali moduli)
     .text

     .globl  _start, start
_start:				// entry point
start:
     jmp     multiboot_entry	// scavalchiamo la struttra richiesta
     				// dal bootstrap loader, che deve
				// trovarsi verso l'inizio del file

     .align  4
     // le seguenti informazioni sono richieste dal bootstrap loader
multiboot_header:
     .long   MULTIBOOT_HEADER_MAGIC 				// valore magico
     .long   MULTIBOOT_HEADER_FLAGS   				// flag
     .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS) // checksum
#ifndef _ELF__
     .long   multiboot_header
     .long   start
     .long   edata
     .long   end
     .long   multiboot_entry
#endif 

multiboot_entry:
     movl    %esp, save_esp
     movl    $(stack + STACK_SIZE), %esp // inizializziamo lo stack

     pushf
     pushal
     pushl   %ds
     pushl   %es
     pushl   %fs
     pushl   %gs
     pushl   %ss

     call    init_gdt
     sgdt    save_gdt
     lgdt    gdt_pointer		// carichiamo la nuova GDT

     ljmp    $SEL_CODICE_SISTEMA, $qui	// ricarichiamo CS
qui:
     movw    $SEL_DATI_SISTEMA,   %cx   // e gli altri selettori
     movw    %cx, %ss
     movw    %cx, %ds
     movw    %cx, %es
     movw    $0,  %cx			// FS e GS non sono usati
     movw    %cx, %fs
     movw    %cx, %gs

     movl    $0, %ebp			// azzeriamo il base pointer
					// (utile per capire dove finisce la 
					// lista dei frame di attivazione)

     call    init_idt			// riempie i gate per le eccezioni
     sidt    save_idt
     lidt    idt_pointer		// carichiamo la nuova IDT
	
     pushl   $0				// resettiamo EFLAG
     popf				// N.B.: interrupt disabilitati
					// (perche', azzerando tutti i flag,
					// abbiamo azzerato anche IF)
     pushl   %ebx			// parametri passati dal loader
     pushl   %eax			
     call    cmain			// il resto dell'inizializzazione
     					// e' scritto in C++
     addl    $8, %esp     
	
     lidt   save_idt
     lgdt   save_gdt
     popl   %ss
     popl   %gs
     popl   %fs
     popl   %es
     popl   %ds
     popal
     popf

     movl save_esp, %esp
     ret


//////////////////////////////////////////////////////////////////
// funzioni di utilita'						//
//////////////////////////////////////////////////////////////////

// stampa sulla console lo stack degli indirizzi di ritorno
// attualmente in pila (utile per il debugging)
stringa_backtrace:
	.asciz "%x  "
buf_backtrace:
	.fill 80
count_backtrace:
	.long 0
off_backtrace:
	.long 0
	.global backtrace
backtrace:
	pushl %esi
	pushl %eax

	movl 12(%esp), %eax
	movl %eax, off_backtrace

	movl $20, count_backtrace
	movl %ebp, %esi
3:
	cmpl $0, count_backtrace
	je 2f
	cmpl $0, %esi
	je 2f
	cmpl $0x8000000, %esi
	jbe 2f

	pushl 4(%esi)
	pushl $stringa_backtrace
	pushl $80
	pushl $buf_backtrace
	call snprintf
	addl $16, %esp
	pushl %eax
	pushl $buf_backtrace
	pushl off_backtrace
	call writevid_n
	addl $12, %esp
	addl $10, off_backtrace
	
	movl (%esi), %esi
	decl count_backtrace
	jmp 3b
2:
	popl %eax
	popl %esi
	ret
	

// inserimento del processo in esecuzione in testa alla coda dei pronti
//  (non salva i registri, viene chiamata dopo salva_stato)
//
inspronti:
        movl esecuzione, %eax
        movl pronti, %ebx
        movl %ebx, 8(%eax)
        movl %eax, pronti
	ret

// offset, all'interno della struttura des_proc, dei campi
// destinati a contere i registri del processore
.set EAX, 40
.set ECX, 44
.set EDX, 48
.set EBX, 52
.set ESP, 56
.set EBP, 60
.set ESI, 64
.set EDI, 68
.set ES, 72
.set SS, 80 
.set DS, 84
.set FS, 88
.set GS, 92

.set CR3, 28

.set FPU, 104

// salva lo stato del processo corrente nel suo descrittore [4.8]
salva_stato:
        pushl %ebx
	pushl %eax

	movl esecuzione, %eax
	movl $0, %ebx
        movw (%eax), %bx		// esecuzione->identifier in ebx
	shrl $3, %ebx			// trasformo selettore->indice
        leal gdt(, %ebx, 8), %eax       // ind. entrata della gdt relativa in eax
	estrai_base			// ind. TSS -> %ebx

        popl %eax

	movl %eax, EAX(%ebx)		// salvataggio dei registri
	movl %ecx, ECX(%ebx)
	movl %edx, EDX(%ebx)
	popl %eax			// vecchio valore di %ebx in %eax
	movl %eax, EBX(%ebx)
	movl %esp, %eax
	addl $4, %eax			// salviamo ind. rit. di salva_stato...
	movl %eax, ESP(%ebx)		// ... prima di memorizzare %esp
	movl %ebp, EBP(%ebx)
	movl %esi, ESI(%ebx)
	movl %edi, EDI(%ebx)
	movw %es, ES(%ebx)
	movw %ss, SS(%ebx)
	movw %ds, DS(%ebx)
	movw %fs, FS(%ebx)
	movw %gs, GS(%ebx)

	movw $SEL_DATI_SISTEMA, %ax		// selettori usati dal nucleo
	movw %ax, %ds
	movw %ax, %es
	// ss contiene gia' il valore corretto
	movw $0, %ax
	movw %ax, %fs
	movw %ax, %gs

	fsave FPU(%ebx)

	ret

// carica lo stato del processo in esecuzione [4.8]
carica_stato:
        movl esecuzione, %edx
	movl $0, %ebx
        movw (%edx), %bx		// esecuzione->identifier in ebx
	
	movl %ebx, %ecx			
	shrl $3, %ebx			// trasformo selettore->indice

        leal gdt(, %ebx, 8), %eax       // ind. entrata della gdt relativa in eax
	estrai_base			// ind. del TSS in %ebx
        andl $0xfffffdff, 4(%eax)       // bit busy del TSS a zero

       	ltr %cx				// nuovo valore in TR

        frstor FPU(%ebx)

	movw GS(%ebx), %ax		// ripristino dei registri
	movw %ax, %gs
	movw FS(%ebx), %ax
	movw %ax, %fs
	movw DS(%ebx), %ax
	movw %ax, %ds
	movw SS(%ebx), %ax
	movw %ax, %ss
	movw ES(%ebx), %ax
	movw %ax, %es

	popl %ecx                       // toglie dalla pila l' ind. di ritorno

	movl CR3(%ebx), %eax		// cambio di direttorio
	movl %eax, %cr3			// NOTA: siamo sicuri della continuita'
					// dell'indirizzamento, in quanto il sistema
					// e' mappato agli stessi indirizzi in tutti
					// gli spazi di memoria

	movl ESP(%ebx), %esp            // nuovo punt. di pila...
	pushl %ecx			// salvataggio ind. di ritorno nella nuova pila

	movl ECX(%ebx), %ecx
	movl EDI(%ebx), %edi
	movl ESI(%ebx), %esi
	movl EBP(%ebx), %ebp
	movl EDX(%ebx), %edx
	movl EAX(%ebx), %eax
	movl EBX(%ebx), %ebx

	ret

// carica la gdt
// Utilizziamo il modello di memoria flat: usiamo solo 4 segmenti 
// sovrapposti, grandi quanto tutto lo spazio di indirizzamento linerare (4GB)
// Due segmenti (indici 1 e 2) servono per il livello di privilegio sistema
// e due (indici 3 e 4) per il livello di privilegio utente.
// Il segmento di indice 0 deve essere nullo.
// A partire dal descrittore di indice 5 caricheremo i descrittori di segmento 
// TSS, ognuno associato alla corrispondente struttura des_proc.
// Usiamo il bit P di presenza nel descrittore per specificare quali 
// descrittori sono effettivamente utilizzati.
// Inizialmente, tutti i bit di presenza sono 0 e passeranno ad 1 quando 
// verrano invocate le primitive activate_p e activate_pe
init_gdt:
	pushl %ebp
	movl %esp, %ebp
	pushl %edi
	pushl %ecx
	pushl %eax

	// azzeriamo la gdt
	movl $gdt, %edi
	movl $0x4000, %ecx
	movl $0, %eax
	rep
	stosl


		//indice	base	limite	P	DPL		TIPO		
	carica_descr	0	0	0	NON_P	0		0		0
	carica_descr	1	0	0xfffff	PRES	LIV_SISTEMA	SEG_CODICE	G_PAGINA
	carica_descr	2	0	0xfffff	PRES	LIV_SISTEMA	SEG_DATI	G_PAGINA
	carica_descr	3	0	0xfffff	PRES	LIV_UTENTE	SEG_CODICE	G_PAGINA
	carica_descr	4	0	0xfffff	PRES	LIV_UTENTE	SEG_DATI	G_PAGINA

	popl %eax
	popl %ecx
	popl %edi
	leave
	ret

// Tipi delle primitive di sistema
.set tipo_a, TIPO_A
.set tipo_t, TIPO_T
.set tipo_g, TIPO_G
.set tipo_si, TIPO_SI
.set tipo_w, TIPO_W
.set tipo_s, TIPO_S
.set tipo_ma, TIPO_MA
.set tipo_mf, TIPO_MF
.set tipo_d, TIPO_D
.set tipo_rl, TIPO_RL
.set tipo_re, TIPO_RE
.set tipo_ep, TIPO_EP
.set tipo_tra, TIPO_TRA

// Tipi delle interruzioni usate per l' interfaccia con il modulo di IO
.set tipo_ape, TIPO_APE
.set tipo_nwfi, TIPO_NWFI
.set tipo_va, TIPO_VA
.set tipo_fg, TIPO_FG
.set tipo_p, TIPO_P
.set tipo_ab, TIPO_AB
.set tipo_l,  TIPO_L

// interruzioni usate dal nucleo stesso (dalle routine della memoria virtuale)
.set io_tipo_hdr, IO_TIPO_HDR
.set io_tipo_hdw, IO_TIPO_HDW
.set io_tipo_dmahdr, IO_TIPO_DMAHDR
.set io_tipo_dmahdw, IO_TIPO_DMAHDW

// funzioni PCI
.set io_tipo_pcif, IO_TIPO_PCIF
.set io_tipo_pcir, IO_TIPO_PCIR
.set io_tipo_pciw, IO_TIPO_PCIW

// carica la idt
// le prime 20 entrate sono definite dall'Intel, e corrispondono
// alle possibili eccezioni. 
init_idt:
	pushl %ebp
	movl %esp, %ebp
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
	carica_gate	0x20		driver_td	LIV_SISTEMA
	carica_gate	0x21		handler_1	LIV_SISTEMA
	carica_gate	0x22		handler_2	LIV_SISTEMA
	carica_gate	0x23		handler_3	LIV_SISTEMA
	carica_gate	0x24		handler_4	LIV_SISTEMA
	carica_gate	0x25		handler_5	LIV_SISTEMA
	carica_gate	0x26		handler_6	LIV_SISTEMA
	carica_gate	0x27		handler_7	LIV_SISTEMA
	carica_gate	0x28		handler_8	LIV_SISTEMA
	carica_gate	0x29		handler_9	LIV_SISTEMA
	carica_gate	0x2A		handler_10	LIV_SISTEMA
	carica_gate	0x2B		handler_11	LIV_SISTEMA
	carica_gate	0x2C		handler_12	LIV_SISTEMA
	carica_gate	0x2D		handler_13	LIV_SISTEMA
	carica_gate	0x2E 		handler_14 	LIV_SISTEMA
	carica_gate	0x2F 		handler_15 	LIV_SISTEMA

	// primitive utente
	carica_gate	tipo_a		a_activate_p	LIV_UTENTE
	carica_gate	tipo_t		a_terminate_p	LIV_UTENTE
	carica_gate	tipo_si		a_sem_ini	LIV_UTENTE
	carica_gate	tipo_w		a_sem_wait	LIV_UTENTE
	carica_gate	tipo_s		a_sem_signal	LIV_UTENTE
	carica_gate	tipo_d		a_delay		LIV_UTENTE
	carica_gate	tipo_l		a_log		LIV_UTENTE
	carica_gate	tipo_ep		a_end_program	LIV_SISTEMA

	// primitive per il livello I/O
	carica_gate	tipo_ape	a_activate_pe	LIV_SISTEMA
	carica_gate	tipo_nwfi	a_nwfi		LIV_SISTEMA
	carica_gate	tipo_fg		a_fill_gate	LIV_SISTEMA
	carica_gate	tipo_p		a_panic		LIV_SISTEMA
	carica_gate	tipo_ab		a_abort_p	LIV_SISTEMA
	carica_gate	tipo_tra	a_trasforma	LIV_SISTEMA

	// PCI
	carica_gate	io_tipo_pcif	a_pci_find	LIV_SISTEMA
	carica_gate	io_tipo_pcir	a_pci_read	LIV_UTENTE
	carica_gate	io_tipo_pciw	a_pci_write	LIV_SISTEMA

	leave
	ret

// carica un gate nella IDT
// parametri: (vedere la macro carica_gate)
.global init_gate
init_gate:
	pushl %ebp
	movl %esp, %ebp

	pushl %ebx
	pushl %ecx
	pushl %eax

	movl $idt, %ebx
	movl 8(%ebp), %ecx		// indice nella IDT
	movl 12(%ebp), %eax		// offset della routine

	movw %ax, (%ebx, %ecx, 8)  	// primi 16 bit dell'offset
	movw $SEL_CODICE_SISTEMA, 2(%ebx, %ecx, 8)

	movw $0, %ax
	movb $0b10001110, %ah 	        // byte di accesso
					// (presente, 32bit, tipo interrupt)
	movb 16(%ebp), %al		// DPL
	shlb $5, %al			// posizione del DPL nel byte di accesso
	orb  %al, %ah			// byte di accesso con DPL in %ah
	movb $0, %al			// la parte bassa deve essere 0
	movl %eax, 4(%ebx, %ecx, 8)	// 16 bit piu' sign. dell'offset
					// e byte di accesso

	popl %eax
	popl %ecx
	popl %ebx
	leave
	ret


// carica un descrittore di segmento in GDT
// parametri: (vedere la macro carica_descr)
.global init_descrittore
init_descrittore:
	pushl %ebp
	movl %esp, %ebp

	pushl %ebx
	pushl %ecx
	pushl %eax
	pushl %edx

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
	
	popl %edx
	popl %eax
	popl %ecx
	popl %ebx

	leave
	ret

// trova un descrittore di segmento TSS non ancora
// utilizzato, e ne restituisce l'indice in %eax
// (0 se tutti occupati)
// lo stato occupato/libero del descrittore e' dato
// dal valore del suo bit di presenza

        .global alloca_tss
alloca_tss:
        pushl %ebp
        movl %esp, %ebp
        pushl %ebx
        pushl %ecx

        movl $gdt, %ebx
        movl $0, %eax
        movl last_tss, %ecx
1:
        incl %ecx
        andl $8191, %ecx
        cmpl last_tss, %ecx
        jne 2f
        movl $0xFFFFFFFF, %eax
        jmp 4f
2:
        testb $PRES, 5(%ebx, %ecx, 8)
        jnz 1b

        pushl $0                // gran. byte
        pushl $SEG_TSS          // tipo
        pushl $LIV_SISTEMA      // dpl
        pushl $PRES             // bit presenza
        pushl $(DIM_DESP - 1)   // limite
        pushl 8(%ebp)           // base
        pushl %ecx              // indice descr.
        call  init_descrittore
        addl $28, %esp

        movl %ecx, last_tss
        movl %ecx, %eax
4:
        popl %ecx
        popl %ebx
        leave
        ret


// rende nuovamente libero un descrittore di segmento TSS
// precedentemente occupato
// parametri: indice in GDT del descrittore da rilasciare
	.global rilascia_tss
rilascia_tss:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %ecx
	pushl %eax

	movl 8(%ebp), %ecx
	movl $gdt, %ebx
	movb $PRES, %al
	notb %al
	andb %al, 5(%ebx, %ecx, 8)

	popl %eax
	popl %ecx
	popl %ebx
	leave
	ret

// dato l'identificatore di un processo,
// ne restituisce il puntatore al descrittore
	.global des_p
des_p:	
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx

	movl $0, %ebx
        movw 8(%ebp), %bx		// esecuzione->identifier in ebx
	shrl $3, %ebx			// indice in %ebx
        leal gdt(, %ebx, 8), %eax       // ind. entrata della gdt relativa in eax
	estrai_base			// ind. TSS -> %ebx
	movl %ebx, %eax

	popl %ebx
	leave
	ret

	.global invalida_TLB  // [6.6]
invalida_TLB:
	pushl %eax
	movl %cr3, %eax
	movl %eax, %cr3
	popl %eax
	ret

// carica il registro cr3
// parametri: indirizzo fisico del nuovo direttorio
	.global loadCR3
loadCR3:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax

	movl 8(%ebp), %eax
	movl %eax, %cr3
	
	popl %eax
	leave
	ret

// restituisce in %eax il contenuto di cr3
	.global readCR3
readCR3:
	movl %cr3, %eax
	ret

// attiva la paginazione
	.global attiva_paginazione
attiva_paginazione:
	pushl %eax

	movl $0, %eax
	movl %eax, %cr4
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0

	popl %eax
	ret

// disattiva la paginazione
	.global disattiva_paginazione
disattiva_paginazione:
	pushl %eax

	movl %cr0, %eax
	andl $0x7FFFFFFF, %eax
	movl %eax, %cr0

	popl %eax
	ret

a_end_program:
	cli
	call reset_8259
	call reset_kbd
	movl shutdown_esp, %esp
	movl shutdown_ebp, %ebp
	call disattiva_paginazione
	ret
	

// dato un indirizzo virtuale (come parametro) usa l'istruzione invlpg per 
// eliminare la corrispondente traduzione dal TLB
	.global invalida_entrata_TLB // [6.4]
invalida_entrata_TLB:
	invlpg 4(%esp)
	# non e' necessario predisporre e utilizzare EBP
	ret
	
// trova efficientemente il primo bit a 0 nella doppia parola passata come 
// parametro (usato nella realizzazione dell'allocatore a mappa di bit)
	.global trova_bit
trova_bit:
	bsfl 4(%esp), %eax
	ret

//////////////////////////////////////////////////////////////////
// hardware gestito direttamente dal nucleo: 			 //
//  PIC, timer, console, swap					 //
//////////////////////////////////////////////////////////////////
// legge un byte da una porta di I/O
	.global inputb
inputb:
	pushl %eax
	pushl %edx
	movw 12(%esp), %dx
	inb %dx, %al
	movl 16(%esp), %edx
	movb %al, (%edx)
	popl %edx
	popl %eax
	ret

// scrive un byte in una porta di I/O
	.global outputb
outputb:
	pushl %eax
	pushl %edx
	movb 12(%esp), %al
	movw 16(%esp), %dx
	outb %al, %dx
	popl %edx
	popl %eax
	ret

// Ingresso di una word da una porta di IO
	.global inputw
inputw:
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
	.global outputw
outputw:
	pushl %eax
	pushl %edx
	movw 12(%esp), %ax
	movl 16(%esp), %edx
	outw %ax, %dx
	popl %edx
	popl %eax
	ret

// Ingresso di una doppia parola da una porta di IO
	.global inputl
inputl:
	pushl %eax
	pushl %edx
	movl 12(%esp), %edx
	inl %dx, %eax
	movl 16(%esp), %edx
	movl %eax, (%edx)
	popl %edx
	popl %eax
	ret

// scrive una doppia parola in una porta di I/O
	.global outputl
outputl:
	pushl %eax
	pushl %edx
	movl 12(%esp), %eax
	movl 16(%esp), %edx
	outl %eax, %dx
	popl %edx
	popl %eax
	ret

// legge una sequenza di parole da una porta di I/O
	.global inputbw
inputbw:
	pushl %eax
	pushl %edx
	pushl %edi
	pushl %ecx
	
	movw 20(%esp), %dx
	movl 24(%esp), %edi
	movl 28(%esp), %ecx
	cld
	rep
	insw
	
	popl %ecx
	popl %edi
	popl %edx
	popl %eax
	ret

// scrive una sequenza di parole in una porta di I/O
	.global outputbw
outputbw:
	pushl %eax
	pushl %edx
	pushl %esi
	pushl %ecx
	
	movw 28(%esp), %dx
	movl 20(%esp), %esi
	movl 24(%esp),%ecx
	cld
	rep
	outsw
	
	popl %ecx
	popl %esi
	popl %edx
	popl %eax
	ret

// PIC
// registri del controllore delle interruzioni
.set ICW1M, 0x20
.set ICW2M, 0x21
.set ICW3M, 0x21
.set ICW4M, 0x21
.set OCW1M, 0x21
.set OCW3M, 0x20
.set ICW1S, 0xa0
.set ICW2S, 0xa1
.set ICW3S, 0xa1
.set ICW4S, 0xa1
.set OCW1S, 0xa1
.set OCW3S, 0xa0
.set OCW2M, 0x20
.set OCW3M, 0x20
.set OCW2S, 0xa0
.set OCW3S, 0xa0

.set EOI, 0x20
.set READ_ISR, 0x0b


// inizializza il controllore delle interruzioni: la base del controllore va
// cambiata rispetto a quella impostata dal BIOS. Infatti, in modo protetto,
// esistono piu' tipi di eccezioni che in modo reale e la base impostata dal
// BIOS va a collidere con alcune di queste.
	.global init_8259
init_8259:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax

	// master
	movb $0x11, %al		// cascata
	outb %al, $ICW1M
	movb $0x20, %al		// nuova base
	outb %al, $ICW2M
	movb $0x04, %al		// slave connesso a IR2
	outb %al, $ICW3M
	movb $0x01, %al		// modo annidato
	outb %al, $ICW4M
	inb  $OCW1M, %al	// leggiamo la maschera corrente
	movb %al, save_pic_m	// e la salviamo per il ritorno al DOS
	movb $0b11111011, %al	// maschera tutte le interruzioni, tranne quelle
	outb %al, $OCW1M	//  provenienti dallo slave
	movb $0x48, %al
	outb %al, $OCW3M	// fully nested

	// slave
	movb $0x11, %al		// cascata
	outb %al, $ICW1S
	movb $0x28, %al		// nuova base
	outb %al, $ICW2S
	movb $0x02, %al		// id. dello slave
	outb %al, $ICW3S
	movb $0x01, %al		// modo annidato
	outb %al, $ICW4S
	inb  $OCW1S, %al	// leggiamo la maschera corrente
	movb %al, save_pic_s	// e la salviamo per il ritorno al DOS
	movb $0b11111111, %al	// maschera tutte le interruzioni
	outb %al, $OCW1S
	movb $0x48, %al		// fully nested
	outb %al, $OCW3S

	popl %eax
	leave
	ret

// ripristina il controllore delle interruzioni, con la base compatibile con il DOS.
// Inizializzo la modalita' fully nasted e i tipi base sono rispettivamente 0x08 per il master
// e 0x70 per lo slave.
		.global reset_8259
reset_8259:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax

	// master
	movb $0x11, %al		// cascata
	outb %al, $ICW1M
	movb $0x08, %al		// nuova base
	outb %al, $ICW2M
	movb $0x04, %al		// slave connesso a IR2
	outb %al, $ICW3M
	movb $0x01, %al		// modo annidato
	outb %al, $ICW4M
	movb save_pic_m, %al	// maschera salvata in precedenza
	outb %al, $OCW1M	
	movb $0x48, %al
	outb %al, $OCW3M	// fully nested

	// slave
	movb $0x11, %al		// cascata
	outb %al, $ICW1S
	movb $0x70, %al		// nuova base
	outb %al, $ICW2S
	movb $0x02, %al		// id. dello slave
	outb %al, $ICW3S
	movb $0x01, %al		// modo annidato
	outb %al, $ICW4S
	movb save_pic_s, %al	// maschera salvata in precedenza
	outb %al, $OCW1S
	movb $0x48, %al		// fully nested
	outb %al, $OCW3S

	popl %eax
	leave
	ret

	.global unmask_irq
unmask_irq:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %ecx

	xorw %cx, %cx
	movb 8(%ebp), %cl
	inb $OCW1S, %al
	movb %al, %ah
	inb $OCW1M, %al
	btrw %cx, %ax
	outb %al, $OCW1M
	movb %ah, %al
	outb %al, $OCW1S

	popl %ecx
	popl %eax
	leave
	ret

	.global mask_irq
mask_irq:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %ecx

	xorw %cx, %cx
	movb 8(%ebp), %cl
	inb $OCW1S, %al
	movb %al, %ah
	inb $OCW1M, %al
	btsw %cx, %ax
	outb %al, $OCW1M
	movb %ah, %al
	outb %al, $OCW1S

	popl %ecx
	popl %eax
	leave
	ret

// timer
// registri dell'interfaccia di conteggio
.set CWR,     0x43
.set CTR_LSB, 0x40
.set CTR_MSB, 0x40

// attiva il timer di sistema
// parametri: il valore da caricare nel registro CTR del timer
	.global attiva_timer
attiva_timer:
	pushl %ebp
	movl %esp, %ebp
        pushl %eax
	
	movb $0x36, %al
	outb %al, $CWR
	movl 8(%ebp), %eax
	outb %al, $CTR_LSB
	movb %ah, %al
	outb %al, $CTR_MSB

	inb $OCW1M, %al
	andb $0b11111110, %al
	outb %al, $OCW1M

        popl %eax
	leave
	ret

	.global disattiva_timer
disattiva_timer:
	pushl %ebp
	movl %esp, %ebp
        pushl %eax
	
	inb $OCW1M, %al
	orb $0b00000001, %al
	outb %al, $OCW1M

        popl %eax
	leave
	ret

// alcune periferiche (in particolare gli hard disk che rispettano lo standard 
// ATA) richiedono il rispetto di alcune temporizzazioni. Per esempio, e' 
// richiesta un'attesa di 400 nanosecondi prima del test del bit BSY nel 
// registro di stato degli hard disk. Per poter realizzare tali attese si puo' 
// utilizzare il Time Stamp Counter (tsc), un contatore che (dai processori 
// Pentium in poi) e' presente nei processori Intel. Tale contatore e' grande 
// 64 bit, viene incrementato ad ogni clock e puo' essere letto tramite 
// l'istruzione rdtsc (read tsc), che ne copia il contenuto nei registri %eax e 
// %edx. Una attesa di un tempo T (troppo piccolo per essere realizzata tramite 
// la primitiva delay) puo' essere realizzata leggendo una prima volta il tsc, 
// calcolando il valore V che dovra' assumere dopo il tempo T e rileggendo (in 
// un ciclo) il tsc fino a quando non assume un valore maggiore o uguale a V.  
// Per calcolare V, pero', e' necessario conoscere la frequenza del clock del 
// processore. La seguente funzione, invocata all'avvio del sistema, calcola 
// una approssimazione di tale frequenza, usando la variabile globale ticks, 
// che viene incrementata dalla rotine del timer ogni volta che questa va in 
// esecuzione. 
// 

bump_ticks:
	incl ticks
	pushl $0
	call inviaEOI
	addl $4, %esp
	iret

	.global calibra_tsc
calibra_tsc:
	pushl %ebp
	movl %esp, %ebp
	subl $16, %esp
	pushl %ecx
	pushl %edx
	pushl %ebx

	movl $idt, %ebx
	movl $0x20, %ecx
	movl (%ebx, %ecx, 8), %eax
	movl %eax, -16(%ebp)
	movl 4(%ebx, %ecx, 8), %eax
	movl %eax, -12(%ebp)

	carica_gate 0x20 bump_ticks LIV_SISTEMA

	cpuid			// l'istruzione cpuid serve da punto di sincronizzazione.
				// Infatti, i processori moderni possono 
				// eseguire le istruzioni senza rispettare 
				// l'ordine in cui il programmatore le ha 
				// scritte, per velocizzare l'esecuzione. In 
				// questo caso, pero', vogliamo che la seguente 
				// istruzione rdtsc venga eseguita esattamente 
				// dove l'abbiamo scritta, perche' ci interessa 
				// il tempo di esecuzione delle istruzioni che 
				// vengono dopo. Cio' si ottiene, appunto, 
				// precedendo l'istruzione rdtsc con 
				// l'istruzione cpuid
	sti			// abilitiamo il timer ad interrompere e, quindi, ad 
				// incrementare la variabile ticks
1:	movl ticks, %ecx
	cmpl $0, %ecx
	je 1b			// aspettiamo che venga incrementata una prima volta
	movl %ecx, -8(%ebp)	// quindi salviamo il suo valore iniziale
	rdtsc			// tsc -> %edx, %eax
	movl %eax, -4(%ebp)	// salviamo solo %eax (parte bassa del tsc): 
				// cio' e' sufficiente a calcolare la 
				// differenza che ci serve
2:	movl ticks, %ecx
	subl -8(%ebp), %ecx
	cmpl $20, %ecx		// aspettiamo che il valore di ticks aumenti di 20
				// (corrispondenti a circa un secondo di tempo 
				// reale)
	jbe 2b
	cli			
	cpuid			// passato un secondo, rileggiamo tsc
	rdtsc
	subl %eax, -4(%ebp)	// la differenza, tra il nuovo valore di tsc e quello salvato
				// precedentemente, e' il numero di cicli di 
				// clock al secondo

	movl $idt, %ebx
	movl $0x20, %ecx
	movl -16(%ebp), %eax
	movl %eax, (%ebx, %ecx, 8)
	movl -12(%ebp), %eax
	movl %eax, 4(%ebx, %ecx, 8)

	movl -4(%ebp), %eax
	
	popl %ebx
	popl %edx
	popl %ecx
	leave
	ret

reset_kbd:
	pushl %eax
	movb $0x60, %al
	outb %al, $0x64
	movb $0x61, %al
	outb %al, $0x60
	popl %eax
	ret


	.global salta_a_main
salta_a_main:
	movl %esp, shutdown_esp
	movl %ebp, shutdown_ebp
	call carica_stato		// carichiamo tr
	iret				// torniamo al chiamante "trasformati" in processo



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
	pushl $0
	pushl $0
	jmp comm_exc

debug:
	pushl $0
	pushl $1
	jmp comm_exc

nmi:
	pushl $0
	pushl $2
	jmp comm_exc

breakpoint:
	pushl $0
	pushl $3
	jmp comm_exc

overflow:
	pushl $0
	pushl $4
	jmp comm_exc

bound_re:
	pushl $0
	pushl $5
	jmp comm_exc

invalid_opcode:
	pushl $0
	pushl $6
	jmp comm_exc

dev_na:
	pushl $0
	pushl $7
	jmp comm_exc

double_fault:
	pushl $8
	jmp comm_exc

coproc_so:
	pushl $0
	pushl $9
	jmp comm_exc

invalid_tss:
	pushl $10
	jmp comm_exc

segm_fault:
	pushl $11
	jmp comm_exc

stack_fault:
	pushl $12
	jmp comm_exc

prot_fault:
	pushl $13
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
	movl 28(%esp), %eax // errore
	movl 32(%esp), %ebx // eip
	pushl %ebx
	pushl %eax
	call c_routine_pf
	addl $8, %esp
	carica_registri
	addl $4, %esp
	iret
.global readCR2
readCR2:
	movl %cr2, %eax
	ret

fp_exc:
	pushl $0
	pushl $16
	jmp comm_exc

ac_exc:
	pushl $17
	jmp comm_exc

mc_exc:
	pushl $0
	pushl $18
	jmp comm_exc

simd_exc:
	pushl $0
	pushl $19
	jmp comm_exc


comm_exc:
	call gestore_eccezioni
	addl $8, %esp
	jmp a_abort_p

//////////////////////////////////////////////////////////
// primitive richiamate dal nucleo stesso	        //
//////////////////////////////////////////////////////////
	.global sem_ini
sem_ini:
	int $tipo_si
	ret

	.global sem_wait
sem_wait:
	int $tipo_w
	ret

	.global activate_p
activate_p:
	int $tipo_a
	ret
	
	.global terminate_p
terminate_p:
	int $tipo_t
	ret

	.global nwfi
nwfi:
	int $tipo_nwfi

	.global panic
panic:
	int $tipo_p
	ret

	.global abort_p
abort_p:
	int $tipo_ab
	ret

	.global end_program
end_program:
	int $tipo_ep
	ret


////////////////////////////////////////////////////////
// handler/driver                                     //
////////////////////////////////////////////////////////
//
// driver del timer	[4.16]
	.extern c_driver_td
driver_td:
	call salva_stato	// interruzioni disabilitate
	call inspronti
	call c_driver_td
	pushl $0
	call inviaEOI
	call carica_stato
	iret


// handler generici	([7.1])
	.extern proc_esterni
handler_1:
	call salva_stato
	call inspronti

	movl $1, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_2:
	call salva_stato
	call inspronti

	movl $2, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_3:
	call salva_stato
	call inspronti

	movl $3, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_4:
	call salva_stato
	call inspronti

	movl $4, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_5:
	call salva_stato
	call inspronti

	movl $5, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_6:
	call salva_stato
	call inspronti

	movl $6, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_7:
	call salva_stato
	call inspronti

	movl $7, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_8:
	call salva_stato
	call inspronti

	movl $8, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_9:
	call salva_stato
	call inspronti

	movl $9, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_10:
	call salva_stato
	call inspronti

	movl $10, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_11:
	call salva_stato
	call inspronti

	movl $11, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_12:
	call salva_stato
	call inspronti

	movl $12, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_13:
	call salva_stato
	call inspronti

	movl $13, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_14:
	call salva_stato
	call inspronti

	movl $14, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

handler_15:
	call salva_stato
	call inspronti

	movl $15, %ecx
	movl a_p(, %ecx, 4), %eax
	movl %eax, esecuzione

	call carica_stato
	iret

	

////////////////////////////////////////////////////////
// a_primitive                                        //
////////////////////////////////////////////////////////
        .extern c_activate_p
a_activate_p:	// routine int $tipo_a [4.6]
	salva_reg_rit
        copia_param 4 6		// salva_registri ha inserito 6 long in pila
	cavallo_di_troia 0
        call c_activate_p
        addl $16, %esp
        carica_reg_rit
        iret

        .extern c_terminate_p
a_terminate_p:	// routine int $tipo_t [4.6]
	movl $terminate_stack_end, %esp
        call c_terminate_p
	call carica_stato
	iret

	.extern c_sem_ini
a_sem_ini:	// routine int $tipo_si [4.11]
	salva_reg_rit
	copia_param 1 6
	call c_sem_ini
	addl $4, %esp
	carica_reg_rit
	iret

	.extern c_sem_wait
a_sem_wait:	// routine int $tipo_w [4.11][6.5]
	call salva_stato
	copia_param 1 0	// ricopiamento del parametro sem
	call c_sem_wait
	//addl $4, %esp	// ripulitura pila (non necessaria)
	call carica_stato
	iret

	.extern c_sem_signal
a_sem_signal:	// routine int $tipo_s [4.11][6.5]
	call salva_stato
	copia_param 1 0	// ricopiamento del parametro sem
	call c_sem_signal
	//addl $4, %esp	// ripulitura pila (non necessaria)
	call carica_stato
	iret

	.extern c_delay
a_delay:	// routine int $tipo_d [4.16][6.5]
	call salva_stato
	copia_param 1 0	// ricopiamento del parametro n
	call c_delay
	// addl $4, %esp // ripulitura pila (non necessaria)
	call carica_stato
	iret


//
// Interfaccia offerta al modulo di IO, inaccessibile dal livello utente
//

	.extern c_activate_pe
a_activate_pe:
	salva_reg_rit
        copia_param 5 6		// salva_registri ha inserito 7 long in pila
	cavallo_di_troia 0
        call c_activate_pe
        addl $20, %esp	
	carica_reg_rit
	iret


a_nwfi:		// routine int $tipo_nwfi [7.3][9.1]
	call salva_stato
	// ( ricopiamento del parametro "controllore"
	copia_param 1 0 
	// )
	call inviaEOI
	// ( ripulitura della pila
	addl $4, %esp
	// )
	call schedulatore
	call carica_stato
	iret
inviaEOI:		// [9.1]
	pushl %eax
	cmpl $1, 8(%esp)
	jne m_eoi

	movb $EOI, %al		// ack al controllore slave
	outb %al, $OCW2S

	movb $READ_ISR, %al	// lettura di ISR dello slave
	outb %al, $OCW3S
	inb $OCW3S, %al
	testb $0xff, %al
	jnz m_noeoi		// ci sono ancora richieste dello slave attive
m_eoi:
	movb $EOI, %al
	outb %al, $OCW2M
m_noeoi:	
	popl %eax
	ret

a_fill_gate:
	salva_registri
	copia_param 3 7
	call init_gate
	addl $12, %esp
	carica_registri
	iret

	.extern c_panic
a_panic:	// routine int $tipo_p
	call salva_stato
	copia_param 1 0
	cavallo_di_troia 1
	call c_panic
1:	nop
	jmp 1b

	.extern c_abort_p
a_abort_p:
	movl $terminate_stack_end, %esp
        call c_abort_p
	call carica_stato
	iret

	.extern c_trasforma
a_trasforma:
	salva_reg_rit
	copia_param 1 6
	call c_trasforma
	addl $4, %esp
	carica_reg_rit
	iret
	.extern c_log
a_log:
	call salva_stato
	copia_param 3 0
	cavallo_di_troia 1
	cavallo_di_troia2 1 2
	call c_log
	addl $12, %esp
	call carica_stato
	iret

////////////////////////////////////////////////////////
// PCI
////////////////////////////////////////////////////////
	.global a_pci_find
	.extern c_pci_find
a_pci_find:
	salva_reg_rit
	copia_param 2 6
	call c_pci_find
	addl $8, %esp
	carica_reg_rit
	iret

	.global a_pci_read
	.extern c_pci_read
a_pci_read:
	salva_reg_rit
	copia_param 3 6
	call c_pci_read
	addl $12, %esp
	carica_reg_rit
	iret

	.global a_pci_write
	.extern c_pci_write
a_pci_write:
	salva_registri
	copia_param 4 7
	call c_pci_write
	addl $24, %esp
	carica_registri
	iret


// Fa uso della variabile globale clocks_per_usec (numero di cicli di clock 
// per microsecondo), il cui valore e' stato calcolato nella fase di 
// inizializzazione del sistema (vedere calibra_tsc)
	.global busywait_usec
busywait_usec:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %edx

	movl 8(%ebp), %eax
	mull clocks_per_usec
	pushl %edx
	pushl %eax
	call busywait_clock
	addl $8, %esp

	popl %edx
	popl %eax
	leave
	ret
	

// attende che sia trascorso un certo numero di cicli di clock
	.global busywait_clock
busywait_clock:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %edx
	pushl %ebx
	pushl %ecx
	rdtsc
	movl %eax, %ebx
	movl %edx, %ecx
1:	rdtsc
	subl %ebx, %eax
	sbbl %ecx, %edx
	cmpl 12(%ebp), %edx
	ja 2f
	cmpl 8(%ebp), %eax
	jbe 1b
2:	popl %ecx
	popl %ebx
	popl %edx
	popl %eax
	leave
	ret

// attende che siano trascorsi approssimativamente 500 nanosecondi
// (serve a realizzare l'attesa di 400 ns richiesta dallo standard ATA, 
// mantenendo un certo margine di sicurezza)
wait_500ns:
	pushl clocks_per_usec
	shrl (%esp)
	pushl $0
	call busywait_clock
	addl $8, %esp
	ret
	
// Seleziona uno dei due drive di un canale ATA
	.global hd_select_device
hd_select_device:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %edx

	movl 8(%ebp),%eax
	cmpl $0,%eax
	je shd_ms
shd_sl:	movb $0xf0,%al
	jmp ms_out
shd_ms:	movb $0xe0,%al
ms_out:	movl 12(%ebp),%edx
	outb %al,%dx

	call wait_500ns

	popl %edx
	popl %eax
	leave
	ret

// invia un comando al drive correntemente selezionato di un canale ATA
	.global hd_write_command	// [9.3.1]
hd_write_command:
	pushl %ebp
	movl %esp, %ebp
	pushl %edx
	pushl %eax

	movb 8(%ebp), %al
	movw 12(%ebp), %dx
	outb %al, %dx

	call wait_500ns

	popl %eax
	popl %edx
	leave
	ret


// Legge lo stato attuale del registro di stato di un canale, per capire quale 
// drive e` attivo
	.global hd_read_device
hd_read_device:
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
	

// Scompone correttamente il blocco iniziale di una operazione su hard disk
// e lo scrive nei registri opportuni
	.global hd_write_address
hd_write_address:	// [9.3.1]
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %edx
	pushl %edi
	
	movl 12(%ebp),%eax
	movl 8(%ebp),%edi	// accede al descrittore per comodita`
	movw 6(%edi),%dx
	outb %al,%dx		// caricato SNR
	movw 2(%edi),%dx
	movb %ah,%al
	outb %al,%dx		// caricato CNL
	shrl $16,%eax
	movw 4(%edi),%dx
	outb %al,%dx		// caricato CNH
	movw 8(%edi),%dx
	inb %dx,%al		// HND in %al
	andb $0xf0,%al		// maschera per l'indirizzo in HND
	andb $0x0f,%ah		// maschera per i 4 bit +sign di primo
	orb  $0xe0,%ah		// seleziona LBA
	orb %ah,%al
	outb %al,%dx		// caricato HND
	
	popl %edi
	popl %edx
	popl %eax
	leave
	ret

////////////////////////////////////////////////////////////////
// sezione dati: tabelle e stack				      //
////////////////////////////////////////////////////////////////
.data
par:		.long 0
.global		ticks
ticks:		.long 0
.global		clocks_per_usec
clocks_per_usec:
		.long 1
.global		mem_upper
mem_upper:	 .long end
.global		fine_codice_sistema
fine_codice_sistema:
		.long etext
.global		 esecuzione	// [4.6]
esecuzione:	.long 0	
.global		pronti		// [4.6]
pronti:	.long 0
.global		processi	// [4.7]
processi:	.long 0
	// puntatori alle tabelle GDT e IDT
	// nel formato richiesto dalle istruzioni LGDT e LIDT
gdt_pointer:
	.word 0xffff		 	// limite della GDT
	.long gdt			// base della GDT
idt_pointer:
	.word 0x7FF			// limite della IDT (256 entrate)
	.long idt			// base della IDT
param_err:
	.asciz "parametro errato %x"
.bss
.global		array_dess		// [4.11]
array_dess:	.space BYTE_SEM
.balign 8
gdt:
	// spazio per 5 descrittori piu' i descrittori di TSS 
	// i descrittori verrano costruiti a tempo di esecuzione
	.space 8 * 8192, 0

.balign 8
idt:
	// spazio per 256 gate
	// verra' riempita a tempo di esecuzione
	.space 8 * 256, 0

	.global stack
stack:
	.space STACK_SIZE, 0
terminate_stack:
	.space STACK_SIZE, 0
terminate_stack_end:
save_gdt:
	.word 0
	.long 0
save_idt:
	.word 0
	.long 0
save_esp:
	.long 0
shutdown_esp:
	.long 0
shutdown_ebp:
	.long 0
save_pic_m:
	.byte 0
save_pic_s:
	.byte 0
	.fill 2, 0
last_tss:
	.long 4
