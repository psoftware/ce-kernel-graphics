MAKE=make
START_SISTEMA=   0x00100000
START_IO=        0x00200000
START_UTENTE=	 0x80000000
SWAPSIZE=10000

CXXFLAGS=-fleading-underscore -fno-exceptions -fno-rtti -g
CPPFLAGS=-nostdinc -Iinclude -g

all: build/sistema \
     build/io 	   \
     build/parse   \
     build/createimg
     
build/sistema: sistema/sist_s.o sistema/sist_cpp.o
	ld -nostdlib -o build/sistema -Ttext $(START_SISTEMA) sistema/sist_s.o sistema/sist_cpp.o

build/io: io/io_s.o io/io_cpp.o
	ld -nostdlib -o build/io -Ttext $(START_IO) io/io_s.o io/io_cpp.o

build/utente: utente/uten_s.o utente/lib.o utente/uten_cpp.o
	ld -nostdlib -o build/utente -Ttext $(START_UTENTE) utente/uten_cpp.o utente/uten_s.o utente/lib.o


# compilazione di sistema.s e sistema.cpp
sistema/sist_s.o: sistema/sistema.S include/costanti.h
	gcc $(CPPFLAGS) -c sistema/sistema.S -o sistema/sist_s.o

sistema/sist_cpp.o: sistema/sistema.cpp include/multiboot.h include/elf.h include/costanti.h
	g++ $(CPPFLAGS) $(CXXFLAGS) -c sistema/sistema.cpp -o sistema/sist_cpp.o

# compilazione di io.s e io.cpp
io/io_s.o: io/io.S include/costanti.h
	gcc $(CPPFLAGS) -c io/io.S -o io/io_s.o

io/io_cpp.o: io/io.cpp include/costanti.h
	g++ $(CPPFLAGS) $(CXXFLAGS) -c io/io.cpp -o io/io_cpp.o

# compilazione di utente.s e utente.cpp
utente/uten_s.o: utente/utente.s
	gcc $(CPPFLAGS) -c utente/utente.s -o utente/uten_s.o

utente/utente.cpp: build/parse utente/prog/* utente/include/*
	build/parse -o utente/utente.cpp utente/prog/*

utente/uten_cpp.o: utente/utente.cpp
	g++ $(CXXFLAGS) $(CPPFLAGS) -Iutente/include -c utente/utente.cpp -o utente/uten_cpp.o

utente/lib.o: utente/lib.cpp utente/include/lib.h
	g++ $(CXXFLAGS) $(CPPFLAGS) -Iutente/include -c utente/lib.cpp -o utente/lib.o

# creazione di parse e createimg
build/parse: util/parse.c util/src.h
	gcc -o build/parse util/parse.c

build/createimg: util/createimg.cpp
	g++ -Iinclude -o build/createimg util/createimg.cpp

swap: build/createimg build/utente
	build/createimg $(SWAPSIZE) build/utente
	

clean:
	rm -f sistema/*.o io/*.o utente/*.o 

reset: clean
	rm -f build/* swap
