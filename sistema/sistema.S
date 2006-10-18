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
        movw 4(%esp, %ecx, 4), %ax      // CS in AX
        andb $0b00000011, %al	        // CPL del chiamante in AL
	cmpb $LIV_SISTEMA, %al		// se sistema, non c'e' stato cambio pila
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


// Carica un gate della IDT
// num: indice (a partire da 0) in IDT del gate da caricare
// routine: indirizzo della routine da associare al gate
// dpl: dpl del gate (LIV_SISTEMA o LIV_UTENTE)
// NOTA: la macro si limita a chiamare la routine _init_gate
//       con gli stessi parametri. Verra' utilizzata per
//       motivi puramente estetici
.macro carica_gate num routine dpl

	pushl $\dpl
	pushl $\routine
	pushl $\num
	call _init_gate
	addl $12, %esp

.endm

// Carica un descrittore della GDT
// num: indice (a partire da 0) in GDT del descrittore da caricare
// base: base del segmento
// limite: campo limite (su 20 bit)
// pres: bit di presenza (usare le costanti PRES e NON_P)
// dpl: dpl del segmento (usare le costanti LIV_SISTEMA o LIV_UTENTE)
// tipo: tipo del gate (usare le costanti SEG_CODICE, SEG_DATI o SEG_TSS)
// gran: granularita' (usare le costanti G_PAGINA o G_BYTE)
// NOTA: la macro si limita a chiamare la routine _init_descrittore
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
	call  _init_descrittore
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
// indirizzi di memoria principale superiori a 1MB) e carica il sistema,
// assieme agli eventuali moduli, in memoria. Quindi, salta alla prima
// istruzione del sistema. Il bootstrap loader puo' anche passare
// delle informazioni al sistema (tramite i registri e la memoria).
//
// In questo sistema usiamo lo standard multiboot, che definisce il formato che 
// il file contentente il sistema deve rispettare e
// il formato delle informazioni passate dal bootstrap loader al sistema.
// Il formato del file contenente il sistema deve essere quello di un
// normale file eseguibile, ma, nei primi 2*4K byte, deve contenere 
// la struttura multiboot_header, definita piu' avanti. 
// Il boot loader, prima di saltare alla prima istruzione del sistema
// (l'entry point specificato nel file eseguibile), lascia nel registro
// %eax un valore di riconoscimento e in %ebx l'indirizzo di una struttura
// dati, contentente varie informazioni (in particolare, la quantita'
// di memoria principale installata nel sistema, il dispositivo da cui
// e' stato eseguito il bootstrap e l'indirizzo di memoria in cui sono
// stati caricati gli eventuali moduli)
     .text

     .globl  start, _start
start:				// entry point
_start:
     jmp     multiboot_entry	// scavalchiamo la struttra richiesta
     				// dal bootstrap loader, che deve
				// trovarsi verso l'inizio del file

     .align  4
     // le seguenti informazioni sono richieste dal bootstrap loader
multiboot_header:
     .long   MULTIBOOT_HEADER_MAGIC 				// valore magico
     .long   MULTIBOOT_HEADER_FLAGS   				// flag
     .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS) // checksum
#ifndef __ELF__
     .long   multiboot_header
     .long   _start
     .long   _edata
     .long   _end
     .long   multiboot_entry
#endif 

multiboot_entry:

     movl    $(_stack + STACK_SIZE), %esp // inizializziamo lo stack
     call    init_gdt
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
     lidt    idt_pointer		// carichiamo la nuova IDT
	
     pushl   $0				// resettiamo EFLAG
     popf				// N.B.: interrupt disabilitati
					// (perche', azzerando tutti i flag,
					// abbiamo azzerato anche IF)

     pushl   %ebx			// parametri passati dal loader
     pushl   %eax			
     call    _cmain			// il resto dell'inizializzazione
     					// e' scritto in C++
     addl    $8, %esp
     // qui non possiamo arrivare, ma non si sa mai
loop:   hlt
     jmp     loop


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
	.global _backtrace
