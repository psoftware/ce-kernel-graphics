#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdio>

using namespace std;
const char UNACTIVE_DELIM = '_';
const char ACTIVE_DELIM = '=';
const char START_STOP_DELIM = '|';
const char TERM_DELIM = 'T';
const char PROCACT_DELIM = 'C';

void concat_n_chars(std::string&, char, int);
string& color_delimiter_chars(string&);

struct graph_elem
{
        char status;
        int pid;
	graph_elem(char s, int p)
	{
		status=s;
		pid=p;
	}
};

struct graph_hash
{
	int time;
	vector<graph_elem> proclist;
	graph_hash(int t)
	{
		time=t;
	}
};

int main(int argc, char* argv[])
{
	/*---   Analisi parametri passati al processo   ---*/
	//----  Copiato da http://www.cplusplus.com  ----
	int maxcolumns = 120;
	bool coloured = false;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			//show_usage(argv[0]);
			return 0;
		}
		else if ((arg == "-w")) {
			if (i + 1 < argc) {
				maxcolumns = atoi(argv[++i]) - 8 - 1; // - (tab chars) - (latest column padding)
			}
			else { // Uh-oh, there was no argument to the destination option.
				std::cerr << "-w option requires one argument." << std::endl;
				return 1;
			}
		}
		else if ((arg == "-c")) {
			coloured = true;
		}
	    }
	//--------

	/*---   Lettura dati log dallo stream stdin   ---*/
	vector<int> processes;
	vector<graph_hash> timeslist;

	int t; char s; int p;

	int lasttime=0, lastindex=0;
	timeslist.push_back(graph_hash(0));
	while(cin >> t && cin >> s && cin >>p)
	{
		if(lasttime == t)
			timeslist[lastindex].proclist.push_back(graph_elem(s,p));
		else
		{
			timeslist.push_back(graph_hash(t));
			timeslist[++lastindex].proclist.push_back(graph_elem(s,p));
			lasttime = t;
		}

	        processes.push_back(p);
	}

	/*---   Creazione delle stringhe/righe relative ad ogni processo   ---*/

	//trovare lista processi (sort + uniq)
	vector<int> procuniq(processes);
	sort(procuniq.begin(), procuniq.end());
	vector<int>::iterator it2 = unique(procuniq.begin(), procuniq.end());
	procuniq.resize(distance(procuniq.begin(),it2));

	//Creo un vettore di stringhe per la stampa successiva a "paragrafi"
	vector<std::string> procstrings;

	//scorro la lista dei processi (cioè per ogni riga da stampare)
	for(int i=0; i<procuniq.size(); i++)
	{
		std::string resultstr = "";

		int old_progstatus = 0;
		int line_length = 0;

		//scorro tutti i tempi in cui è avvenuto qualcosa (start/stop processo)
		for(int j=0; j<timeslist.size(); j++)
		{
			//Cerco lo stato del pid selezionato nell'istante di tempo timelist[j].time
			//In breve: se procuniq[i] è l'ultimo a partire (status=='S') nell'istante di
			//tempo timeslist[j].time, allora significa che il processo è partito e
			//non è stato vittima di preemption (nello stesso istante di tempo).
			//Se non è così allora mi segno che il processo ha subito preemption
			//(otherprocesstarted=true). Se il processo non viene trovato in lista, allora
			//significa che è nello stato bloccato (progstatus==0)
			//Segno anche se il processo è stato creato nell'istante di tempo considerato
			//(processcreated=true)
			//Forse c'è qualche variabile ridondante
			int progstatus = 0;
			int otherprocessstarted = false;
			bool processcreated = false;
			for(int k=0; k<timeslist[j].proclist.size(); k++)
				if(timeslist[j].proclist[k].pid == procuniq[i])
				{
					if(timeslist[j].proclist[k].status == 'T')
					{
						progstatus = -1;
						break;
					}
					else if(timeslist[j].proclist[k].status == 'P')
					{
						processcreated = true;
					}
					else if(timeslist[j].proclist[k].status == 'S')
					{
						progstatus = 1;
						otherprocessstarted = false;
					}
				}
				else if(timeslist[j].proclist[k].status == 'S')
					{
						if(progstatus == 1)
							progstatus = 0;
						otherprocessstarted = true;
					}
				
		


			//cout << "ps:" << progstatus << " other:" << otherprocessstarted << " older:" << old_progstatus << endl;
			//In base allo stato del programma nel precedente istante di tempo e del corrente stato, decido quale carattere stampare
			if(progstatus == -1)
				resultstr += TERM_DELIM;
			else if(progstatus == 1)
				if(processcreated)
					resultstr += PROCACT_DELIM;
				else
					resultstr += START_STOP_DELIM;
			else if(progstatus == 0) // e' accaduto un evento di terminazione/partenza/schedulazione ad un altro processo
			{
				if(!otherprocessstarted)
				{
					if(old_progstatus == 1)
					{
						resultstr += ACTIVE_DELIM;
						progstatus = 1; //L'inserimento dell'evento di creazione processo fa pensare all'algoritmo che qualche processo si è avviato
					}
					else
						if(processcreated)
							resultstr += PROCACT_DELIM;
						else
							resultstr += UNACTIVE_DELIM;
				}
				else if (old_progstatus == 1)
					resultstr += START_STOP_DELIM;
				else
					resultstr += UNACTIVE_DELIM;
			}
			else 
			{
				cerr << "ERRORE";
				return 1;
			}

			//Nell'ultimo ciclo non devo stampare più niente perchè non ho un range
			//ma solo un estremo da considerare
			if(j==timeslist.size()-1)
				break;

			//Stampo un numero di caratteri calcolabile come la differenza tra l'estremo
			//considerato e il successivo, dove per estremo si intende un istante in cui
			//e' avvenuto qualcosa (start/stop processo)

			//Controllo limite lunghezza riga 1
			int toprint_length = timeslist[j+1].time - timeslist[j].time - 1;

			if(progstatus == 1)
				concat_n_chars(resultstr, ACTIVE_DELIM, toprint_length);
			else if(progstatus == 0)
				concat_n_chars(resultstr, UNACTIVE_DELIM, toprint_length);
			else
				concat_n_chars(resultstr, UNACTIVE_DELIM, toprint_length);


			old_progstatus = progstatus;
		}

		procstrings.push_back(resultstr);
		
	}


	//La dimensione delle stringhe in procstring è la stessa per tutte, quindi prendo solo la prima come riferimento
	//for(int j=0; j<procstrings.size(); j++)
		//cout << procstrings[j] << endl;

	/*---   Stampa paragrafata e formattata delle stringhe   ---*/
	int paragrafs_count = procstrings[0].size() / maxcolumns; int i;

	for(i=0; i<paragrafs_count+1; i++)
	{
		for(int j=0; j<procstrings.size(); j++)
		{
			cout << 'p' << procuniq[j] << '\t';
			string temp;
			if(i==paragrafs_count)
				temp = procstrings[j].substr(i*maxcolumns, procstrings[j].size() - i*maxcolumns);
			else
				temp = procstrings[j].substr(i*maxcolumns, maxcolumns);
			cout << ((coloured) ? color_delimiter_chars(temp) : temp) << endl;
		}
		cout << endl;
	}

	return 0;
}

//Funzione che concatena a str n caratteri symb per howmany volte
void concat_n_chars(string& str, char symb, int howmany)
{
	for(int i=0; i<howmany;i++)
		str += symb;
}

//Funzione che sostituisce ad alcuni caratteri delimitatori la stringa necessaria per colorarli in console
string& color_delimiter_chars(string& str)
{
	size_t index = 0;
	while (true) {
		index = str.find("C", index);
		if (index == string::npos) break;
		str.replace(index, 1, "\033[1;32mC\033[0m");
		index += 12;
	}

	index = 0;
	while (true) {
		index = str.find("T", index);
		if (index == string::npos) break;
		str.replace(index, 1, "\033[1;31mT\033[0m");
		index += 12;
	}
	return str;
}
