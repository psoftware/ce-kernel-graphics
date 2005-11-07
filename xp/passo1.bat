@echo off
rem Script di compilazione per Windows XP (Passo 1)
@echo on
rem Compilazione dei programmi di utilita'
cd ..\util
gcc costr.c -o ../build/costr.exe
gcc embed.c -o ../build/embed.exe
gcc -DWIN_XP parse.c -o ../build/parse.exe
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
cd ..\..\xp
rem Passo 1 completato
