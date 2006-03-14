BINS=isutf8 ifdata pee
PERLSCRIPTS=vidir vipe ts combine sponge
MANS=sponge.1 vidir.1 vipe.1 isutf8.1 ts.1 combine.1 ifdata.1 pee.1
CFLAGS=-O2 -g -Wall

all: $(BINS) $(MANS)

clean:
	rm -f $(BINS) $(MANS)

install:
	mkdir -p $(PREFIX)/usr/bin
	install -s $(BINS) $(PREFIX)/usr/bin
	install $(PERLSCRIPTS) $(PREFIX)/usr/bin
	
	mkdir -p $(PREFIX)/usr/share/man/man1
	install $(MANS) $(PREFIX)/usr/share/man/man1

check: isutf8
	./check-isutf8

isutf8.1: isutf8.docbook
	docbook2x-man $<

ifdata.1: ifdata.docbook
	docbook2x-man $<

pee.1: pee.docbook
	docbook2x-man $<

%.1: %
	pod2man --center=" " --release="moreutils" $< > $@;
