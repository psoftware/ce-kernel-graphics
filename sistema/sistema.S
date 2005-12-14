# sistema.s

#define ASM 1
#include "multiboot.h"
#include "costanti.h"


###############################################################
# MACRO                                                       #
###############################################################
# definiamo delle macro che verranno utilizzate piu' avanti

# salvataggio dei registri in pila
.macro salva_registri

	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp

.endm

# caricamento dei registri dalla pila (duale rispetto a salva_registri)
.macro carica_registri

	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx
	popl %eax

.endm

# salvataggio dei registri in pila per chiamate di sistema che ritornano
#  un valore in eax, che non viene salvato
.macro salva_reg_rit

	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp

.endm


# ripristino dei registri (tutti meno eax) dalla pila (duale rispetto a
#  salva_reg_rit)
.macro carica_reg_rit

	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx

.endm

# copia i parametri dalla pila utente (o sistema) 
# alla pila sistema, da usare nelle a_primitive
# n_long: numero di parole lunghe da copiare
# n_reg: numero di registri salvati in cima alla pila
#        (questa informazione e' necessaria, in 
#         quanto copia_param verra' chiamata dopo
#         il salvataggio dei registri in pila, e tale
#         numero varia da primitiva a primitiva)
.macro copia_param n_long n_reg

        movl $\n_reg, %ecx
        movw 4(%esp, %ecx, 4), %ax      # CS in AX
        andb $0b00000011, %al	        # CPL del chiamante in AL
	cmpb $LIV_SISTEMA, %al		# se sistema, non c'e' stato cambio pila
        je 1f                           # copia da pila sistema 
        movl 12(%esp, %ecx, 4), %eax    # vecchio ESP (della pila utente) in EAX
        leal 4(%eax), %esi		# indirizzo del primo parametro in ESI
        jmp 2f
1:	leal 16(%esp, %ecx, 4), %esi	# indirizzo del primo parametro in ESI
2:	movl $\n_long, %eax		# creazione in pila dello spazio per
        shll $2, %eax			#  la copia dei parametri
        subl %eax, %esp
        leal (%esp), %edi		# indirizzo della destinazione del
					#  primo parametro in EDI
        movl $\n_long, %ecx
        cld
        rep movsl			# copia dei parametri

.endm


# Carica un gate della IDT
# num: indice (a partire da 0) in IDT del gate da caricare
# routine: indirizzo della routine da associare al gate
# dpl: dpl del gate (LIV_SISTEMA o LIV_UTENTE)
# NOTA: la macro si limita a chiamare la routine _init_gate
#       con gli stessi parametri. Verra' utilizzata per
#       motivi puramente estetici
.macro carica_gate num routine dpl

	pushl $\dpl
	pushl $\routine
	pushl $\num
	call _init_gate
	addl $12, %esp

.endm

# Carica un descrittore della GDT
# num: indice (a partire da 0) in GDT del descrittore da caricare
# base: base del segmento
# limite: campo limite (su 20 bit)
# pres: bit di presenza (usare le costanti PRES e NON_P)
# dpl: dpl del segmento (usare le costanti LIV_SISTEMA o LIV_UTENTE)
# tipo: tipo del gate (usare le costanti SEG_CODICE, SEG_DATI o SEG_TSS)
# gran: granularita' (usare le costanti G_PAGINA o G_BYTE)
# NOTA: la macro si limita a chiamare la routine _init_descrittore
#       con gli stessi parametri. Verra' utilizzata per
#       motivi puramente estetici
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

# Estrae la base del segmento da un descrittore.
# Si aspetta l'indirizzo del descrittore in %eax,
# lascia la base del segmento in %ebx
# NOTA: il formato dei descrittori di segmento dei 
#       processori Intel x86, per motivi di compatibilita'
#       con i processori Intel 286 (che erano a 16 bit),
#       e' piu' complicato di quello visto a lezione.
#       In particolare, i byte che compongono il campo base
#       non sono consecutivi
.macro estrai_base 

        movb 7(%eax), %bh	# bit 31:24 della base in %bh
        movb 4(%eax), %bl	# bit 23:16 della base in %bl
        shll $16, %ebx		# bit 31:16 nella parte alta di %ebx
	movw 2(%eax), %bx	# bit 15:0 nella parte basse di %ebx
	
.endm


##########################################################################
# AVVIO                                                                  #
##########################################################################
# Il bootstrap loader attiva il modo protetto (per poter accedere agli
# indirizzi di memoria principale superiori a 1MB) e carica il sistema,
# assieme agli eventuali moduli, in memoria. Quindi, salta alla prima
# istruzione del sistema. Il bootstrap loader puo' anche passare
# delle informazioni al sistema (tramite i registri e la memoria).
#
# In questo sistema, usiamo lo standard multiboot, che definisce il 
# formato che il file contentente il sistema deve rispettare, e
# il formato delle informazioni passate dal bootstrap loader al sistema.
# Il formato del file contenente il sistema deve essere quello di un
# normale file eseguibile, ma, nei primi 2*4K byte, deve contenere 
# la struttura multiboot_header, definita piu' avanti. 
# Il boot loader, prima di saltare alla prima istruzione del sistema
# (l'entry point specificato nel file eseguibile) lascia nel registro
# %eax un valore di riconoscimento, e in %ebx l'indirizzo di una struttura
# dati, contentente varie informazioni (in particolare, la quantita'
# di memoria principale installata nel sistema, il dispositivo da cui
# e' stato eseguito il bootstrap, e l'indirizzo di memoria in cui sono
# stati caricati gli eventuali moduli)
     .text

     .globl  start, _start
start:				# entry point
_start:
     jmp     multiboot_entry	# scavalchiamo la struttra richiesta
     				# dal bootstrap loader, che deve
				# trovarsi verso l'inizio del file

     .align  4
multiboot_header:
     .long   MULTIBOOT_HEADER_MAGIC 				# valore magico
     .long   MULTIBOOT_HEADER_FLAGS   				# flag
     .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS) # checksum
#ifndef __ELF__
     .long   multiboot_header
     .long   _start
     .long   _edata
     .long   _end
     .long   multiboot_entry
#endif /* ! __ELF__ */

multiboot_entry:

     movl    $(stack + STACK_SIZE), %esp # inizializziamo lo stack
     call    init_gdt
     lgdt    gdt_pointer		# carichiamo la nuova GDT

     ljmp    $SEL_CODICE_SISTEMA, $qui	# ricarichiamo CS
qui:
     movw    $SEL_DATI_SISTEMA,   %cx   # e gli altri selettori
     movw    %cx, %ss
     movw    %cx, %ds
     movw    %cx, %es
     movw    $0, %cx			# FS e GS non sono usati
     movw    %cx,  %fs
     movw    %cx,  %gs

     movl    $0, %ebp

     call    init_idt			# riempie i gate per le eccezioni
     lidt    idt_pointer		# carichiamo la nuova IDT
	
     pushl   $0				# resettiamo EFLAG
     popf				# N.B.: interrupt disabilitati

     pushl   %ebx			# parametri passati dal loader
     pushl   %eax			
     call    _cmain			# il resto dell'inizializzazione
     					# e' scritto in C++
     addl    $8, %esp

     pushl   $halt_message
     call    _printk

loop:   hlt
     jmp     loop

halt_message:
     .asciz  "Halted."

#################################################################
# funzioni di utilita'						#
#################################################################

# stampa sulla console gli lo stack degli indirizzi di ritorno
# attualmente in pila (utile per il debugging)
stringa_backtrace:
	.asciz "%x\n"
	.global _backtrace
_backtrace:
	pushl %ebp
	movl %esp, %ebp
	subl $4, %esp
	pushl %esi

	movl $5, -4(%ebp)
	movl %ebp, %esi
3:
	cmpl $0, -4(%ebp)
	je 2f
	cmpl $0, %esi
	je 2f
	jmp 1f
