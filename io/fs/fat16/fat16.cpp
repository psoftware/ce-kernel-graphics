#include "structs.h"
#include "libstr.h"

natw *FAT;
boot_block bb;

natw BOOT_BLOCK=0;
natw START_FAT_BLOCK=1;
natw START_DIRECTORYROOT_BLOCK;
natw START_DATAAREA;

natb *cluster;
void read_file_raw(io_pointer *p, natb* result, int bytecount)
{
	//flog(LOG_INFO, "print_file: CHIAMATA con bytecount = %d p->remaining_size = %d", bytecount, p->remaining_size);

	cluster = reinterpret_cast<natb*>(mem_alloc(bb.bytesperblock));
	int cluster_counter = 0;
	for(natw cl=p->cluster; bytecount>0 && p->remaining_size>0; cl=FAT[cl])
	{
		//questo è l'ultimo blocco del cluster, che non devo superare
		natl lastclusterblock = START_DATAAREA + (cl-2)*bb.cluster_size + 4;
		//flog(LOG_INFO, "reading cluster %d bytecount %d lastclusterblock %d", cl, bytecount, lastclusterblock);

		for(natl bl=p->block; bl<lastclusterblock && bytecount>0 && p->remaining_size>0; bl++) //per ogni blocco del cluster
		{
			//controllo limite dimensione blocco
			natq bytelimit = (bytecount + p->block_offset >= bb.bytesperblock) ? bb.bytesperblock : bytecount + p->block_offset;

			//controllo limite file
			bytelimit = (bytelimit - p->block_offset > p->remaining_size) ? p->remaining_size + p->block_offset : bytelimit;

			natb errore;
			c_readhd_n(reinterpret_cast<natw*>(cluster), START_DATAAREA + (cl-2)*bb.cluster_size + bl, 1, errore);
			//flog(LOG_INFO, "block %d fetched. blockoffset = %d, bytelimit = %d bytecount = %d p->remaining_size = %d", START_DATAAREA + (cl-2)*bb.cluster_size + bl, p->block_offset, bytelimit, bytecount, p->remaining_size);
			for(natq i=p->block_offset; i<bytelimit; i++)
			{
				result[(i - p->block_offset) + (bl - p->block)*bb.bytesperblock + cluster_counter*bb.cluster_size*bb.bytesperblock]=cluster[i];
				//flog(LOG_INFO, "content %c cluster[%d] in result[%d]", cluster[i], i, (i - p->block_offset) + (bl - p->block)*bb.bytesperblock + cluster_counter*bb.cluster_size*bb.bytesperblock);
			}

			//aggiorno il numero di byte letti e quelli rimanenti da leggere
			bytecount -= bytelimit - p->block_offset;
			p->remaining_size -= bytelimit - p->block_offset;

			//aggiorno l'offset di lettura del blocco, gestendo il limite di 512 byte
			p->block_offset = bytelimit;
			if(p->block_offset == bb.bytesperblock)
				p->block_offset = 0;
		}

		p->cluster = cl;
		p->block = 0;
		cluster_counter++;
	}
}

//fonte: https://www.win.tue.nl/~aeb/linux/fs/fat/fat-1.html
natb get_shortname_checksum(const char * name)
{
	natb sum = 0;
	for (int i=0; i<11; i++) {
		sum = (sum >> 1) + ((sum & 1) << 7);	/* rotate */
		sum += name[i];							/* add next name byte */
	}
	return sum;
}

void assembly_name(natb *entry, char *string)
{
	//ogni entrata contiene 12 caratteri, il numero di sequenza parte da 1
	natb offset = ((entry[0] & 0x0f)-1)*13;

	//attenzione, i caratteri sarebbero UNICODE, ma questo nucleo non sa cosa siano ancora,
	//per questo li converto in ASCII, mi basta prendere un byte sì e uno no
	for(int i=1, j=offset; i<10; i+=2, j++)	//5 caratteri
		string[j]=entry[i];
	for(int i=14, j=offset+5; i<25; i+=2, j++) //6 caratteri
		string[j]=entry[i];
	for(int i=28, j=offset+11; i<31; i+=2, j++) //2 caratteri
		string[j]=entry[i];

	//entrata finale del nome del file
	if((entry[0] & 0x0f)==0x40)
		string[offset+12+1]='\0';

}

