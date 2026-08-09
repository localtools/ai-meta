# ai_meta — C11; links zlib for PNG zTXt / compressed iTXt inflate
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS += -Iinclude -Isrc
LDFLAGS ?= -lz
AR      ?= ar

LIB_SRCS = \
	src/ai_meta.c \
	src/util.c \
	src/png_meta.c \
	src/jpeg_meta.c \
	src/webp_meta.c \
	src/exif_basic.c

LIB_OBJS = $(LIB_SRCS:.c=.o)

PREFIX ?= /usr/local

.PHONY: all clean test shared static cli fixtures install uninstall

all: static cli test

static: build/libai_meta.a

shared: build/libai_meta.dylib build/libai_meta.so

build:
	mkdir -p build

build/libai_meta.a: $(LIB_OBJS) | build
	$(AR) rcs $@ $(LIB_OBJS)

build/libai_meta.so: $(LIB_OBJS) | build
	$(CC) -shared -fPIC -o $@ $(LIB_OBJS) $(LDFLAGS)

build/libai_meta.dylib: $(LIB_OBJS) | build
	$(CC) -dynamiclib -o $@ $(LIB_OBJS) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

cli: build/ai_meta
build/ai_meta: tools/ai_meta_cli.c build/libai_meta.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tools/ai_meta_cli.c build/libai_meta.a $(LDFLAGS)

build/gen_fixtures: tests/gen_fixtures.c build/libai_meta.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/gen_fixtures.c build/libai_meta.a $(LDFLAGS)

fixtures: build/gen_fixtures
	mkdir -p tests/fixtures
	./build/gen_fixtures tests/fixtures

build/test_ai_meta: tests/test_ai_meta.c build/libai_meta.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_ai_meta.c build/libai_meta.a $(LDFLAGS)

test: fixtures build/test_ai_meta
	./build/test_ai_meta

build/ai_meta.pc: ai_meta.pc.in | build
	sed 's|@PREFIX@|$(PREFIX)|g' $< > $@

install: static cli build/ai_meta.pc
	install -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib \
		$(DESTDIR)$(PREFIX)/lib/pkgconfig $(DESTDIR)$(PREFIX)/bin
	install -m 644 include/ai_meta.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 build/libai_meta.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 build/ai_meta.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig/
	install -m 755 build/ai_meta $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/include/ai_meta.h \
		$(DESTDIR)$(PREFIX)/lib/libai_meta.a \
		$(DESTDIR)$(PREFIX)/lib/pkgconfig/ai_meta.pc \
		$(DESTDIR)$(PREFIX)/bin/ai_meta

clean:
	rm -f src/*.o build/libai_meta.a build/libai_meta.so build/libai_meta.dylib \
		build/ai_meta build/test_ai_meta build/gen_fixtures build/ai_meta.pc
	rm -rf tests/fixtures
