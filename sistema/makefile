all: sistema

sist_s.o: sistema.S ../include/costanti.h
	gcc -I../include -g -c sistema.S -o sist_s.o

sist_cpp.o: sistema.cpp multiboot.h ../include/elf.h ../include/costanti.h
	g++ -g -I../include -fleading-underscore -fno-exceptions -fno-rtti -c sistema.cpp -o sist_cpp.o

sistema: sist_s.o sist_cpp.o
	ld -nostdlib -Ttext 100000 sist_s.o sist_cpp.o -o sistema
 
clean:
	rm -f sist_s.o sist_cpp.o
