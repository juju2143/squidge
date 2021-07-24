CC=gcc
LD=ld
QJSC=qjsc
JSFLAGS=-M Graphics,Graphics
CFLAGS=-D_REENTRANT -fPIC -Wall
LDFLAGS=-fPIC
LIBS=-lm -ldl -lpthread -lSDL2 -L/lib/quickjs -lquickjs

all: squidge

squidge: demo.o gfx.o squidge.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.js
	$(QJSC) $(JSFLAGS) -c -o $@ $<

gfx.so: gfx.shared.o
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)

gfx.shared.o: gfx.c
	$(CC) $(CFLAGS) -DJS_SHARED_LIBRARY -c -o $@ $<

clean:
	rm -f *.o *.so demo.c squidge