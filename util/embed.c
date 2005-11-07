//
// embed.c
// Riceve in ingresso una mappa prodotta da ld e scrive in ../immagine i valori
//  necessari
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct simbolo {
	char *nome;
	unsigned valore;
};

struct simbolo iotab[] = {
	{ "_io_init", 0 },
	{ "_io_end", 0 },
	{ 0, 0 }
};

struct simbolo usrtab[] = {
	{ "_main", 0 },
	{ "_u_usr_shtext_end", 0 },
	{ "_u_sys_shtext_end", 0 },
	{ "_u_data", 0 },
	{ "_u_usr_shdata_end", 0 },
	{ "_u_sys_shdata_end", 0 },
	{ "_u_end", 0 },
	{ 0, 0 }
};

#define LINE_SIZE 256
char line[LINE_SIZE];

void leggi_simboli(const char *nome, struct simbolo *tab)
{
	FILE *in;
	char *pc;
	int i;

	if((in = fopen(nome, "r")) == NULL) {
		fprintf(stderr, "Impossibile aprire la mappa %s\n", nome);
		exit(-1);
	}

	while(!feof(in)) {
		fgets(line, LINE_SIZE, in);

		for(i = 0; tab[i].nome; ++i) {
			if((pc = strstr(line, tab[i].nome)) != NULL &&
   					pc > line && isspace(*(pc - 1)) &&
        				((isspace(*(pc + strlen(tab[i].nome)))) ||
            				!strncmp(pc + strlen(tab[i].nome), "=.", 2))) {
				tab[i].valore = strtol(line, &pc, 16);
				break;
			}
		}
	}

	fclose(in);
}

void scrivi_simboli()
{
	FILE *img;
	int i;
	char *v;

	if((img = fopen("sistema.bin", "rb+")) == NULL) {
		fprintf(stderr, "Impossibile trovare l' immagine del nucleo\n");
		exit(-1);
	}

	fseek(img, 0, SEEK_SET);
	for(i = 0; iotab[i].nome; ++i) {
		v = (char *)&iotab[i].valore;
		fputc(v[0], img);
		fputc(v[1], img);
		fputc(v[2], img);
		fputc(v[3], img);
	}

	for(i = 0; usrtab[i].nome; ++i) {
		v = (char *)&usrtab[i].valore;
		fputc(v[0], img);
		fputc(v[1], img);
		fputc(v[2], img);
		fputc(v[3], img);
	}

	fclose(img);
}

int main()
{
	leggi_simboli("io.map", iotab);
	leggi_simboli("utente.map", usrtab);
	scrivi_simboli();

	return 0;
}

