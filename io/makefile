all: io

io_s.o: io.S ../include/costanti.h
	g++ -I../include -c io.S -o io_s.o

io_cpp.o: io.cpp ../include/costanti.h
	g++ -I../include -fleading-underscore -fno-exceptions -c io.cpp -o io_cpp.o

clean:
	rm -f io_s.o io_cpp.o

io: io_s.o io_cpp.o
	ld -nostdlib io_s.o io_cpp.o -Ttext 0x200000 -o io
 
