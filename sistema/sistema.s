# sistema.s
#

##############################################################################
##                                  COSTANTI                                ##
##############################################################################

# Devono corrispondere ai valori usati in sistema.cpp
#

.set STACK_SIZE, 0x800          # dimensione dello stack iniziale
.set KERNEL_CODE, 0x8           # selettore del segm. codice del nucleo
.set KERNEL_DATA, 0x10          # selettore del segm. dati del nucleo
.set USER_CODE, 0x1b            # selettore del segm. codice dei progr. utente
.set USER_DATA, 0x23            # selettore del segm. dati dei progr. utente

# Il vettore dei descrittori di semaforo ha 64 elementi di
#  8 byte l' uno, assumendo interi e puntatori su 32 bit
# Deve corrispondere alla dimensione specificata in sistema.cpp per _array_dess
#
.set BYTE_SEM, 8*64

# Indirizzi di caricamento dei moduli del sistema
#
.set USER_LOADADDR, 0x02000000
.set IO_LOADADDR, 0x00198000
.set KERN_LOADADDR, 0x00100000

##############################################################################
##                   MACRO PER LA MANIPOLAZIONE DEI PARAMETRI               ##
##############################################################################

# copia dei parametri di una chiamata di sistema dalla pila del chiamante,
#  sia esso utente o sistema, alla pila sistema
#
# n_long: numero di interi su 32 bit da copiare
# offset: spiazzamento (in parole di 32 bit) dei parametri nella pila
#  sorgente
#
# Replicato in io.s per le primitive di IO
#
.macro copia_param n_long offset

        movl $\offset, %ecx
        movl 4(%esp, %ecx, 4), %eax     # cs in eax
        testl $3, %eax			# verifica del livello di privilegio
					#  del chiamante
        jz 1f                           # copia da pila sistema (cpl = 0)

        movl 12(%esp, %ecx, 4), %eax    # vecchio esp (della pila utente)
					#  in eax
        leal 4(%eax), %esi		# indirizzo del primo parametro in
					#  esi
        jmp 2f

1:
        leal 16(%esp, %ecx, 4), %esi	# indirizzo del primo parametro in esi
2:
        movl $\n_long, %eax		# creazione in pila dello spazio per
        shll $2, %eax			#  la copia dei parametri
        subl %eax, %esp
        leal (%esp), %edi		# indirizzo della destinazione del
					#  primo parametro in edi

        movl $\n_long, %ecx
        cld
        rep
           movsl			# copia dei parametri

.endm

# salvataggio dei registri in pila
# Replicato in io.s
#
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
# Replicato in io.s
#
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
#
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
#
.macro carica_reg_rit

	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx

.endm

##############################################################################
##                                SEZIONE DATI                              ##
##############################################################################

	.data
	.global _esecuzione
_esecuzione:	.long 0

	.global _pronti
_pronti:	.long 0

	.global _array_dess
_array_dess:	.space BYTE_SEM

gdt_ptr: .long 0
idt_ptr: .long 0x10000

gdt_pd: .word 0xffff
        .long 0

idt_pd: .word 0x800
        .long 0x10000

##############################################################################
##                               SEZIONE TESTO                              ##
##############################################################################

	.text
# Il bootloader carica questo codice all' indirizzo 0x8000 e lo esegue
#  (a partire da start)
#

# variabili usate per il caricamento dei programmi (sono impostate
#  nell' immagine dopo la compilazione del nucleo)
#
	.global _io_init
_io_init:		.long	0

	.global _io_end
_io_end:		.long	0	# offset hardcoded

	.global _main
_main:			.long	0

	.global _u_usr_shtext_end
_u_usr_shtext_end:	.long	0

	.global _u_sys_shtext_end
_u_sys_shtext_end:	.long	0

	.global _u_data
_u_data:		.long	0

	.global _u_usr_shdata_end
_u_usr_shdata_end:	.long	0

	.global _u_sys_shdata_end
_u_sys_shdata_end:	.long	0

	.global _u_end
_u_end:			.long	0	# offset hardcoded

##############################################################################
##                            CODICE DI AVVIAMENTO                          ##
##############################################################################

        .global start