1:
	pushl 4(%esi)
	pushl $stringa_backtrace
	call _printk
	addl $8, %esp
	movl (%esi), %esi
	decl -4(%ebp)
	jmp 3b
2:
	popl %ecx
	popl %esi
	leave
	ret
	

# inserimento del processo in _esecuzione in testa alla coda dei pronti
#  (non salva i registri, viene chiamata dopo salva_stato)
#
inspronti:
        movl _esecuzione, %eax
        movl _pronti, %ebx
        movl %ebx, 8(%eax)
        movl %eax, _pronti
	ret

# offset, all'interno della struttura des_proc, dei campi
# destinati a contere i registri del processore
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

# salva lo stato del processo corrente nel suo descrittore
#
salva_stato:
        pushl %ebx
	pushl %eax

	movl _esecuzione, %eax
	movl $0, %ebx
        movw (%eax), %bx		# esecuzione->identifier in ebx
        leal gdt(, %ebx, 8), %eax       # ind. entrata della gdt relativa in eax
	estrai_base			# ind. TSS -> %ebx

        popl %eax

	movl %eax, EAX(%ebx)		# salvataggio dei registri
	movl %ecx, ECX(%ebx)
	movl %edx, EDX(%ebx)
	popl %eax			# vecchio valore di %ebx in %eax
	movl %eax, EBX(%ebx)
	movl %esp, %eax
	addl $4, %eax			# saltiamo ind. rit. di salva_stato...
	movl %eax, ESP(%ebx)		# ...prima di memorizzare %esp
	movl %ebp, EBP(%ebx)
	movl %esi, ESI(%ebx)
	movl %edi, EDI(%ebx)
	movw %es, ES(%ebx)
	movw %ss, SS(%ebx)
	movw %ds, DS(%ebx)
	movw %fs, FS(%ebx)
	movw %gs, GS(%ebx)

	movw $SEL_DATI_SISTEMA, %ax		# selettori usati dal nucleo
	movw %ax, %ds
	movw %ax, %es
	# ss contiene gia' il valore corretto
	movw $0, %ax
	movw %ax, %fs
	movw %ax, %gs

	fsave FPU(%ebx)

	ret

# carica lo stato del processo in _esecuzione
#
carica_stato:
        movl _esecuzione, %edx
	movl $0, %ebx
        movw (%edx), %bx		# esecuzione->identifier in ebx
	
	movl %ebx, %ecx			
	shll $3, %ecx			# trasformo indice->selettore

        leal gdt(, %ebx, 8), %eax       # ind. entrata della gdt relativa in eax
	estrai_base			# ind. del TSS in %ebx
        andl $0xfffffdff, 4(%eax)       # bit busy del TSS a zero

       	ltr %cx				# nuovo valore in TR

        frstor FPU(%ebx)

	movw GS(%ebx), %ax		# ripristino dei registri
	movw %ax, %gs
	movw FS(%ebx), %ax
	movw %ax, %fs
	movw DS(%ebx), %ax
	movw %ax, %ds
	movw SS(%ebx), %ax
	movw %ax, %ss
	movw ES(%ebx), %ax
	movw %ax, %es

	popl %ecx                       # toglie dalla pila l' ind. di ritorno

	movl CR3(%ebx), %eax		# cambio di direttorio
	movl %eax, %cr3			# NOTA: siamo sicuri della continuita'
					# dell'indirizzamento, in quanto il sistema
					# e' mappato agli stessi indirizzi in tutti
					# gli spazi di memoria

	movl ESP(%ebx), %esp            # nuovo punt. di pila...
	pushl %ecx			# salvataggio ind. di ritorno nella nuova pila

	movl ECX(%ebx), %ecx
	movl EDI(%ebx), %edi
	movl ESI(%ebx), %esi
	movl EBP(%ebx), %ebp
	movl EDX(%ebx), %edx
	movl EAX(%ebx), %eax
	movl EBX(%ebx), %ebx

	ret

# carica la gdt
# Utilizziamo il modello di memoria flat: usiamo solo 4 segmenti 
# sovrapposti, grandi quanto tutto lo spazio di indirizzamento linerare (4GB)
# Due segmenti (indici 1 e 2) servono per il livello di privilegio sistema
# e due (indici 3 e 4) per il livello di privilegio utente.
# Il segmento di indice 0 deve essere nullo.
# A partire dal descrittore di indice 5, carichiamo i descrittori di segmento TSS,
# ognuno associato alla corrispondente struttura des_proc nell'array array_desp.
# Prepariamo staticamente un descrittore per ognuno dei MAX_PROCESSI possibili,
# anche se ne verranno utilizzati di meno. Usiamo il bit P di presenza nel descittore
# per specificare quali descrittori sono effettivamente utilizzati.
# Inizialmente, tutti i bit di presenza sono 0, e passeranno ad 1 quando verrano
# invocate le primitive activate_p e activate_pe
init_gdt:
	pushl %ebp
	movl %esp, %ebp
	pushl %ecx
	pushl %ebx

			#indice	base	limite	P	DPL		TIPO		G
	carica_descr	0	0	0	NON_P	0		0		0
	carica_descr	1	0	0xfffff	PRES	LIV_SISTEMA	SEG_CODICE	G_PAGINA
	carica_descr	2	0	0xfffff	PRES	LIV_SISTEMA	SEG_DATI	G_PAGINA
	carica_descr	3	0	0xfffff	PRES	LIV_UTENTE	SEG_CODICE	G_PAGINA
	carica_descr	4	0	0xfffff	PRES	LIV_UTENTE	SEG_DATI	G_PAGINA

	movl $_array_desp, %ebx
	movl $5, %ecx
ciclo_tss:
	cmpl $(MAX_PROCESSI + 5), %ecx
	jl carica_tss
	jmp fine_carica_tss
carica_tss:
	pushl $G_BYTE
	pushl $SEG_TSS
	pushl $LIV_SISTEMA
	pushl $NON_P
	pushl $(SIZE_DESP-1)
	pushl %ebx
	pushl %ecx
	call _init_descrittore
	addl $28, %esp
	incl %ecx
	addl $SIZE_DESP, %ebx
	jmp ciclo_tss
fine_carica_tss:
	popl %ebx
	popl %ecx
	leave
	ret

# Tipi delle primitive di sistema
.set tipo_a, TIPO_A
.set tipo_t, TIPO_T
.set tipo_b, TIPO_B
.set tipo_g, TIPO_G
.set tipo_si, TIPO_SI
.set tipo_w, TIPO_W
.set tipo_s, TIPO_S
.set tipo_ma, TIPO_MA
.set tipo_mf, TIPO_MF
.set tipo_d, TIPO_D

# Tipi delle interruzioni usate per l' interfaccia con il modulo di IO
# Devono corrispondere con i valori usati in io.s
#

.set tipo_ae, TIPO_AE
.set tipo_nwfi, TIPO_NWFI
.set tipo_va, TIPO_VA
.set tipo_p, TIPO_P
.set tipo_fg, TIPO_FG
.set tipo_r, TIPO_R

# per la console
.set tipo_cr, TIPO_CR
.set tipo_cw, TIPO_CW
.set tipo_cs, TIPO_CS
.set tipo_cl, TIPO_CL
.set tipo_cu, TIPO_CU
.set tipo_ci, TIPO_CI

.set io_tipo_rhdn, IO_TIPO_RHDN
.set io_tipo_whdn, IO_TIPO_WHDN

