START_SISTEMA=   0x0000000000200200
SWAP=		 swap.img
-include util/start.mk

LIBCE ?= $(HOME)/CE

NCC ?= g++
NLD ?= ld

COMM_CFLAGS=$(CFLAGS) \
	-Wall 			\
	-nostdlib		\
	-fno-exceptions 	\
	-fno-rtti 		\
	-fno-stack-protector 	\
	-fno-pic 		\
	-Iinclude		\
	-I$(LIBCE)/include/ce	\
	-mno-red-zone		\
	-O2			\
	-gdwarf-2

COMM_LDFLAGS=\
       -nostdlib

COMM_LDLIBS=

NCFLAGS=$(COMM_CFLAGS) -m64 -mcmodel=large
ifdef DEBUG
	NCFLAGS+=-DDEBUG
endif
NLDFLAGS=$(COMM_LDLIBS) -melf_x86_64 -L$(LIBCE)/lib64/ce
NLDLIBS=$(COMM_LDLIBS) -lce64

ifdef AUTOCORR
	NCFLAGS+=-DAUTOCORR
endif

WINDOWS_CPP_FILES := $(wildcard io/windows/*.cpp)
WINDOWS_OBJ_FILES = $(patsubst io/windows/%.cpp,io/windows/%.o,$(WINDOWS_CPP_FILES))

RESOURCES_CPP_FILES := $(wildcard io/windows/resources/*.cpp)
RESOURCES_OBJ_FILES = $(patsubst io/windows/resources/%.cpp,io/windows/resources/%.o,$(RESOURCES_CPP_FILES))

all: \
     build/sistema \
     build/parse   \
     build/creatimg \
     utente/prog

$(WINDOWS_OBJ_FILES): $(wildcard io/windows/*.h)

build/sistema: sistema/sist_s.o sistema/sist_cpp.o
	$(NLD) $(NLDFLAGS) -o build/sistema -Ttext $(START_SISTEMA) sistema/sist_s.o sistema/sist_cpp.o $(NLDLIBS)

build/io: io/io_s.o io/io_cpp.o $(WINDOWS_OBJ_FILES) $(RESOURCES_OBJ_FILES)
	$(NLD) $(NLDFLAGS) -o build/io -Ttext $(START_IO) io/io_s.o io/io_cpp.o $(WINDOWS_OBJ_FILES) $(RESOURCES_OBJ_FILES) $(NLDLIBS)

build/utente: utente/uten_s.o utente/lib.o utente/uten_cpp.o
	$(NLD) $(NLDFLAGS) -o build/utente -Ttext $(START_UTENTE) utente/uten_cpp.o utente/uten_s.o utente/lib.o $(NLDLIBS)

# compilazione di sistema.s e sistema.cpp
sistema/sist_s.o: sistema/sistema.s include/costanti.h
	$(NCC) $(NCFLAGS) -x assembler-with-cpp -c sistema/sistema.s -o sistema/sist_s.o

sistema/sist_cpp.o: sistema/sistema.cpp include/costanti.h
	$(NCC) $(NCFLAGS) -c sistema/sistema.cpp -o sistema/sist_cpp.o

# compilazione di io.s e io.cpp
io/io_s.o: io/io.s include/costanti.h
	$(NCC) $(NCFLAGS) -x assembler-with-cpp -c io/io.s -o io/io_s.o

io/io_cpp.o: io/io.cpp include/costanti.h io/windows/consts.h
	$(NCC) $(NCFLAGS) -c io/io.cpp -o io/io_cpp.o

## compilazione modulo windows di io
io/windows/%.o: io/windows/%.cpp
	$(NCC) $(NCFLAGS) -c -o $@ $<

## compilazione risorse per modulo windows dell'io
io/windows/resources/%.o: io/windows/resources/%.cpp
	$(NCC) $(NCFLAGS) -c -o $@ $<

# compilazione di utente.s e utente.cpp
utente/uten_s.o: utente/utente.s include/costanti.h
	$(NCC) $(NCFLAGS) -x assembler-with-cpp -c utente/utente.s -o utente/uten_s.o

utente/utente.cpp: build/parse  utente/prog/*.in utente/include/* utente/prog
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

util/elf32.o:  include/costanti.h util/interp.h include/elf.h util/elf32.h util/elf32.cpp
	g++ -c -g -Iinclude -o util/elf32.o util/elf32.cpp

util/elf64.o:  include/costanti.h util/interp.h include/elf.h include/elf64.h util/elf64.cpp
	g++ -c -g -Iinclude -o util/elf64.o util/elf64.cpp

util/interp.o: include/costanti.h util/interp.h util/interp.cpp
	g++ -c -g -Iinclude -o util/interp.o util/interp.cpp

util/swap.o: include/costanti.h util/swap.h util/swap.cpp
	g++ -c -g -Iinclude -o util/swap.o util/swap.cpp

util/fswap.o: include/costanti.h util/swap.h util/fswap.cpp
	g++ -c -g -Iinclude -o util/fswap.o util/fswap.cpp

util/creatimg.o: include/costanti.h util/interp.h util/swap.h util/creatimg.cpp
	g++ -c -g -Iinclude -o util/creatimg.o util/creatimg.cpp

build/creatimg: util/creatimg.o util/elf32.o util/elf64.o util/coff.o util/interp.o util/swap.o util/fswap.o
	g++ -g -o build/creatimg util/creatimg.o util/elf32.o util/elf64.o util/coff.o util/interp.o util/swap.o util/fswap.o

# creazione del file di swap
$(SWAP): util/start.mk
	truncate -s $(SWAP_SIZE) $(SWAP)

.PHONY: swap clean reset
swap: build/creatimg build/io build/utente $(SWAP)
	build/creatimg $(SWAP) build/io build/utente && ln -fs $(SWAP) .swap

clean:
	rm -f sistema/*.o io/*.o utente/*.o util/*.o
	rm -f io/windows/*.o
	rm -f util/start.mk util/start.gdb util/start.pl

clean-res:
	rm -f io/windows/resources/*.o

clean-all: \
	clean \
	clean-res

reset: clean
	rm -f build/* swap

build:
	mkdir -p $@

utente/prog:
	mkdir -p $@

build/mkstart: include/costanti.h util/mkstart.cpp
	g++ -g -Iinclude -o build/mkstart util/mkstart.cpp

util/start.mk: include/costanti.h include/tipo.h build/mkstart
	build/mkstart
