include config.mk

CC=gcc
SRC=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(SRC))

HEADERS=$(wildcard *.h)

ICONS=$(wildcard icons/*.png)
XPMS=$(patsubst icons/%.png,icons/%.xpm,$(ICONS))

FONTS=$(wildcard fonts/*.otf)
FONTS_H=$(patsubst fonts/%.otf,fonts/%.h,$(FONTS))

PREFIX := /usr/local

# require gnumake
icons/%.xpm: icons/%.png
	convert $< $@

fonts/%.h: fonts/%.otf
	xxd -i $< > $@

%.o: %.c $(HEADERS) config.mk
	$(CC) -c $(CFLAGS) -o $@ $<

bin/classic-colors: $(XPMS) $(FONTS_H) $(OBJS)
	mkdir -p bin
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) 

.PHONY: install
install: bin/classic-colors
	install -d "$(PREFIX)/bin/"
	install -m 755 bin/classic-colors "$(PREFIX)/bin/classic-colors"
	install -d "${PREFIX}/share/classic-colors/help/"
	install -m 0644 -t "${PREFIX}/share/classic-colors/help" help/*

.PHONY: clean
clean:
	rm -f bin/classic-colors
	rm -f *.o 

.PHONY: clean-icons
clean-icons:
	rm -f fonts/*.h
	rm -f icons/*.xpm
