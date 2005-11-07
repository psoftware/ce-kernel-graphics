/*
 * src.h
 */


static const char *intest_utente[] = {
	"#include \"sys.h\"",
	"",
	"short dummy;",
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

