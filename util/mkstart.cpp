#include <fstream>
#include <iomanip>
#include <costanti.h>
#include <tipo.h>

using namespace std;

static inline natq norm(natq a)
{
	return ((a & (1UL << 47)) ? (a | 0xffff000000000000UL) : (a & 0x0000ffffffffffffUL));
}

int main()
{
	natq start_io = norm(((natq)I_MIO_C << 39UL));
	natq start_utente = norm(((natq)I_UTN_C << 39UL));

	ofstream startmk("util/start.mk");
	startmk << "MEM=" << MEM_TOT/MiB << endl;
	startmk << "SWAP_SIZE=" << DIM_SWAP << endl;
	startmk << hex;
	startmk << "START_IO=0x"      << start_io << endl;
	startmk << "START_UTENTE=0x"  << start_utente << endl;
	startmk.close();

	ofstream startgdb("util/start.gdb");
	startgdb << "set $START_IO="      << start_io << endl;
	startgdb << "set $START_UTENTE="  << start_utente << endl;
	startgdb.close();

	ofstream startpl("util/start.pl");
	startpl << hex;
	startpl << "$START_IO='"      << setw(16) << setfill('0') << start_io << "';" << endl;
	startpl << "$START_UTENTE='"  << setw(16) << setfill('0') << start_utente << "';" << endl;
	startpl << "1;" << endl;
	startpl.close();

	return 0;
}
