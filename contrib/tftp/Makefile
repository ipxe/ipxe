#
# Copyright (c) 1987 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)Makefile	5.8 (Berkeley) 9/20/88
#
# We override /usr/include/arpa/tftp.h with our own because
# we want tu_block to be unsigned short, not short as on most platforms
#
CFLAGS=	-I. -O2 -Dsin=sin_x
SRCS=	main.c tftp.c tftpsubs.c tftpd.c
OBJS=	main.o tftp.o tftpsubs.o
DOBJS=	tftpd.o tftpsubs.o
CC=	gcc
LIBS=	# -linet

all: tftp tftpd

tftp: ${OBJS}
	${CC} -o $@ ${CFLAGS} ${OBJS} # -linet

tftpd: ${DOBJS}
	${CC} -o $@ ${CFLAGS} ${DOBJS} ${LIBS}

clean:
	rm -f ${OBJS} ${DOBJS} core tftp tftpd

cleandir: clean
	rm -f tags .depend

depend: ${SRCS}
	mkdep ${CFLAGS} ${SRCS}

install:
	install -s -o root -g root -m 755 tftp /usr/bin/tftp
	install -c -o root -g root -m 444 tftp.1 /usr/man/man1
	install -s -o root -g root -m 755 tftpd /usr/sbin/in.tftpd
	install -c -o root -g root -m 444 tftpd.8 /usr/man/man8

lint: ${SRCS}
	lint ${CFLAGS} ${SRCS}

tags: ${SRCS}
	ctags ${SRCS}
