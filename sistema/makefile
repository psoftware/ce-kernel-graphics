all: $(BUILDDIR)/sistema.bin

$(BUILDDIR)/sistema.bin: sist_s.o sist_cpp.o
	ld -nostdlib sist_s.o sist_cpp.o --oformat binary \
		-T $(BASEDIR)/lds/sistema.lds \
		-o $(BUILDDIR)/sistema.bin \
		-Map $(BUILDDIR)/sistema.map

sist_s.o: sistema.s
	as sistema.s -o sist_s.o

sist_cpp.o: sistema.cpp
	g++ $(CXXFLAGS) -fleading-underscore -fno-exceptions -c sistema.cpp -o sist_cpp.o

clean:
	rm -f sist_s.o sist_cpp.o

boot.o: boot.S multiboot.h
	gcc -c -g boot.S

kernel.o: kernel.c multiboot.h
	gcc -fno-builtin -fleading-underscore -c -g kernel.c

kernel: boot.o sist_cpp.o
	ld -nostdlib -Ttext 100000 -s boot.o sist_cpp.o -o kernel
 
