/*
 *	Port 9100+n daemon
 *	Accepts a connection from port 9100+n and copy stream to
 *	/dev/lpn, where n = 0,1,2.
 *
 *	Run standalone as: p910nd [0|1|2]
 *
 *	Run under inetd as:
 *	p910n stream tcp nowait root /usr/sbin/tcpd p910nd [0|1|2]
 *	 where p910n is an /etc/services entry for
 *	 port 9100, 9101 or 9102 as the case may be.
 *	 root can be replaced by any uid with rw permission on /dev/lpn
 *
 *	Port 9100+n will then be passively opened
 *	n defaults to 0
 *
 *	Version 0.8
 *	Allow specifying address to bind to
 *
 *	Version 0.7
 *	Bidirectional data transfer
 *
 *	Version 0.6
 *	Arne Bernin fixed some cast warnings, corrected the version number
 *	and added a -v option to print the version.
 *
 *	Version 0.5
 *	-DUSE_LIBWRAP and -lwrap enables hosts_access (tcpwrappers) checking.
 *
 *	Version 0.4
 *	Ken Yap (ken_yap@users.sourceforge.net), April 2001
 *	Placed under GPL.
 *
 *	Added -f switch to specify device which overrides /dev/lpn.
 *	But number is still required get distinct ports and locks.
 *
 *	Added locking so that two invocations of the daemon under inetd
 *	don't try to open the printer at the same time. This can happen
 *	even if there is one host running clients because the previous
 *	client can exit after it has sent all data but the printer has not
 *	finished printing and inetd starts up a new daemon when the next
 *	request comes in too soon.
 *
 *	Various things could be Linux specific. I don't
 *	think there is much demand for this program outside of PCs,
 *	but if you port it to other distributions or platforms,
 *	I'd be happy to receive your patches.
 */

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<getopt.h>
#include	<ctype.h>
#include	<string.h>
#include	<fcntl.h>
#include	<netdb.h>
#include	<syslog.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/resource.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>

#ifdef	USE_LIBWRAP
#include	"tcpd.h"
int		allow_severity, deny_severity;
extern		int hosts_ctl(char *daemon, char *client_name,
		char *client_addr, char *client_user);
#endif

#define		BASEPORT	9100
#define		PIDFILE		"/var/run/p910%cd.pid"
#ifdef		LOCKFILE_DIR
#define		LOCKFILE	LOCKFILE_DIR "/p910%cd"
#else
#define		LOCKFILE	"/var/lock/subsys/p910%cd"
#endif
#define		PRINTERFILE	"/dev/lp%c"
#define		LOGOPTS		LOG_ERR

static char	*progname;
static char	version[] = "p910nd Version 0.8";
static int	lockfd = -1;
static char	*device = 0;
static int	bidir = 0;
static char	*bindaddr = 0;

void usage(void)
{
	fprintf(stderr, "Usage: %s [-f device] [-i bindaddr] [-bv] [0|1|2]\n", progname);
	exit(1);
}

void show_version (void)
{
	fprintf(stdout, "%s \n", version);
}

FILE *open_printer(int lpnumber)
{
	FILE		*f;
	char		lpname[sizeof(PRINTERFILE)];

#ifdef	TESTING
	(void)snprintf(lpname, sizeof(lpname), "/dev/tty");
#else
	(void)snprintf(lpname, sizeof(lpname), PRINTERFILE, lpnumber);
#endif
	if (device == 0)
		device = lpname;
	if ((f = fopen(device, bidir ? "w+" : "w")) == NULL)
	{
		syslog(LOGOPTS, "%s: %m\n", device);
		exit(1);
	}
	return (f);
}

int get_lock(int lpnumber)
{
	char		lockname[sizeof(LOCKFILE)];
	struct flock	lplock;

	(void)snprintf(lockname, sizeof(lockname), LOCKFILE, lpnumber);
	if ((lockfd = open(lockname, O_CREAT|O_RDWR)) < 0)
	{
		syslog(LOGOPTS, "%s: %m\n", lockname);
		return (0);
	}
	memset(&lplock, 0, sizeof(lplock));
	lplock.l_type = F_WRLCK;
	lplock.l_pid = getpid();
	if (fcntl(lockfd, F_SETLKW, &lplock) < 0)
	{
		syslog(LOGOPTS, "%s: %m\n", lockname);
		return (0);
	}
	return (1);
}

