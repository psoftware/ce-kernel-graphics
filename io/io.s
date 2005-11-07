# io.s
#

################################################################################
##                                 COSTANTI                                   ##
################################################################################

# Tipi delle interruzioni delle chiamate di sistema usate
# Devono coincidere con quelli usati in sistema.s e utente.s
#
.set tipo_t, 0x31
.set tipo_si, 0x34
.set tipo_w, 0x35
.set tipo_s, 0x36

# Tipi delle interruzioni usate per interfacciarsi al modulo
#  sistema
# Devono coincidere con quelli usati in sistema.s
#
.set tipo_ae, 0x40
.set tipo_nwfi, 0x41
.set tipo_va, 0x42
.set tipo_p, 0x43
.set tipo_fg, 0x44
.set tipo_r, 0x45
.set tipo_cr, 0x50
.set tipo_cw, 0x51
.set tipo_cs, 0x52
.set tipo_cl, 0x53
.set tipo_cu, 0x54
.set tipo_ci, 0x55

# Tipi delle interruzioni usate dalle primitive di IO
# Devono coincidere con quelli usati in utente.s
#
.set io_tipo_rsen, 0x60
.set io_tipo_rseln, 0x61
.set io_tipo_wsen, 0x62
.set io_tipo_wse0, 0x63
.set io_tipo_tr, 0x64
.set io_tipo_tw, 0x65


################################################################################
##                  MACRO PER LA MANIPOLAZIONE DEI PARAMETRI                  ##
################################################################################

# Copia dei parametri di una chiamata di sistema dalla pila utente
#  alla pila sistema
# Replicato in sistema.s
#
.macro copia_param n_long offset
        movl $\offset, %ecx
        movl 4(%esp, %ecx, 4), %eax     # cs in eax
        testl $3, %eax			# verifica del livello di privilegio
					#  del chiamante
        jz 1f                           # copia da pila sistema

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

# Salvataggio dei registri in pila
# Replicato in sistema.s
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

# Caricamento dei registri dalla pila (duale rispetto a salva_registri)
# Replicato in sistema.s
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

################################################################################
##                             SEZIONE DATI                                   ##
################################################################################

	.data

# Descrittori delle interfacce seriali
#
	.global _com		# non comi, como
_com:	.long	0x03f8		# com[0].indreg.iRBR
	.long	0x03f8		# com[0].indreg.iTHR
	.long	0x03fd		# com[0].indreg.iLSR
	.long	0x03f9		# com[0].indreg.iIER
	.long	0x03fa		# com[0].indreg.iIIR
	.long	0		# com[0].mutex
	.long	0		# com[0].sincr
	.long	0		# com[0].cont
	.long	0		# com[0].punt
	.long	0		# com[0].funzione
	.long	0		# com[0].stato
	.long	0x02f8		# com[1].indreg.iRBR
	.long	0x02f8		# com[1].indreg.iTHR
	.long	0x03fd		# com[1].indreg.iLSR
	.long	0x02f9		# com[1].indreg.iIER
	.long	0x02fa		# com[1].indreg.iIIR
	.long	0		# com[1].mutex
	.long	0		# com[1].sincr
	.long	0		# com[1].cont
	.long	0		# com[1].punt
	.long	0		# com[1].funzione
	.long	0		# com[1].stato

################################################################################
##                            SEZIONE TESTO                                   ##
################################################################################

	.text

################################################################################
##                          CHIAMATE DI SISTEMA                               ##
################################################################################

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

################################################################################
##                     INTERFACCIA VERSO IL MODULO SISTEMA                    ##
################################################################################

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
 
################################################################################
##                         FUNZIONI DI SUPPORTO                               ##
################################################################################

# Ingresso di un byte da una porta di IO
# Replicato in sistema.s
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

# Uscita di un byte su una porta di IO
# Replicato in sistema.s
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

# Indirizzi delle porte del controllore delle interruzioni
#
.set OCW1M, 0x21
.set OCW2M, 0x20
.set EOI, 0x20

# Inizio dell' ingresso da una interfaccia seriale
#
	.global _go_inputse
_go_inputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		# ind. di IER in edx
	inb %dx, %al
	orb $0x01, %al			# abilitazione dell' interfaccia a
					#  generare interruzioni in ingresso
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
	outb %al, $OCW1M		# abilitazione del controllore ad
					#  accettare richieste di interruzione
					#  da parte dell' interfaccia
	sti

	popl %ebx
	popl %eax
	ret

# Fine dell' ingresso da un' interfaccia seriale
#
	.global _halt_inputse
_halt_inputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		# ind. di IER in edx
	inb %dx, %al
	and $0xfe, %al
	outb %al, %dx			# disabilitazione della generazione
					#  di interruzioni

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
	outb %al, $OCW1M		# disabilitazione dell 'interruzione al
					#  controllore
	sti

	popl %ebx
	popl %eax
	ret

# Inizio dell' uscita su interfaccia seriale
#
	.global _go_outputse
_go_outputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		# ind. di IER in edx
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

	popl %ebx
	popl %eax
	ret

# Fine dell' uscita su interfaccia seriale
#
	.global _halt_outputse
_halt_outputse:
	pushl %eax
	pushl %edx

	movl 12(%esp), %edx		# ind. di IER in edx
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

	popl %ebx
	popl %eax
	ret

# Indirizzi delle porte delle interfacce seriali
#
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

# Inizializzazione delle interfacce seriali
# 
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
	movw $MCR1, %dx			# abilitazione porta 3-state
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

# Abilitazione del controllore delle interruzioni ad inoltrare le richieste
#  provenienti dalla tastera
#
	.global _abilita_tastiera
_abilita_tastiera:
	inb $OCW1M, %al
	andb $0b11111101, %al
	outb %al, $OCW1M
	ret

# Costanti per i parametri attuali di _fill_gate
# Devono coincidere con i valori specificati in sistema.s
#
.set IDT_DPL3, 0x6000
.set IDT_TYPE_TRAP, 0x0f00

# Chiama _fill_gate con i parametri specificati
#
.macro fill_io_gate gate off
	pushl $IDT_DPL3
	pushl $IDT_TYPE_TRAP
	pushl $\off
	pushl $\gate
	call _fill_gate
	addl $16, %esp
.endm

# Inizializzazione dei gate per le primitive di IO
#
	.global _fill_io_gates
_fill_io_gates:
	fill_io_gate io_tipo_rsen a_readse_n
	fill_io_gate io_tipo_rseln a_readse_ln
	fill_io_gate io_tipo_wsen a_writese_n
	fill_io_gate io_tipo_wse0 a_writese_0
	fill_io_gate io_tipo_tr a_term_read_n
	fill_io_gate io_tipo_tw a_term_write_n
	ret

################################################################################
##                              PRIMITIVE DI IO                               ##
################################################################################

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
	.extern _c_writese_0	# non _c_writese_ln, che va lo stesso
a_writese_0:
	salva_registri
	copia_param 3 7
	call _c_writese_0
	addl $12, %esp
	carica_registri
	iret

	.global a_term_read_n
	.extern _c_term_read_n
a_term_read_n:	# routine int $io_tipo_tr
	salva_registri
	copia_param 3 7
	call _c_term_read_n
	addl $12, %esp
	carica_registri
	iret

	.global a_term_write_n
	.extern _c_term_write_n
a_term_write_n: # routine int $io_tipo_tw
	salva_registri
	copia_param 3 7
 	call _c_term_write_n
 	addl $12, %esp
 	carica_registri
	iret

