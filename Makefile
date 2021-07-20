CC=gcc
LD=ld
CFLAGS=-DJS_SHARED_LIBRARY -D_REENTRANT -fPIC -Wall
LDFLAGS=-l:quickjs/libquickjs.a -lSDL2

all: gfx.so

gfx.so: gfx.o
	$(LD) $(LDFLAGS) -shared -o $@ $^

gfx.o: gfx.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f *.o *.so