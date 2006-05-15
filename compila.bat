set START_SISTEMA=0x00100000
set START_IO=0x40000000
set START_UTENTE=0x80000000
set SWAP=swap.img

set CPPFLAGS=-nostdinc -Iinclude -g -fno-builtin

rem creazione di parse e creatimg
gcc -DWIN -o build/parse.exe util/parse.c
gcc -g -Iinclude -o build/creatimg.exe util/creatimg.cpp -lgpp

rem compilazione di sistema
gcc %CPPFLAGS% -c sistema/sistema.S -o sistema/sist_s.o
gcc %CPPFLAGS% -c sistema/sistema.cpp -o sistema/sist_cpp.o
ld -nostdlib -o build/sistema -Ttext %START_SISTEMA% sistema/sist_s.o sistema/sist_cpp.o

rem compilazione di io
gcc %CPPFLAGS% -c io/io.S -o io/io_s.o
gcc %CPPFLAGS% -c io/io.cpp -o io/io_cpp.o
ld -nostdlib -o build/io -Ttext %START_IO% io/io_s.o io/io_cpp.o

rem compilazione di utente
gcc %CPPFLAGS% -c utente/utente.s -o utente/uten_s.o
build\parse -o utente\utente.cpp utente\prog\*.in
gcc %CPPFLAGS% -Iutente/include -c utente/utente.cpp -o utente/uten_cpp.o
gcc %CPPFLAGS% -Iutente/include -c utente/lib.cpp -o utente/lib.o
ld -nostdlib -o build/utente -Ttext %START_UTENTE% utente/uten_cpp.o utente/uten_s.o utente/lib.o

rem creazione dello swap
build\creatimg %SWAP% build\io build\utente
