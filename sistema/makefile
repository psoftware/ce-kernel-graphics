all: sistema

$(BUILDDIR)/sistema.bin: sist_s.o sist_cpp.o
	ld -nostdlib sist_s.o sist_cpp.o --oformat binary \
		-T $(BASEDIR)/lds/sistema.lds \
		-o $(BUILDDIR)/sistema.bin \
		-Map $(BUILDDIR)/sistema.map

sist_s.o: sistema.s
	as sistema.s -o sist_s.o

sist_cpp.o: sistema.cpp multiboot.h elf.h costanti.h
	g++ -fleading-underscore -fno-exceptions -O2 -c sistema.cpp -o sist_cpp.o

clean:
	rm -f sist_s.o sist_cpp.o

boot.o: boot.S multiboot.h costanti.h
	gcc -c -g boot.S

kernel.o: kernel.c multiboot.h
	gcc -fno-builtin -fleading-underscore -c -g kernel.c

sistema: boot.o sist_cpp.o
	ld -nostdlib -Ttext 100000 boot.o sist_cpp.o -o sistema
 
