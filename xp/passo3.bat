@echo off
rem Script di compilazione per Windows XP (Passo 3)
@echo on
rem Compilazione dei programmi utente (sezioni condivise e collegamento)
cd ..\utente\shared
gcc -I../include -c sh_sys.cpp -o sh_sys.o
gcc -I../include -c lib.cpp -o lib.o
gcc -I../include -c sh_usr.cpp -o sh_usr.o
gcc -I../include -c utente.cpp -o uten_cpp.o
as utente.s -o uten_s.o
ld -Ur -T ../../lds/susr.lds -nostdlib -o usr.o sh_usr.o lib.o uten_s.o uten_cpp.o
ld -Ur -T ../../lds/ssys.lds -nostdlib -o sys.o sh_sys.o
cd ..
ld --oformat binary -o ../build/utente.bin shared/usr.o shared/sys.o prog/prog.o -T ../lds/utente.lds -Map ../build/utente.map
rem Costruzione dell' immagine
cd ..\build
embed.exe
costr.exe
cd ..\xp
