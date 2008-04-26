BINS=isutf8 ifdata ifne pee sponge mispipe lckdo
PERLSCRIPTS=vidir vipe ts combine zrun
MANS=sponge.1 vidir.1 vipe.1 isutf8.1 ts.1 combine.1 ifdata.1 ifne.1 pee.1 zrun.1 mispipe.1 lckdo.1
CFLAGS=-O2 -g -Wall
INSTALL_BIN?=install -s

all: $(BINS) $(MANS)

clean:
	rm -f $(BINS) $(MANS)

install:
	mkdir -p $(DESTDIR)/usr/bin
	$(INSTALL_BIN) $(BINS) $(DESTDIR)/usr/bin
	install $(PERLSCRIPTS) $(DESTDIR)/usr/bin
	
	mkdir -p $(DESTDIR)/usr/share/man/man1
	install $(MANS) $(DESTDIR)/usr/share/man/man1

check: isutf8
	./check-isutf8

isutf8.1: isutf8.docbook
	docbook2x-man $<

ifdata.1: ifdata.docbook
	docbook2x-man $<

ifne.1: ifne.docbook
	docbook2x-man $<

pee.1: pee.docbook
	docbook2x-man $<

sponge.1: sponge.docbook
	docbook2x-man $<

mispipe.1: mispipe.docbook
	docbook2x-man $<

lckdo.1: lckdo.docbook
	docbook2x-man $<

%.1: %
	pod2man --center=" " --release="moreutils" $< > $@;
