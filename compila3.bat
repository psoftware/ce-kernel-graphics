set START_UTENTE=0x80000000

set CPPFLAGS=-nostdinc -Iinclude -g -fno-builtin -fcall-saved-edi -fcall-saved-esi -fcall-saved-ebx

rem compilazione di utente
gcc %CPPFLAGS% -c utente/utente.s -o utente/uten_s.o
build\parse -o utente\utente.cpp utente\prog\*.in
gcc %CPPFLAGS% -Iutente/include -c utente/utente.cpp -o utente/uten_cpp.o
gcc %CPPFLAGS% -Iutente/include -c utente/lib.cpp -o utente/lib.o
ld -nostdlib -o build/utente -Ttext %START_UTENTE% utente/uten_cpp.o utente/uten_s.o utente/lib.o
