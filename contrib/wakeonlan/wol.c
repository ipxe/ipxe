/*****************************************************************************
 *
 * wol.c - Wake-On-LAN utility to wake a networked PC
 *
 * by R. Edwards (bob@cs.anu.edu.au), January 2000
 * (in_ether routine adapted from net-tools-1.51/lib/ether.c by
 * Fred N. van Kempen)
 * added file input, some minor changes for compiling for NetWare
 * added switches -q and -d=<ms>, added Win32 target support
 * by G. Knauf (gk@gknw.de), 30-Jan-2001
 * added switches -b=<bcast> and -p=<port>
 * by G. Knauf (gk@gknw.de), 10-Okt-2001
 * added OS/2 target support
 * by G. Knauf (gk@gknw.de), 24-May-2002
 *
 * This utility allows a PC with WOL configured to be powered on by
 * sending a "Magic Packet" to it's network adaptor (see:
 * http://www.amd.com/products/npd/overview/20212.html).
 * Only the ethernet dest address needs to be given to make this work.
 * Current version uses a UDP broadcast to send out the Magic Packet.
 *
 * compile with: gcc -Wall -o wol wol.c
 * with Solaris: (g)cc -o wol wol.c -lsocket -lnsl
 * with MingW32: gcc -Wall -o wol wol.c -lwsock32
 *
 * usage: wol <dest address>
 * where <dest address> is in [ddd.ddd.ddd.ddd-]xx:xx:xx:xx:xx:xx format.
 * or: wol [-q] [-b=<bcast>] [-p=<port>] [-d=<ms>] -f=<File name>
 * where <File name> is a file containing one dest address per line,
 * optional followed by a hostname or ip separated by a blank.
 * -b sets optional broadcast address, -p sets optional port,
 * -q supresses output, -d=<ms> delays ms milliseconds between sending.
 *
 * Released under GNU Public License January, 2000.
 */

#define VERSION "1.12.2 (c) G.Knauf http://www.gknw.de/"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WATTCP
  #define strncasecmp strnicmp
  #include <ctype.h>
  #include <dos.h>
  #include <tcp.h>
#else
#ifdef WIN32                            /* Win32 platform */
  #define USE_WINSOCKAPI
  #define delay Sleep
  #if (defined(__LCC__) || defined(__BORLANDC__))
    #define strncasecmp strnicmp
  #else
    #define strncasecmp _strnicmp
  #endif
#elif defined(N_PLAT_NLM)               /* NetWare platform */
#ifdef __NOVELL_LIBC__
  #include <ctype.h>
#else
  extern int isdigit(int c);            /* no ctype.h for NW3.x */
  #include <nwthread.h>
  #define strncasecmp strnicmp
#endif
#elif defined(__OS2__)                  /* OS/2 platform */
  #ifdef __EMX__
    #define strncasecmp strnicmp
  #endif
  extern int DosSleep(long t);
  #define delay DosSleep
#else                                   /* all other platforms */
  #define delay(t) usleep(t*1000)
#endif
#ifndef N_PLAT_NLM                      /* ! NetWare platform */
  #include <ctype.h>
#endif
#ifndef WIN32                           /* ! Win32 platform */
  #include <unistd.h>
#endif
#ifdef USE_WINSOCKAPI                   /* Winsock2 platforms */
  #ifdef N_PLAT_NLM                     /* NetWare platform */
    #include <ws2nlm.h>
  #else
    #include <winsock.h>
  #endif
  #define close(s) { \
    closesocket(s); \
    WSACleanup(); \
  }
#else                                   /* Socket platforms */
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #if defined(__OS2__) && !defined(__EMX__)
    #include <utils.h>
  #else
    #include <arpa/inet.h>
  #endif
#endif

#endif

static int read_file (char *destfile);
static int in_ether (char *bufp, unsigned char *addr);
static int send_wol (char *dest, char *host);


