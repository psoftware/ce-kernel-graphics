set START_IO=0x40000000

set CPPFLAGS=-nostdinc -Iinclude -g -fno-builtin -fcall-saved-edi -fcall-saved-esi -fcall-saved-ebx

rem compilazione di io
gcc %CPPFLAGS% -c io/io.S -o io/io_s.o
gcc %CPPFLAGS% -c io/io.cpp -o io/io_cpp.o
ld -nostdlib -o build/io -Ttext %START_IO% io/io_s.o io/io_cpp.o
