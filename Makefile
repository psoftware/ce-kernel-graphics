START_BOOT=	 0x00100000
START_SISTEMA=   0x00400000
START_IO=        0x40400000
START_UTENTE=	 0x80000000
SWAP_SIZE=	 20M
SWAP=		 swap.img

LIBCE ?= $(HOME)/CE

NCC ?= g++
NLD ?= ld

COMM_CFLAGS=\
	-Wall 			\
	-nostdlib		\
	-fno-exceptions 	\
	-fno-rtti 		\
	-fno-stack-protector 	\
	-fno-pic 		\
	-fcall-saved-esi 	\
	-fcall-saved-edi 	\
	-fcall-saved-ebx 	\
	-Iinclude		\
	-I$(LIBCE)/include/ce	\
	-g

COMM_LDFLAGS=\
       -nostdlib

COMM_LDLIBS=

NCFLAGS=$(COMM_CFLAGS) -m64 -mcmodel=large
NLDFLAGS=$(COMM_LDLIBS) -melf_x86_64 -L$(LIBCE)/lib64/ce
NLDLIBS=$(COMM_LDLIBS) -lce64

BCC ?= $(NCC)
BLD ?= $(NLD)
BCFLAGS = $(COMM_CFLAGS) -m32
BLDFLAGS = $(COMM_LDFLAGS) -melf_i386 -L$(LIBCE)/lib/ce
BLDLIBS = $(COMM_LDLIBS) -lce

ifdef AUTOCORR
	NCFLAGS+=-DAUTOCORR
endif

all: \
     build/boot \
     build/sistema \
     build/parse   \
     build/creatimg \
     utente/prog

build/boot: boot/boot_s.o boot/boot_cpp.o
	$(BLD) $(BLDFLAGS) -o build/boot -Ttext $(START_BOOT) boot/boot_s.o boot/boot_cpp.o $(BLDLIBS)

# compilazione di boot.s e boot.cpp
boot/boot_s.o: boot/boot.S include/costanti.h
	$(BCC) $(BCFLAGS) -c boot/boot.S -o boot/boot_s.o

boot/boot_cpp.o: boot/boot.cpp include/mboot.h include/costanti.h
	$(BCC) $(BCFLAGS) -c boot/boot.cpp -o boot/boot_cpp.o
     
build/sistema: sistema/sist_s.o sistema/sist_cpp.o
	$(NLD) $(NLDFLAGS) -o build/sistema -Ttext $(START_SISTEMA) sistema/sist_s.o sistema/sist_cpp.o $(NLDLIBS)

build/io: io/io_s.o io/io_cpp.o
	$(NLD) $(NLDFLAGS) -o build/io -Ttext $(START_IO) io/io_s.o io/io_cpp.o $(NLDLIBS)

build/utente: utente/uten_s.o utente/lib.o utente/uten_cpp.o
	$(NLD) $(NLDFLAGS) -o build/utente -Ttext $(START_UTENTE) utente/uten_cpp.o utente/uten_s.o utente/lib.o $(NLDLIBS)

# compilazione di sistema.s e sistema.cpp
sistema/sist_s.o: sistema/sistema.S include/costanti.h
	$(NCC) $(NCFLAGS) -c sistema/sistema.S -o sistema/sist_s.o

sistema/sist_cpp.o: sistema/sistema.cpp include/mboot.h include/costanti.h
	$(NCC) $(NCFLAGS) -c sistema/sistema.cpp -o sistema/sist_cpp.o

# compilazione di io.s e io.cpp
io/io_s.o: io/io.S include/costanti.h
	$(NCC) $(NCFLAGS) -c io/io.S -o io/io_s.o

io/io_cpp.o: io/io.cpp include/costanti.h
	$(NCC) $(NCFLAGS) -c io/io.cpp -o io/io_cpp.o

# compilazione di utente.s e utente.cpp
utente/uten_s.o: utente/utente.S include/costanti.h
	$(NCC) $(NCFLAGS) -c utente/utente.S -o utente/uten_s.o

utente/utente.cpp: build/parse utente/prog/*.in utente/include/* utente/prog
	build/parse -o utente/utente.cpp utente/prog/*.in

utente/uten_cpp.o: utente/utente.cpp
	$(NCC) $(NCFLAGS) -Iutente/include -c utente/utente.cpp -o utente/uten_cpp.o

utente/lib.o: utente/lib.cpp utente/include/lib.h
	$(NCC) $(NCFLAGS) -Iutente/include -c utente/lib.cpp -o utente/lib.o

# creazione di parse e createimg
build/parse: util/parse.c util/src.h
	gcc -o build/parse util/parse.c

util/coff.o: include/costanti.h util/interp.h util/coff.h util/dos.h util/coff.cpp
	g++ -c -g -Iinclude -o util/coff.o util/coff.cpp

util/elf32.o:  include/costanti.h util/interp.h util/elf.h util/elf32.h util/elf32.cpp
	g++ -c -g -Iinclude -o util/elf32.o util/elf32.cpp

util/elf64.o:  include/costanti.h util/interp.h util/elf.h util/elf64.h util/elf64.cpp
	g++ -c -g -Iinclude -o util/elf64.o util/elf64.cpp

util/interp.o: include/costanti.h util/interp.h util/interp.cpp
	g++ -c -g -Iinclude -o util/interp.o util/interp.cpp

util/swap.o: include/costanti.h util/swap.h util/swap.cpp
	g++ -c -g -Iinclude -o util/swap.o util/swap.cpp

util/fswap.o: include/costanti.h util/swap.h util/fswap.cpp
	g++ -c -g -Iinclude -o util/fswap.o util/fswap.cpp

util/creatimg.o: util/interp.h util/swap.h util/creatimg.cpp
	g++ -c -g -Iinclude -o util/creatimg.o util/creatimg.cpp

build/creatimg: build util/creatimg.o util/elf32.o util/elf64.o util/coff.o util/interp.o util/swap.o util/fswap.o
	g++ -g -o build/creatimg util/creatimg.o util/elf32.o util/elf64.o util/coff.o util/interp.o util/swap.o util/fswap.o

# creazione del file di swap
$(SWAP):
	truncate -s $(SWAP_SIZE) $(SWAP)

.PHONY: swap clean reset
swap: build/creatimg build/io build/utente $(SWAP)
	build/creatimg $(SWAP) build/io build/utente && ln -fs $(SWAP) .swap

clean:
	rm -f boot/*.o sistema/*.o io/*.o utente/*.o util/*.o

reset: clean
	rm -f build/* swap

build:
	mkdir -p $@

utente/prog:
	mkdir -p $@
