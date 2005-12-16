/*
 * src.h
 */


static const char *intest_utente[] = {
	"short dummy __attribute__ (( section (\"RESIDENT\") ));",
	"void dd(int i)",
	"{",
	"\tint lavoro;",
	"\tdo",
	"\t\tgive_num(lavoro);",
	"\twhile(lavoro != 1);",
	"\tterminate_p();",
	"}",
	"",
	0
};

