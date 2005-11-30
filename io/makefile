all: io

$(BUILDDIR)/io.bin: io_s.o io_cpp.o
	ld -nostdlib io_s.o io_cpp.o --oformat binary -T $(BASEDIR)/lds/io.lds \
		-o $(BUILDDIR)/io.bin -Map $(BUILDDIR)/io.map

io_s.o: io.s
	as io.s -o io_s.o

io_cpp.o: io.cpp
	g++ $(CXXFLAGS) -fleading-underscore -fno-exceptions -c io.cpp -o io_cpp.o

clean:
	rm -f io_s.o io_cpp.o

io: io_s.o io_cpp.o
	ld -nostdlib io_s.o io_cpp.o -Ttext 0x200000 -o io
 
