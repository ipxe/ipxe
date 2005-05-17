/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)tftp.c	5.7 (Berkeley) 6/29/88";
#endif /* not lint */

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

/*
 * TFTP User Program -- Protocol Machines
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/tftp.h>

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>

extern	int errno;

extern  struct sockaddr_in sin;         /* filled in by main */
extern  int     f;                      /* the opened socket */
extern  int     trace;
extern  int     verbose;
extern  int     rexmtval;
extern  int     maxtimeout;
extern	int	segsize;

#define PKTSIZE    (1432+4) /* SEGSIZE+4 */
char    ackbuf[PKTSIZE];
int	timeout;
jmp_buf	toplevel;
jmp_buf	timeoutbuf;

#ifndef OACK
#define OACK	6
#endif

void timer(int sig)
{

	signal(SIGALRM, timer);
	timeout += rexmtval;
	if (timeout >= maxtimeout) {
		printf("Transfer timed out.\n");
		longjmp(toplevel, -1);
	}
	longjmp(timeoutbuf, 1);
}

strnlen(s, n)
	char *s;
	int n;
{
	int i = 0;

	while (n-- > 0 && *s++) i++;
	return(i);
}

/*
 * Parse an OACK package and set blocksize accordingly
 */
parseoack(cp, sz)
	char *cp;
	int sz;
{
	int n;
	
	segsize = 512;
	while (sz > 0 && *cp) {
		n = strnlen(cp, sz);
		if (n == 7 && !strncmp("blksize", cp, 7)) {
			cp += 8;
			sz -= 8;
			if (sz <= 0)
				break;
			for (segsize = 0, n = strnlen(cp, sz); n > 0;
			     n--, cp++, sz--) {
				if (*cp < '0' || *cp > '9')
					break;
				segsize = 10*segsize + *cp - '0'; }
		}
		cp += n + 1;
		sz -= n + 1;
	}
	if (segsize < 8 || segsize > 1432) {
		printf("Remote host negotiated illegal blocksize %d\n",
		       segsize);
		segsize = 512;
		longjmp(timeoutbuf, -1);
	}
}

/*
 * Send the requested file.
 */
sendfile(fd, name, mode)
	int fd;
	char *name;
	char *mode;
{
	register struct tftphdr *ap;       /* data and ack packets */
	struct tftphdr *r_init(), *dp;
	register int size, n;
	u_short block = 0;
	register unsigned long amount = 0;
	struct sockaddr_in from;
	int fromlen;
	int convert;            /* true if doing nl->crlf conversion */
	FILE *file;

	startclock();           /* start stat's clock */
	dp = r_init();          /* reset fillbuf/read-ahead code */
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "r");
	convert = !strcmp(mode, "netascii");

	signal(SIGALRM, timer);
	do {
		if (block == 0)
			size = makerequest(WRQ, name, dp, mode) - 4;
		else {
		/*      size = read(fd, dp->th_data, SEGSIZE);   */
			size = readit(file, &dp, convert);
			if (size < 0) {
				nak(errno + 100);
				break;
			}
			dp->th_opcode = htons((u_short)DATA);
			dp->th_block = htons(block);
		}
		timeout = 0;
		(void) setjmp(timeoutbuf);
send_data:
		if (trace)
			tpacket("sent", dp, size + 4);
		n = sendto(f, dp, size + 4, 0, (struct sockaddr *)&sin,
			   sizeof (sin));
		if (n != size + 4) {
			perror("tftp: sendto");
			goto abort;
		}
		if (block) /* do not start reading until the blocksize
			      has been negotiated */
			read_ahead(file, convert);
		for ( ; ; ) {
			alarm(rexmtval);
			do {
				fromlen = sizeof (from);
				n = recvfrom(f, ackbuf, sizeof (ackbuf), 0,
					     (struct sockaddr *)&from,
					     &fromlen);
			} while (n <= 0);
			alarm(0);
			if (n < 0) {
				perror("tftp: recvfrom");
				goto abort;
			}
			sin.sin_port = from.sin_port;   /* added */
			if (trace)
				tpacket("received", ap, n);
			/* should verify packet came from server */
			ap->th_opcode = ntohs(ap->th_opcode);
			if (ap->th_opcode == ERROR) {
				printf("Error code %d: %s\n", ap->th_code,
					ap->th_msg);
				goto abort;
			}
			if (ap->th_opcode == ACK) {
				int j;

				ap->th_block = ntohs(ap->th_block);

				if (block == 0) {
					if (trace)
						printf("server does not know "
						       "about RFC1782; reset"
						       "ting blocksize\n");
					segsize = 512;
				}
				if (ap->th_block == block) {
					break;
				}
				/* On an error, try to synchronize
				 * both sides.
				 */
				j = synchnet(f);
				if (j && trace) {
					printf("discarded %d packets\n",
							j);
				}
				if (ap->th_block == (block-1)) {
					goto send_data;
				}
			}
			else if (ap->th_opcode == OACK) {
				if (block) {
					printf("protocol violation\n");
					longjmp(toplevel, -1);
				}
				parseoack(&ap->th_stuff, n - 2);
				break;
			}
		}
		if (block > 0)
			amount += size;
		else
			read_ahead(file, convert);
		block++;
	} while (size == segsize || block == 1);
