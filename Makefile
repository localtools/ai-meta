# ai_meta — C11; links zlib for PNG zTXt / compressed iTXt inflate
CC       ?= cc
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS += -Iinclude -Isrc
LDFLAGS  ?= -lz
AR       ?= ar

LIB_SRCS = \
	src/ai_meta.c \
	src/util.c \
	src/png_meta.c \
	src/jpeg_meta.c \
	src/webp_meta.c \
	src/exif_basic.c

LIB_OBJS = $(LIB_SRCS:.c=.o)

PREFIX ?= /usr/local

VERSION_MAJOR := $(shell sed -n 's/^\#define AI_META_VERSION_MAJOR //p' include/ai_meta.h)
VERSION_MINOR := $(shell sed -n 's/^\#define AI_META_VERSION_MINOR //p' include/ai_meta.h)
VERSION_PATCH := $(shell sed -n 's/^\#define AI_META_VERSION_PATCH //p' include/ai_meta.h)
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
	OS_NAME    := darwin
	SHARED_EXT := dylib
	SHARED_LDFLAGS := -dynamiclib -install_name @rpath/libai_meta.dylib
else
	OS_NAME    := linux
	SHARED_EXT := so
	SHARED_LDFLAGS := -shared -Wl,-soname,libai_meta.so.$(VERSION_MAJOR)
endif

# Normalize CPU arch labels used in release asset names
ARCH := $(UNAME_M)
ifeq ($(ARCH),amd64)
	ARCH := x86_64
endif
ifeq ($(ARCH),aarch64)
	ARCH := arm64
endif

SHARED_LIB := build/libai_meta.$(SHARED_EXT)
DIST_NAME  := ai_meta-$(VERSION)-$(OS_NAME)-$(ARCH)
DIST_ROOT  := dist/$(DIST_NAME)
DIST_TARBALL := dist/$(DIST_NAME).tar.gz

.PHONY: all clean test shared static cli fixtures install uninstall dist package release-notes-check

all: static cli test

static: build/libai_meta.a

shared: $(SHARED_LIB)

build:
	mkdir -p build

build/libai_meta.a: $(LIB_OBJS) | build
	$(AR) rcs $@ $(LIB_OBJS)

build/libai_meta.so: $(LIB_OBJS) | build
	$(CC) $(SHARED_LDFLAGS) -o $@ $(LIB_OBJS) $(LDFLAGS)

build/libai_meta.dylib: $(LIB_OBJS) | build
	$(CC) $(SHARED_LDFLAGS) -o $@ $(LIB_OBJS) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c -o $@ $<

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
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' $< > $@

install: static cli build/ai_meta.pc
	install -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib \
		$(DESTDIR)$(PREFIX)/lib/pkgconfig $(DESTDIR)$(PREFIX)/bin
	install -m 644 include/ai_meta.h $(DESTDIR)$(PREFIX)/include/
	install -m 644 build/libai_meta.a $(DESTDIR)$(PREFIX)/lib/
	install -m 644 build/ai_meta.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig/
	install -m 755 build/ai_meta $(DESTDIR)$(PREFIX)/bin/
	@if [ -f "$(SHARED_LIB)" ]; then \
		install -m 755 "$(SHARED_LIB)" "$(DESTDIR)$(PREFIX)/lib/"; \
	fi

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/include/ai_meta.h \
		$(DESTDIR)$(PREFIX)/lib/libai_meta.a \
		$(DESTDIR)$(PREFIX)/lib/libai_meta.so \
		$(DESTDIR)$(PREFIX)/lib/libai_meta.dylib \
		$(DESTDIR)$(PREFIX)/lib/pkgconfig/ai_meta.pc \
		$(DESTDIR)$(PREFIX)/bin/ai_meta

# Portable binary/library package for GitHub Releases
dist package: static shared cli
	rm -rf "$(DIST_ROOT)"
	mkdir -p "$(DIST_ROOT)/include" "$(DIST_ROOT)/lib/pkgconfig" "$(DIST_ROOT)/bin"
	cp include/ai_meta.h "$(DIST_ROOT)/include/"
	cp build/libai_meta.a "$(SHARED_LIB)" "$(DIST_ROOT)/lib/"
	cp build/ai_meta "$(DIST_ROOT)/bin/"
	@if command -v strip >/dev/null 2>&1; then \
		if [ "$(OS_NAME)" = "darwin" ]; then strip -x "$(DIST_ROOT)/bin/ai_meta" || true; \
		else strip "$(DIST_ROOT)/bin/ai_meta" || true; fi; \
	fi
	sed -e 's|@PREFIX@|$${pcfiledir}/../..|g' -e 's|@VERSION@|$(VERSION)|g' ai_meta.pc.in \
		> "$(DIST_ROOT)/lib/pkgconfig/ai_meta.pc"
	cp LICENSE NOTICE README.md "$(DIST_ROOT)/"
	printf '%s\n' "$(VERSION)" > "$(DIST_ROOT)/VERSION"
	mkdir -p dist
	tar -C dist -czf "$(DIST_TARBALL)" "$(DIST_NAME)"
	@echo "Created $(DIST_TARBALL)"

release-notes-check:
	@test -n "$(VERSION)" || (echo "VERSION unresolved" >&2; exit 1)
	@echo "ai_meta $(VERSION) ($(OS_NAME)-$(ARCH))"

clean:
	rm -f src/*.o build/libai_meta.a build/libai_meta.so build/libai_meta.dylib \
		build/ai_meta build/test_ai_meta build/gen_fixtures build/ai_meta.pc
	rm -rf tests/fixtures dist
