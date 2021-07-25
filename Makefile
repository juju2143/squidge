JSFLAGS=-M Graphics,Graphics
CFLAGS=-D_REENTRANT -fPIC -Wall
LDFLAGS=-fPIC
LIBS=-lm -lpthread -lSDL2 -lquickjs

ifdef CONFIG_WIN32
	CROSS_PREFIX=x86_64-w64-mingw32-
	EXT=.exe
	WIN32="CONFIG_WIN32=y"
	CONFIG_LOCAL=y
else
	LIBS+=-ldl
endif

CC=$(CROSS_PREFIX)gcc
LD=$(CROSS_PREFIX)ld
MAKE=make
QJSC=qjsc

ifdef CONFIG_LOCAL
	LIBS+=-Lquickjs
	CFLAGS+=-I.
	QJSC=quickjs/qjsc
	PROGS=quickjs/libquickjs.a quickjs/qjsc dlls
else
	LIBS+=-L/lib/quickjs
endif

all: $(PROGS) squidge$(EXT)

squidge$(EXT): demo.o gfx.o squidge.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.js
	$(QJSC) $(JSFLAGS) -c -o $@ $<

quickjs/libquickjs.a:
	$(MAKE) -C quickjs libquickjs.a

quickjs/qjsc:
	$(MAKE) -C quickjs -E "override undefine CONFIG_WIN32" qjsc

gfx.so: gfx.shared.o
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)

gfx.shared.o: gfx.c
	$(CC) $(CFLAGS) -DJS_SHARED_LIBRARY -c -o $@ $<

dlls:
	cp /usr/$(CROSS_PREFIX:-=)/bin/libwinpthread-1.dll .
	cp /usr/$(CROSS_PREFIX:-=)/bin/SDL2.dll .

clean:
	rm -f *.o *.so *.dll demo.c squidge$(EXT)
	$(MAKE) -C quickjs clean