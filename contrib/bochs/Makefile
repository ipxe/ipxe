all : tunctl serial-console.1

%.1 : %
	pod2man $< > $@

tunctl : tunctl.c
	$(CC) -o $@ $<

clean :
	rm -f serial-console.1 tunctl