abort:
	fclose(file);
	stopclock();
	if (amount > 0)
		printstats("Sent", amount);
}

/*
 * Receive a file.
 */
recvfile(fd, name, mode)
	int fd;
	char *name;
	char *mode;
{
	register struct tftphdr *ap;
	struct tftphdr *dp, *w_init();
	register int n, size;
	u_short block = 1;
	unsigned long amount = 0;
	struct sockaddr_in from;
	int fromlen, firsttrip = 1;
	FILE *file;
	int convert;                    /* true if converting crlf -> lf */
	int waitforoack = 1;

	startclock();
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "w");
	convert = !strcmp(mode, "netascii");

	signal(SIGALRM, timer);
	do {
		if (firsttrip) {
			size = makerequest(RRQ, name, ap, mode);
			firsttrip = 0;
		} else {
			ap->th_opcode = htons((u_short)ACK);
			ap->th_block = htons(block);
			size = 4;
			block++;
		}
		timeout = 0;
		(void) setjmp(timeoutbuf);
send_ack:
		if (trace)
			tpacket("sent", ap, size);
		if (sendto(f, ackbuf, size, 0, (struct sockaddr *)&sin,
		    sizeof (sin)) != size) {
			alarm(0);
			perror("tftp: sendto");
			goto abort;
		}
		if (!waitforoack)
			write_behind(file, convert);
		for ( ; ; ) {
			alarm(rexmtval);
			do  {
				fromlen = sizeof (from);
				n = recvfrom(f, dp, PKTSIZE, 0,
				    (struct sockaddr *)&from, &fromlen);
			} while (n <= 0);
			alarm(0);
			if (n < 0) {
				perror("tftp: recvfrom");
				goto abort;
			}
			sin.sin_port = from.sin_port;   /* added */
			if (trace)
				tpacket("received", dp, n);
			/* should verify client address */
			dp->th_opcode = ntohs(dp->th_opcode);
			if (dp->th_opcode == ERROR) {
				printf("Error code %d: %s\n", dp->th_code,
					dp->th_msg);
				goto abort;
			}
			if (dp->th_opcode == DATA) {
				int j;

				if (waitforoack) {
					if (trace)
						printf("server does not know "
						       "about RFC1782; reset"
						       "ting blocksize\n");
					waitforoack = 0;
					segsize = 512;
				}
				dp->th_block = ntohs(dp->th_block);
				if (dp->th_block == block) {
					break;          /* have next packet */
				}
				/* On an error, try to synchronize
				 * both sides.
				 */
				j = synchnet(f);
				if (j && trace) {
					printf("discarded %d packets\n", j);
				}
				if (dp->th_block == (block-1)) {
					goto send_ack;  /* resend ack */
				}
			}
			else if (dp->th_opcode == OACK) {
				if (block != 1 || !waitforoack) {
					printf("protocol violation\n");
					longjmp(toplevel, -1);
				}
				waitforoack = 0;
				parseoack(&dp->th_stuff, n - 2);
				ap->th_opcode = htons((u_short)ACK);
				ap->th_block = htons(0);
				size = 4;
				goto send_ack;
			}
		}
		/* size = write(fd, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, convert);
		if (size < 0) {
			nak(errno + 100);
			break;
		}
		amount += size;
	} while (size == segsize);
abort:                                          /* ok to ack, since user */
	ap->th_opcode = htons((u_short)ACK);    /* has seen err msg */
	ap->th_block = htons(block);
	(void) sendto(f, ackbuf, 4, 0, (struct sockaddr *)&sin, sizeof (sin));
	write_behind(file, convert);            /* flush last buffer */
	fclose(file);
	stopclock();
	if (amount > 0)
		printstats("Received", amount);
}

