/*
 * parse.c
 * NOTA: snprintf non disponibile sotto windows (libc del djgpp di calcolatori)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src.h"

#define DEF_PRIO	20

FILE *input, *output;		/* ingresso e uscita del parser */
char look;			/* carattere sotto esame */

int riga, colonna, pos;		/* posizione corrente */

#define LUN_NOME 256		/* lunghezza massima di un identificatore */
char nome[LUN_NOME];		/* ultimo identificatore (o intero) letto */

#define LUN_RIGA 1024
char ultima_riga[LUN_RIGA];

/*
 * Stampa di un messaggio di errore in presenza di un errore nel parsing
 *  ed uscita dal programma
 */
inline void atteso(const char *msg, char c)
{
	char buf[256];

#if defined WIN || defined WIN_XP
	if(msg)
		sprintf(buf, "Aspettavo %s", msg);
	else
		sprintf(buf, "Aspettavo %c", c);
#else
	if(msg)
		snprintf(buf, 256, "Aspettavo %s", msg);
	else
		snprintf(buf, 256, "Aspettavo %c", c);
#endif

	fseek(input, pos, SEEK_SET);
	fgets(ultima_riga, LUN_RIGA, input);
	if(ultima_riga[strlen(ultima_riga) - 1] == '\n')
		ultima_riga[strlen(ultima_riga) - 1] = 0;
	
	printf("\n%s\n%*s\n%s\n", ultima_riga, colonna, "^", buf);
	exit(-1);
}

/*
 * Stampa di un messaggio di errore generico ed uscita dal programma
 */
void errore(const char *msg)
{
	printf("Errore: %s\n", msg);
	exit(-1);
}

/*
 * Legge il carattere successivo dall' ingresso
 */
inline void leggi_car()
{
	look = fgetc(input);
	if(look == '\n') {
		++riga;
		pos += colonna + 1;
		colonna = 0;
	} else if(look == '\t')
		colonna += 8 - colonna % 8;
	else
		++colonna;
}

/*
 * Salta i caratteri di spaziatura letti
 */
inline void salta_spazi()
{
	while(isspace(look))
		leggi_car();
}

inline void copia_car();

/*
 * Salta un commento (riportandolo sull' uscita)
 */
inline void salta_commento()
{
	copia_car();
	leggi_car();

	if(look == '/') {
		while(!feof(input) && look != '\n') {
			copia_car();
			leggi_car();
		}
		copia_car();
	} else if(look == '*') {
		do {
			copia_car();
			leggi_car();
			while(!feof(input) && look != '*') {
				copia_car();
				leggi_car();
			}
			copia_car();
			leggi_car();
		} while(look != '/');
		copia_car();
		leggi_car();
	} else
		ungetc(look, input);
}

/*
 * Legge l' identificatore successivo (comprende il carattere look attuale),
 *  lo lascia in nome e avanza
 */
inline void leggi_nome(int salta)
{
	int i = 0;

	if(look != '_' && !isalpha(look))
		atteso("un identificatore", 0);

	while(look == '_' || isalnum(look)) {
		if(i >= LUN_NOME)
			errore("identificatore troppo lungo");

		nome[i++] = look;
		leggi_car();
	}

	if(salta)
		salta_spazi();

	nome[i] = 0;
}

/*
 * Legge il numero successivo e lascia la stringa che lo rappresenta in nome,
 *  si comporta come leggi_nome
 */
inline void leggi_numero(int salta)
{
	int i = 0;

	if(!isdigit(look))
		atteso("un numero", 0);

	while(isdigit(look)) {
		if(i >= LUN_NOME)
			errore("numero troppo lungo");

		nome[i++] = look;
		leggi_car();
	}

	if(salta)
		salta_spazi();

	nome[i] = 0;
}

/*
 * Verifica che look sia uguale a c ed avanza
 */
inline void trova(char c, int salta)
{
	if(look != c)
		atteso(0, c);
	leggi_car();

	if(salta)
		salta_spazi();
}

/*
 * Verifica che nome sia uguale a n ed avanza
 */
