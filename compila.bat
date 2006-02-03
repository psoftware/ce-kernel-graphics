rem Script di compilazione
rem Compilazione del modulo di IO
cd io
gcc -c -nostdinc -I../include io.S -o io_s.o
gcc -c -nostdinc -I../include io.cpp -o io_cpp.o
ld -nostdlib io_s.o io_cpp.o --oformat a.out-i386 -Ttext 200000 -o ../build/io
cd ..
rem Compilazione del nucleo
cd sistema
gcc -c -nostdinc -I../include sistema.S -o sist_s.o
gcc -c -nostdinc -I../include sistema.cpp -o sist_cpp.o
ld -nostdlib sist_s.o sist_cpp.o --oformat a.out-i386 -Ttext 100000 -o ../build/sistema
cd ..
