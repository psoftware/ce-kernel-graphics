#include "structs.h"
#include "libstr.h"

natw *FAT;
boot_block bb;

natw BOOT_BLOCK=0;
natw START_FAT_BLOCK=1;
natw START_DIRECTORYROOT_BLOCK;
natw START_DATAAREA;

natb *cluster;
void print_file(natw firstcluster, natl size)
{
	int signsize = size;
	cluster = reinterpret_cast<natb*>(mem_alloc(bb.bytesperblock));
	for(natw cl=firstcluster; signsize>0; cl=FAT[cl],signsize-=bb.cluster_size*bb.bytesperblock)
	{
		int blocktoread;
		if(signsize >= bb.cluster_size*bb.bytesperblock)
			blocktoread = bb.cluster_size;
		else
			blocktoread = (signsize / bb.bytesperblock) + 1;

		for(int bl=0; bl<blocktoread; bl++) //per ogni blocco del cluster
		{
			natb errore;
			c_readhd_n(reinterpret_cast<natw*>(cluster), START_DATAAREA + (cl-2)*bb.cluster_size + bl, 1, errore);
			flog(LOG_INFO, "block %d fetched", START_DATAAREA + (cl-2)*bb.cluster_size + bl);
			for(natb i=0; i<10; i++)
				flog(LOG_INFO, "content %c", cluster[i]);
		}

		flog(LOG_INFO, "reading cluster %d signsize %d", cl, signsize);
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
void file_list(natl directoryblock, const char * rootpath="")
{
	//GUIDA http://www.tavi.co.uk/phobos/fat.html
	
	natb errore;
	//per ogni blocco della root directory
	//fast ceiling (x + y - 1) / y
	natl maxblockcount = (bb.directory_count*sizeof(directory_entry) - 512 - 1) / 512;
	for(natb i=0; i<maxblockcount; i++)
	{
		memset(reinterpret_cast<natb*>(&temp_fileentry), 0, sizeof(temp_fileentry));
		c_readhd_n(reinterpret_cast<natw*>(temp_fileentry), directoryblock + i, 1, errore);

		//faccio una copia del blocco perchè questa funzione è ricorsiva e sovrascriverebbe l'istanza globale
		directory_entry fileentry[16];
		memcpy(reinterpret_cast<natb*>(&fileentry), reinterpret_cast<natb*>(&temp_fileentry), sizeof(temp_fileentry));

		bool is_lfn=false;
		for(natb j=0; j<16;j++)
		{
			//fine della root directory
			if(fileentry[j].filename[0] == 0x00)
				return;

			//cartella dot
			if(fileentry[j].filename[0] == 0x2e)
				continue;

			//entrata LFN, rappresenta una parte del nome del file successivo da leggere
			//manca il controllo del checksum, si assume che le entrate lfn siano contigue e sempre
			//relative al file che si troverà successivamente
			char lnf[255];
			if(fileentry[j].attributes & 0x0f)
			{
				is_lfn=true;
				assembly_name(reinterpret_cast<natb*>(&fileentry[j]), lnf);
				continue;
			}

			//sotto cartella
			if(fileentry[j].attributes & 0x10)
			{
				char str_slash[] = "/";
				char partial_path[255];
				partial_path[0] = '\0';
				strcat(partial_path, rootpath);
				strcat(partial_path, str_slash);
				strcat(partial_path, lnf);
				flog(LOG_INFO, "####### trovata cartella: %s | cluster %d (blocco %d)", partial_path, fileentry[j].starting_cluster,START_DATAAREA+(fileentry[j].starting_cluster-2)*bb.cluster_size);
				file_list(START_DATAAREA + (fileentry[j].starting_cluster-2)*bb.cluster_size, partial_path);
				flog(LOG_INFO, "####### fine cartella: %s ", lnf, fileentry[j].starting_cluster);
			}
			else
				//print_file(fileentry[j].starting_cluster, fileentry[j].size);
				flog(LOG_INFO, "   filename = %s startcluster = %d size = %p", lnf, fileentry[j].starting_cluster, fileentry[j].size);
		}
	}
}

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

bool find_file(const char *fullpath, natl directoryblock=START_DIRECTORYROOT_BLOCK, natb level=0)
{	
	natb errore;
	char target_file[255] = "";
	extract_sublevel_filename(target_file, fullpath, level);

	natb stop_level = get_max_level(fullpath) - 1;

	flog(LOG_INFO, "find_file(%s,%d,%d) target=%s stop=%d", fullpath, directoryblock, level, target_file, stop_level);

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

		bool is_lfn=false;
		for(natb j=0; j<16;j++)
		{
			//fine della root directory
			if(fileentry[j].filename[0] == 0x00)
				return false;

			//cartella dot
			if(fileentry[j].filename[0] == 0x2e)
				continue;

			//entrata LFN, rappresenta una parte del nome del file successivo da leggere
			//manca il controllo del checksum, si assume che le entrate lfn siano contigue e sempre
			//relative al file che si troverà successivamente
			char lnf[255];
			if(fileentry[j].attributes & 0x0f)
			{
				is_lfn=true;
				assembly_name(reinterpret_cast<natb*>(&fileentry[j]), lnf);
				continue;
			}

			flog(LOG_INFO, "sto scorrendo =%s", lnf);
			//sono arrivato al livello richiesto ma ho trovato una cartella
			if((fileentry[j].attributes & 0x10) && level==stop_level && streq(target_file, lnf)) 
			{
				flog(LOG_INFO, "   Il percorso indicato rappresenta una cartella e non un file!");
				return true;
			}
			else if(level==stop_level && streq(target_file, lnf))
			{
				flog(LOG_INFO, "   TROVATO filename = %s startcluster = %d size = %p", lnf, fileentry[j].starting_cluster, fileentry[j].size);
				print_file(fileentry[j].starting_cluster, fileentry[j].size);
				return true;
			}
			else if(level==stop_level)
				continue;	//non serve andare avanti

			//sotto cartella
			if(fileentry[j].attributes & 0x10)
			{
				//flog(LOG_INFO, "####### trovata cartella: %s | cluster %d (blocco %d)", partial_path, fileentry[j].starting_cluster,START_DATAAREA+(fileentry[j].starting_cluster-2)*bb.cluster_size);
				bool found = find_file(fullpath, START_DATAAREA + (fileentry[j].starting_cluster-2)*bb.cluster_size, level+1);
				if(found)
					return true;
				//flog(LOG_INFO, "####### fine cartella: %s ", lnf, fileentry[j].starting_cluster);
			}
		}
	}
}




void prova_fat16()
{
	memset(reinterpret_cast<natb*>(&bb), 0, sizeof(bb));
	natb errore;
	//c_readhd_n(natw vetti[], natl primo,natb quanti, natb &errore)

	c_readhd_n(reinterpret_cast<natw*>(&bb), BOOT_BLOCK, 1, errore);
	flog(LOG_INFO, "fat16: cluster size=%d fat_count=%d blocksize=%d reservedblocks=%d", bb.cluster_size, bb.FAT_count, bb.bytesperblock, bb.first_reserved_blocks);
	flog(LOG_INFO, "fat16: directoryentries=%d total_block_count=%d fat_size=%d total_block_count_extended=%d", bb.directory_count, bb.total_block_count, bb.FAT_size, bb.total_block_count_extended);
	flog(LOG_INFO, "fat16: disk_label=%s", bb.disk_label);

	//carico in memoria la tabella FAT
	FAT = reinterpret_cast<natw*>(mem_alloc(bb.FAT_size*512));	//blocchi*512 (natw)
	c_readhd_n(FAT, START_FAT_BLOCK, bb.FAT_size, errore);

	//calcolo il blocco di partenza della directory root (blocco di partenza + blocchi fat * numero di tabelle fat)
	START_DIRECTORYROOT_BLOCK = START_FAT_BLOCK + bb.FAT_size*bb.FAT_count;

	//calcolo del blocco iniziale dell'area dei dati (32 è la dimensione di una)
	START_DATAAREA = START_DIRECTORYROOT_BLOCK + ((bb.directory_count*sizeof(directory_entry)) + 512 - 1) / 512;
	flog(LOG_INFO, "fat16: START_FAT_BLOCK = %d START_DIRECTORYROOT_BLOCK = %d START_DATAAREA = %d", START_FAT_BLOCK, START_DIRECTORYROOT_BLOCK, START_DATAAREA);
	//file_list(START_DIRECTORYROOT_BLOCK);
	find_file("/ciao.txt");
	find_file("/nomeestremamenteesageratamentetroppolunghissimo.txt");
	find_file("/cartella");
	find_file("/cartella/filecartella.txt");
	find_file("/cartella/cartellafiglia/");
	find_file("/cartella/cartellafiglia/filecartellafiglia.txt");
}