_backtrace:
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
	call _snprintf
	addl $16, %esp
	pushl %eax
	pushl $buf_backtrace
	pushl off_backtrace
	call _writevid_n
	addl $12, %esp
	addl $10, off_backtrace
	
	movl (%esi), %esi
	decl count_backtrace
	jmp 3b
2:
	popl %eax
	popl %esi
	ret
	

// inserimento del processo in _esecuzione in testa alla coda dei pronti
//  (non salva i registri, viene chiamata dopo salva_stato)
//
inspronti:
        movl _esecuzione, %eax
        movl _pronti, %ebx
        movl %ebx, 8(%eax)
        movl %eax, _pronti
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

// salva lo stato del processo corrente nel suo descrittore
//
salva_stato:
        pushl %ebx
	pushl %eax

	movl _esecuzione, %eax
	movl $0, %ebx
        movw (%eax), %bx		// esecuzione->identifier in ebx
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

// carica lo stato del processo in _esecuzione
//
carica_stato:
        movl _esecuzione, %edx
	movl $0, %ebx
        movw (%edx), %bx		// esecuzione->identifier in ebx
	
	movl %ebx, %ecx			
	shll $3, %ecx			// trasformo indice->selettore

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
.set tipo_r, TIPO_R

// Tipi delle interruzioni usate per l' interfaccia con il modulo di IO
.set tipo_ae, TIPO_AE
.set tipo_nwfi, TIPO_NWFI
.set tipo_va, TIPO_VA
.set tipo_fg, TIPO_FG
.set tipo_p, TIPO_P
.set tipo_ab, TIPO_AB
.set tipo_l,  TIPO_L

// interruzioni usate dal nucleo stesso (dalle routine della memoria virtuale)
.set io_tipo_rhdn, IO_TIPO_RHDN
.set io_tipo_whdn, IO_TIPO_WHDN

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
	carica_gate	14 		page_fault 	LIV_SISTEMA
	carica_gate	16 		fp_exc 		LIV_SISTEMA
	carica_gate	17 		ac_exc 		LIV_SISTEMA
	carica_gate	18 		mc_exc 		LIV_SISTEMA
	carica_gate	19 		simd_exc 	LIV_SISTEMA

	// driver/handler
	carica_gate	0x20		driver_t	LIV_SISTEMA
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
	carica_gate	0x2E 		driver_hd0 	LIV_SISTEMA
	carica_gate	0x2F 		driver_hd1 	LIV_SISTEMA

	// primitive utente
	carica_gate	tipo_a		a_activate_p	LIV_UTENTE
	carica_gate	tipo_t		a_terminate_p	LIV_UTENTE
	carica_gate	tipo_g		a_give_num	LIV_UTENTE
	carica_gate	tipo_si		a_sem_ini	LIV_UTENTE
	carica_gate	tipo_w		a_sem_wait	LIV_UTENTE
	carica_gate	tipo_s		a_sem_signal	LIV_UTENTE
	carica_gate	tipo_ma		a_mem_alloc	LIV_UTENTE
	carica_gate	tipo_mf		a_mem_free	LIV_UTENTE
	carica_gate	tipo_d		a_delay		LIV_UTENTE
	carica_gate	tipo_rl		a_readlog	LIV_UTENTE
	carica_gate	tipo_r		a_resident	LIV_UTENTE
	carica_gate	tipo_l		a_log		LIV_UTENTE

	// primitive per il livello I/O
	carica_gate	tipo_ae		a_activate_pe	LIV_SISTEMA
	carica_gate	tipo_nwfi	a_nwfi		LIV_SISTEMA
	carica_gate	tipo_va		a_verifica_area	LIV_SISTEMA
	carica_gate	tipo_fg		a_fill_gate	LIV_SISTEMA
	carica_gate	tipo_p		a_panic		LIV_SISTEMA
	carica_gate	tipo_ab		a_abort_p	LIV_SISTEMA

	// gestione HD (derivato da Faggioli)
	carica_gate	io_tipo_rhdn	a_readhd_n	LIV_SISTEMA
	carica_gate	io_tipo_whdn	a_writehd_n	LIV_SISTEMA

	leave
	ret