# carica la idt
# le prime 20 entrate sono definite dall'Intel, e corrispondono
# alle possibili eccezioni. 
init_idt:
	pushl %ebp
	movl %esp, %ebp
	#		indice		routine			dpl
	# gestori eccezioni:
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

	# driver/handler
	carica_gate	0x20		driver_t	LIV_SISTEMA
	carica_gate	0x21		handler_KBD	LIV_SISTEMA
	carica_gate	0x23		handler_COM2	LIV_SISTEMA
	carica_gate	0x24		handler_COM1	LIV_SISTEMA
	carica_gate	0x2E 		handler_HD0 	LIV_SISTEMA
	carica_gate	0x2F 		handler_HD1 	LIV_SISTEMA

	# primitive utente
	carica_gate	tipo_a		a_activate_p	LIV_UTENTE
	carica_gate	tipo_t		a_terminate_p	LIV_UTENTE
	carica_gate	tipo_b		a_begin_p	LIV_UTENTE
	carica_gate	tipo_g		a_give_num	LIV_UTENTE
	carica_gate	tipo_si		a_sem_ini	LIV_UTENTE
	carica_gate	tipo_w		a_sem_wait	LIV_UTENTE
	carica_gate	tipo_s		a_sem_signal	LIV_UTENTE
	carica_gate	tipo_ma		a_mem_alloc	LIV_UTENTE
	carica_gate	tipo_mf		a_mem_free	LIV_UTENTE
	carica_gate	tipo_d		a_delay		LIV_UTENTE

	# primitive per il livello I/O
	carica_gate	tipo_ae		a_activate_pe	LIV_SISTEMA
	carica_gate	tipo_nwfi	a_nwfi		LIV_SISTEMA
	carica_gate	tipo_va		a_verifica_area	LIV_SISTEMA
	carica_gate	tipo_p		a_panic		LIV_SISTEMA
	carica_gate	tipo_fg		a_fill_gate	LIV_SISTEMA
	carica_gate	tipo_r		_reboot		LIV_SISTEMA

	# gestione console (di Checconi)
	carica_gate	tipo_cr		a_con_read	LIV_SISTEMA
	carica_gate	tipo_cw		a_con_write	LIV_SISTEMA
	carica_gate	tipo_cs		a_con_save	LIV_SISTEMA
	carica_gate	tipo_cl		a_con_load	LIV_SISTEMA
	carica_gate	tipo_cu		a_con_update	LIV_SISTEMA
	carica_gate	tipo_ci		a_con_init	LIV_SISTEMA
	leave
	ret

# carica un gate nella IDT
# parametri: (vedere la macro carica_gate)
.global _init_gate
_init_gate:
	pushl %ebp
	movl %esp, %ebp

	pushl %ebx
	pushl %ecx
	pushl %eax

	movl $idt, %ebx
	movl 8(%ebp), %ecx		# indice nella IDT
	movl 12(%ebp), %eax		# offset della routine

	movw %ax, (%ebx, %ecx, 8)  	# primi 16 bit dell'offset
	movw $SEL_CODICE_SISTEMA, 2(%ebx, %ecx, 8)

	movw $0, %ax
	movb $0b10001110, %ah 	        # byte di accesso
					# (presente, 32bit, tipo interrupt)
	movb 16(%ebp), %al		# DPL
	shlb $5, %al			# posizione del DPL nel byte di accesso
	orb  %al, %ah			# byte di accesso con DPL in %ah
	movb $0, %al			# la parte bassa deve essere 0
	movl %eax, 4(%ebx, %ecx, 8)	# 16 bit piu' sign. dell'offset
					# e byte di accesso

	popl %eax
	popl %ecx
	popl %ebx
	leave
	ret

# carica un descrittore di segmento in GDT
# parametri: (vedere la macro carica_descr)
.global _init_descrittore
_init_descrittore:
	pushl %ebp
	movl %esp, %ebp

	pushl %ebx
	pushl %ecx
	pushl %eax
	pushl %edx

	movl $gdt, %ebx
	movl 8(%ebp), %ecx		# indice GDT -> %ecx
	movl 16(%ebp), %edx		# limite -> %edx
	movw %dx,  (%ebx, %ecx, 8)	# bit 15:00 limite -> 1a parola descr.
	movw 12(%ebp), %ax		# bit 15:00 base -> %ax
	movw %ax, 2(%ebx, %ecx, 8)	# -> 2a parola descr.
	movb 14(%ebp), %al		# bit 23:16 base -> %al
	orb  24(%ebp), %ah		# DPL
	shlb $5, %ah			# posizione del DPL nel byte di accesso
	orb  20(%ebp), %ah		# bit di presenza
	orb  28(%ebp), %ah		# tipo
	movw %ax, 4(%ebx, %ecx, 8)	# -> 3a parola descr.
	movb 15(%ebp), %dh		# bit 31:24 base -> %dh
	shrl $16, %edx			# bit 19:16 limite -> low nibble %dl
	orb  $0b01000000, %dl		# operandi su 32 bit
	orb  32(%ebp), %dl		# granularita'
	movw %dx, 6(%ebx, %ecx, 8)	# -> 4a parola descr.
	
	popl %edx
	popl %eax
	popl %ecx
	popl %ebx

	leave
	ret

# trova un desctrittore di segmento TSS non ancora
# utilizzato, e ne restituisce l'indice in %eax
# (0 se tutti occupati)
# lo stato occupato/libero del descrittore e' dato
# dal valore del suo bit di presenza
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
	cmpl $(5+MAX_PROCESSI), %ecx
	jl 2f
	jmp 3f
2:
	testb $PRES, 5(%ebx, %ecx, 8)
	jnz 4f
	orb $PRES, 5(%ebx, %ecx, 8)
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

# rende nuovamente libero un descrittore di segmento TSS
# precedentemente occupato
# parametri: indice in GDT del descrittore da rilasciare
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

# carica il registro cr3
# parametri: indirizzo fisico del nuovo direttorio
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

# restituisce in %eax il contenuto di cr3
	.global _leggi_cr3
_leggi_cr3:
	movl %cr3, %eax
	ret

# attiva la paginazione
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

# restituisce in %eax il contenuto di cr2
	.global _leggi_cr2
_leggi_cr2:
	movl %cr2, %eax
	ret

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

##################################################################
# hardware gestito direttamente dal nucleo: 			 #
#  PIC, timer, console, swap					 #
##################################################################
_reboot:
die:	jmp die				

# Replicato in io.s
#
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

# Replicato in io.s
#
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


# PIC
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


# inizializza il controllore delle interruzioni la base del controllore va
# cambiata rispetto a quella impostata dal BIOS, in quanto, in modo protetto,
# esistono piu' tipi di eccezioni che in modo reale, e la base impostata dal
# BIOS va a collidere con alcune di queste.
	.global _init_8259
_init_8259:
	pushl %ebp
	movl %esp, %ebp
	pushl %eax

	# master
	movb $0x11, %al		# cascata
	outb %al, $ICW1M
	movb $0x20, %al		# nuova base
	outb %al, $ICW2M
	movb $0x04, %al		# slave connesso a IR2
	outb %al, $ICW3M
	movb $0x01, %al		# modo annidato
	outb %al, $ICW4M
	movb $0b11111011, %al	# maschera tutte le interruzioni, tranne quelle
	outb %al, $OCW1M	#  provenienti dallo slave
	movb $0x48, %al
	outb %al, $OCW3M	# fully nested

	# slave
	movb $0x11, %al		# cascata
	outb %al, $ICW1S
	movb $0x28, %al		# nuova base
	outb %al, $ICW2S
	movb $0x02, %al		# id. dello slave
	outb %al, $ICW3S
	movb $0x01, %al		# modo annidato
	outb %al, $ICW4S
	movb $0b11111111, %al	# maschera tutte le interruzioni
	outb %al, $OCW1S
	movb $0x48, %al		# fully nested
	outb %al, $OCW3S

	popl %eax
	leave
	ret

# timer
.set CWR,     0x43
.set CTR_LSB, 0x40
.set CTR_MSB, 0x40

# attiva il timer di sistema
# parametri: il valore da caricare nel registro CTR del timer
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


	.global _salta_a_main
_salta_a_main:
	call carica_stato
	iret

# console
	.extern _c_con_read
a_con_read:
	salva_registri
	copia_param 2 7
	call _c_con_read
	addl $8, %esp
	carica_registri
	iret

	.extern _c_con_write
a_con_write:
	salva_registri
	copia_param 2 7
	call _c_con_write
	addl $8, %esp
	carica_registri
	iret

	.extern _c_con_save
a_con_save:
	salva_registri
	copia_param 2 7
	call _c_con_save
	addl $8, %esp
	carica_registri
	iret

	.extern _c_con_load
a_con_load:
	salva_registri
	copia_param 2 7
	call _c_con_load
	addl $8, %esp
	carica_registri
	iret

	.extern _c_con_update
a_con_update:
	salva_registri
	copia_param 3 7
	call _c_con_update
	addl $12, %esp
	carica_registri
	iret

	.extern _c_con_init
a_con_init:
	salva_registri
	copia_param 1 7
	call _c_con_init
	addl $4, %esp
	carica_registri
	iret

################################################################
# gestori delle eccezioni				       #
################################################################
# alcune eccezioni lasciano in pila un ulteriore parola lunga
# (il cui significato dipende dal tipo di eccezione)
# Per uniforimita', facciamo eseguire una pushl $0 come
# prima istruzione di tutte le eccezioni che non prevedono
# questa ulteriore parola lunga.
# Inoltre, il trattamento di tutte le eccezioni e' simile:
# stampare un avviso sulla console e interrompere il processo
# che ha causato l'eccezione. Per questo motivo, ogni gestore
# mette in pila il numero corrispondente al suo tipo di eccezione
# e salta al codice comune per tutti.
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

page_fault:
	salva_registri
	movl 32(%esp), %eax
	movl 28(%esp), %ebx
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

########################################################
# primitive richiamate dal nucleo stesso (page fault)  #
########################################################
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

########################################################
# handler/driver                                       #
########################################################
	.extern _c_driver_t
driver_t:
	call salva_stato
	call inspronti
	call _c_driver_t
	movb $EOI, %al         # ack al controllore
	outb %al, $OCW2M
	call carica_stato
	iret

	.extern _pe_tast
handler_KBD:
	call salva_stato
	call inspronti

	movl _pe_tast, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

.set IIR1, 0x03fa
.set IIR2, 0x02fa

	.extern _in_com, _out_com
handler_COM1:
	call salva_stato
	call inspronti

	movw $IIR1, %dx
	inb %dx, %al
	andb $0x06, %al
	cmpb $0x02, %al
	jne 1f
	movl _out_com, %eax
	jmp 2f
1:
	movl _in_com, %eax
2:
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_COM2:
	call salva_stato
	call inspronti

	movw $IIR2, %dx
	inb %dx, %al
	andb $0x06, %al
	cmpb $0x02, %al
	jne 1f
	movl _out_com + 4, %eax
	jmp 2f
1:
	movl _in_com + 4, %eax
2:
	movl %eax, _esecuzione

	call carica_stato
	iret

	.extern _ata
handler_HD0:
	call salva_stato
	call inspronti

	movl _ata, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret

handler_HD1:
	call salva_stato
	call inspronti

	movl _ata+4, %eax
	movl %eax, _esecuzione

	call carica_stato
	iret


########################################################
# a_primitive                                          #
########################################################
        .extern _c_activate_p
a_activate_p:	# routine int $tipo_a
	salva_registri
        copia_param 6 7		# salva_registri ha inserito 7 long in pila
        call _c_activate_p
        addl $24, %esp
        carica_registri
        iret

        .extern _c_terminate_p
a_terminate_p:	# routine int $tipo_t
        call salva_stato
        call _c_terminate_p
        call carica_stato
	iret

        .extern _c_begin_p
a_begin_p:	# routine int $tipo_b
	call salva_stato
        call _c_begin_p
        call carica_stato
	iret

        .extern _c_give_num
