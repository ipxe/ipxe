#if 0

#include "resolv.h"
#include "string.h"
#include <gpxe/dns.h>
#include "nic.h"
#include "nmb.h"

/*
 * Convert a standard NUL-terminated string to an NBNS query name.
 *
 * Returns a pointer to the character following the constructed NBNS
 * query name.
 *
 */
static inline char * nbns_make_name ( char *dest, const char *name ) {
	char nb_name[16];
	char c;
	int i;
	uint16_t *d;

	*(dest++) = 32; /* Length is always 32 */

	/* Name encoding is as follows: pad the name with spaces to
	 * length 15, and add a NUL.  Take this 16-byte string, split
	 * it into nibbles and add 0x41 to each nibble to form a byte
	 * of the resulting name string.
	 */
	memset ( nb_name, ' ', 15 );
	nb_name[15] = '\0';
	memcpy ( nb_name, name, strlen ( name ) ); /* Do not copy NUL */

	d = ( uint16_t * ) dest;
	for ( i = 0 ; i < 16 ; i++ ) {
		c = nb_name[i];
		*( d++ )  = htons ( ( ( c | ( c << 4 ) ) & 0x0f0f ) + 0x4141 );
	}
	dest = ( char * ) d;

	*(dest++) = 0; /* Terminating 0-length name component */
	return dest;
}

/*
 * Resolve a name using NMB
 *
 */
static int nmb_resolv ( struct in_addr *addr, const char *name ) {
	struct dns_query query;
	struct dns_query_info *query_info;
	struct dns_header *reply;
	struct dns_rr_info *rr_info;
	struct dns_rr_info_nb *rr_info_nb;
	struct sockaddr_in nameserver;

	DBG ( "NMB resolving %s\n", name );

	/* Set up the query data */
	nameserver.sin_addr.s_addr = INADDR_BROADCAST;
	nameserver.sin_port = NBNS_UDP_PORT;
	memset ( &query, 0, sizeof ( query ) );
	query.dns.id = htons ( 1 );
	query.dns.flags = htons ( DNS_FLAG_QUERY | DNS_FLAG_OPCODE_QUERY |
				  DNS_FLAG_RD | DNS_FLAG_BROADCAST );
	query.dns.qdcount = htons ( 1 );
	query_info = ( void * )	nbns_make_name ( query.payload, name );
	query_info->qtype = htons ( DNS_TYPE_NB );
	query_info->qclass = htons ( DNS_CLASS_IN );

	/* Issue query, wait for reply */
	reply = dns_query ( &query,
			    ( ( ( char * ) query_info )
			      + sizeof ( *query_info )
			      - ( ( char * ) &query ) ),
			    &nameserver );
	if ( ! reply ) {
		DBG ( "NMB got no response via %@ (port %d)\n",
		      nameserver.sin_addr.s_addr,
		      nameserver.sin_port );
		return 0;
	}

	/* Search through response for useful answers. */
	rr_info = dns_find_rr ( &query, reply );
	if ( ! rr_info ) {
		DBG ( "NMB got invalid response\n" );
		return 0;
	}

	/* Check type of response */
	if ( ntohs ( rr_info->type ) != DNS_TYPE_NB ) {
		DBG ( "NMB got answer type %hx (wanted %hx)\n",
		      ntohs ( rr_info->type ), DNS_TYPE_NB );
		return 0;
	}

	/* Read response */
	rr_info_nb = ( struct dns_rr_info_nb * ) rr_info;
	*addr = rr_info_nb->nb_address;
	DBG ( "NMB found address %@\n", addr->s_addr );

	return 1;
}

struct resolver nmb_resolver __resolver = {
	.name = "NMB",
	.resolv = nmb_resolv,
};

#endif