// carica un gate nella IDT
// parametri: (vedere la macro carica_gate)
.global _init_gate
_init_gate:
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
.global _init_descrittore
_init_descrittore:
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
	.global _alloca_tss
_alloca_tss:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %ecx
	
	movl $gdt, %ebx
	movl $0, %eax
	movl $5, %ecx
1:
	cmpl $8192, %ecx
	jl 2f
	jmp 3f
2:
	testb $PRES, 5(%ebx, %ecx, 8)
	jnz 4f

	pushl $0		// gran. byte
	pushl $SEG_TSS		// tipo
	pushl $LIV_SISTEMA	// dpl
	pushl $PRES		// bit presenza
	pushl $(SIZE_DESP - 1)	// limite
	pushl 8(%ebp)		// base
	pushl %ecx		// indice descr.
	call  _init_descrittore
	addl $28, %esp

	movl %ecx, %eax
	jmp 3f
4:	
	incl %ecx
	jmp 1b

3:
	popl %ecx
	popl %ebx
	leave
	ret

// rende nuovamente libero un descrittore di segmento TSS
// precedentemente occupato
// parametri: indice in GDT del descrittore da rilasciare
	.global _rilascia_tss
_rilascia_tss:
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
	.global _des_p
_des_p:	
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx

	movl $0, %ebx
        movw 8(%ebp), %bx		// esecuzione->identifier in ebx
        leal gdt(, %ebx, 8), %eax       // ind. entrata della gdt relativa in eax
	estrai_base			// ind. TSS -> %ebx
	movl %ebx, %eax

	popl %ebx
	leave
	ret

// carica il registro cr3
// parametri: indirizzo fisico del nuovo direttorio
	.global _carica_cr3
_carica_cr3:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax

	movl 8(%ebp), %eax
	movl %eax, %cr3
	
	popl %eax
	leave
	ret

// restituisce in %eax il contenuto di cr3
	.global _leggi_cr3
_leggi_cr3:
	movl %cr3, %eax
	ret

// attiva la paginazione
	.global _attiva_paginazione
_attiva_paginazione:
	pushl %eax

	movl $0, %eax
	movl %eax, %cr4
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0

	popl %eax
	ret

// dato un indirizzo virtuale (come parametro) usa l'istruzione invlpg per 
// eliminare la corrispondente traduzione dal TLB
	.global _invalida_entrata_TLB
_invalida_entrata_TLB:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax

	movl 8(%ebp), %eax
	invlpg (%eax)
	
	popl %eax
	leave
	ret
	
// trova efficientemente il primo bit a 0 nella doppia parola passata come 
// parametro (usato nella realizzazione dell'allocatore a mappa di bit)
	.global _trova_bit
_trova_bit:
	bsfl 4(%esp), %eax
	ret

//////////////////////////////////////////////////////////////////
// hardware gestito direttamente dal nucleo: 			 //
//  PIC, timer, console, swap					 //
//////////////////////////////////////////////////////////////////
// legge un byte da una porta di I/O
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

// scrive un byte in una porta di I/O
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

// legge una sequenza di parole da una porta di I/O
	.global _inputbuffw
_inputbuffw:
	pushl %eax
	pushl %edx
	pushl %edi
	pushl %ecx
	
	movl 20(%esp), %edx
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
	.global _init_8259
_init_8259:
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
	movb $0b11111111, %al	// maschera tutte le interruzioni
	outb %al, $OCW1S
	movb $0x48, %al		// fully nested
	outb %al, $OCW3S

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
	.global _attiva_timer
_attiva_timer:
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

	.global _disattiva_timer
_disattiva_timer:
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
// registro di stato degli hard disk. Per poter realizzare tali attese, si puo' 
// utilizzare il Time Stamp Counter (tsc), un contatore che (dai processori 
// Pentium in poi) e' presente nei processori Intel. Tale contatore e' grande 
// 64 bit, viene incrementato ad ogni clock e puo' esseere letto tramite 
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
	.global _calibra_tsc
_calibra_tsc:
	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp
	pushl %ecx
	pushl %edx

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
				// incrementare la variabile _ticks
1:	movl _ticks, %ecx
	cmpl $0, %ecx
	je 1b			// aspettiamo che venga incrementata una prima volta
	movl %ecx, -8(%ebp)	// quindi salviamo il suo valore iniziale
	rdtsc			// tsc -> %edx, %eax
	movl %eax, -4(%ebp)	// salviamo solo %eax (parte bassa del tsc): 
				// cio' e' sufficiente a calcolare la 
				// differenza che ci serve
2:	movl _ticks, %ecx
	subl -8(%ebp), %ecx
	cmpl $20, %ecx		// aspettiamo che il valore di _ticks aumenti di 20
				// (corrispondenti a circa un secondo di tempo 
				// reale)
	jbe 2b
	cli			
	cpuid			// passato un secondo, rileggiamo tsc
	rdtsc
	subl %eax, -4(%ebp)	// la differenza, tra il nuovo valore di tsc e quello salvato
				// precedentemente, e' il numero di cicli di 
				// clock al secondo

	movl -4(%ebp), %eax
	
	popl %edx
	popl %ecx
	leave
	ret


// trasforma_in_processo viene chiamata nella fase di inizializzazione, in modo 
// che il codice di inizializzazione venga eseguito nel contesto di un processo 
// (il processo main) che, quindi, puo' invocare le primitive di sistema, anche 
// bloccanti (in particolare, e' necessario invocare le primitive per leggere 
// le informazioni iniziali dallo swap)
	.global _salta_a_main
_salta_a_main:
	// creiamo in pila la struttura che la salva_stato e la successiva iret 
	// si aspettano
	popl %eax			// indirizzo di ritorno in %eax		
	pushl $0x00000200		// eflag, IF=1
	pushl $SEL_CODICE_SISTEMA	// cs
	pushl %eax			// eip
	call carica_stato		// carichiamo tr
	iret				// torniamo al chiamante "trasformati" in processo


// salta_a_main viene chiamata come ultima operazione della fase di 
// inizializzazione, per passare ad eseguire il codice, a livello utente, della 
// funzione main. Per passare da livello sistema a livello utente, l'unico modo
// e' usare una iret, preparando opportunamente le informazioni in pila.
// La funzione richiede due parametri: l'entry point di main e l'indirizzo 
// della pila utente
	.global _salta_a_utente
_salta_a_utente:
        movl _esecuzione, %edx
	movl $0, %ebx
        movw (%edx), %bx		// esecuzione->identifier in ebx
	
	movl %ebx, %ecx			
	shll $3, %ecx			// trasformo indice->selettore

        leal gdt(, %ebx, 8), %eax       // ind. entrata della gdt relativa in eax
	estrai_base			// ind. del TSS in %ebx
        andl $0xfffffdff, 4(%eax)       // bit busy del TSS a zero

       	ltr %cx				// nuovo valore in TR
	movw $SEL_DATI_UTENTE, %ax	// sostituiamo i selettori dati sistema
					// con selettori dati utente
	movw %ax, %ds
	movw %ax, %es
	popl %eax			// indirizzo di ritorno, che scartiamo
	popl %eax			// primo parametro: indirizzo di main -> %eax
	popl %ebx			// secondo parametro: indirizzo pila utente ->%ebx
	// prepariamo le informazioni che la iret si aspetta nel caso di 
	// ritorno da livello sistema a livello utente
	pushl $SEL_DATI_UTENTE		// selettore della nuova pila
	subl $8, %ebx
	pushl %ebx			// puntatore alla testa della nuova pila
	pushl $0x00000200		// eflag (interrupt abilitati)
	pushl $SEL_CODICE_UTENTE	// selettore del segmento codice 
	pushl %eax			// nuovo eip
	iret


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
// _c_page_fault passandole tre parametri:
// - la coppia (cs, eip), salvata in pila del meccanismo di eccezione. Tale 
// coppia ci permette di individuare l'istruzione che aveva causato il fault e 
// di sapere se il fault si e' verificato mentre il processore era in stato 
// utente o in stato sistema (se era in stato sistema, si tratta probabilmente 
// di un bug nel nucleo, nel qual caso vogliamo fermare tutto)
// - il contenuto del registro speciale %cr2, che contiene l'indirizzo virtuale 
// non tradotto che ha generato il fault
page_fault:
	salva_registri
	movl 32(%esp), %eax	// cs salvato dall'eccezione
	movl 28(%esp), %ebx	// eip salvato dall'eccezione
	pushl %eax		
	pushl %ebx
	movl %cr2, %eax
	pushl %eax
	call _c_page_fault
	addl $12, %esp
	carica_registri
	addl $4, %esp
	iret

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
	call _gestore_eccezioni
	addl $8, %esp
	call _c_terminate_p
	call carica_stato
	iret