char *progname;
int quiet = 0;
int twait = 0;
unsigned int port = 60000;
unsigned long bcast = 0xffffffff;

int main (int argc, char *argv[]) {

    int cmdindx = 0;
    progname    = argv[0];

    if (argc > 1) {
        /* parse input parameters */
        for (argc--, argv++; *argv; argc--, argv++) {
            char *bp;
            char *ep;

            if (strncasecmp (*argv, "-", 1) == 0) {
                if (strncasecmp (*argv, "-F=", 3) == 0) {
                    bp = *argv + 3;
                    read_file (bp);
                } else if (strncasecmp (*argv, "-B=", 3) == 0) {
                    bp = *argv + 3;
                    bcast = inet_addr(bp);
                    if (bcast == -1) {
                        fprintf (stderr, "%s: expected address argument at %s\n", progname, *argv);
                        exit (1);
                    }
                } else if (strncasecmp (*argv, "-D=", 3) == 0) {
                    bp = *argv + 3;
                    twait = strtol (bp, &ep, 0);
                    if (ep == bp || *ep != '\0') {
                        fprintf (stderr, "%s: expected integer argument at %s\n", progname, *argv);
                        exit (1);
                    }
                } else if (strncasecmp (*argv, "-P=", 3) == 0) {
                    bp = *argv + 3;
                    port = strtol (bp, &ep, 0);
                    if (ep == bp || *ep != '\0') {
                        fprintf (stderr, "%s: expected integer argument at %s\n", progname, *argv);
                        exit (1);
                    }
                } else if (strncasecmp (*argv, "-Q", 2) == 0) {
                    quiet = 1;
                } else if (strncasecmp (*argv, "-V", 2) == 0) {
                    fprintf (stderr, "\r%s Version %s\n", progname, VERSION);
                    exit (0);
                } else {
                    fprintf (stderr, "\r%s: invalid or unknown option %s\n", progname, *argv);
                    exit (1);
                }
            } else {
                send_wol (*argv, "");
            }
        cmdindx++;
        }
        return (0);
    } else {
        /* No arguments given -> usage message */
        fprintf (stderr, "\rUsage: %s [-q] [-b=<bcast>] [-p=<port>] [-d=<ms>] -f=<file> | <dest>\n", progname);
        fprintf (stderr, "       need at least hardware address or file option\n");
        return (-1);
    }
}



static int in_ether (char *bufp, unsigned char *addr) {

    char c, *orig;
    int i;
    unsigned char *ptr = addr;
    unsigned val;

    i = 0;
    orig = bufp;
    while ((*bufp != '\0') && (i < 6)) {
        val = 0;
        c = *bufp++;
        if (isdigit(c))
            val = c - '0';
        else if (c >= 'a' && c <= 'f')
            val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            val = c - 'A' + 10;
        else {
#ifdef DEBUG
            fprintf (stderr, "\rin_ether(%s): invalid ether address!\n", orig);
#endif
            errno = EINVAL;
            return (-1);
        }
        val <<= 4;
        c = *bufp;
        if (isdigit(c))
            val |= c - '0';
        else if (c >= 'a' && c <= 'f')
            val |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            val |= c - 'A' + 10;
        else if (c == ':' || c == 0)
            val >>= 4;
        else {
#ifdef DEBUG
            fprintf (stderr, "\rin_ether(%s): invalid ether address!\n", orig);
#endif
            errno = EINVAL;
            return (-1);
        }
        if (c != 0)
            bufp++;
        *ptr++ = (unsigned char) (val & 0377);
        i++;

        /* We might get a semicolon here - not required. */
        if (*bufp == ':') {
            if (i == 6) {
                ;           /* nothing */
            }
            bufp++;
        }
    }
    if (bufp - orig != 17) {
        return (-1);
    } else {
        return (0);
    }
} /* in_ether */


