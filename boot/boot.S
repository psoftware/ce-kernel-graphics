// sistema.s

#define ASM 1
#include "mboot.h"
#include "costanti.h"


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

#define STACK_SIZE                      0x4000
#define MULTIBOOT_HEADER_FLAGS         0x00000003


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

multiboot_entry:

     cli
     pushl   %ebx			// parametri passati dal loader
     pushl   %eax			
     call    cmain			// il resto dell'inizializzazione
     					// e' scritto in C++

//////////////////////////////////////////////////////////////////
// funzioni di utilita'						//
//////////////////////////////////////////////////////////////////

	
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
	cli
	movl 4(%esp), %eax
	subl $jmp_off_rel, %eax
	movl %eax, jmp_off

	movl %cr4, %eax
        orl $0x00000020, %eax  //cr4.pae
        movl %eax, %cr4

        movl $0xC0000080, %ecx   //efer msr
        rdmsr
        orl $0x00000100, %eax   //efer.lme
        wrmsr

        movl %cr0, %eax
        orl $0x80000000, %eax   //cr0.pg
        movl %eax, %cr0
	lgdt gdt64_ptr
	ljmp $8, $long_mode

	

.code64
long_mode:
	pushq $0
	popfq
	movw $16, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	#movw $0, %ax
	movw %ax, %ss
#    	mov $0x1F201F201F201F20, %rax     
#	mov $0xB8000, %edi
#    	mov $500, %rcx            
#        rep stosq                        
# 
#        mov $0x00b8000, %edi
# 
#        mov $0x1F6C1F6C1F651F48, %rax
#        movq %rax, (%edi)
# 
#        mov $0x1F6F1F571F201F6F, %rax
#        movq %rax, 8(%edi)
# 
#        mov $0x1F211F641F6C1F72, %rax
#        mov %rax, 16(%edi)
	.byte 0xe9
jmp_off:
	.long 0
jmp_off_rel:
	nop
.code32

	

////////////////////////////////////////////////////////////////
// sezione dati: tabelle e stack			      //
////////////////////////////////////////////////////////////////
.data
.global pml4
.balign 4096
pml4:
	.long 0x00000003 + pdpte, 0
.balign 4096
pdpte:
	.long 0x00000003 + pd, 0
.balign 4096
pd:
	.long 0x00000083, 0
	.long 0x00200083, 0
	.long 0x00400083, 0
	.long 0x00600083, 0
	.long 0x00800083, 0
	.long 0x00a00083, 0
	.long 0x00c00083, 0
.balign 4096
gdt64:
	.quad 0
	.quad 0x00209a0000000000
	.quad 0x0040920000000000
gdt64_end:
gdt64_ptr:
	.word gdt64_end - gdt64
	.long gdt64


	.global stack
stack:
	.space STACK_SIZE, 0
terminate_stack:
	.space STACK_SIZE, 0
terminate_stack_end:

