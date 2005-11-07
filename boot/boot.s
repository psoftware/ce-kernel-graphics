# boot.s
# Codice per il caricamento da floppy disk
#
# Il contenuto di questo file e` strettamente collegato alle modalita`
#  di avviamento dei processori della famiglia i386
#
# Si abilita qui il modo protetto, anche se cio' comporta un doppio caricamento
# della gdt (qui non e` ancora disponibile il codice che ne crea la versione
# definitiva) per limitare a questo file il codice in modo reale.
#
# Questo bootloader lascia il sistema nelle seguenti condizioni:
#  - modo protetto
#  - linea A20 abilitata
#  - idt vuota
#  - interruzioni disabilitate
#  - esp privo di significato
#  - ds, es, fs, gs, ss privi di significato
#  - cs con base 0 e limite 0xfffff, granularita' di pagina, codice
#  - gdt minima (un segmento dati, con base 0 e limite 0xfffff, granularita'
#   di pagina, scrivibile, di selettore 0x10 ed uno codice, di selettore 0x08
#   caricato in cs)
#

# Codice a 16 bit
	.code16

.set SEGM_BOOT, 0x07c0	# questo codice viene caricato a 0x7c00 dal BIOS
			# ed eseguito in modo reale

.set SEGM_SIST, 0x0800	# il sistema viene caricato a 0x8000, non ci sono
			# intersezioni con il codice di boot e la pila iniziale.
			# Limita la lunghezza massima complessiva di nucleo,
			#  modulo di IO e programmi utente a 608KB

.set MAX_ERR, 5		# numero massimo di errori in lettura accettati

	.text

start:
	cli
	movw $SEGM_BOOT, %ax 	# inizializzazione dei registri selettore
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss

	movw $0x400, %sp	# pila a 0x8000 (massimo 512 bytes)

	movw $(msg - start), %si # messaggio di benvenuto
	call stampa_stringa

	call leggi		# caricamento da floppy
	call ferma		# spegnimento del motore del floppy

	cli
	call abilita_A20	# abilitazione linea A20

	lidt idt_pd		# idt temporanea (vuota)
	lgdt gdt_pd		# gdt temporanea (solo due segmenti)

	movl $0x0001, %eax	# abilitazione del modo protetto
	movl %eax, %cr0

	ljmp $8, $0x8024	# salto a start (in sistema.s)
				# non si salta a start perche' non c'e'
				#  collegamento tra i moduli

# routine per caricare l' immagine in SEGM_SIST (carica anche
#  il modulo di IO ed i programmi utente)
#

da_leggere: .word 0		# numero di settori da leggere
n_err:	.word 0			# numero di errori durante il caricamento

leggi:
	xorw %ax, %ax		# reset del controllore del floppy
	xorw %dx, %dx
	int $0x13

	movw dimensione, %ax	# inizializzazione di da_leggere con
	movw %ax, da_leggere	#  il numero di settori dell' immagine
	movw $SEGM_SIST, %ax	# segmento di destinazione in es
	movw %ax, %es
	movw $0x0002, %cx	# secondo settore del primo cilindro
	movw $0x0000, %dx	# prima testina del primo drive

ripeti:
	movw $0x0201, %ax	# leggi un settore
	xorw %bx, %bx		# offset a zero

	int $0x13		# effettua lettura (tramite bios)
				# nota: la chiamata non modifica cx e dx

	jc err_lettura		# gestione dell' errore

	movw da_leggere, %ax	# ogni otto settori letti stampa '.'
	andw $7, %ax
	cmpw $7, %ax
	jne 1f
	movb $'., %al
	call stampa_carattere

1:
	decw da_leggere		# aggiornamento contatore
	cmpw $0, da_leggere
	je fine

	movw %es, %ax		# aggiornamento segmento di destinazione
	addw $0x20, %ax
	movw %ax, %es

	incb %cl		# incremento settore
	cmpb $18, %cl		# ogni traccia ha 18 settori
	jbe ripeti		# ancora settori sulla traccia corrente

	xorb $1, %dh		# cambia la testina
	movb $1, %cl		# ricomincia dal primo settore
	jnz ripeti		# verifica se il cilindro e' finito (in
				#  questo caso la testina e' tornata 0)

	incb %ch		# incremento del cilindro (per il floppy
				#  viene usato solo ch)
	jmp ripeti

fine:
	ret

err_lettura:
	incw n_err		# incrementa il numero di errori
	cmpw $MAX_ERR, n_err
	je max_err		# raggiunto il numero massimo

	movw $(err - start), %si # stampa di un messaggio...
	call stampa_stringa
	call stampa_codice	# e del relativo codice

	jmp leggi		# riavvio della lettura

max_err:
	movw $(stop - start), %si # stampa di un messaggio di errore
	call stampa_stringa
	cli
	hlt			# bloccaggio del sistema

# Spegnimento del motore del floppy
#
ferma:
	movw $0x3f2, %dx
	movb $0, %al
	outb %al, %dx
	ret

# Abilitazione della linea A20
#
abilita_A20:
	call svuota_buffer
	movb $0xd1, %al
	outb %al, $0x64
	call svuota_buffer
	movb $0xdf, %al
	outb %al, $0x60
	call svuota_buffer
	ret

# Svuotamento del buffer della tastiera
#
svuota_buffer:
	jmp 1f
1:	jmp 1f
1:
	inb $0x64, %al
	testb $2, %al
	jne svuota_buffer
	ret

# Stampa a schermo (tramite BIOS) del carattere in al
#
stampa_carattere:
	pushw %ax
	pushw %bx

	movb $0x0e, %ah
	movw $0x0001, %bx
	int $0x10

	popw %bx
	popw %ax

	ret

# Stampa a schermo della stringa (terminata da 0) in ds:si
#
stampa_stringa:
	pushw %ax

1:
	lodsb
	cmpb $0, %al
	je 2f

	call stampa_carattere
	jmp 1b

2:
	popw %ax

	ret

# Stampa a schermo della rappresentazione esadecimale del valore in ah
#
stampa_codice:
	pushw %ax

	movb %ah, %al
	shrb $4, %al
	call stampa_cifra
	movb %ah, %al
	andb $0x0f, %al
	call stampa_cifra

	popw %ax

	ret

stampa_cifra:
	cmpb $0x09, %al
	ja 1f
	addb $'0, %al
	jmp 2f
1:
	subb $0x0a, %al
	addb $'a, %al
2:
	call stampa_carattere
	ret

# Pseudo-descrittori delle idt e gdt temporanee utilizzate nel passaggio al
#  modo protetto
idt_pd:	.word 0, 0, 0
gdt_pd: .word 24, 0x7c00 + gdt, 0

# Gdt temporanea
#
gdt:
	.word 0, 0, 0, 0
	.word 0xffff, 0x0000, 0x9a00, 0x00cf
	.word 0xffff, 0x0000, 0x9200, 0x00cf

# Messaggi di avviamento e di errore
#
msg:	.asciz "Caricamento in corso..."
err:	.asciz "\n\rErrore di lettura, codice: "
stop:	.asciz "\n\rCaricamento fallito, sistema bloccato.\n\r"

	.org 508

dimensione: .word 0		# numero di settori da leggere (viene impostato
				#  in fase di compilazione)
	.word 0xaa55		# signature del bootsector (deve essere negli
				#  ultimi due byte)