makerequest(request, name, tp, mode)
	int request;
	char *name, *mode;
	struct tftphdr *tp;
{
	register char *cp;

	tp->th_opcode = htons((u_short)request);
	cp = tp->th_stuff;
	strcpy(cp, name);
	cp += strlen(name);
	*cp++ = '\0';
	strcpy(cp, mode);
	cp += strlen(mode);
	*cp++ = '\0';
	strcpy(cp, "blksize");
	cp += 7;
	*cp++ = '\0';
	sprintf(cp, "%d", segsize);
	cp += strlen(cp) + 1;
	return (cp - (char *)tp);
}

struct errmsg {
	int	e_code;
	const char	*e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ -1,		0 }
};

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
nak(error)
	int error;
{
	register struct tftphdr *tp;
	int length;
	register struct errmsg *pe;
/*	extern char *sys_errlist[]; */

	tp = (struct tftphdr *)ackbuf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((u_short)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = sys_errlist[error - 100];
		tp->th_code = EUNDEF;
	}
	strcpy(tp->th_msg, pe->e_msg);
	length = strlen(pe->e_msg) + 4;
	if (trace)
		tpacket("sent", tp, length);
	if (sendto(f, ackbuf, length, 0, (struct sockaddr *)&sin, sizeof (sin))
	    != length)
		perror("nak");
}

topts(cp, sz)
	char *cp;
	int sz;
{
	int n, i = 0;
	
	while (sz > 0 && *cp) {
		n = strnlen(cp, sz);
		if (n > 0) {
			printf("%s%s=", i++ ? ", " : "", cp);
			cp += n + 1;
			sz -= n + 1;
			if (sz <= 0)
				break;
			n = strnlen(cp, sz);
			if (n > 0)
				printf("%s", cp);
		}
		cp += n + 1;
		sz -= n + 1;
	}
}

tpacket(s, tp, n)
	char *s;
	struct tftphdr *tp;
	int n;
{
	static char *opcodes[] =
	   { "#0", "RRQ", "WRQ", "DATA", "ACK", "ERROR", "OACK" };
	register char *cp, *file;
	u_short op = ntohs(tp->th_opcode);
	char *index();

	if (op < RRQ || op > OACK)
		printf("%s opcode=%x ", s, op);
	else
		printf("%s %s ", s, opcodes[op]);
	switch (op) {

	case RRQ:
	case WRQ:
		n -= 2;
		file = cp = tp->th_stuff;
		cp = index(cp, '\0');
		printf("<file=%s, mode=%s, opts: ", file, cp + 1);
		topts(index(cp + 1, '\000') + 1, n - strlen(file)
		      - strlen(cp + 1) - 2);
		printf(">\n");
		break;

	case DATA:
		printf("<block=%d, %d bytes>\n", ntohs(tp->th_block), n - 4);
		break;

	case ACK:
		printf("<block=%d>\n", ntohs(tp->th_block));
		break;

	case ERROR:
		printf("<code=%d, msg=%s>\n", ntohs(tp->th_code), tp->th_msg);
		break;
	case OACK:
		printf("<");
		topts(tp->th_stuff, n - 2);
		printf(">\n");
		break;
	}
}

struct timeval tstart;
struct timeval tstop;
struct timezone zone;

startclock() {
	gettimeofday(&tstart, &zone);
}

stopclock() {
	gettimeofday(&tstop, &zone);
}

printstats(direction, amount)
char *direction;
unsigned long amount;
{
	double delta;
			/* compute delta in 1/10's second units */
	delta = ((tstop.tv_sec*10.)+(tstop.tv_usec/100000)) -
		((tstart.tv_sec*10.)+(tstart.tv_usec/100000));
	delta = delta/10.;      /* back to seconds */
	printf("%s %ld bytes in %.1f seconds", direction, amount, delta);
	if ((verbose) && (delta >= 0.1))
			printf(" [%.0f bits/sec]", (amount*8.)/delta);
	putchar('\n');
}