start:				# offset hardcoded nel bootloader
	movw $0x10, %ax		# caricamento dei registri selettori con
	                        # valori significativi nel modo protetto
				# NOTA: il puntatore di pila non e`
				# significativo, ma le interruzioni sono
				# disabilitate

	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss

        movl $0x8000, %esi              # spostamento del nucleo (e dei
        movl $KERN_LOADADDR, %edi	#  programmi utente) ad 1Mb

        movl $_end, %ecx
        subl $KERN_LOADADDR, %ecx	# dimensione del nucleo in ecx

	movl 0x00008004, %eax		# _io_end in ecx
					# non si puo' usare il simbolo
					#  perche' questo codice non e'
					#  ancora caricato all' indirizzo
					#  0x100000
	subl $IO_LOADADDR, %eax		# lunghezza del modulo di io in eax
	addl %eax, %ecx			# totale delle dimesioni (sistema
					#  e io) in ecx

        movl 0x00008020, %ecx		# _u_end in ecx
        subl $USER_LOADADDR, %ecx	# lunghezza in bytes dei programmi
        				#  utente in ecx
        addl %eax, %ecx			# totale in ecx

        shll $2, %ecx
        cld
        rep
        	movsl			# trasferimento ad 1Mb

        ljmp $KERNEL_CODE, $start_high

start_high:
        movl $(_stack + STACK_SIZE), %esp       # stack iniziale

	pushl $0                                # flag in uno stato noto
	popfl

	call i8259_init	        	# iniz. i8259
	call idt_init			# iniz. idt
        call gdt_init			# iniz. gdt

	ljmp $KERNEL_CODE, $go		# ricarica cs
go:
	movw $KERNEL_DATA, %ax		# ricarica ds, es ed ss
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0, %ax			# fs e gs non sono usati
	movw %ax, %fs
	movw %ax, %gs

        jmp _cpp_init                   # esegue l' inizializzazione di alto
                                        #  livello (in sistema.cpp)

	.comm _stack, STACK_SIZE

##############################################################################
##                           FUNZIONI AD USO INTERNO                        ##
##############################################################################

# offset dei registri del processore nella struttura des_proc
# Devono rispettare le regole di corrispondenza con la struct
#

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

.set FPU, 112

# salva lo stato del processo corrente nel suo descrittore
#
salva_stato:
        pushl %ebx
	pushl %eax
        pushl %edx

	xorl %ebx, %ebx
	movl _esecuzione, %edx
        movw (%edx), %bx		# esecuzione->identifier in ebx
        shrl $3, %ebx			# indice del tss in ebx
        movl gdt_ptr, %edx
        leal (%edx, %ebx, 8), %eax      # ind. entrata della gdt relativa in eax
        movl (%eax), %ebx
        movl 4(%eax), %edx
        shrl $16, %ebx
        movl %edx, %eax
        andl $0xff, %eax
        shll $16, %eax
        orl %eax, %ebx
        andl $0xff000000, %edx
        orl %edx, %ebx                  # ind. tss in %ebx

        popl %edx
        popl %eax

	movl %eax, EAX(%ebx)		# salvataggio dei registri
	movl %ecx, ECX(%ebx)
	movl %edx, EDX(%ebx)
	movl (%esp), %eax
	movl %eax, EBX(%ebx)
	movl %esp, %eax
	addl $8, %eax
	movl %eax, ESP(%ebx)
	movl %ebp, EBP(%ebx)
	movl %esi, ESI(%ebx)
	movl %edi, EDI(%ebx)
	movw %es, ES(%ebx)
	movw %ss, SS(%ebx)
	movw %ds, DS(%ebx)
	movw %fs, FS(%ebx)
	movw %gs, GS(%ebx)

	movw $KERNEL_DATA, %ax		# selettori usati dal nucleo
	movw %ax, %ds
	movw %ax, %es
	xorw %ax, %ax
	movw %ax, %fs
	movw %ax, %gs

	fsave FPU(%ebx)

	addl $4, %esp

	ret