void free_lock(void)
{
	if (lockfd >= 0)
		(void)close(lockfd);
}

/* Copy network socket to FILE f until EOS */
int copy_stream(int fd, FILE *f)
{
	int		nread;
	char		buffer[8192];

	if (bidir) {
		FILE	*nf;

		if ((nf = fdopen(fd, "w")) == NULL) {
			syslog(LOGOPTS, "fdopen: %m\n");
		}
		for (;;) {
			fd_set	readfds;
			int result;
			int maxfd = fileno(f) > fd ? fileno(f) : fd;
			FD_ZERO(&readfds);
			FD_SET(fileno(f), &readfds);
			FD_SET(fd, &readfds);
			result = select(maxfd + 1, &readfds, 0, 0, 0);
			if (result < 0)
				return (result);
			if (result == 0)
				continue;
			if (FD_ISSET(fd, &readfds)) {
				nread = read(fd, buffer, sizeof(buffer));
				if (nread <= 0)
					break;
				(void)fwrite(buffer, sizeof(char), nread, f);
			}
			if (FD_ISSET(fileno(f), &readfds)) {
				nread = read(fileno(f), buffer, sizeof(buffer));
				if (nread > 0 && nf != NULL) {
					(void)fwrite(buffer, sizeof(char), nread, nf);
					(void)fflush(nf);
				}
			}
		}
		(void)fflush(f);
		(void)fclose(nf);
		return (0);
	} else {
		while ((nread = read(fd, buffer, sizeof(buffer))) > 0)
			(void)fwrite(buffer, sizeof(char), nread, f);
		(void)fflush(f);
		return (nread);
	}
}

void one_job(int lpnumber)
{
	FILE		*f;
	struct sockaddr_in	client;
	socklen_t	clientlen = sizeof(client);

	if (getpeername(0, (struct sockaddr*) &client, &clientlen) >= 0)
		syslog(LOGOPTS, "Connection from %s port %hu\n",
			inet_ntoa(client.sin_addr),
			ntohs(client.sin_port));
	if (get_lock(lpnumber) == 0)
		return;
	f = open_printer(lpnumber);
	if (copy_stream(0, f) < 0)
		syslog(LOGOPTS, "copy_stream: %m\n");
	fclose(f);
	free_lock();
}

