@echo off
rem Script di compilazione
@echo on
rem Compilazione dei programmi di utilita'
cd util
gcc costr.c -o ../build/costr.exe
gcc embed.c -o ../build/embed.exe
gcc -DWIN parse.c -o ../build/parse.exe
cd ..
rem Compilazione del bootloader
cd boot
as boot.s -o boot.o
objcopy -O binary boot.o ../build/bsect.bin
cd ..
rem Compilazione del modulo di IO
cd io
as io.s -o io_s.o
gcc -c io.cpp -o io_cpp.o
ld -nostdlib io_s.o io_cpp.o --oformat binary -T ../lds/io.lds -o ../build/io.bin -Map ../build/io.map
cd ..
rem Compilazione del nucleo
cd sistema
as sistema.s -o sist_s.o
gcc -c sistema.cpp -o sist_cpp.o
ld -nostdlib sist_s.o sist_cpp.o --oformat binary -T ../lds/sistema.lds -o ../build/sistema.bin -Map ../build/sistema.map
cd ..
rem Compilazione dei programmi utente
cd utente\prog
..\..\build\parse.exe
cd ..\shared
gcc -I../include -c sh_sys.cpp -o sh_sys.o
gcc -I../include -c lib.cpp -o lib.o
gcc -I../include -c sh_usr.cpp -o sh_usr.o
gcc -I../include -c utente.cpp -o uten_cpp.o
as utente.s -o uten_s.o
ld -Ur -T ../../lds/susr.lds -nostdlib -o usr.o sh_usr.o lib.o uten_s.o uten_cpp.o
ld -Ur -T ../../lds/ssys.lds -nostdlib -o sys.o sh_sys.o
cd ..
ld --oformat binary -o ../build/utente.bin shared/usr.o shared/sys.o prog/prog.o -T ../lds/utente.lds -Map ../build/utente.map
cd ..
rem Costruzione dell' immagine
cd build
embed.exe
costr.exe
cd ..