inline void trova_nome(const char *n, int salta)
{
	leggi_nome(0);
	if(strcmp(nome, n))
		atteso(n, 0);
	if(salta)
		salta_spazi();
}

/*
 * Copia look sull' uscita
 */
inline void copia_car()
{
	fprintf(output, "%c", look);
}

/*
 * Copia nome sull' uscita
 */
inline void copia_nome()
{
	fprintf(output, "%s", nome);
}

/*
 * Scrive sull' uscita t
 */
inline void emetti(const char *t)
{
	fprintf(output, "%s", t);
}

/*
 * Alloca s bytes, verificando la presenza di errori
 */
inline void *xmalloc(size_t s)
{
	void *rv;

	if(!(rv = malloc(s)))
		errore("memoria insufficiente");

	return rv;
}

struct file_elem {
	char *testo;
	struct file_elem *succ;
};

struct file_elem *utente[2], *fine[2];

#define GLOB 0
#define MAIN 1

void agg_riga(int sez, char *r)
{
	struct file_elem *n;

	n = xmalloc(sizeof(struct file_elem));

	n->testo = strdup(r);
	if(!n->testo)
		errore("memoria insufficiente");

	n->succ = 0;

	if(utente[sez]) {
		fine[sez]->succ = n;
		fine[sez] = n;
	} else {
		utente[sez] = n;
		fine[sez] = n;
	}
}

void rilascia_righe()
{
	struct file_elem *ep;
	int sez;

	for(sez = 0; sez < 2; ++sez) {
		ep = utente[sez];
		while(ep) {
			ep = utente[sez]->succ;
			free(utente[sez]->testo);
			free(utente[sez]);
			utente[sez] = ep;
		}
		fine[sez] = 0;
	}
}

#define MAIN_HEAD "\nint main()\n{\n\tbool ris;\n\tactivate_p(dd, 0, 1, LIV_UTENTE, dummy, ris);\n"

#if defined WIN || defined WIN_XP
#define MAIN_TAIL "\tbegin_p();\n\n\tterminate_p();\n}\n\nextern\"C\" void __main()\n{\n}\n"
#else
#define MAIN_TAIL "\tbegin_p();\n\n\tterminate_p();\n}\n"
#endif

void scrivi_utente()
{
	FILE *u;
	struct file_elem *ep;
	int i;

	u = fopen("../shared/utente.cpp", "w");
	if(!u)
		errore("impossibile aprire utente.cpp");

	for(i = 0; intest_utente[i]; ++i)
		fprintf(u, "%s\n", intest_utente[i]);

	for(ep = utente[0]; ep; ep = ep->succ)
		fprintf(u, "%s", ep->testo);

	fprintf(u, "%s", MAIN_HEAD);
	for(ep = utente[1]; ep; ep = ep->succ)
		fprintf(u, "%s", ep->testo);

	fprintf(u, "%s", MAIN_TAIL);

	fclose(u);
}

#define GLOB_FMT "short %s;\nextern void %s(int);\n"
#define PROC_FMT "\tactivate_p(%s, %d, %d, %s, %s, ris);\n\n"

#define MAX_INT_LEN 12

void agg_proc(const char *nome_proc, const char *corpo_proc, int par_att,
	int prio, int liv)
{
	int dim1, dim2, dim;
	char *buf;

	dim1 = strlen(GLOB_FMT) + strlen(nome_proc) + strlen(corpo_proc);
	dim2 = strlen(PROC_FMT) + strlen(nome_proc) + strlen(corpo_proc) +
		MAX_INT_LEN * 2 + strlen("LIV_SISTEMA");

	dim = dim1 < dim2 ? dim2: dim1;

	buf = malloc(dim);
	if(!buf)
		errore("memoria insufficiente");

#if defined WIN || defined WIN_XP
	sprintf(buf, PROC_FMT, corpo_proc, par_att, prio,
		liv == 3? "LIV_UTENTE": "LIV_SISTEMA", nome_proc);
	agg_riga(MAIN, buf);

	sprintf(buf, GLOB_FMT, nome_proc, corpo_proc);
	agg_riga(GLOB, buf);
#else
	snprintf(buf, dim, PROC_FMT, corpo_proc, par_att, prio,
		liv == 3? "LIV_UTENTE": "LIV_SISTEMA", nome_proc);
	agg_riga(MAIN, buf);

	snprintf(buf, dim, GLOB_FMT, nome_proc, corpo_proc);
	agg_riga(GLOB, buf);
#endif

	free(buf);
}