void server(int lpnumber)
{
	struct rlimit	resourcelimit;
#ifdef	USE_GETPROTOBYNAME
	struct protoent	*proto;
#endif
	int		netfd, fd, one = 1;
	socklen_t	clientlen;
	struct sockaddr_in	netaddr, client;
	char		pidfilename[sizeof(PIDFILE)];
	FILE		*f;
	int		ipret;

#ifndef	TESTING
	switch (fork())
	{
	case -1:
		syslog(LOGOPTS, "fork: %m\n");
		exit (1);
	case 0:		/* child */
		break;
	default:	/* parent */
		exit(0);
	}
	/* Now in child process */
	resourcelimit.rlim_max = 0;
	if (getrlimit(RLIMIT_NOFILE, &resourcelimit) < 0)
	{
		syslog(LOGOPTS, "getrlimit: %m\n");
		exit(1);
	}
	for (fd = 0; fd < resourcelimit.rlim_max; ++fd)
		(void)close(fd);
	if (setsid() < 0)
	{
		syslog(LOGOPTS, "setsid: %m\n");
		exit(1);
	}
	(void)chdir("/");
	(void)umask(022);
	fd = open("/dev/null", O_RDWR);	/* stdin */
	(void)dup(fd);			/* stdout */
	(void)dup(fd);			/* stderr */
	(void)snprintf(pidfilename, sizeof(pidfilename), PIDFILE, lpnumber);
	if ((f = fopen(pidfilename, "w")) == NULL)
	{
		syslog(LOGOPTS, "%s: %m\n", pidfilename);
		exit(1);
	}
	(void)fprintf(f, "%d\n", getpid());
	(void)fclose(f);
	if (get_lock(lpnumber) == 0)
		exit(1);
#endif
	f = open_printer(lpnumber);
#ifdef	USE_GETPROTOBYNAME
	if ((proto = getprotobyname("tcp")) == NULL)
	{
		syslog(LOGOPTS, "Cannot find protocol for TCP!\n");
		exit(1);
	}
	if ((netfd = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0)
#else
	if ((netfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
#endif
	{
		syslog(LOGOPTS, "socket: %m\n");
		exit(1);
	}
	if (setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
	{
		syslog(LOGOPTS, "setsocketopt: %m\n");
		exit(1);
	}
	netaddr.sin_port = htons(BASEPORT + lpnumber - '0');
	if (bindaddr == 0) {
		netaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		ipret = inet_pton(AF_INET, bindaddr, &netaddr.sin_addr.s_addr);
		if (ipret < 0) {
			syslog(LOGOPTS, "inet_pton: %m\n");
			exit(1);
		} else if (ipret == 0) {
			syslog(LOGOPTS, "inet_pton: invalid bind IP address\n");
			exit(1);
		}
	}
	memset(netaddr.sin_zero, 0, sizeof(netaddr.sin_zero));
	if (bind(netfd, (struct sockaddr*) &netaddr, sizeof(netaddr)) < 0)
	{
		syslog(LOGOPTS, "bind: %m\n");
		exit(1);
	}
	if (listen(netfd, 5) < 0)
	{
		syslog(LOGOPTS, "listen: %m\n");
		exit(1);
	}
	clientlen = sizeof(client);
	memset(&client, 0, sizeof(client));
	while ((fd = accept(netfd, (struct sockaddr*) &client, &clientlen)) >= 0)
	{
#ifdef	USE_LIBWRAP
		if (hosts_ctl("p910nd", STRING_UNKNOWN,
			inet_ntoa(client.sin_addr), STRING_UNKNOWN) == 0) {
			syslog(LOGOPTS, "Connection from %s port %hd rejected\n",
				inet_ntoa(client.sin_addr),
				ntohs(client.sin_port));
			close(fd);
			continue;
		}
#endif
		syslog(LOGOPTS, "Connection from %s port %hd accepted\n",
			inet_ntoa(client.sin_addr),
			ntohs(client.sin_port));
		/*write(fd, "Printing", 8);*/
		if (copy_stream(fd, f) < 0)
			syslog(LOGOPTS, "copy_stream: %m\n");
		(void)close(fd);
	}
	syslog(LOGOPTS, "accept: %m\n");
	free_lock();
	exit(1);
}

int is_standalone(void)
{
	struct sockaddr_in	bind_addr;
	socklen_t		ba_len;

	/*
	 * Check to see if a socket was passed to us from inetd.
	 *
	 * Use getsockname() to determine if descriptor 0 is indeed a socket
	 * (and thus we are probably a child of inetd) or if it is instead
	 * something else and we are running standalone.
	 */
	ba_len = sizeof(bind_addr);
	if (getsockname(0, (struct sockaddr*) &bind_addr, &ba_len) == 0)
		return (0);		/* under inetd */
	if (errno != ENOTSOCK)		/* strange... */
		syslog(LOGOPTS, "getsockname: %m\n");
	return (1);
}

int main(int argc, char *argv[])
{
	int		c, lpnumber;
	char		*p;

	if (argc <= 0)		/* in case not provided in inetd.conf */
		progname = "p910nd";
	else
	{
		progname = argv[0];
		if ((p = strrchr(progname, '/')) != 0)
			progname = p + 1;
	}
	lpnumber = '0';
	while ((c = getopt(argc, argv, "bi:f:v")) != EOF)
	{
		switch (c)
		{
		case 'b':
			bidir = 1;
			break;
		case 'f':
			device = optarg;
			break;
		case 'i':
			bindaddr = optarg;
			break;
		case 'v':
	      	        show_version();
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0)
	{
		if (isdigit(argv[0][0]))
			lpnumber = argv[0][0];
	}
	/* change the n in argv[0] to match the port so ps will show that */
	if ((p = strstr(progname, "p910n")) != NULL)
		p[4] = lpnumber;
	
	/* We used to pass (LOG_PERROR|LOG_PID|LOG_LPR|LOG_ERR) to syslog, but
	 * syslog ignored the LOG_PID and LOG_PERROR option.  I.e. the intention
	 * was to add both options but the effect was to have neither.
	 * I disagree with the intention to add PERROR.	 --Stef	 */
	openlog (p, LOG_PID, LOG_LPR);
	if (is_standalone())
		server(lpnumber);
	else
		one_job(lpnumber);
	return (0);
}