//////////////////////////////////////////////////////////
// primitive richiamate dal nucleo stesso (page fault)  //
//////////////////////////////////////////////////////////
// le routine che gestiscono il page fault sono particolari, in quanto possono 
// a loro volta invocare altre routine del nucleo stesso. In particolare, sono 
// necessarie le seguenti:
//
	.global _sem_wait
_sem_wait:
	int $tipo_w
	ret

	.global _sem_signal
_sem_signal:
	int $tipo_s
	ret

	.global _terminate_p
_terminate_p:
	int $tipo_t
	ret

	.global _readhd_n
_readhd_n:
	int $io_tipo_rhdn
	ret

	.global _writehd_n
_writehd_n:
	int $io_tipo_whdn
	ret

	.global _panic
_panic:
	int $tipo_p
	ret

	.global _abort_p
_abort_p:
	int $tipo_ab
	ret

	.global _nwfi
_nwfi:
	int $tipo_nwfi
	ret
////////////////////////////////////////////////////////
// handler/driver                                     //
////////////////////////////////////////////////////////
//
// driver del timer
	.extern _c_driver_t
driver_t:
	call salva_stato
	call inspronti
	call _c_driver_t
	movb $EOI, %al         // ack al controllore
	outb %al, $OCW2M
	call carica_stato
	iret


// handler generici
	.extern _proc_esterni
handler_1:
	call salva_stato
	call inspronti

	movl _proc_esterni + 4, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_2:
	call salva_stato
	call inspronti

	movl _proc_esterni + 8, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_3:
	call salva_stato
	call inspronti

	movl _proc_esterni + 12, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_4:
	call salva_stato
	call inspronti

	movl _proc_esterni + 16, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_5:
	call salva_stato
	call inspronti

	movl _proc_esterni + 20, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_6:
	call salva_stato
	call inspronti

	movl _proc_esterni + 24, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_7:
	call salva_stato
	call inspronti

	movl _proc_esterni + 28, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_8:
	call salva_stato
	call inspronti

	movl _proc_esterni + 32, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_9:
	call salva_stato
	call inspronti

	movl _proc_esterni + 36, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_10:
	call salva_stato
	call inspronti

	movl _proc_esterni + 40, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_11:
	call salva_stato
	call inspronti

	movl _proc_esterni + 44, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_12:
	call salva_stato
	call inspronti

	movl _proc_esterni + 48, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_13:
	call salva_stato
	call inspronti

	movl _proc_esterni + 52, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

// driver degli hard disk sul canale primario
	.extern _c_driver_hd
driver_hd0:
	call salva_stato
	call inspronti
	pushl $0
	call _c_driver_hd
	movb $EOI, %al		// ack al controllore slave
	outb %al, $OCW2S
	movb $READ_ISR, %al	// lettura di ISR dello slave
	outb %al, $OCW3S
	inb $OCW3S, %al
	testb $0xff, %al
	jnz 1f
	movb $EOI, %al
	outb %al, $OCW2M
1:	call carica_stato
	iret