#define INT_FMT "int %s;\n"
#define SEM_FMT "\tsem_ini(%s, %d, ris);\n"

void agg_sem(const char *nome_sem, int valore)
{
	char *buf;
	int dim1, dim2, dim;

	dim1 = strlen(SEM_FMT) + strlen(nome_sem) + MAX_INT_LEN;
	dim2 = strlen(INT_FMT) + strlen(nome_sem);
	dim = dim1 < dim2 ? dim2: dim1;

	buf = xmalloc(dim);

#if defined WIN || defined WIN_XP
	sprintf(buf, SEM_FMT, nome_sem, valore);
	agg_riga(MAIN, buf);

	sprintf(buf, INT_FMT, nome_sem);
	agg_riga(GLOB, buf);
#else
	snprintf(buf, dim, SEM_FMT, nome_sem, valore);
	agg_riga(MAIN, buf);

	snprintf(buf, dim, INT_FMT, nome_sem);
	agg_riga(GLOB, buf);
#endif

	free(buf);
}

/*
 * Effettua il parsing di un costrutto process
 */
void process()
{
	char nome_proc[LUN_NOME], corpo_proc[LUN_NOME];
	int pa = 0, prio, liv;

	salta_spazi();
	leggi_nome(1);			// nome del processo (id)
	strcpy(nome_proc, nome);
	trova_nome("body", 1);
	leggi_nome(1);			// nome del corpo
	strcpy(corpo_proc, nome);
	trova('(', 1);
	if(look != ')') {
		leggi_numero(1);
		pa = strtol(nome, 0, 10);
	}
	trova(')', 1);

	if(look == ',') {
		trova(',', 1);
		leggi_numero(1);		// proirita'
		prio = strtol(nome, 0, 10);
		if(look == ',') {
			trova(',', 1);
			leggi_nome(1);		// livello
			if(!strcmp(nome, "LIV_UTENTE"))
				liv = 3;
			else if(!strcmp(nome, "LIV_SISTEMA"))
				liv = 0;
			else
				atteso("un valore per il livello (LIV_SISTEMA, LIV_UTENTE)", 0);
		} else
			liv = 3;
	} else {
		prio = DEF_PRIO;
		liv = 3;
	}

	trova(';', 1);

	agg_proc(nome_proc, corpo_proc, pa, prio, liv);
}

void avanza()
{
	int cnt = 0;

	if(look == '}')
		return;

	if(look == '/')
		salta_commento();
	else
		copia_car();

	while(!feof(input)) {
		leggi_car();
		if(look == '}' && cnt == 0)
			return;
		if(look == '{')
			++cnt;
		else if(look == '}')
			--cnt;
		else if(look == '/')
			salta_commento();

		copia_car();
	}
}

void process_body()
{
	emetti("void ");
	salta_spazi();
	leggi_nome(1);			// legge il nome della funzione
	copia_nome();

	trova('(', 1);
	if(look == ')') {		// parametro formale omesso
		trova(')', 1);
		emetti("(int)");
	} else {
		emetti("(");
		trova_nome("int", 1);
		emetti("int ");
		leggi_nome(1);		// nome del parametro formale
		copia_nome();
		trova(')', 1);
		emetti(")");
	}

	trova('{', 0);
	emetti("\n{");
	avanza();
	emetti("\n\tterminate_p();\n}\n");
	trova('}', 1);
}

void semaphore()
{
	char nome_sem[LUN_NOME];
	int valore;

	salta_spazi();
	leggi_nome(1);
	strcpy(nome_sem, nome);
	trova_nome("value", 1);
	leggi_numero(1);
	valore = strtol(nome, 0, 10);
	trova(';', 1);

	emetti("extern int ");
	emetti(nome_sem);
	emetti(";\n");

	agg_sem(nome_sem, valore);
}

