#ifndef FATSTRUCTS_H
#define FATSTRUCTS_H

struct boot_block {
	//roba per bootloader
	natb reserved[3];

	//optional manufacturer description (non usato)
	char manufacturer_text[8];

	//dovrebbe essere sempre 512
	natw bytesperblock;

	//quanti blocchi ci sono per ogni cluster
	natb cluster_size;

	//quanti blocchi riservati ci sono all'inizio del disco
	natw first_reserved_blocks;

	//quante tabelle fat ci sono (probabilmente non sarà usato, perchè è sempre uno)
	natb FAT_count;

	//quante entrate ci sono nella Root Directory
	natw directory_count;

	//quanti sono i blocchi totali del disco (se sono più di 65536 viene usata total_block_count_extended)
	natw total_block_count;

	//non usato?
	natb media_descriptor;

	//dimensione dei una tabella FAT (in numero di blocchi)
	natw FAT_size;

	//non usati (servono solo al bootloader)
	natw blocks_per_track;
	natw heads_per_surface;

	//non usato
	natl hidden_blocks_count;

	//numero dei blocchi totali nel caso siano più di 65536
	natl total_block_count_extended;

	//non usati, servono solo al bootloader 
	natw physical_drive_number;
	natb ext_boot_record_signature;

	//codice identificativo del disco
	natl serial_number;

	//label disco (11 bytes)
	char disk_label[11];

	//identificatore filesystem
	natq fat16_magic;

	//448 bytes pagging
	natb padding[448];

	natw boot_block_signature;
} __attribute__ ((__packed__));

//entrata dell Root Directory
struct directory_entry {
	char filename[8];
	char filenameext[3];
	natb attributes;
	natb reserved[10];
	natw creation_time;
	natw creation_date;
	natw starting_cluster;
	natl size;
} __attribute__ ((__packed__));

struct io_pointer {
	int result;
	natq pid;
	bool used;

	natw cluster;
	natl block;
	natl block_offset;
	natl remaining_size;

	io_pointer()
	{
		used = false;
	}
};

#endif