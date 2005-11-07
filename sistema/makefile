all: $(BUILDDIR)/sistema.bin

$(BUILDDIR)/sistema.bin: sist_s.o sist_cpp.o
	ld -nostdlib sist_s.o sist_cpp.o --oformat binary \
		-T $(BASEDIR)/lds/sistema.lds \
		-o $(BUILDDIR)/sistema.bin \
		-Map $(BUILDDIR)/sistema.map

sist_s.o: sistema.s
	as sistema.s -o sist_s.o

sist_cpp.o: sistema.cpp
	g++ $(CXXFLAGS) -c sistema.cpp -o sist_cpp.o

clean:
	rm -f sist_s.o sist_cpp.o
 
