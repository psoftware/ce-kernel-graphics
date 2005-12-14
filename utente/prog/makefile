all: prog.o

prog.o: *.in sorgenti.lst
	rm -f prog.o
	$(BUILDDIR)/parse
	g++ -I../include -o prog.o -fleading-underscore -fno-exceptions -fno-rtti -c `grep -v '^#' sorgenti.lst|sed 's/\.in/.cpp/'`
clean:
	rm -f gen_uten.sh ../shared/utente.cpp
	rm -f *.o *.cpp

