all: usr.o

CPPFLAGS=-I../include

usr.o: lib.o uten_s.o uten_cpp.o
	ld -r -nostdlib -o usr.o lib.o uten_s.o uten_cpp.o

uten_s.o: utente.s
	as utente.s -o uten_s.o

uten_cpp.o: utente.cpp
	g++ $(CXXFLAGS) $(CPPFLAGS) -c utente.cpp -o uten_cpp.o

clean:
	rm -f *.o

