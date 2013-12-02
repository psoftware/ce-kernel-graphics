// sistema.cpp
//
#include "mboot.h"		// *****
#include "costanti.h"		// *****
#include "elf64.h"

#include "libce.h"

// carica un nuovo valore in cr3 [vedi sistema.S]
extern "C" void loadCR3(addr dir);

// restituisce il valore corrente di cr3 [vedi sistema.S]
extern "C" addr readCR3();

// attiva la paginazione [vedi sistema.S]
extern "C" void attiva_paginazione(natl entry);


void ini_COM1();

natl carica_modulo(multiboot_module_t* mod) {
	Elf64_Ehdr* elf_h = (Elf64_Ehdr*)mod->mod_start;

	if (!(elf_h->e_ident[EI_MAG0] == ELFMAG0 &&
	      elf_h->e_ident[EI_MAG1] == ELFMAG1 &&
	      elf_h->e_ident[EI_MAG2] == ELFMAG2 &&
	      elf_h->e_ident[EI_MAG2] == ELFMAG2))
	{
		flog(LOG_ERR, "Formato del modulo '%s' non riconosciuto", mod->cmdline);
		return 0;
	}

	if (!(elf_h->e_ident[EI_CLASS] == ELFCLASS64  &&
	      elf_h->e_ident[EI_DATA]  == ELFDATA2LSB &&
	      elf_h->e_type	       == ET_EXEC     &&
	      elf_h->e_machine 	       == EM_AMD64))
	{ 
		flog(LOG_ERR, "Il modulo '%s' non contiene un esegubile per x86_64", 
				mod->cmdline);
		return 0;
	}

	Elf64_Phdr* elf_ph = (Elf64_Phdr*)(mod->mod_start + elf_h->e_phoff);
	for (int i = 0; i < elf_h->e_phnum; i++) {
		if (elf_ph->p_type != PT_LOAD)
			continue;

		memcpy((void*)elf_ph->p_vaddr,
		       (void*)(mod->mod_start + elf_ph->p_offset),
		       elf_ph->p_filesz);
		flog(LOG_INFO, "Copiata sezione di %d byte all'indirizzo %p",
				(long)elf_ph->p_filesz, (void*)elf_ph->p_vaddr);
		memset((void*)(elf_ph->p_vaddr + elf_ph->p_filesz), 0,
		       elf_ph->p_memsz - elf_ph->p_filesz);
		flog(LOG_INFO, "azzerati ulteriori %d byte",
				elf_ph->p_memsz - elf_ph->p_filesz);
		elf_ph = (Elf64_Phdr*)((unsigned int)elf_ph + elf_h->e_phentsize);
	}
	flog(LOG_INFO, "entry point %p", elf_h->e_entry);
	return (natl)elf_h->e_entry;
	//free((void*)mod->mod_start, mod->mod_end - mod->mod_start);
}


extern "C" natl pml4;
extern "C" void cmain (natl magic, multiboot_info_t* mbi)
{
	natl entry;
	
	
	// (* inizializzazione Seriale per il Debugging
	ini_COM1();
	flog(LOG_INFO, "Boot loader Calcolatori Elettronici, v0.01");
	// *)
	
	// (* controlliamo di essere stati caricati
	//    da un bootloader che rispetti lo standard multiboot
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		flog(LOG_ERR, "Numero magico non valido: 0x%x", magic);
	}
	// *)

	// (* Carichiamo i vari moduli
	//    Il numero dei moduli viene passato dal bootloader in mods_count
	if (mbi->flags & MULTIBOOT_INFO_MODS) {

		flog(LOG_INFO, "mods_count = %d, mods_addr = 0x%x",
				mbi->mods_count, mbi->mods_addr);
		multiboot_module_t* mod = (multiboot_module_t*) mbi->mods_addr;
		for (unsigned int i = 0; i < mbi->mods_count; i++) {
			flog(LOG_INFO, "mod[%d]:%s: start 0x%x end 0x%x",
					i, mod->cmdline, mod->mod_start, mod->mod_end);
			entry = carica_modulo(mod);
		}
	}
	// *)
	loadCR3(&pml4);
	attiva_paginazione(entry);

	
	
	flog(LOG_INFO,"Paginazione attivata");
	return;
}


const ioaddr iRBR = 0x03F8;		// DLAB deve essere 0
const ioaddr iTHR = 0x03F8;		// DLAB deve essere 0
const ioaddr iLSR = 0x03FD;
const ioaddr iLCR = 0x03FB;
const ioaddr iDLR_LSB = 0x03F8;		// DLAB deve essere 1
const ioaddr iDLR_MSB = 0x03F9;		// DLAB deve essere 1
const ioaddr iIER = 0x03F9;		// DLAB deve essere 0
const ioaddr iMCR = 0x03FC;
const ioaddr iIIR = 0x03FA;


void ini_COM1()
{	natw CBITR = 0x000C;		// 9600 bit/sec.
	natb dummy;
	outputb(0x80, iLCR);		// DLAB 1
	outputb(CBITR, iDLR_LSB);
	outputb(CBITR >> 8, iDLR_MSB);
	outputb(0x03, iLCR);		// 1 bit STOP, 8 bit/car, parita dis, DLAB 0
	outputb(0x00, iIER);		// richieste di interruzione disabilitate
	inputb(iRBR, dummy);		// svuotamento buffer RBR
}

void serial_o(natb c)
{	natb s;
	do 
	{	inputb(iLSR, s);    }
	while (! (s & 0x20));
	outputb(c, iTHR);
}

// invia un nuovo messaggio sul log
extern "C" void do_log(log_sev sev, const char* buf, natl quanti)
{
	const char* lev[] = { "DBG", "INF", "WRN", "ERR", "USR" };
	if (sev > MAX_LOG) {
		flog(LOG_WARN, "Livello di log errato: %d", sev);
	}
	const natb* l = (const natb*)lev[sev];
	while (*l)
		serial_o(*l++);
	serial_o((natb)'\t');
	natb idbuf[10];
	snprintf((char*)idbuf, 10, "%d", 0);
	l = idbuf;
	while (*l)
		serial_o(*l++);
	serial_o((natb)'\t');

	for (natl i = 0; i < quanti; i++)
		serial_o(buf[i]);
	serial_o((natb)'\n');
}
extern "C" void c_log(log_sev sev, const char* buf, natl quanti)
{
	do_log(sev, buf, quanti);
}

// )