# carica lo stato del processo in _esecuzione
#
carica_stato:
	xorl %ebx, %ebx
        movl _esecuzione, %edx
        movw (%edx), %bx		# esecuzione->identifier in ebx
        pushl %ebx

        shrl $3, %ebx
        movl gdt_ptr, %edx
        leal (%edx, %ebx, 8), %eax      # ind. entrata della gdt relativa in eax
        andl $0xfffffdff, 4(%eax)       # bit busy del tss a zero

        popl %ebx
       	ltr %bx

        movl (%eax), %ebx
        movl 4(%eax), %edx
        shrl $16, %ebx
        movl %edx, %eax
        andl $0xff, %eax
        shll $16, %eax
        orl %eax, %ebx
        andl $0xff000000, %edx
        orl %edx, %ebx                  # base del tss in %ebx

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

	movl CR3(%ebx), %eax
	movl %eax, %cr3			# il nucleo e' in esecuzione nella mem.
					#  fisica in memoria virtuale, presente
     					#  in tutti i direttori agli stessi
					#  indirizzi

	movl EDI(%ebx), %edi
	movl ESI(%ebx), %esi
	movl EBP(%ebx), %ebp
	movl EDX(%ebx), %edx
	popl %ecx                       # toglie dalla pila l' ind. di ritorno
	movl ESP(%ebx), %eax            # nuovo punt. di pila...
	subl $4, %eax                   #
	movl %ecx, (%eax)               # ..col vecchio ind. di ritorno
	movl %eax, %esp
	movl ECX(%ebx), %ecx
	movl EAX(%ebx), %eax
	movl EBX(%ebx), %ebx

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

##############################################################################
##                           CHIAMATE DI SISTEMA                            ##
##############################################################################

#
# Entry-points
#
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
	call salva_stato	# main ha gia' un descrittore
        call _c_begin_p
        call attiva_timer
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

.set EOI, 0x20
.set READ_ISR, 0x0b

.set OCW2M, 0x20
.set OCW3M, 0x20
.set OCW2S, 0xa0
.set OCW3S, 0xa0

	.extern _c_nwfi
a_nwfi:		# routine int $tipo_nwfi
	call salva_stato

	testl $1, 4(%esp)
	jz m_ack

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
	call _panic			# panic blocca il sistema

a_fill_gate:
	salva_registri
	copia_param 4 7
	call fill_gate
	addl $16, %esp
	carica_registri
	iret

##############################################################################
##                                 DRIVER                                   ##
##############################################################################

# NOTA: il testo riporta '.extern _c_driver' e usa 'call _c_driver_t'.
# Funziona anche l' altra versione, cosi` e` piu` coerente, dato che
#  _c_driver non e` definito. E' necessario mandare l' ack al
# controllore delle interruzioni
#

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

##############################################################################
##                         GESTIONE DELLA CONSOLE                           ##
##############################################################################

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

##############################################################################
##                       CODICE DI INIZIALIZZAZIONE                         ##
##############################################################################

#
# Controllore delle interruzioni
#

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

.macro delay

	jmp 0f
0:
	jmp 0f
0:

.endm

# inizializzazione del controllore delle interruzioni
# (non salva i registri, viene chiamata all' avvio)
#
i8259_init:
	movb $0x11, %al
	outb %al, $ICW1M
	delay
	movb $0x20, %al
	outb %al, $ICW2M
	delay
	movb $0x04, %al
	outb %al, $ICW3M
	delay
	movb $0x01, %al
	outb %al, $ICW4M
	delay
	movb $0b11111011, %al	# maschera tutte le interruzioni, tranne quelle
	outb %al, $OCW1M	#  provenienti dallo slave
	delay
	movb $0x40, %al
	outb %al, $OCW3M
	delay

	movb $0x11, %al
	outb %al, $ICW1S
	delay
	movb $0x28, %al
	outb %al, $ICW2S
	delay
	movb $0x02, %al
	outb %al, $ICW3S
	delay
	movb $0x01, %al
	outb %al, $ICW4S
	delay
	movb $0b11111111, %al	# maschera tutte le interruzioni
	outb %al, $OCW1S
	delay
	movb $0x40, %al
	outb %al, $OCW3S
	delay

	ret

