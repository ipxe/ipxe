#include "stdlib.h"
#include "string.h"
#include "proto.h"
#include "resolv.h"
#include "url.h"

static struct protocol protocols[0] __protocol_start;
static struct protocol default_protocols[0] __default_protocol_start;
static struct protocol protocols_end[0] __protocol_end;

/*
 * Parse protocol portion of a URL.  Return 0 if no "proto://" is
 * present.
 *
 */
static inline int parse_protocol ( struct url_info *info, const char **p ) {
	const char *q = *p;

	info->protocol = q;
	for ( ; *q ; q++ ) {
		if ( memcmp ( q, "://", 3 ) == 0 ) {
			info->protocol_len = q - info->protocol;
			*p = q + 3;
			return 1;
		}
	}
	return 0;
}

/*
 * Parse the host:port portion of a URL.  Also fills in sin_port.
 *
 */
static inline void parse_host_port ( struct url_info *info, const char **p ) {
	info->host = *p;
	for ( ; **p && ( **p != '/' ) ; (*p)++ ) {
		if ( **p == ':' ) {
			info->host_len = *p - info->host;
			info->port = ++(*p);
			info->sin.sin_port = strtoul ( *p, p, 10 );
			info->port_len = *p - info->port;
			return;
		}
	}
	/* No ':' separator seen; it's all the host part */
	info->host_len = *p - info->host;
}

/*
 * Identify the protocol
 *
 */
static inline int identify_protocol ( struct url_info *info ) {
	struct protocol *proto;

	if ( info->protocol_len ) {
		char *terminator;
		char temp;

		/* Explcitly specified protocol */
		terminator = ( char * ) &info->protocol[info->protocol_len];
		temp = *terminator;
		*terminator = '\0';
		for ( proto = protocols ; proto < protocols_end ; proto++ ) {
			if ( memcmp ( proto->name, info->protocol,
				      info->protocol_len + 1 ) == 0 ) {
				info->proto = proto;
				break;
			}
		}
		*terminator = temp;
	} else {
		/* No explicitly specified protocol */
		if ( default_protocols < protocols_end )
			info->proto = default_protocols;
	}
	return ( ( int ) info->proto ); /* NULL indicates failure */
}

/*
 * Resolve the host portion of the URL
 *
 */
static inline int resolve_host ( struct url_info *info ) {
	char *terminator;
	char temp;
	int success;

	if ( ! info->host_len ) {
		/* No host specified - leave sin.sin_addr empty to
		 * indicate use of DHCP-supplied next-server
		 */
		return 1;
	}

	terminator = ( char * ) &info->host[info->host_len];
	temp = *terminator;
	*terminator = '\0';
	success = resolv ( &info->sin.sin_addr, info->host );
	*terminator = temp;
	return success;
}

/*
 * Parse a URL string into its constituent parts.  Perform name
 * resolution if required (and if resolver code is linked in), and
 * identify the protocol.
 *
 * We accept URLs of the form
 *
 *   [protocol://[host][:port]/]path/to/file
 *
 * We return true for success, 0 for failure (e.g. unknown protocol).
 * Note that the "/" before path/to/file *will* be counted as part of
 * the filename, if it is present.
 *
 */
int parse_url ( struct url_info *info, const char *url ) {
	const char *p;

	/* Fill in initial values */
	memset ( info, 0, sizeof ( *info ) );
	info->url = url;
	info->protocol = url;
	info->host = url;
	info->port = url;
	info->file = url;

	/* Split the URL into substrings, and fill in sin.sin_port */
	p = url;
	if ( parse_protocol ( info, &p ) )
		parse_host_port ( info, &p );
	info->file = p;

	/* Identify the protocol */
	if ( ! identify_protocol ( info ) )
		return 0;

	/* Resolve the host name to an IP address */
	if ( ! resolve_host ( info ) )
		return 0;

	return 1;
}