static int read_file (char *destfile) {

    FILE    *pfile = NULL;
    char    dest[64];
    char    host[32];
    char    buffer[512];

    pfile = fopen (destfile, "r+");

    if (pfile) {
        while (fgets (buffer, 511, pfile) != NULL) {
            if (buffer[0] != '#' && buffer[0] != ';') {
                dest[0] = host[0] = '\0';
                sscanf (buffer, "%s %s", dest, host);
                send_wol (dest, host);
            }
        }
        fclose (pfile);
        return (0);
    } else {
        fprintf (stderr, "\r%s: destfile '%s' not found\n", progname, destfile);
        return (-1);
    }
}


static int send_wol (char *dest, char *host) {

    int i, j;
    int packet;
    struct sockaddr_in sap;
    unsigned char ethaddr[8];
    unsigned char *ptr;
    unsigned char buf [128];
    unsigned long bc;
    char mask[32];
    char *tmp;
#ifdef USE_WINSOCKAPI
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
#endif
#ifdef WATTCP
    static udp_Socket sock;
    udp_Socket *s;
#else
    int optval = 1;
#endif

    /* Fetch the broascast address if present. */
    if ((tmp = strstr(dest,"-"))) {
printf("found: %s\n", tmp);
       tmp[0] = 32;
       sscanf (dest, "%s %s", mask, dest);
       bc = inet_addr(mask);
printf("bc: string %s address %08lX\n", mask, bc);
       if (bc == -1) {
           fprintf (stderr, "\r%s: expected address argument at %s\n", progname, mask);
           return (-1);
       }
    } else
       bc = bcast;

    /* Fetch the hardware address. */
    if (in_ether (dest, ethaddr) < 0) {
        fprintf (stderr, "\r%s: invalid hardware address\n", progname);
        return (-1);
    }

#ifdef USE_WINSOCKAPI
    /* I would like to have Socket Vers. 1.1 */
    wVersionRequested = MAKEWORD(1, 1);
    err = WSAStartup (wVersionRequested, &wsaData);
    if (err != 0) {
        fprintf (stderr, "\r%s: couldn't init Winsock Version 1.1\n", progname);
        WSACleanup ();
        return (-1);
    }
#endif

    /* setup the packet socket */
#ifdef WATTCP
    sock_init();
    s = &sock;
    if (!udp_open( s, 0, bc, port, NULL )) {
#else
    if ((packet = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
#endif
        fprintf (stderr, "\r%s: socket failed\n", progname);
#ifdef USE_WINSOCKAPI
        WSACleanup ();
#endif
        return (-1);
    }

#ifndef WATTCP
    /* Set socket options */
    if (setsockopt (packet, SOL_SOCKET, SO_BROADCAST, (char *)&optval, sizeof (optval)) < 0) {
        fprintf (stderr, "\r%s: setsocket failed %s\n", progname, strerror (errno));
        close (packet);
        return (-1);
    }

    /* Set up broadcast address */
    sap.sin_family = AF_INET;
    sap.sin_addr.s_addr = bc;                 /* broadcast address */
    sap.sin_port = htons(port);
#endif

    /* Build the message to send - 6 x 0xff then 16 x dest address */
    ptr = buf;
    for (i = 0; i < 6; i++)
        *ptr++ = 0xff;
    for (j = 0; j < 16; j++)
        for (i = 0; i < 6; i++)
            *ptr++ = ethaddr [i];

    /* Send the packet out */
#ifdef WATTCP
    sock_write( s, buf, 102 );
    sock_close( s );
#else
    if (sendto (packet, (char *)buf, 102, 0, (struct sockaddr *)&sap, sizeof (sap)) < 0) {
        fprintf (stderr, "\r%s: sendto failed, %s\n", progname, strerror(errno));
        close (packet);
        return (-1);
    }
    close (packet);
#endif
    if (!quiet) fprintf (stderr, "\r%s: packet sent to %04X:%08lX-%s %s\n",
            progname, port, (unsigned long)htonl(bc), dest, host);
    if (twait > 0 ) {
        delay (twait);
    }
    return (0);
}

