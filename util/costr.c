/*
 * Costruisce l' immagine di un floppy disk avviabile che
 *  contiene il nucleo ed il bootloader
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 512
char buffer[BUF_SIZE];

FILE *out;
int nsect;

void aggiungi(const char *nome)
{
	FILE *in;

        if((in = fopen(nome, "rb")) == NULL) {
                fprintf(stderr, "Impossibile aprire %s: %s\n",
			nome, strerror(errno));
                exit(-1);
        }

        fread(buffer, 1, BUF_SIZE, in);
        while(!feof(in)) {
                fwrite(buffer, 1, BUF_SIZE, out);
                fread(buffer, 1, BUF_SIZE, in);
                ++nsect;
        }

	fclose(in);
}

int main()
{
	int pos;

        if((out = fopen("fd0.img", "wb")) == NULL) {
                perror("Impossibile aprire fd0.img");
                exit(-1);
        }

        aggiungi("bsect.bin");
        aggiungi("sistema.bin");
        aggiungi("io.bin");
        aggiungi("utente.bin");

        memset(buffer, 0, BUF_SIZE);
        while((pos = ftell(out)) < 2880 * 512)
                fwrite(buffer, 1, BUF_SIZE, out);

	--nsect;			/* bsect.bin non deve essere contato */
        fseek(out, 508, SEEK_SET);
        fputc(((char *)&nsect)[0], out);
        fputc(((char *)&nsect)[1], out);

        fclose(out);

        return 0;
}