//per ogni blocco ho 16 entrate
directory_entry temp_fileentry[16];
natb get_max_level(const char *source)
{
	natb previous_slash = 0;
	natb level = 0;
	for(natb i=1; i<strlen(source)+1; i++)
		if(source[i]=='/' || source[i]=='\0')
		{
			if(i==previous_slash+1)	//condizione per gestire i doppi slash
				previous_slash=i;
			else
			{
				previous_slash=i;
				level++;
			}
		}

	return level;
}

io_pointer find_file(const char *fullpath, natl directoryblock=START_DIRECTORYROOT_BLOCK, natb level=0)
{
	io_pointer pointer_res;
	pointer_res.cluster = 0;
	pointer_res.result = -1;

	natb errore;
	char target_file[255] = "";
	extract_sublevel_filename(target_file, fullpath, level);

	natb stop_level = get_max_level(fullpath) - 1;

	//flog(LOG_INFO, "find_file(%s,%d,%d) target=%s stop=%d", fullpath, directoryblock, level, target_file, stop_level);

	//per ogni blocco della root directory
	natl maxblockcount = (bb.directory_count*sizeof(directory_entry) - 512 - 1) / 512;
	for(natb i=0; i<maxblockcount; i++)
	{
		memset(reinterpret_cast<natb*>(&temp_fileentry), 0, sizeof(temp_fileentry));
		c_readhd_n(reinterpret_cast<natw*>(temp_fileentry), directoryblock + i, 1, errore);

		//faccio una copia del blocco perchè questa funzione è ricorsiva e sovrascriverebbe l'istanza globale
		//non posso fare a meno dell'istanza globale perchè è richiesta dal driver. Un'alternativa sarebbe l'heap
		directory_entry fileentry[16];
		memcpy(reinterpret_cast<natb*>(&fileentry), reinterpret_cast<natb*>(&temp_fileentry), sizeof(temp_fileentry));

		//bool is_lfn=false;
		for(natb j=0; j<16;j++)
		{
			//fine della root directory
			if(fileentry[j].filename[0] == 0x00)
				return pointer_res;

			//cartella dot
			if(fileentry[j].filename[0] == 0x2e)
				continue;

			//entrata LFN, rappresenta una parte del nome del file successivo da leggere
			//manca il controllo del checksum, si assume che le entrate lfn siano contigue e sempre
			//relative al file che si troverà successivamente
			char lnf[255];
			if(fileentry[j].attributes & 0x0f)
			{
				//is_lfn=true;
				assembly_name(reinterpret_cast<natb*>(&fileentry[j]), lnf);
				continue;
			}

			//flog(LOG_INFO, "sto scorrendo =%s", lnf);
			//sono arrivato al livello richiesto ma ho trovato una cartella
			if((fileentry[j].attributes & 0x10) && level==stop_level && streq(target_file, lnf)) 
			{
				//flog(LOG_INFO, "   Il percorso indicato rappresenta una cartella e non un file!");
				pointer_res.result = -2;
				return pointer_res;
			}
			else if(level==stop_level && streq(target_file, lnf))
			{
				pointer_res.cluster = fileentry[j].starting_cluster;
				pointer_res.block = 0;
				pointer_res.block_offset = 0;
				pointer_res.remaining_size = fileentry[j].size;
				pointer_res.result = 0;
				//flog(LOG_INFO, "   TROVATO filename = %s startcluster = %d startblock = %d size = %p", lnf, fileentry[j].starting_cluster, pointer_res.block, fileentry[j].size);
				return pointer_res;
			}
			else if(level==stop_level)
				continue;	//non serve andare avanti

			//sotto cartella
			if(fileentry[j].attributes & 0x10)
			{
				//flog(LOG_INFO, "####### trovata cartella: %s | cluster %d (blocco %d)", partial_path, fileentry[j].starting_cluster,START_DATAAREA+(fileentry[j].starting_cluster-2)*bb.cluster_size);
				io_pointer child_pointer_res = find_file(fullpath, START_DATAAREA + (fileentry[j].starting_cluster-2)*bb.cluster_size, level+1);
				if(child_pointer_res.result==-2 || child_pointer_res.result==0)	//se ho trovato una cartella al posto del file o ho trovato il file, devo restituire l'io_pointer
					return child_pointer_res;
				//flog(LOG_INFO, "####### fine cartella: %s ", lnf, fileentry[j].starting_cluster);
			}
		}
	}

	//tecnicamente non ci dovrei arrivare qui
	return pointer_res;
}