#
# Timer
#

.set CWR, 0x43
.set CTR_LSB, 0x40
.set CTR_MSB, 0x40

.set DELAY, 59659		# 1193180*0.05, per avere un periodo di 50ms

attiva_timer:
        pushl %eax

	movb $0x36, %al
	outb %al, $CWR
	movl $DELAY, %eax
	outb %al, $CTR_LSB
	movb %ah, %al
	outb %al, $CTR_MSB

	inb $OCW1M, %al
	andb $0b11111110, %al
	outb %al, $OCW1M

        popl %eax

	ret

#
# Segmentazione
#

.set IDT_P, 0x8000

.set IDT_DPL0, 0x0000
.set IDT_DPL3, 0x6000

.set IDT_TYPE_INT, 0x0e00
.set IDT_TYPE_TRAP, 0x0f00

fill_gate:
        movl 4(%esp), %eax
        movl idt_ptr, %ebx
        leal (%ebx, %eax, 8), %edx
        movl $KERNEL_CODE, %eax
        shll $16, %eax
        movl 8(%esp), %ebx
        andl $0xffff, %ebx
        orl %ebx, %eax
        movl %eax, (%edx)

        movl 8(%esp), %eax
        andl $0xffff0000, %eax
        orl $IDT_P, %eax
        orl 12(%esp), %eax
        orl 16(%esp), %eax
        movl %eax, 4(%edx)

	ret

# idt_fill_gate(g, off, tipo, dpl)
#
.macro idt_fill_gate g off tipo dpl

	pushl $\dpl
	pushl $\tipo
	pushl $\off
	pushl $\g
	call fill_gate
	addl $16, %esp

.endm

# Tipi delle interruzioni usate per le chiamate di sistema
# Devono corrispondere con i valori usati in io.s e utente.s
#

.set tipo_a, 0x30
.set tipo_t, 0x31
.set tipo_b, 0x32
.set tipo_g, 0x33
.set tipo_si, 0x34
.set tipo_w, 0x35
.set tipo_s, 0x36
.set tipo_ma, 0x37
.set tipo_mf, 0x38
.set tipo_d, 0x39

# Tipi delle interruzioni usate per l' interfaccia con il modulo di IO
# Devono corrispondere con i valori usati in io.s
#

.set tipo_ae, 0x40
.set tipo_nwfi, 0x41
.set tipo_va, 0x42
.set tipo_p, 0x43
.set tipo_fg, 0x44
.set tipo_r, 0x45

# Tipi delle interruzioni per la gestione della console, usate dal
#  sottosistema di IO per realizzare i terminali virtuali
# Devono corrispondere con i valori usati in io.s
#

.set tipo_cr, 0x50
.set tipo_cw, 0x51
.set tipo_cs, 0x52
.set tipo_cl, 0x53
.set tipo_cu, 0x54
.set tipo_ci, 0x55

# inizializzazione della idt
# (non salva i registri, viene chiamata all' avvio)
#
idt_init:
	movl idt_ptr, %edi
	xorl %eax, %eax
	movl $0x200, %ecx
	rep
		stosl

	idt_fill_gate 0 divide_error IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 1 debug IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 2 nmi IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 3 breakpoint IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 4 overflow IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 5 bound_re IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 6 invalid_opcode IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 7 dev_na IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 8 double_fault IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 9 coproc_so IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 10 invalid_tss IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 11 segm_fault IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 12 stack_fault IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 13 prot_fault IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 14 page_fault IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 16 fp_exc IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 17 ac_exc IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 18 mc_exc IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 19 simd_exc IDT_TYPE_INT IDT_DPL0

	idt_fill_gate 0x20 driver_t IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 0x21 handler_KBD IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 0x23 handler_COM2 IDT_TYPE_INT IDT_DPL0
	idt_fill_gate 0x24 handler_COM1 IDT_TYPE_INT IDT_DPL0

	idt_fill_gate tipo_a a_activate_p IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_t a_terminate_p IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_b a_begin_p IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_g a_give_num IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_si a_sem_ini IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_w a_sem_wait IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_s a_sem_signal IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_ma a_mem_alloc IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_mf a_mem_free IDT_TYPE_INT IDT_DPL3
	idt_fill_gate tipo_d a_delay IDT_TYPE_INT IDT_DPL3

	idt_fill_gate tipo_ae a_activate_pe IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_nwfi a_nwfi IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_va a_verifica_area IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_p a_panic IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_fg a_fill_gate IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_r _reboot IDT_TYPE_INT IDT_DPL0

	idt_fill_gate tipo_cr a_con_read IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_cw a_con_write IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_cs a_con_save IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_cl a_con_load IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_cu a_con_update IDT_TYPE_INT IDT_DPL0
	idt_fill_gate tipo_ci a_con_init IDT_TYPE_INT IDT_DPL0

	lidt idt_pd
	ret