a_give_num:	# routine int $tipo_g
	salva_registri
        copia_param 1 7
        call _c_give_num
        addl $4, %esp
        carica_registri
	iret

	.extern _c_sem_ini
a_sem_ini:	# routine int $tipo_si
	salva_registri
	copia_param 3 7
	call _c_sem_ini
	addl $12, %esp
	carica_registri
	iret

	.extern _c_sem_wait
a_sem_wait:	# routine int $tipo_w
	call salva_stato
	copia_param 1 0
	call _c_sem_wait
	# addl $4, %esp			# non necessario
	call carica_stato
	iret

	.extern _c_sem_signal
a_sem_signal:	# routine int $tipo_s
	call salva_stato
	copia_param 1 0
	call _c_sem_signal
	# addl $4, %esp			# non necessario
	call carica_stato
	iret

	.extern _c_mem_alloc
a_mem_alloc:	# routine int $tipo_ma
	salva_reg_rit
	copia_param 1 6
	call _c_mem_alloc
	addl $4, %esp
	carica_reg_rit
	iret

	.extern _c_mem_free
a_mem_free:	# routine int $tipo_mf
	salva_registri
	copia_param 1 7
	call _c_mem_free
	addl $4, %esp
	carica_registri
	iret

	.extern _c_delay
a_delay:	# routine int $tipo_d
	call salva_stato
	copia_param 1 0
	call _c_delay
	# addl $4, %esp			# non necessario
	call carica_stato
	iret

	.global _abort_p
_abort_p:
	call _c_terminate_p
	call carica_stato
	iret

#
# Interfaccia offerta al modulo di IO, inaccessibile dal livello utente
#

	.extern _c_activate_pe
a_activate_pe:
	salva_registri
        copia_param 7 7		# salva_registri ha inserito 7 long in pila
        call _c_activate_pe
        addl $28, %esp	
	carica_registri
	iret


	.extern _c_nwfi
a_nwfi:		# routine int $tipo_nwfi
	call salva_stato

	cmpl $1, 16(%esp)
	jne m_ack

	movb $EOI, %al		# ack al controllore slave
	outb %al, $OCW2S

	movb $READ_ISR, %al	# lettura di ISR dello slave
	outb %al, $OCW3S
	inb $OCW3S, %al
	testb $0xff, %al
	jnz m_noack		# ci sono ancora richieste dello slave attive
m_ack:
	movb $EOI, %al
	outb %al, $OCW2M
m_noack:	
	call _schedulatore
	call carica_stato
	iret

	.extern _verifica_area
a_verifica_area:
	salva_reg_rit
	copia_param 3 6
	call _verifica_area
	addl $12, %esp
	carica_reg_rit
	iret

	.extern _panic
a_panic:
	copia_param 1 0
	call _panic			# panic blocca il sistema

a_fill_gate:
	salva_registri
	copia_param 3 7
	call _init_gate
	addl $12, %esp
	carica_registri
	iret

###############################################################
# sezione dati: tabelle e stack				      #
###############################################################
.data
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
.global		_array_desp
_array_desp:	.space SIZE_DESP * MAX_PROCESSI
	
	# puntatori alle tabelle GDT e IDT
	# nel formato richiesto dalle istruzioni LGDT e LIDT
gdt_pointer:
	.word (5+MAX_PROCESSI)*8-1 	# limite della GDT
	.long gdt			# base della GDT
idt_pointer:
	.word 0x7FF			# limite della IDT (256 entrate)
	.long idt			# base della IDT

.align 16
gdt:
	# spazio per 5 descrittori piu' i descrittori di TSS 
	# i descrittori verrano costruiti a tempo di esecuzione
	.space 8 * (5 + MAX_PROCESSI), 0

	.align 16
idt:
	# spazio per 256 gate
	# verra' riempita a tempo di esecuzione
	.space 8 * 256, 0

stack:
	.space STACK_SIZE, 0