// driver degli hard disk sul canale secondario
driver_hd1:
	call salva_stato
	call inspronti
	pushl $1
	call _c_driver_hd
	movb $EOI, %al		// ack al controllore slave
	outb %al, $OCW2S
	movb $READ_ISR, %al	// lettura di ISR dello slave
	outb %al, $OCW3S
	inb $OCW3S, %al
	testb $0xff, %al
	jnz 1f
	movb $EOI, %al
	outb %al, $OCW2M
1:	call carica_stato
	iret
	

////////////////////////////////////////////////////////
// a_primitive                                        //
////////////////////////////////////////////////////////
        .extern _c_activate_p
a_activate_p:	// routine int $tipo_a
	salva_reg_rit
        copia_param 4 6		// salva_registri ha inserito 6 long in pila
        call _c_activate_p
        addl $16, %esp
        carica_reg_rit
        iret

        .extern _c_terminate_p
a_terminate_p:	// routine int $tipo_t
	movl $terminate_stack_end, %esp
        call _c_terminate_p
	call carica_stato
	iret

        .extern _c_give_num
a_give_num:	// routine int $tipo_g
	salva_reg_rit
        call _c_give_num
        carica_reg_rit
	iret

	.extern _c_sem_ini
a_sem_ini:	// routine int $tipo_si
	salva_reg_rit
	copia_param 1 6
	call _c_sem_ini
	addl $4, %esp
	carica_reg_rit
	iret

	.extern _c_sem_wait
a_sem_wait:	// routine int $tipo_w
	call salva_stato
	copia_param 1 0
	call _c_sem_wait
	// addl $4, %esp			// non necessario
	call carica_stato
	iret

	.extern _c_sem_signal
a_sem_signal:	// routine int $tipo_s
	call salva_stato
	copia_param 1 0
	call _c_sem_signal
	// addl $4, %esp			// non necessario
	call carica_stato
	iret

	.extern _c_mem_alloc
a_mem_alloc:	// routine int $tipo_ma
	salva_reg_rit
	copia_param 1 6
	call _c_mem_alloc
	addl $4, %esp
	carica_reg_rit
	iret

	.extern _c_mem_free
a_mem_free:	// routine int $tipo_mf
	salva_registri
	copia_param 1 7
	call _c_mem_free
	addl $4, %esp
	carica_registri
	iret

	.extern _c_delay
a_delay:	// routine int $tipo_d
	call salva_stato
	copia_param 1 0
	call _c_delay
	// addl $4, %esp			// non necessario
	call carica_stato
	iret

	.extern _c_readlog
a_readlog:	// routine int $tipo_rl
	salva_registri
	copia_param 1 7
	call _c_readlog
	addl $4, %esp
	carica_registri
	iret

	.extern _c_resident
a_resident:	// routine int $tipo_r
	salva_reg_rit
	copia_param 2 6
	call _c_resident
	addl $8, %esp
	carica_reg_rit
	iret


//
// Interfaccia offerta al modulo di IO, inaccessibile dal livello utente
//

	.extern _c_activate_pe
a_activate_pe:
	salva_reg_rit
        copia_param 5 6		// salva_registri ha inserito 7 long in pila
        call _c_activate_pe
        addl $20, %esp	
	carica_reg_rit
	iret


	.extern _c_nwfi
a_nwfi:		// routine int $tipo_nwfi
	call salva_stato

	cmpl $1, 16(%esp)
	jne m_ack

	movb $EOI, %al		// ack al controllore slave
	outb %al, $OCW2S

	movb $READ_ISR, %al	// lettura di ISR dello slave
	outb %al, $OCW3S
	inb $OCW3S, %al
	testb $0xff, %al
	jnz m_noack		// ci sono ancora richieste dello slave attive
m_ack:
	movb $EOI, %al
	outb %al, $OCW2M
m_noack:	
	call _schedulatore
	call carica_stato
	iret

	.extern _c_verifica_area
a_verifica_area:
	salva_reg_rit
	copia_param 3 6
	call _c_verifica_area
	addl $12, %esp
	carica_reg_rit
	iret

a_fill_gate:
	salva_registri
	copia_param 3 7
	call _init_gate
	addl $12, %esp
	carica_registri
	iret

	.extern _c_panic
