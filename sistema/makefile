all: sistema

sist_s.o: sistema.S
	gcc -c sistema.S -o sist_s.o

sist_cpp.o: sistema.cpp multiboot.h elf.h costanti.h
	g++ -fleading-underscore -fno-exceptions -fno-rtti -O2 -c sistema.cpp -o sist_cpp.o

sistema: sist_s.o sist_cpp.o
	ld -nostdlib -Ttext 100000 sist_s.o sist_cpp.o -o sistema
 
clean:
	rm -f sist_s.o sist_cpp.o