io_pointer iopointers_table[32];
int free_iopointer_index=0;
int open_file(const char * filepath)
{
	io_pointer *p = &iopointers_table[free_iopointer_index];
	*p = find_file(filepath);

	if(p->result==-1)
		return -1;	//file non trovato
	if(p->result==-2)
		return -2;	//il percorso indicato rappresenta una cartella

	flog(LOG_INFO, "open_file: startcluster = %d size = %d", p->cluster, p->remaining_size);
	return free_iopointer_index++;
}

int read_file(natb fd, natb *dest, natl bytescount)
{
	io_pointer *p = &iopointers_table[fd];
	//if(esecuzione->id != p->pid)
		//return -1;

	int available_bytes = p->remaining_size;
	read_file_raw(p, dest, bytescount);
	return available_bytes - p->remaining_size;
}

void fat16_init()
{
	//azzero la struttura, è utile per fare debug (nel caso non funzioni la read)
	memset(reinterpret_cast<natb*>(&bb), 0, sizeof(bb));
	natb errore;

	//carico il boot_block
	c_readhd_n(reinterpret_cast<natw*>(&bb), BOOT_BLOCK, 1, errore);
	flog(LOG_INFO, "fat16: cluster_size=%d blocksize=%d", bb.cluster_size, bb.bytesperblock);
	flog(LOG_INFO, "fat16: total_block_count=%d total_block_count_extended=%d directory_count=%d", bb.total_block_count, bb.total_block_count_extended, bb.directory_count);
	flog(LOG_INFO, "fat16: FAT_size=%d FAT_count=%d blocksize=%d", bb.FAT_size, bb.FAT_count, bb.bytesperblock);

	//leggo la label del disco
	const int LABEL_LENGTH = 11;
	char label[LABEL_LENGTH+1];
	strcpy(label, bb.disk_label, LABEL_LENGTH);
	flog(LOG_INFO, "fat16: disk_label=%s", label);

	//carico in memoria la tabella FAT (solo la prima)
	FAT = reinterpret_cast<natw*>(mem_alloc(bb.FAT_size*512));	//blocchi*512 (natw)
	c_readhd_n(FAT, START_FAT_BLOCK, bb.FAT_size, errore);

	//calcolo il blocco di partenza della directory root (blocco di partenza + blocchi fat * numero di tabelle fat)
	START_DIRECTORYROOT_BLOCK = START_FAT_BLOCK + bb.FAT_size*bb.FAT_count;

	//calcolo del blocco iniziale dell'area dei dati (32 è la dimensione di una), l'espressione strana è per fare il ceiling del numero
	START_DATAAREA = START_DIRECTORYROOT_BLOCK + ((bb.directory_count*sizeof(directory_entry)) + 512 - 1) / 512;
	flog(LOG_INFO, "fat16: START_FAT_BLOCK=%d START_DIRECTORYROOT_BLOCK=%d START_DATAAREA=%d", START_FAT_BLOCK, START_DIRECTORYROOT_BLOCK, START_DATAAREA);

	//===== TEST
	int fd = open_file("/nomeestremamenteesageratamentetroppolunghissimo.txt");
	flog(LOG_INFO, "la open_file mi ha restituito %d", fd);

	natb prova[1525];
	flog(LOG_INFO, "### read1");
	int res = read_file(fd, prova, 1524);
	prova[res] = '\0';
	flog(LOG_INFO, "read1: %d %s", res, prova);
	flog(LOG_INFO, "### read2");
	res = read_file(fd, prova, 716);
	prova[res] = '\0';
	flog(LOG_INFO, "read2: %d %s", res, prova);
	flog(LOG_INFO, "### read3");
	res = read_file(fd, prova, 9);
	prova[res] = '\0';
	flog(LOG_INFO, "read3: %d %s", res, prova);
	flog(LOG_INFO, "### read4");
	res = read_file(fd, prova, 9);
	prova[res] = '\0';
	flog(LOG_INFO, "read4: %d %s", res, prova);
	//==========
}