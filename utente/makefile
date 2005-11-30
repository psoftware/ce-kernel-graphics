utente:
	cd prog ; $(MAKE)
	cd shared ; $(MAKE)
	ld -nostdlib -o utente -Ttext 0x80000000 shared/usr.o shared/sys.o prog/prog.o

clean:
	cd prog ; $(MAKE) clean
	cd shared ; $(MAKE) clean
	rm -f *.o