#
# GDT
#

# seg_fill_desc(desc, base, limite, tipo, dpl, g)
#
# riempie il descrittore o il gate
# (distrugge il contenuto dei registri)
#

seg_fill_desc:
        movl 4(%esp), %ebx      # ind. descrittore in %ebx
        movl 12(%esp), %eax     # limite
        andl $0xffff, %eax
        movl 8(%esp), %edx      # base
        andl $0xffff, %edx
        shll $16, %edx
        orl %edx, %eax
        movl %eax, (%ebx)       # prima parola lunga

        movl 8(%esp), %eax      # base
        andl $0xff000000, %eax
        orl 24(%esp), %eax      # granularita'
        movl 12(%esp), %edx     # limite
        andl $0x000f0000, %edx
        orl %edx, %eax
        orl $0x00008000, %eax   # presente
        orl 20(%esp), %eax      # dpl
        orl 16(%esp), %eax      # tipo
        movl 8(%esp), %edx      # base
        andl $0x00ff0000, %edx
        shrl $16, %edx
        orl %edx, %eax
        movl %eax, 4(%ebx)      # seconda parola lunga

        ret

.set SEG_DPL0, 0x0000
.set SEG_DPL3, 0x6000

.set SEG_G, 0x00800000
.set SEG_P, 0x00008000

.set SEG_TYPE_DATA, 0x00401000	# b = 1 s = 1 type = data
.set SEG_TYPE_CODE, 0x00401800	# d = 1 s = 1 type = code

.set SEG_DATA_W, 0x0200

.set SEG_CODE_C, 0x0400
.set SEG_CODE_R, 0x0200

.set SEG_TYPE_TSS, 0x0900

.set SEG_TYPE_WDATA, 0x00401200
.set SEG_TYPE_RCODE, 0x00401a00

.macro gdt_fill_desc sel base limite tipo dpl g

        movl $\sel, %eax
        shrl $3, %eax
        movl gdt_ptr, %ebx
        leal (%ebx, %eax, 8), %edx
        pushl $\g
        pushl $\dpl
        pushl $\tipo
        pushl $\limite
        pushl $\base
        pushl %edx
        call seg_fill_desc
        addl $24, %esp

.endm

.macro gdt_fill_tss_desc sel base limite

        movl $\sel, %eax
        shrl $3, %eax
        movl gdt_ptr, %edx
        leal (%ebx, %eax, 8), %edx
        pushl $0
        pushl $SEG_DPL0
        pushl $SEG_TYPE_TSS
        pushl $\limite
        pushl $\base
        pushl %edx
        call seg_fill_desc
        addl $24, %esp

.endm

# inizializzazione della gdt
# (non salva i registri, viene chiamata all' avvio)
#

gdt_init:
        movl gdt_ptr, %edi
        xorl %eax, %eax
        movl $0x4000, %ecx
        rep
           stosl

        gdt_fill_desc KERNEL_CODE 0 0xfffff SEG_TYPE_RCODE SEG_DPL0 SEG_G
        gdt_fill_desc KERNEL_DATA 0 0xfffff SEG_TYPE_WDATA SEG_DPL0 SEG_G
        gdt_fill_desc USER_CODE 0 0xfffff SEG_TYPE_RCODE SEG_DPL3 SEG_G
        gdt_fill_desc USER_DATA 0 0xfffff SEG_TYPE_WDATA SEG_DPL3 SEG_G

        lgdt gdt_pd
        ret

