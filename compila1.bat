set START_SISTEMA=0x00100000

set CPPFLAGS=-nostdinc -Iinclude -g -fno-builtin -fcall-saved-edi -fcall-saved-esi -fcall-saved-ebx

rem creazione di parse e creatimg
gcc -DWIN -o build/parse.exe util/parse.c
gcc -g -Iinclude -o build/creatimg.exe util/creatimg.cpp -lgpp

rem compilazione di sistema
gcc %CPPFLAGS% -c sistema/sistema.S -o sistema/sist_s.o
gcc %CPPFLAGS% -c sistema/sistema.cpp -o sistema/sist_cpp.o
ld -nostdlib -o build/sistema -Ttext %START_SISTEMA% sistema/sist_s.o sistema/sist_cpp.o
