all:
	cd prog ; $(MAKE)
	cd shared ; $(MAKE)
	ld --oformat binary -o $(BUILDDIR)/utente.bin \
		shared/usr.o shared/sys.o prog/prog.o \
		-T $(BASEDIR)/lds/utente.lds -Map $(BUILDDIR)/utente.map

clean:
	cd prog ; $(MAKE) clean
	cd shared ; $(MAKE) clean
	rm -f *.o

