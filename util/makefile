all: $(BUILDDIR)/costr $(BUILDDIR)/embed $(BUILDDIR)/parse $(BUILDDIR)/createimg

$(BUILDDIR)/costr: costr.c
	gcc costr.c -o $(BUILDDIR)/costr

$(BUILDDIR)/embed: embed.c
	gcc embed.c -o $(BUILDDIR)/embed

$(BUILDDIR)/parse: parse.c src.h
	gcc parse.c -g -o $(BUILDDIR)/parse

$(BUILDDIR)/createimg: createimg.cpp
	g++ -g -I../include createimg.cpp -o $(BUILDDIR)/createimg

clean:

