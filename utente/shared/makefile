all: usr.o sys.o

CPPFLAGS=-I../include

usr.o: sh_usr.o lib.o uten_s.o uten_cpp.o
	ld -r -nostdlib -o usr.o sh_usr.o lib.o uten_s.o uten_cpp.o

sys.o: sh_sys.o
	ld -r -nostdlib -o sys.o sh_sys.o

uten_s.o: utente.s
	as utente.s -o uten_s.o

uten_cpp.o: utente.cpp
	g++ $(CXXFLAGS) $(CPPFLAGS) -c utente.cpp -o uten_cpp.o

clean:
	rm -f *.o

