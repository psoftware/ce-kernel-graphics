all: $(BUILDDIR)/bsect.bin

$(BUILDDIR)/bsect.bin: boot.o
	objcopy -O binary boot.o $(BUILDDIR)/bsect.bin

clean:
	rm -f boot.o