a_panic:	// routine int $tipo_p
	call salva_stato
	copia_param 1 0
	call _c_panic
1:	nop
	jmp 1b

	.extern _c_abort_p
a_abort_p:
	movl $terminate_stack_end, %esp
        call _c_abort_p
	call carica_stato
	iret

	.extern _c_log
a_log:
	call salva_stato
	copia_param 3 0
	call _c_log
	addl $12, %esp
	call carica_stato
	iret

/////////////////////////////////////////////////////////////////////
// SWAP                                                           
// Funzioni di utilita' per la gestione degli hard disk
////////////////////////////////////////////////////////////////////


// lettura di n settori
	.global a_readhd_n
	.extern _c_readhd_n
a_readhd_n:
	salva_registri
	copia_param 6 7
	call _c_readhd_n
	addl $24, %esp
	carica_registri
	iret

// scrittura di n settori
	.global a_writehd_n
	.extern _c_writehd_n
a_writehd_n:
	salva_registri
	copia_param 6 7
	call _c_writehd_n
	addl $24, %esp
	carica_registri
	iret

// Esegue un probing del canale ATA specificato tentando di capire se
//  ad esso sono collegati nessuno, uno o piu` dispositivi
//
	.global _test_canale
_test_canale:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %edx

	movw 8(%ebp),%dx
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
	movw 12(%ebp),%dx 
	outb %al,%dx
	movb $0xAA,%al
	movw 16(%ebp),%dx
	outb %al,%dx
	movw 12(%ebp),%dx
	outb %al,%dx
	movb $0x55,%al
	movw 16(%ebp),%dx
	outb %al,%dx
	movw 12(%ebp),%dx
	outb %al,%dx
	movb $0xAA,%al
	movw 16(%ebp),%dx
	outb %al,%dx
	movw 12(%ebp),%dx
	inb %dx,%al
	movb %al,%ah
	movw 16(%ebp),%dx
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
	leave
	ret

// Assmendo che il canale sia "attivo" cerca di capire se il dispositivo
//  indirizzato e` un ATA (hard disk) o ATAPI (chrom)
//
	.global _leggi_signature
_leggi_signature:
	pushl %ebp
	movl %esp, %ebp
	pushl %edx
	movw 8(%ebp),%dx
	inb %dx,%al
	cmpb $0x01,%al
	jne sgerr
	movw 12(%ebp),%dx
	inb %dx,%al
	cmpb $0x01,%al
	jne sgerr
	movw 16(%ebp),%dx
	inb %dx,%al
	cmpb $0x00,%al
	je sgata1
	cmpb $0x14,%al
	je sgpi1
	jmp sgerr
sgata1:	movw 20(%ebp),%dx
	inb %dx,%al
	cmpb $0x00,%al
	jne sgerr
	jmp sgata
sgpi1:	movw 20(%ebp),%dx
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
sgret:	
	popl %edx
	leave
	ret

// invia un software reset agli hard disk del canale passato come parametro
	.global _hd_software_reset
_hd_software_reset:
	pushl %ebp
	movl %esp, %ebp
	pushl %edx
	pushl %eax

	// il bit di reset deve essere a 0 per almeno 5usec
	pushl $5
	pushl $0
	call _busywait_usec
	addl $8, %esp

	movw 8(%ebp),%dx
	movb $0x0E,%al
	outb %al,%dx

	// il bit di reset deve essere a 1 per almeno 5usec
	pushl $5
	pushl $0
	call _busywait_usec
	addl $8, %esp

	movw 8(%ebp),%dx
	movb $0x02,%al
	outb %al,%dx

	// il bit di reset deve essere mantenuto a 0 per almeno 2msec 
	pushl $2000
	pushl $0
	call _busywait_usec
	addl $8, %esp

	popl %eax
	popl %edx
	leave
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

// attende che sia trascorso il numero di microsecondi passato come parametro.
// Fa uso della variabile globale _clocks_per_usec (numero di cicli di clock 
// per microsecondo), il cui valore e' stato calcolato nella fase di 
// inizializzazione del sistema (vedere _calibra_tsc)
	.global _busywait_usec
