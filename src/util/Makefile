BLIB = ../bin/blib.a
CFLAGS = -Os

all : hijack prototester

hijack : hijack.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -Wall -lpcap -o $@ $<

prototester.o : prototester.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -Wall -o $@ -c $< -idirafter ../include

prototester : prototester.o $(BLIB)
	$(CC) -o $@ $< -lc $(BLIB)

clean :
	rm -f hijack prototester *.o
