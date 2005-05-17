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
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tftpd.c	5.8 (Berkeley) 6/18/88";
#endif /* not lint */

/*
 * Trivial file transfer protocol server.
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 *
 * Further modifications by Markus Gutschke <gutschk@math.uni-muenster.de>
 *  - RFC1782 option parsing
 *  - RFC1783 extended blocksize
 *  - "-c" option for changing the root directory
 *  - "-d" option for debugging output
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <arpa/tftp.h>

#include <alloca.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <setjmp.h>
#include <syslog.h>

#define	TIMEOUT		5

#ifndef	OACK
#define	OACK	06
#endif

#ifndef EOPTNEG
#define	EOPTNEG	8
#endif

extern	int errno;
struct	sockaddr_in sin = { AF_INET };
int	peer;
int	rexmtval = TIMEOUT;
int	maxtimeout = 5*TIMEOUT;

#define	PKTSIZE	(1432+4) /* SEGSIZE+4 */
int	segsize = SEGSIZE;
char	buf[PKTSIZE];
char	ackbuf[PKTSIZE];
struct	sockaddr_in from;
int	fromlen;

char	*rootdir = NULL;
int	debug = 0;

struct filters {
	struct filters *next;
	char           *fname;
} *filters = NULL;
int     isfilter = 0;

