/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * Portions copyright (C) 2004 Anselm M. Hoffmeister
 * <stockholm@users.sourceforge.net>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/async.h>
#include <gpxe/udp.h>
#include <gpxe/resolv.h>
#include <gpxe/dns.h>

/** @file
 *
 * DNS protocol
 *
 */

/* The DNS server */
struct in_addr nameserver = { INADDR_NONE };

/**
 * Compare DNS reply name against the query name from the original request
 *
 * @v dns		DNS request
 * @v reply		DNS reply
 * @v rname		Reply name
 * @ret	zero		Names match
 * @ret non-zero	Names do not match
 */
static int dns_name_cmp ( struct dns_request *dns, struct dns_header *reply, 
			  const char *rname ) {
	const char *qname = dns->query.payload;
	int i;

	while ( 1 ) {
		/* Obtain next section of rname */
		while ( ( *rname ) & 0xc0 ) {
			rname = ( ( ( char * ) reply ) +
				  ( ntohs( *((uint16_t *)rname) ) & ~0xc000 ));
		}
		/* Check that lengths match */
		if ( *rname != *qname )
			return -1;
		/* If length is zero, we have reached the end */
		if ( ! *qname )
			return 0;
		/* Check that data matches */
		for ( i = *qname + 1; i > 0 ; i-- ) {
			if ( *(rname++) != *(qname++) )
				return -1;
		}
	}
}

/**
 * Skip over a (possibly compressed) DNS name
 *
 * @v name		DNS name
 * @ret name		Next DNS name
 */
static const char * dns_skip_name ( const char *name ) {
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

/**
 * Find an RR in a reply packet corresponding to our query
 *
 * @v dns		DNS request
 * @v reply		DNS reply
 * @ret rr		DNS RR, or NULL if not found
 */
static union dns_rr_info * dns_find_rr ( struct dns_request *dns,
					 struct dns_header *reply ) {
	int i, cmp;
	const char *p = ( ( char * ) reply ) + sizeof ( struct dns_header );
	union dns_rr_info *rr_info;

	/* Skip over the questions section */
	for ( i = ntohs ( reply->qdcount ) ; i > 0 ; i-- ) {
		p = dns_skip_name ( p ) + sizeof ( struct dns_query_info );
	}

	/* Process the answers section */
	for ( i = ntohs ( reply->ancount ) ; i > 0 ; i-- ) {
		cmp = dns_name_cmp ( dns, reply, p );
		p = dns_skip_name ( p );
		rr_info = ( ( union dns_rr_info * ) p );
		if ( cmp == 0 )
			return rr_info;
		p += ( sizeof ( rr_info->common ) +
		       ntohs ( rr_info->common.rdlength ) );
	}

	return NULL;
}

/**
 * Convert a standard NUL-terminated string to a DNS name
 *
 * @v string		Name as a NUL-terminated string
 * @v buf		Buffer in which to place DNS name
 * @ret next		Byte following constructed DNS name
 *
 * DNS names consist of "<length>element" pairs.
 */
static char * dns_make_name ( const char *string, char *buf ) {
	char *length_byte = buf++;
	char c;

	while ( ( c = *(string++) ) ) {
		if ( c == '.' ) {
			*length_byte = buf - length_byte - 1;
			length_byte = buf;
		}
		*(buf++) = c;
	}
	*length_byte = buf - length_byte - 1;
	*(buf++) = '\0';
	return buf;
}

/**
 * Convert an uncompressed DNS name to a NUL-terminated string
 *
 * @v name		DNS name
 * @ret string		NUL-terminated string
 *
 * Produce a printable version of a DNS name.  Used only for debugging.
 */
static inline char * dns_unmake_name ( char *name ) {
	char *p;
	unsigned int len;

	p = name;
	while ( ( len = *p ) ) {
		*(p++) = '.';
		p += len;
	}

	return name + 1;
}

/**
 * Decompress a DNS name
 *
 * @v reply		DNS replay
 * @v name		DNS name
 * @v buf		Buffer into which to decompress DNS name
 * @ret next		Byte following decompressed DNS name
 */
static char * dns_decompress_name ( struct dns_header *reply,
				    const char *name, char *buf ) {
	int i, len;

	do {
		/* Obtain next section of name */
		while ( ( *name ) & 0xc0 ) {
			name = ( ( char * ) reply +
				 ( ntohs ( *((uint16_t *)name) ) & ~0xc000 ) );
		}
		/* Copy data */
		len = *name;
		for ( i = len + 1 ; i > 0 ; i-- ) {
			*(buf++) = *(name++);
		}
	} while ( len );
	return buf;
}

/**
 * Mark DNS request as complete
 *
 * @v dns		DNS request
 * @v rc		Return status code
 */
static void dns_done ( struct dns_request *dns, int rc ) {

	/* Stop the retry timer */
	stop_timer ( &dns->timer );

	/* Close UDP connection */
	udp_close ( &dns->udp );

	/* Mark async operation as complete */
	async_done ( &dns->async, rc );
}

/**
 * Send next packet in DNS request
 *
 * @v dns		DNS request
 */
static void dns_send_packet ( struct dns_request *dns ) {
	static unsigned int qid = 0;

	/* Increment query ID */
	dns->query.dns.id = htons ( ++qid );

	DBGC ( dns, "DNS %p sending query ID %d\n", dns, qid );

	/* Start retransmission timer */
	start_timer ( &dns->timer );

	/* Send the data */
	udp_send ( &dns->udp, &dns->query,
		   ( ( ( void * ) dns->qinfo ) - ( ( void * ) &dns->query )
		     + sizeof ( dns->qinfo ) ) );
}

/**
 * Handle DNS retransmission timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void dns_timer_expired ( struct retry_timer *timer, int fail ) {
	struct dns_request *dns =
		container_of ( timer, struct dns_request, timer );

	if ( fail ) {
		dns_done ( dns, -ETIMEDOUT );
	} else {
		dns_send_packet ( dns );
	}
}

/**
 * Receive new data
 *
 * @v udp		UDP connection
 * @v data		Received data
 * @v len		Length of received data
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 */
static int dns_newdata ( struct udp_connection *conn, void *data, size_t len,
			 struct sockaddr_tcpip *st_src __unused,
			 struct sockaddr_tcpip *st_dest __unused ) {
	struct dns_request *dns =
		container_of ( conn, struct dns_request, udp );
	struct dns_header *reply = data;
	union dns_rr_info *rr_info;
	struct sockaddr_in *sin;
	unsigned int qtype = dns->qinfo->qtype;

	/* Sanity check */
	if ( len < sizeof ( *reply ) ) {
		DBGC ( dns, "DNS %p received underlength packet length %zd\n",
		       dns, len );
		return -EINVAL;
	}

	/* Check reply ID matches query ID */
	if ( reply->id != dns->query.dns.id ) {
		DBGC ( dns, "DNS %p received unexpected reply ID %d "
		       "(wanted %d)\n", dns, ntohs ( reply->id ),
		       ntohs ( dns->query.dns.id ) );
		return -EINVAL;
	}

	DBGC ( dns, "DNS %p received reply ID %d\n", dns, ntohs ( reply->id ));

	/* Stop the retry timer.  After this point, each code path
	 * must either restart the timer by calling dns_send_packet(),
	 * or mark the DNS operation as complete by calling
	 * dns_done()
	 */
	stop_timer ( &dns->timer );

	/* Search through response for useful answers.  Do this
	 * multiple times, to take advantage of useful nameservers
	 * which send us e.g. the CNAME *and* the A record for the
	 * pointed-to name.
	 */
	while ( ( rr_info = dns_find_rr ( dns, reply ) ) ) {
		switch ( rr_info->common.type ) {

		case htons ( DNS_TYPE_A ):

			/* Found the target A record */
			DBGC ( dns, "DNS %p found address %s\n",
			       dns, inet_ntoa ( rr_info->a.in_addr ) );
			sin = ( struct sockaddr_in * ) dns->sa;
			sin->sin_family = AF_INET;
			sin->sin_addr = rr_info->a.in_addr;

			/* Mark operation as complete */
			dns_done ( dns, 0 );
			return 0;

		case htons ( DNS_TYPE_CNAME ):

			/* Found a CNAME record; update query and recurse */
			DBGC ( dns, "DNS %p found CNAME\n", dns );
			dns->qinfo = ( void * ) dns_decompress_name ( reply,
							 rr_info->cname.cname,
							 dns->query.payload );
			dns->qinfo->qtype = htons ( DNS_TYPE_A );
			dns->qinfo->qclass = htons ( DNS_CLASS_IN );
			
			/* Terminate the operation if we recurse too far */
			if ( ++dns->recursion > DNS_MAX_CNAME_RECURSION ) {
				DBGC ( dns, "DNS %p recursion exceeded\n",
				       dns );
				dns_done ( dns, -ELOOP );
				return 0;
			}
			break;

		default:
			DBGC ( dns, "DNS %p got unknown record type %d\n",
			       dns, ntohs ( rr_info->common.type ) );
			break;
		}
	}
	
	/* Determine what to do next based on the type of query we
	 * issued and the reponse we received
	 */
	switch ( qtype ) {

	case htons ( DNS_TYPE_A ):
		/* We asked for an A record and got nothing;
		 * try the CNAME.
		 */
		DBGC ( dns, "DNS %p found no A record; trying CNAME\n", dns );
		dns->qinfo->qtype = htons ( DNS_TYPE_CNAME );
		dns_send_packet ( dns );
		return 0;

	case htons ( DNS_TYPE_CNAME ):
		/* We asked for a CNAME record.  If we got a response
		 * (i.e. if the next A query is already set up), then
		 * issue it, otherwise abort.
		 */
		if ( dns->qinfo->qtype == htons ( DNS_TYPE_A ) ) {
			dns_send_packet ( dns );
			return 0;
		} else {
			DBGC ( dns, "DNS %p found no CNAME record\n", dns );
			dns_done ( dns, -ENXIO );
			return 0;
		}

	default:
		assert ( 0 );
		dns_done ( dns, -EINVAL );
		return 0;
	}
}

/** DNS UDP operations */
struct udp_operations dns_udp_operations = {
	.newdata = dns_newdata,
};

/**
 * Reap asynchronous operation
 *
 * @v async		Asynchronous operation
 */
static void dns_reap ( struct async *async ) {
	struct dns_request *dns =
		container_of ( async, struct dns_request, async );

	free ( dns );
}

/**
 * Handle SIGKILL
 *
 * @v async		Asynchronous operation
 */
static void dns_sigkill ( struct async *async, enum signal signal __unused ) {
	struct dns_request *dns =
		container_of ( async, struct dns_request, async );

	dns_done ( dns, -ECANCELED );
}

/** DNS asynchronous operations */
static struct async_operations dns_async_operations = {
	.reap = dns_reap,
	.signal = {
		[SIGKILL] = dns_sigkill,
	},
};

/**
 * Resolve name using DNS
 *
 * @v name		Host name to resolve
 * @v sa		Socket address to fill in
 * @v parent		Parent asynchronous operation
 * @ret rc		Return status code
 */
int dns_resolv ( const char *name, struct sockaddr *sa,
		 struct async *parent ) {
	struct dns_request *dns;
	union {
		struct sockaddr_tcpip st;
		struct sockaddr_in sin;
	} server;
	int rc;

	/* Allocate DNS structure */
	dns = malloc ( sizeof ( *dns ) );
	if ( ! dns ) {
		rc = -ENOMEM;
		goto err;
	}
	memset ( dns, 0, sizeof ( *dns ) );
	dns->sa = sa;
	dns->timer.expired = dns_timer_expired;
	dns->udp.udp_op = &dns_udp_operations;
	async_init ( &dns->async, &dns_async_operations, parent );

	/* Create query */
	dns->query.dns.flags = htons ( DNS_FLAG_QUERY | DNS_FLAG_OPCODE_QUERY |
				       DNS_FLAG_RD );
	dns->query.dns.qdcount = htons ( 1 );
	dns->qinfo = ( void * ) dns_make_name ( name, dns->query.payload );
	dns->qinfo->qtype = htons ( DNS_TYPE_A );
	dns->qinfo->qclass = htons ( DNS_CLASS_IN );

	/* Identify nameserver */
	memset ( &server, 0, sizeof ( server ) );
	server.sin.sin_family = AF_INET;
	server.sin.sin_port = htons ( DNS_PORT );
	server.sin.sin_addr = nameserver;
	if ( server.sin.sin_addr.s_addr == INADDR_NONE ) {
		DBGC ( dns, "DNS %p no name servers\n", dns );
		rc = -ENXIO;
		goto err;
	}

	/* Open UDP connection */
	DBGC ( dns, "DNS %p using nameserver %s\n", dns, 
	       inet_ntoa ( server.sin.sin_addr ) );
	udp_connect ( &dns->udp, &server.st );
	if ( ( rc = udp_open ( &dns->udp, 0 ) ) != 0 )
		goto err;

	/* Send first DNS packet */
	dns_send_packet ( dns );

	return 0;	

 err:
	DBGC ( dns, "DNS %p could not create request: %s\n", 
	       dns, strerror ( rc ) );
	async_uninit ( &dns->async );
	free ( dns );
	return rc;
}

/** DNS name resolver */
struct resolver dns_resolver __resolver ( RESOLV_NORMAL ) = {
	.name = "DNS",
	.resolv = dns_resolv,
};
