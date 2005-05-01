#include "string.h"
#include "resolv.h"
#include "etherboot.h" /* for arptable */
#include "url.h"

/*
 * Parse a URL and deduce a struct protocol *, a struct sockaddr_in
 * and a char *filename.
 *
 * We accept URLs of the form
 *
 *   [protocol://[host][:port]/]path/to/file
 *
 * Returns 1 for success, 0 for failure (e.g. unknown protocol).
 *
 */
int parse_url ( char *url, struct protocol **proto,
		struct sockaddr_in *server, char **filename ) {
	char *p;
	char *protocol = NULL;
	char *host = NULL;
	char *port = NULL;
	int rc = 0;

	DBG ( "URL parsing \"%s\"\n", url );

	/* If no protocol is present, the whole URL will be a filename */
	*filename = url;

	/* Search for a protocol delimiter.  If found, parse out the
	 * host and port parts of the URL, inserting NULs to terminate
	 * the different sections.
	 */
	for ( p = url ; *p ; p++ ) {
		if ( memcmp ( p, "://", 3 ) != 0 )
			continue;

		/* URL has an explicit protocol */
		*p = '\0';
		p += 3;
		protocol = url;
		host = p;

		/* Search for port and file delimiters */
		for ( ; *p ; p++ ) {
			if ( *p == ':' ) {
				*p = '\0';
				port = p + 1;
				continue;
			}
			if ( *p == '/' ) {
				*(p++) = '\0';
				break;
			}
		}
		*filename = p;

		break;
	}
	DBG ( "URL protocol \"%s\" host \"%s\" port \"%s\" file \"%s\"\n",
	      protocol ? protocol : "(default)", host ? host : "(default)",
	      port ? port : "(default)", *filename );

	/* Identify the protocol */
	*proto = identify_protocol ( protocol );
	if ( ! *proto ) {
		DBG ( "URL unknown protocol \"%s\"\n",
		      protocol ? protocol : "(default)" );
		goto out;
	}

	/* Identify the host */
	server->sin_addr = arptable[ARP_SERVER].ipaddr;
	if ( host && host[0] ) {
		if ( ! resolv ( &server->sin_addr, host ) ) {
			DBG ( "URL unknown host \"%s\"\n", host );
			goto out;
		}
	}

	/* Identify the port */
	server->sin_port = (*proto)->default_port;
	if ( port && port[0] ) {
		server->sin_port = strtoul ( port, NULL, 10 );
	}

	rc = 1;

 out:
	/* Fill back in the original URL */
	if ( protocol ) {
		(*filename)[-1] = '/';
		if ( port )
			port[-1] = ':';
		host[-3] = ':';
	}
	return rc;
}