# trasferimento del controllo a main() (a livello di privilegio utente)
#

# offset in des_proc, devono seguire le regole di corrispondenza
#
.set ESP0, 4
.set SS0, 8

	.extern _main_des
        .global _call_main
_call_main:
	movl $_main_des, %ebx

	movl ESP0(%ebx), %esp		# caricamento nuova pila sistema

	movw $USER_DATA, %ax
	movw %ax, %ds
	movw %ax, %es

	pushl SS(%ebx)			# ss
	pushl ESP(%ebx)			# esp
	pushl $0x00000200		# eflags (IF = 1 dopo iret)
	pushl $USER_CODE		# cs
	pushl _main			# eip

        iret

# gestori di eccezioni hardware
#
divide_error:
	movl $0, %eax
	jmp nocode_exc

debug:
	movl $1, %eax
	jmp nocode_exc

nmi:
	movl $2, %eax
	jmp nocode_exc

breakpoint:
	movl $3, %eax
	jmp nocode_exc

overflow:
	movl $4, %eax
	jmp nocode_exc

bound_re:
	movl $5, %eax
	jmp nocode_exc

invalid_opcode:
	movl $6, %eax
	jmp nocode_exc

dev_na:
	movl $7, %eax
	jmp nocode_exc

double_fault:
	movl $8, %eax
	jmp code_exc

coproc_so:
	movl $9, %eax
	jmp nocode_exc

invalid_tss:
	movl $10, %eax
	jmp code_exc

segm_fault:
	movl $11, %eax
	jmp code_exc

stack_fault:
	movl $12, %eax
	jmp code_exc

prot_fault:
	movl $13, %eax
	jmp code_exc

page_fault:
	movl $14, %eax
	jmp code_exc

fp_exc:
	movl $16, %eax
	jmp nocode_exc

ac_exc:
	movl $17, %eax
	jmp code_exc

mc_exc:
	movl $18, %eax
	jmp nocode_exc

simd_exc:
	movl $19, %eax
	jmp nocode_exc

code_exc:
	movl (%esp), %ebx
	movl 4(%esp), %ecx

	jmp comm_exc

nocode_exc:
	movl $0, %ebx
	movl (%esp), %ecx

comm_exc:
	pushl %ecx
	pushl %ebx
	pushl %eax

	call _hwexc

	call carica_stato
	iret

	.global _load_io
_load_io:
	pushl %ecx
	pushl %esi
	pushl %edi

	movl _io_end, %ecx
	subl $IO_LOADADDR, %ecx
	shrl $2, %ecx
	movl $IO_LOADADDR, %edi
	movl $_end, %esi
	cld
	rep
		movsl

	popl %edi
	popl %esi
	popl %ecx

	ret

        .global _des_p
_des_p:
	pushl %ebx
	pushl %edx

	xorl %ebx, %ebx
	movw 12(%esp), %bx		# identificatore in ebx
        shrl $3, %ebx			# indice del tss in ebx
        movl gdt_ptr, %edx
        leal (%edx, %ebx, 8), %eax      # ind. entrata della gdt relativa in eax
        movl (%eax), %ebx
        movl 4(%eax), %edx
        shrl $16, %ebx
        movl %edx, %eax
        andl $0xff, %eax
        shll $16, %eax
        orl %ebx, %eax
        andl $0xff000000, %edx
        orl %edx, %eax                  # ind. tss in %eax
 
	popl %edx
	popl %ebx

	ret

        .global _cur_des
_cur_des:
	movl _esecuzione, %eax
	movw (%eax), %ax
	pushl %eax
	call _des_p
	addl $4, %esp

	ret

null_idt:
	.space 8*32

null_idtr:
	.word 8*32 - 1
	.long null_idt

	.global _reboot
_reboot:
	lidt null_idtr
	xorl %ecx, %ecx
	divl %ecx			# la divisione per 0 provoca un'
 					#  eccezione non gestita
die:	jmp die				# non si sa mai...

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

	.global _abort_p
_abort_p:
	call _c_terminate_p
	call carica_stato
	iret

