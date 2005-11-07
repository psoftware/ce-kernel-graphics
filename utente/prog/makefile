all: prog.o

prog.o: *.in sorgenti.lst
	rm -f prog.o
	$(BUILDDIR)/parse
	chmod a+x gen_uten.sh
	./gen_uten.sh

clean:
	rm -f gen_uten.sh ../shared/utente.cpp
	rm -f *.o *.cpp