_busywait_usec:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %edx

	movl 8(%ebp), %eax
	mull _clocks_per_usec
	pushl %edx
	pushl %eax
	call _busywait_clock
	addl $8, %esp

	popl %edx
	popl %eax
	leave
	ret
	

// attende che sia trascorso un certo numero di cicli di clock
	.global _busywait_clock
_busywait_clock:
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
	cmpl 8(%ebp), %edx
	ja 2f
	cmpl 12(%ebp), %eax
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
	pushl _clocks_per_usec
	shrl (%esp)
	pushl $0
	call _busywait_clock
	addl $8, %esp
	ret
	
// Seleziona uno dei due drive di un canale ATA
	.global _hd_select_device
_hd_select_device:
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
	.global _hd_write_command
_hd_write_command:
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
	.global _hd_read_device
_hd_read_device:
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
	
// Disabilita il controllore di interruzioni ad inoltrare richieste provenienti
//  da uno dei due controller ATA. Slave PIC, linee 14 o 15
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
//  da uno dei due controller ATA. Slave PIC, linee 14 o 15
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
	.global _hd_write_address
_hd_write_address:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %edx
	pushl %edi
	
	movl 12(%ebp),%eax
	movl 8(%ebp),%edi	// accede al descrittore per comodita`
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
	andb $0xf0,%al		// maschera per l'indirizzo in DRV_HD
	andb $0x0f,%ah		// maschera per i 4 bit +sign di primo
	orb  $0xe0,%ah		// seleziona LBA
	orb %ah,%al
	outb %al,%dx		// caricato iDRV_HD
	
	popl %edi
	popl %edx
	popl %eax
	leave
	ret

////////////////////////////////////////////////////////////////
// sezione dati: tabelle e stack				      //
////////////////////////////////////////////////////////////////
.data
.global		_ticks
_ticks:		.long 0
.global		_clocks_per_usec
_clocks_per_usec:
		.long 1
.global		_mem_upper
_mem_upper:	 .long _end
.global		_fine_codice_sistema
_fine_codice_sistema:
		.long _etext
.global		 _esecuzione
_esecuzione:	.long 0
.global		_pronti
_pronti:	.long 0
.global		_array_dess
_array_dess:	.space SIZE_DESS * MAX_SEMAFORI 
// Descrittore Hard Disk
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
	.long	0		//hd[0].disco[0].tot_sett
	.long	0		//hd[0].disco[0].part
	.long	0		//hd[0].disco[1].{presente,dma} (sono byte!)
	.long	0		//hd[0].disco[1].tot_sett
	.long	0		//hd[0].disco[1].part
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
	.long	0		//hd[1].disco[0].tot_sett
	.long	0		//hd[1].disco[0].part
	.long	0		//hd[1].disco[1].{presente,dma} (sono byte!)
	.long	0		//hd[1].disco[1].tot_sett
	.long	0		//hd[1].disco[1].part
	.long	0		//hd[1].comando
				//hd[1].errore (byte!)
	.long	0		//hd[1].cont (byte!)
	.long	0		//hd[1].punt
	.long	0		//hd[1].mutex
	.long	0		//hd[1].sincr
	
	// puntatori alle tabelle GDT e IDT
	// nel formato richiesto dalle istruzioni LGDT e LIDT
gdt_pointer:
	.word 0xffff		 	// limite della GDT
	.long gdt			// base della GDT
idt_pointer:
	.word 0x7FF			// limite della IDT (256 entrate)
	.long idt			// base della IDT

.balign 16
gdt:
	// spazio per 5 descrittori piu' i descrittori di TSS 
	// i descrittori verrano costruiti a tempo di esecuzione
	.space 8 * 8192, 0

.balign 16
idt:
	// spazio per 256 gate
	// verra' riempita a tempo di esecuzione
	.space 8 * 256, 0

	.global _stack
_stack:
	.space STACK_SIZE, 0
terminate_stack:
	.space STACK_SIZE, 0
terminate_stack_end:
