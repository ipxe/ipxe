/**************************************************************************
*
*    dns_resolver.c: Etherboot support for resolution of host/domain
*    names in filename parameters
*    Written 2004 by Anselm M. Hoffmeister
*    <stockholm@users.sourceforge.net>
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*    This code is using nuts and bolts from throughout etherboot.
*    It is a fresh implementation according to the DNS RFC, #1035
*    
*    REVISION HISTORY:
*    ================
*    2004-05-10 File created
*    2004-05-19 First release to CVS
*    2004-05-22 CNAME support first stage finished
*    2004-05-24 First "stable" release to CVS
*    2004-08-28 Improve readability, set recursion flag
*    2005-04-30 Tidied up to the point of being a complete rewrite (mcb30)
***************************************************************************/

#include "etherboot.h"
#include "nic.h"
#include "resolv.h"
#include "dns.h"

/*
 *	await_dns
 *	Shall be called on any incoming packet during the resolution process
 *	(as is the case with all the other await_ functions in etherboot)
 *	Param:	as any await functions
 *
 */
static int await_dns ( int ival, void *ptr,
		       unsigned short ptype __unused,
		       struct iphdr *ip __unused,
		       struct udphdr *udp, struct tcphdr *tcp __unused ) {
	struct dns_header **header = ptr;

	if ( ! udp )
		return 0;
	if ( ntohs ( udp->dest ) != ival )
		return 0;
	*header = ( struct dns_header * ) ( udp + 1 );
	return 1;
}

/*
 * Send a name server query and wait for a response.  Query is retried
 * up to DNS_MAX_RETRIES times.  Returns a pointer to the answer
 * packet, or NULL if no answer was received.
 *
 */
struct dns_header * dns_query ( struct dns_query *query,
				unsigned int query_len, 
				struct sockaddr_in *nameserver ) {
	long timeout;
	int retry;
	struct dns_header *reply;

	for ( retry = 0 ; retry < DNS_MAX_RETRIES ; retry++ ) {
		udp_transmit ( nameserver->sin_addr.s_addr,
			       nameserver->sin_port, nameserver->sin_port,
			       query_len, query );
		timeout = rfc2131_sleep_interval ( TIMEOUT, retry );
		if ( ! await_reply ( await_dns, nameserver->sin_port,
				     &reply, timeout ) )
			continue;
		if ( reply->id != query->dns.id ) {
			DBG ( "DNS received unexpected reply ID %d "
			      "(wanted %d)\n",
			      ntohs ( reply->id ), ntohs ( query->dns.id ) );
			continue;
		}
		/* We have a valid reply! */
		return reply;
	}
	return NULL;
}

/*
 * Compare two DNS names to see if they are the same.  Takes
 * compressed names in the reply into account (though the query name
 * must be uncompressed).  Returns 0 for a match (for consistency with
 * strcmp et al).
 *
 */
static inline int dns_name_cmp ( const char *qname, const char *rname,
				 struct dns_header *reply ) {
	int i;
	while ( 1 ) {
		/* Obtain next section of rname */
		while ( ( *rname ) & 0xc0 ) {
			rname = ( ( char * ) reply +
				  ( ntohs( *((uint16_t *)rname) ) & ~0xc000 ));
		}
		/* Check that lengths match */
		if ( *rname != *qname )
			return 1;
		/* If length is zero, we have reached the end */
		if ( ! *qname )
			return 0;
		/* Check that data matches */
		for ( i = *qname + 1; i > 0 ; i-- ) {
			if ( *(rname++) != *(qname++) )
				return 1;
		}
	}
}

/*
 * Skip over a DNS name, which may be compressed
 *
 */
static inline const char * dns_skip_name ( const char *name ) {
	while ( 1 ) {
		if ( ! *name ) {
			/* End of name */
			return ( name + 1);
		}
		if ( *name & 0xc0 ) {
			/* Start of a compressed name */
			return ( name + 2 );
		}
		/* Uncompressed name portion */
		name += *name + 1;
	}
}

/*
 * Find a Resource Record in a reply packet corresponding to our
 * query.  Returns a pointer to the RR, or NULL if no answer found.
 *
 */
static struct dns_rr_info * dns_find_rr ( struct dns_query *query,
					  struct dns_header *reply ) {
	int i;
	const char *p = ( ( char * ) reply ) + sizeof ( struct dns_header );

	/* Skip over the questions section */
	for ( i = ntohs ( reply->qdcount ) ; i > 0 ; i-- ) {
		p = dns_skip_name ( p ) + sizeof ( struct dns_query_info );
	}

	/* Process the answers section */
	for ( i = ntohs ( reply->ancount ) ; i > 0 ; i-- ) {
		if ( dns_name_cmp ( query->payload, p, reply ) == 0 ) {
			return ( ( struct dns_rr_info * ) p );
		}
		p = dns_skip_name ( p );
		p += ( sizeof ( struct dns_rr_info ) +
		       ntohs ( ( ( struct dns_rr_info * ) p )->rdlength ) );
	}

	return NULL;
}

/*
 * Convert a standard NUL-terminated string to a DNS query name,
 * consisting of "<length>element" pairs.
 *
 * Returns a pointer to the character following the constructed DNS
 * query name.
 *
 */
static inline char * dns_make_name ( char *dest, const char *name ) {
	char *length_byte = dest++;
	char c;

	while ( ( c = *(name++) ) ) {
		if ( c == '.' ) {
			*length_byte = dest - length_byte - 1;
			length_byte = dest;
		}
		*(dest++) = c;
	}
	*length_byte = dest - length_byte - 1;
	*(dest++) = '\0';
	return dest;
}

/*
 * Decompress a DNS name.
 *
 * Returns a pointer to the character following the decompressed DNS
 * name.
 *
 */
static inline char * dns_decompress_name ( char *dest, const char *name,
					   struct dns_header *header ) {
	int i, len;

	do {
		/* Obtain next section of name */
		while ( ( *name ) & 0xc0 ) {
			name = ( ( char * ) header +
				 ( ntohs ( *((uint16_t *)name) ) & ~0xc000 ) );
		}
		/* Copy data */
		len = *name;
		for ( i = len + 1 ; i > 0 ; i-- ) {
			*(dest++) = *(name++);
		}
	} while ( len );
	return dest;
}

/*
 * Resolve a name using DNS
 *
 */
static int dns_resolv ( struct in_addr *addr, const char *name ) {
	struct dns_query query;
	struct dns_query_info *query_info;
	struct dns_header *reply;
	struct dns_rr_info *rr_info;
	struct sockaddr_in nameserver;
	uint16_t qtype;
	unsigned int recursion = 0;
	unsigned int id = 1;

	DBG ( "DNS resolving %s\n", name );

	/* Set up the query data */
	nameserver.sin_addr = arptable[ARP_NAMESERVER].ipaddr;
	nameserver.sin_port = DNS_UDP_PORT;
	memset ( &query, 0, sizeof ( query ) );
	query.dns.flags = htons ( DNS_FLAG_QUERY | DNS_FLAG_OPCODE_QUERY |
				  DNS_FLAG_RD );
	query.dns.qdcount = htons ( 1 );
	query_info = ( void * )	dns_make_name ( query.payload, name );
	query_info->qtype = htons ( DNS_TYPE_A );
	query_info->qclass = htons ( DNS_CLASS_IN );

	while ( 1 ) {
		/* Transmit current query, wait for reply */
		query.dns.id = htons ( id++ );
		qtype = ntohs ( query_info->qtype );
		reply = dns_query ( &query,
				    ( ( ( char * ) query_info )
				      + sizeof ( *query_info )
				      - ( ( char * ) &query ) ),
				    &nameserver );
		if ( ! reply ) {
			DBG ( "DNS got no response from server %@ (port %d)\n",
			      nameserver.sin_addr.s_addr,
			      nameserver.sin_port );
			return 0;
		}

		/* Search through response for useful answers.  Do
		 * this multiple times, to take advantage of useful
		 * nameservers which send us e.g. the CNAME *and* the
		 * A record for the pointed-to name.
		 */
		while ( ( rr_info = dns_find_rr ( &query, reply ) ) ) {
			switch ( ntohs ( rr_info->type ) ) {
			case DNS_TYPE_A: {
				/* Found the target A record */
				struct dns_rr_info_a *rr_info_a =
					( struct dns_rr_info_a * ) rr_info;
				*addr = rr_info_a->in_addr;
				DBG ( "DNS found address %@\n", addr->s_addr );
				return 1; }
			case DNS_TYPE_CNAME: {
				/* Found a CNAME record - update the query */
				struct dns_rr_info_cname *rr_info_cname =
					( struct dns_rr_info_cname * ) rr_info;
				char *cname = rr_info_cname->cname;

				DBG ( "DNS found CNAME\n" );
				query_info = ( void * )
					dns_decompress_name ( query.payload,
							      cname, reply );
				query_info->qtype = htons ( DNS_TYPE_A );
				query_info->qclass = htons ( DNS_CLASS_IN );

				if ( ++recursion > DNS_MAX_CNAME_RECURSION ) {
					DBG ( "DNS recursion exceeded\n" );
					return 0;
				}
				break; }
			default:
				DBG ( "DNS got unknown record type %d\n",
				      ntohs ( rr_info->type ) );
				return 0;
			}
		}
		
		/* Determine what to do next based on the type of
		 * query we issued and the reponse we received
		 */
		switch ( qtype ) {
		case DNS_TYPE_A :
			/* We asked for an A record and got nothing;
			 * try the CNAME.
			 */
			DBG ( "DNS found no A record; trying CNAME\n" );
			query_info->qtype = htons ( DNS_TYPE_CNAME );
			break;
		case DNS_TYPE_CNAME :
			/* We asked for a CNAME record.  If we didn't
			 * get any response (i.e. the next A query
			 * isn't already set up), then abort.
			 */
			if ( query_info->qtype != htons ( DNS_TYPE_A ) ) {
				DBG ( "DNS found no CNAME record\n" );
				return 0;
			}
			break;
		default:
			DBG ( "DNS internal error - inconsistent state\n" );
		}
	}
}

static struct resolver dns_resolver __resolver = {
	.name = "DNS",
	.resolv = dns_resolv,
};