#define LUN_FNAME 512
char inname[LUN_FNAME];
char outname[LUN_FNAME];
char objname[LUN_FNAME];

void imposta_nomi()
{
	char *p;
	int l;

	if(p = strstr(inname, ".in"))
		l = p - inname;
	else
		l = LUN_FNAME;

	l = l < LUN_FNAME - 5 ? l: LUN_FNAME - 5;
	memset(outname, 0, LUN_FNAME);
	memset(objname, 0, LUN_FNAME);
	strncpy(outname, inname, l);
	strncat(outname, ".cpp", LUN_FNAME);

	strncpy(objname, inname, l);
	strncat(objname, ".o", LUN_FNAME);
}

void parse_file()
{
	if(!(input = fopen(inname, "r")) || !(output = fopen(outname, "w")))
		errore("impossibile aprire i file di ingresso e uscita");

	riga = colonna = pos = 0;
	leggi_car();
	
	while(!feof(input)) {
		if(look == '/')
			salta_commento();
		else if(look == 'p' || look == 's') {
			leggi_nome(0);
			if(!strcmp(nome, "process"))
				process();
			else if(!strcmp(nome, "process_body"))
				process_body();
			else if(!strcmp(nome, "semaphore"))
				semaphore();
			else
				copia_nome();
		} else {
			copia_car();
			leggi_car();
		}
	}

	fclose(input);
	fclose(output);
}

FILE *script;

#define LUN_OGGETTI 4096
char oggetti[LUN_OGGETTI];

#if defined WIN
#define SCRIPT_HEAD "rem Script per la compilazione dei programmi utente\n"
#define SCRIPT_NAME "gen_uten.bat"
#endif

#if defined WIN_XP
#define SCRIPT_HEAD "rem Script per la compilazione per Windows XP (Passo 2)\ncd ..\\utente\\prog\n"
#define SCRIPT_NAME "../../xp/passo2.bat"
#endif

#if !defined WIN && !defined WIN_XP
#define SCRIPT_HEAD "#!/bin/sh\n"
#define SCRIPT_NAME "gen_uten.sh"
#endif

#define SCRIPT_TAIL_FMT_BASE "\nld -r -nostdlib -o prog.o %s\n"

#ifndef WIN_XP
#define SCRIPT_TAIL_FMT SCRIPT_TAIL_FMT_BASE
#else
#define SCRIPT_TAIL_FMT SCRIPT_TAIL_FMT_BASE "cd ..\\..\\xp\n"
#endif

void apri_script()
{
	if(!(script = fopen(SCRIPT_NAME, "w")))
		errore("impossibile aprire "SCRIPT_NAME);

	fprintf(script, "%s", SCRIPT_HEAD);
}

void chiudi_script()
{
	fprintf(script, SCRIPT_TAIL_FMT, oggetti);
	fclose(script);
}

#if defined WIN || defined WIN_XP
#define COMP_FMT "gcc -I../include -c %s -o %s\n"
#else
#define COMP_FMT "g++ -I../include -fleading-underscore -fno-exceptions -c %s -o %s\n"
#endif

void agg_script()
{
	fprintf(script, COMP_FMT, outname, objname);
	strncat(oggetti, objname, LUN_OGGETTI);
	strncat(oggetti, " ", LUN_OGGETTI);
}

int main()
{
	FILE *nomi;

	if(!(nomi = fopen("sorgenti.lst", "r")))
		errore("impossibile aprire sorgenti.lst");

	apri_script();

	while(!feof(nomi)) {
		fgets(inname, LUN_FNAME, nomi);
		if(inname[strlen(inname) - 1] == '\n')
			inname[strlen(inname) - 1] = 0;
	
		if(strlen(inname) > 0 && inname[0] != '#') {
			imposta_nomi();
			parse_file();
			agg_script();
		}
	}

	chiudi_script();

	scrivi_utente();
	rilascia_righe();

	fclose(nomi);

#ifdef WIN
	system("gen_uten.bat");
#endif

	return 0;
}