main(argc, argv)
	char *argv[];
{
	register struct tftphdr *tp;
	register int n;
	int on = 1;
	extern int optind;
	extern char *optarg;

	openlog(argv[0], LOG_PID, LOG_DAEMON);

	while ((n = getopt(argc, argv, "c:dr:")) >= 0) {
		switch (n) {
		case 'c':
			if (rootdir)
				goto usage;
			rootdir = optarg;
			break;
		case 'd':
			debug++;
			break;
		case 'r': {
			struct filters *fp = (void *)
				             malloc(sizeof(struct filters) +
						    strlen(optarg) + 1);
			fp->next  = filters;
			fp->fname = (char *)(fp + 1);
			strcpy(fp->fname, optarg);
			filters   = fp;
			break; }
		default:
		usage:
			syslog(LOG_ERR, "Usage: %s [-c chroot] "
			       "[-r readfilter] [-d]\n",
			       argv[0]);
			exit(1);
		}
	}
	if (argc-optind != 0)
		goto usage;

	ioctl(0, FIONBIO, &on);
/*	if (ioctl(0, FIONBIO, &on) < 0) {
		syslog(LOG_ERR, "ioctl(FIONBIO): %m\n");
		exit(1);
	}
*/
	fromlen = sizeof (from);
	n = recvfrom(0, buf, segsize+4, 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		syslog(LOG_ERR, "recvfrom: %m\n");
		exit(1);
	}
	/*
	 * Now that we have read the message out of the UDP
	 * socket, we fork and exit.  Thus, inetd will go back
	 * to listening to the tftp port, and the next request
	 * to come in will start up a new instance of tftpd.
	 *
	 * We do this so that inetd can run tftpd in "wait" mode.
	 * The problem with tftpd running in "nowait" mode is that
	 * inetd may get one or more successful "selects" on the
	 * tftp port before we do our receive, so more than one
	 * instance of tftpd may be started up.  Worse, if tftpd
	 * break before doing the above "recvfrom", inetd would
	 * spawn endless instances, clogging the system.
	 */
	{
		int pid;
		int i, j;

		for (i = 1; i < 20; i++) {
		    pid = fork();
		    if (pid < 0) {
				sleep(i);
				/*
				 * flush out to most recently sent request.
				 *
				 * This may drop some request, but those
				 * will be resent by the clients when
				 * they timeout.  The positive effect of
				 * this flush is to (try to) prevent more
				 * than one tftpd being started up to service
				 * a single request from a single client.
				 */
				j = sizeof from;
				i = recvfrom(0, buf, segsize+4, 0,
				    (struct sockaddr *)&from, &j);
				if (i > 0) {
					n = i;
					fromlen = j;
				}
		    } else {
				break;
		    }
		}
		if (pid < 0) {
			syslog(LOG_ERR, "fork: %m\n");
			exit(1);
		} else if (pid != 0) {
			exit(0);
		}
	}
	from.sin_family = AF_INET;
	alarm(0);
	close(0);
	close(1);
	peer = socket(AF_INET, SOCK_DGRAM, 0);
	if (peer < 0) {
		syslog(LOG_ERR, "socket: %m\n");
		exit(1);
	}
	if (bind(peer, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		syslog(LOG_ERR, "bind: %m\n");
		exit(1);
	}
	if (connect(peer, (struct sockaddr *)&from, sizeof(from)) < 0) {
		syslog(LOG_ERR, "connect: %m\n");
		exit(1);
	}
	tp = (struct tftphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	if (tp->th_opcode == RRQ || tp->th_opcode == WRQ)
		tftp(tp, n);
	exit(1);
}

int	validate_access();
int	sendfile(), recvfile();

struct formats {
	char	*f_mode;
	int	(*f_validate)();
	int	(*f_send)();
	int	(*f_recv)();
	int	f_convert;
} formats[] = {
	{ "netascii",	validate_access,	sendfile,	recvfile, 1 },
	{ "octet",	validate_access,	sendfile,	recvfile, 0 },
#ifdef notdef
	{ "mail",	validate_user,		sendmail,	recvmail, 1 },
#endif
	{ 0 }
};

int	set_blksize();

struct options {
	char	*o_opt;
	int	(*o_fnc)();
} options[] = {
	{ "blksize",	set_blksize },
	{ 0 }
};

/*
 * Set a non-standard block size (c.f. RFC1783)
 */

set_blksize(val, ret)
	char *val;
	char **ret;
{
	static char b_ret[5];
	int sz = atoi(val);

	if (sz < 8) {
		if (debug)
			syslog(LOG_ERR, "Requested packetsize %d < 8\n", sz);
		return(0);
	} else if (sz > PKTSIZE-4) {
		if (debug)
			syslog(LOG_INFO, "Requested packetsize %d > %d\n",
			       sz, PKTSIZE-4);
		sz = PKTSIZE-4;
	} else if (debug)
		syslog(LOG_INFO, "Adjusted packetsize to %d octets\n", sz);
	
	segsize = sz;
	sprintf(*ret = b_ret, "%d", sz);
	return(1);
}

/*
 * Parse RFC1782 style options
 */

do_opt(opt, val, ap)
	char *opt;
	char *val;
	char **ap;
{
	struct options *po;
	char *ret;

	for (po = options; po->o_opt; po++)
		if (strcasecmp(po->o_opt, opt) == 0) {
			if (po->o_fnc(val, &ret)) {
				if (*ap + strlen(opt) + strlen(ret) + 2 >=
				    ackbuf + sizeof(ackbuf)) {
					if (debug)
						syslog(LOG_ERR,
						       "Ackbuf overflow\n");
					nak(ENOSPACE);
					exit(1);
				}
				*ap = strrchr(strcpy(strrchr(strcpy(*ap, opt),
							     '\000')+1, val),
					      '\000')+1;
			} else {
				nak(EOPTNEG);
				exit(1);
			}
			break;
		}
	if (debug && !po->o_opt)
		syslog(LOG_WARNING, "Unhandled option: %d = %d\n", opt, val);
	return;
}

/*
 * Handle initial connection protocol.
 */
tftp(tp, size)
	struct tftphdr *tp;
	int size;
{
	register char *cp;
	int argn = 0, ecode;
	register struct formats *pf;
	char *filename, *mode;
	char *val, *opt;
	char *ap = ackbuf+2;
	int  isopts;

	((struct tftphdr *)ackbuf)->th_opcode = ntohs(OACK);
	filename = cp = tp->th_stuff;
again:
	while (cp < buf + size) {
		if (*cp == '\0')
			break;
		cp++;
	}
	if (*cp != '\0') {
		if (debug)
			syslog(LOG_WARNING, "Received illegal request\n");
		nak(EBADOP);
		exit(1);
	}
	if (!argn++) {
		mode = ++cp;
		goto again;
	} else {
		if (debug && argn == 3)
			syslog(LOG_INFO, "Found RFC1782 style options\n");
		*(argn & 1 ? &val : &opt) = ++cp;
		if (argn & 1)
			do_opt(opt, val, &ap);
		if (cp < buf + size && *cp != '\000')
			goto again;
	}
	
	for (cp = mode; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	for (pf = formats; pf->f_mode; pf++)
		if (strcmp(pf->f_mode, mode) == 0)
			break;
	if (pf->f_mode == 0) {
		if (debug)
			syslog(LOG_WARNING, "Unknown data format: %s\n", mode);
		nak(EBADOP);
		exit(1);
	}

	if (rootdir) {
		cp = alloca(strlen(rootdir) + strlen(filename) + 1);
		if (cp == NULL) {
			nak(100+ENOMEM);
			exit(1);
		}
		if (*filename != '/') {
			if (debug)
				syslog(LOG_ERR,
				       "Filename has to be absolute: %s\n",
				       filename);
			nak(EACCESS);
			exit(1);
		}
		filename = strcat(strcpy(cp, rootdir), filename);
	}
	
	ecode = (*pf->f_validate)(filename, tp->th_opcode);
	if (ecode) {
		nak(ecode, ERROR);
		exit(1);
	}
	isopts = ap != (ackbuf+2);
	(tp->th_opcode == WRQ ? *pf->f_recv : *pf->f_send)
		(pf, isopts ? ackbuf : NULL, isopts ? ap-ackbuf : 0);
	exit(0);
}


FILE *file;

/*
 * Validate file access.  Since we
 * have no uid or gid, for now require
 * file to exist and be publicly
 * readable/writable.
 * Note also, full path name must be
 * given as we have no login directory.
 */
validate_access(filename, mode)
	char *filename;
	int mode;
{
	struct stat stbuf;
	int	fd;
	char	*cp;

	isfilter = 0;
	if (mode == RRQ) {
		struct filters *fp = filters;
		for (; fp; fp = fp->next) {
			if (!strcmp(fp->fname,
				    filename +
				    (rootdir ? strlen(rootdir) : 0))) {
				if (debug)
					syslog(LOG_INFO, "Opening input "
					       "filter: %s\n", filename);
				if ((file = popen(filename, "r")) == NULL) {
					syslog(LOG_ERR, "Failed to open input "
					       "filter\n");
					return (EACCESS); }
				fd = fileno(file);
				isfilter = 1;
				return (0);
			}
		}
	}
				       
	if (*filename != '/') {
		if (debug)
			syslog(LOG_ERR, "Filename has to be absolute: %s\n",
			       filename);
		return (EACCESS);
	}
	for (cp = filename; *cp; cp++)
		if (*cp == '~' || *cp == '$' ||
		    (*cp == '/' && cp[1] == '.' && cp[2] == '.')) {
			if (debug)
				syslog(LOG_ERR, "Illegal filename: %s\n",
				       filename);
			return (EACCESS);
		}
	if (debug)
		syslog(LOG_INFO, "Validating \"%s\" for %sing\n",
		       filename, mode == RRQ ? "read" : "writ");
	if (stat(filename, &stbuf) < 0)
		return (errno == ENOENT ? ENOTFOUND : EACCESS);
	if (mode == RRQ) {
		if ((stbuf.st_mode&(S_IREAD >> 6)) == 0)
			return (EACCESS);
	} else {
		if ((stbuf.st_mode&(S_IWRITE >> 6)) == 0)
			return (EACCESS);
	}
	fd = open(filename, mode == RRQ ? 0 : 1);
	if (fd < 0)
		return (errno + 100);
	file = fdopen(fd, (mode == RRQ)? "r":"w");
	if (file == NULL) {
		return errno+100;
	}
	return (0);
}

int	timeout;
jmp_buf	timeoutbuf;

void timer(int sig)
{

	timeout += rexmtval;
	if (timeout >= maxtimeout) {
		if (debug)
			syslog(LOG_WARNING, "Timeout!\n");
		exit(1);
	}
	longjmp(timeoutbuf, 1);
}

/*
 * Send the requested file.
 */
sendfile(pf, oap, oacklen)
	struct formats *pf;
	struct tftphdr *oap;
	int oacklen;
{
	struct tftphdr *dp, *r_init();
	register struct tftphdr *ap;    /* ack packet */
	register int size, n;
	u_short block = 1;

	signal(SIGALRM, timer);

	ap = (struct tftphdr *)ackbuf;

	if (oap) {
		timeout = 0;
		(void) setjmp(timeoutbuf);
	oack:
		if (send(peer, oap, oacklen, 0) != oacklen) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				syslog(LOG_ERR, "tftpd: read: %m\n");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs(ap->th_block);
			
			if (ap->th_opcode == ERROR) {
				if (debug)
					syslog(LOG_ERR, "Client does not "
					       "accept options\n");
				goto abort; }
			
			if (ap->th_opcode == ACK) {
				if (ap->th_block == 0) {
					if (debug)
						syslog(LOG_DEBUG,
						       "RFC1782 option "
						       "negotiation "
						       "succeeded\n");
					break;
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				goto oack;
			}
		}
	}
	
	dp = r_init();
	do {
		size = readit(file, &dp, pf->f_convert);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((u_short)DATA);
		dp->th_block = htons(block);
		timeout = 0;
		(void) setjmp(timeoutbuf);

send_data:
		if (send(peer, dp, size + 4, 0) != size + 4) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		read_ahead(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);        /* read the ack */
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				syslog(LOG_ERR, "tftpd: read: %m\n");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs(ap->th_block);

			if (ap->th_opcode == ERROR)
				goto abort;
			
			if (ap->th_opcode == ACK) {
				if (ap->th_block == block) {
					break;
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (ap->th_block == (block -1)) {
					goto send_data;
				}
			}

		}
		block++;
	} while (size == segsize);
abort:
	if (isfilter)
		pclose(file);
	else
		(void) fclose(file);
	isfilter = 0;
}

void justquit(int sig)
{
	exit(0);
}


/*
 * Receive a file.
 */
recvfile(pf, oap, oacklen)
	struct formats *pf;
	struct tftphdr *oap;
	int oacklen;
{
	struct tftphdr *dp, *w_init();
	register struct tftphdr *ap;    /* ack buffer */
	register int acksize, n, size;
	u_short block = 0;

	signal(SIGALRM, timer);
	dp = w_init();
	do {
		timeout = 0;

		if (!block++ && oap) {
			ap = (struct tftphdr *)oap;
			acksize = oacklen;
		} else {
			ap = (struct tftphdr *)ackbuf;
			ap->th_opcode = htons((u_short)ACK);
			ap->th_block = htons(block-1);
			acksize = 4;
		}
		(void) setjmp(timeoutbuf);
send_ack:
		if (send(peer, (char *)ap, acksize, 0) != acksize) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, dp, segsize+4, 0);
			alarm(0);
			if (n < 0) {            /* really? */
				syslog(LOG_ERR, "tftpd: read: %m\n");
				goto abort;
			}
			dp->th_opcode = ntohs((u_short)dp->th_opcode);
			dp->th_block = ntohs(dp->th_block);
			if (dp->th_opcode == ERROR)
				goto abort;
			if (dp->th_opcode == DATA) {
				if (dp->th_block == block) {
					break;   /* normal */
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (dp->th_block == (block-1))
					goto send_ack;          /* rexmit */
			}
		}
		/*  size = write(file, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, pf->f_convert);
		if (size != (n-4)) {                    /* ahem */
			if (size < 0) nak(errno + 100);
			else nak(ENOSPACE);
			goto abort;
		}
	} while (size == segsize);
	write_behind(file, pf->f_convert);
	if (isfilter)
		pclose(file);
	else
		(void) fclose(file);            /* close data file */
	isfilter = 0;

	ap = (struct tftphdr *)ackbuf;
	ap->th_opcode = htons((u_short)ACK);    /* send the "final" ack */
	ap->th_block = htons(block);
	(void) send(peer, ackbuf, 4, 0);

	signal(SIGALRM, justquit);      /* just quit on timeout */
	alarm(rexmtval);
	n = recv(peer, buf, segsize, 0); /* normally times out and quits */
	alarm(0);
	if (n >= 4 &&                   /* if read some data */
	    dp->th_opcode == DATA &&    /* and got a data block */
	    block == dp->th_block) {	/* then my last ack was lost */
		(void) send(peer, ackbuf, 4, 0);     /* resend final ack */
	}
abort:
	return;
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
	{ EOPTNEG,	"Failure to negotiate RFC1782 options" },
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
/*	extern char *sys_errlist[];	*/

	tp = (struct tftphdr *)buf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((u_short)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = sys_errlist[error -100];
		tp->th_code = EUNDEF;   /* set 'undef' errorcode */
	}
	strcpy(tp->th_msg, pe->e_msg);
	length = strlen(pe->e_msg);
	tp->th_msg[length] = '\0';
	length += 5;
	if (debug)
		syslog(LOG_ERR, "Negative acknowledge: %s\n", tp->th_msg);
	if (send(peer, buf, length, 0) != length)
		syslog(LOG_ERR, "nak: %m\n");
}
