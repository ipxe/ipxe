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
*    $Revision$
*    $Author$
*    $Date$
*
*    REVISION HISTORY:
*    ================
*    2004-05-10 File created
*    2004-05-19 First release to CVS
*    2004-05-22 CNAME support first stage finished
*    2004-05-24 First "stable" release to CVS
*    2004-08-28 Improve readability, set recursion flag
***************************************************************************/

#ifdef DNS_RESOLVER
#include "etherboot.h"
#include "nic.h"
#include "dns_resolver.h"

#define	MAX_DNS_RETRIES	3
#define	MAX_CNAME_RECURSION 0x30
#undef DNSDEBUG

int	donameresolution ( char * hostname, int hnlength, char * deststring );

/*
 *	dns_resolver
 *	Function: Main function for name resolution - will be called by other
 *		parts of etherboot
 *	Param:	string filename (not containing proto prefix like "tftp://")
 *	Return:	string filename, with hostname replaced by IP in dotted quad
 *		or NULL for resolver error
 *		In case no substitution is necessary, return will be = param
 *		The returned string, if not == input, will be temporary and
 *		probably be overwritten during the next call to dns_resolver
 *		The returned string may (or may not) only contain an IP
 *		address, or an IP followed by a ":" or "/", or be NULL(=error)
 */
char *	dns_resolver ( char * filename ) {
	int	i = 0, j, k;
	static char ipaddr[16] = { 0 };
	// Search for "end of hostname" (which might be either ":" or "/")
	for ( j = i; (filename[j] != ':') && (filename[j] != '/'); ++j ) {
		// If no hostname delimiter was found, assume no name present
		if ( filename[j] == 0 )	return	filename;
	}
	// Check if the filename is an IP, in which case, leave unchanged
	k = j - i - 1;
	while ( ( '.' == filename[i+k] ) ||
		( ( '0' <= filename[i+k] ) && ( '9' >= filename[i+k] ) ) ) {
		--k;
		if ( k < 0 ) return filename; // Only had nums and dots->IP
	}
	// Now that we know it's a full hostname, attempt to resolve
	if ( donameresolution ( filename + i, j - i, ipaddr ) ) {
		return	NULL;	// Error in resolving - Fatal.
	}
	// Return the dotted-quad IP which resulted
	return	ipaddr;
}

/*
 *	await_dns
 *	Shall be called on any incoming packet during the resolution process
 *	(as is the case with all the other await_ functions in etherboot)
 *	Param:	as any await functions
 *	Return:	see dns_resolver.h for constant return values + descriptions
 */
static int await_dns (int ival, void *ptr,
	unsigned short ptype __unused, struct iphdr *ip __unused,
	struct udphdr *udp, struct tcphdr *tcp __unused) {
	int	i, j, k;
	unsigned char *p = (unsigned char *)udp + sizeof(struct udphdr);
	// p is set to the beginning of the payload
	unsigned char *q;
	unsigned int querytype = QUERYTYPE_A;
	if (  0 == udp  )	// Parser couldn't find UDP header
		return RET_PACK_GARBAG;	// Not a UDP packet
	if (( UDP_PORT_DNS != ntohs (udp->src )) ||
	    ( UDP_PORT_DNS != ntohs (udp->dest)) )
		// Neither source nor destination port is "53"
		return RET_PACK_GARBAG;	// UDP port wrong
	if (( p[QINDEX_ID  ] != 0) ||
	    ( p[QINDEX_ID+1] != (ival & 0xff)))
		// Checking if this packet has set (inside payload)
		// the sequence identifier that we expect
		return RET_PACK_GARBAG;	// Not matching our request ID
	if (( p[QINDEX_FLAGS  ] & QUERYFLAGS_MASK ) != QUERYFLAGS_WANT )
		// We only accept responses to the query(ies) we sent
		return	RET_PACK_GARBAG;	// Is not response=opcode <0>
	querytype = (ival & 0xff00) >> 8;
	if (((p[QINDEX_NUMANSW+1] + (p[QINDEX_NUMANSW+1]<<8)) == 0 ) ||
	     ( ERR_NOSUCHNAME == (p[QINDEX_FLAGS+1] & 0x0f) ) ) {
		// Answer section has 0 entries, or "no such name" returned
		if ( QUERYTYPE_A == querytype) {
			// It was an A type query, so we should try if there's
			// an alternative "CNAME" record available
			return	RET_RUN_CNAME_Q; // So try CNAME query next
		} else if ( QUERYTYPE_CNAME == querytype) {
			// There's no CNAME either, so give up
			return	RET_NOSUCHNAME;
		} else {
			// Anything else? No idea what. Should not happen.
			return	RET_NOSUCHNAME; // Bail out with error
		}
	}
	if ( 0 != ( p[QINDEX_FLAGS+1] & 0x0f ) )
		// The response packet's flag section tells us:
		return	RET_NOSUCHNAME;	// Another (unspecific) error occured
	// Now we have an answer packet in response to our query. Next thing
	// to do is to search the payload for the "answer section", as there is
	// a question section first usually that needs to be skipped
	// If question section was not repeated, that saves a lot of work :
	if ( 0 >= (i = ((p[QINDEX_NUMQUEST] << 8) + p[QINDEX_NUMQUEST+1]))) {
		q = p+ QINDEX_QUESTION;	// No question section, just continue;
	} else if ( i >= 2 ) {		// More than one query section? Error!
		return	RET_NOSUCHNAME;	// That's invalid for us anyway - 
					// We only place one query at a time
	} else {
		// We have to skip through the question section first to
		// find the beginning of the answer section
		q = p + QINDEX_QUESTION;
		while ( 0 != q[0] )
			q += q[0] + 1; // Skip through
		q += 5; // Skip over end-\0 and query type section
	}
	// Now our pointer shows the beginning of the answer section
	// So now move it past the (repeated) query string, we only
	// want the answer
	while ( 0 != q[0] ) {
		if ( 0xc0 == ( q[0] & 0xc0 ) ) { // Pointer
			++q;
			break;
		}
		q += q[0] + 1;
	}
	++q;
	// Now check wether it's an INET host address (resp. CNAME)?
	// There seem to be nameservers out there (Bind 9, for example),
	// that return CNAMEs when no As are there, e.g. in
	//   testname.test.   IN CNAME othername.test.
	// case, bind 9 would return the CNAME record on an A query.
	// Accept this case as it saves a lot of work (an extra query)
	if (( QUERYTYPE_CNAME == q[1] ) &&
	    ( QUERYTYPE_A     == querytype )) {
		// A query, but CNAME response.
		// Do simulate having done a CNAME query now and just assume
		// the answer we are working on here is that of a CNAME query
		// This works for single-depth CNAMEs fine.
		// For deeper recursion, we won't parse the answer
		// packet deeper but rely on a separate CNAME query
		querytype = QUERYTYPE_CNAME;
	}
	// Now check wether the answer packet is of the expected type
	// (remember, we just tweaked CNAME answers to A queries already)
	if ( (q[0] != 0) ||
	     (q[1] !=   querytype) ||   // query type matches?
	     (q[2] != ((QUERYCLASS_INET & 0xff00) >> 8)) ||
	     (q[3] !=  (QUERYCLASS_INET & 0x00ff))) { // class IN response?
		return	RET_DNSERROR;	// Should not happen. DNS server bad?
	}
	q += 8;	// skip querytype/-class/ttl which are uninteresting now
	if ( querytype == QUERYTYPE_A ) {
		// So what we sent was an A query - expect an IPv4 address
		// Check if datalength looks satisfactory
		if ( ( 0 != q[0] ) || ( 4 != q[1] ) ) {
			// Data length is not 4 bytes
			return	RET_DNSERROR;
		}
		// Go to the IP address and copy it to the response buffer
		p = ptr + QINDEX_STORE_A;
		for ( i = 0; i < 4; ++i ) {
			p[i] = q[i+2];
		}
		return	RET_GOT_ADDR; // OK! Address RETURNED! VERY FINE!
	} else if ( querytype == QUERYTYPE_CNAME ) { // CNAME QUERY
		// The pointer "q" now stands on the "payload length" of the
		// CNAME data [2 byte field]. We will have to check wether this
		// name is referenced in a following response, which would save
		// us further queries, or if it is standalone, which calls for
		// making a separate query
		// This statement above probably needs clarification. To help
		// the reader understand what's going on, imagine the DNS
		// answer to be 4-section: query(repeating what we sent to the
		// DNS server), answer(like the CNAME record we wanted),
		// additional (which might hold the A for the CNAME - esp.
		// Bind9 seems to provide us with this info when we didn't
		// query for it yet) and authoritative (the DNS server's info
		// on who is responsible for that data). For compression's
		// sake, instead of specifying the full hostname string all
		// the time, one can instead specify something like "same as on
		// payload offset 0x123" - if this happens here, we can check
		// if that payload offset given here by coincidence is that of
		// an additional "A" record which saves us sending a separate
		// query.
		// We will look if there's  a/ two or more answer sections
		// AND  b/ the next is a reference to us.
		p = (unsigned char *)udp + sizeof(struct udphdr);
		i = q + 2 - p;
		if ( p[7] > 1 ) { // More than one answer section
			// Check the next if it's a ref
			// For that, we have to locate the next. Do this with "q".
			p = q + (q[0] * 0x100) + q[1] + 2;
			if ( (p[0] == (0xc0 + ((i & 0xf00)/256))) &&
			     (p[1] == (i & 0xff) ) &&
			     (p[2] == ((QUERYTYPE_A     & 0xff00) >> 8)) &&
			     (p[3] ==  (QUERYTYPE_A     & 0x00ff)) &&
			     (p[4] == ((QUERYCLASS_INET & 0xff00) >> 8 )) &&
			     (p[5] ==  (QUERYCLASS_INET & 0x00ff)) &&
			     (p[10]== 0) &&		// Data length expected
			     (p[11]== 4)) {		// to be <4> (IP-addr)
				// Behind that sections: TTL, data length(4)
				for ( i = 0; i < 4; ++i ) {
				    ((unsigned char*)ptr)[QINDEX_STORE_A+i] =
					    p[12+i];
				}
				return	RET_GOT_ADDR;
			}
		}
		// Reference not found, next query (A) needed (because CNAME
		// queries usually return another hostname, not an IP address
		// as we need it - that's the point in CNAME, anyway :-)
		p = (unsigned char *)udp + sizeof(struct udphdr);
#ifdef DNSDEBUG
		printf ( " ->[");
#endif
		k = QINDEX_QUESTION;
		i = (q-p) + 2;
		// Compose the hostname that needs to be queried for
		// This looks complicated (and is a little bit) because
		// we might not be able to copy verbatim, but need to
		// check wether the CNAME looks like
		// "servername" "plus what offset 0x123 of the payload says"
		// (this saves transfer bandwidth, which is why DNS allows
		// this, but it makes programmers' lives more difficult)
		while (p[i] != 0) {
			if ( (((unsigned char *)p)[i] & 0xc0) != 0 ) {
				i = ((p[i] & 0x3f) * 0x100) + p[i+1];
				continue;
			}
			((unsigned char *)ptr)[k] = p[i];
			for ( j = i + 1; j <= i + p[i]; ++j ) {
				((unsigned char *)ptr)[k+j-i] = p[j];
#ifdef DNSDEBUG
				printf ( "%c", p[j] );
#endif
			}
			k += ((unsigned char *)ptr)[k] + 1;
			i += p[i] + 1;
#ifdef DNSDEBUG
			printf ( (p[i] ? ".": "") );
#endif
		}
		((unsigned char *)ptr)[k] = 0;
#ifdef DNSDEBUG
		printf ( "].." );
#endif
		// So we need to run another query, this time try to
		// get an "A" record for the hostname that the last CNAME
		// query pointed to
		return	RET_RUN_NEXT_A;
	}
	// We only accept CNAME or A replies from the nameserver, every-
	// thing else is of no use for us.
	// Must be an invalid packet that the nameserver sent.
	return	RET_DNSERROR;
}

int	chars_to_next_dot ( char * countfrom, int maxnum ) {
	// Count the number of characters of this part of a hostname
	int i;
	for ( i = 1; i < maxnum; ++i ) {
		if ( countfrom[i] == '.' ) return i;
	}
	return	maxnum;
}

/*
 *	donameresolution
 *	Function: Compose the initial query packet, handle answers until
 *		a/ an IP address is retrieved
 *		b/ too many CNAME references occured (max. MAX_DNS_RETRIES)
 *		c/ No matching record for A or CNAME can be found
 *	Param:	string hostname, length (hostname needs no \0-end-marker),
 *		string to which dotted-quad-IP shall be written
 *	Return:	0 for success, >0 for failure
 */
int	donameresolution ( char * hostname, int hnlength, char * deststring ) {
	unsigned char	querybuf[260+sizeof(struct iphdr)+sizeof(struct udphdr)];
		// 256 for the DNS query payload, +4 for the result temporary
	unsigned char	*query = &querybuf[sizeof(struct iphdr)+sizeof(struct udphdr)];
		// Pointer to the payload
	int	i, h = hnlength;
	long	timeout;
	int	retry, recursion;
	// Setup the query data
	query[QINDEX_ID  ]	= (QUERYIDENTIFIER & 0xff00) >> 8;
	query[QINDEX_ID+1]	=  QUERYIDENTIFIER & 0xff;
	query[QINDEX_FLAGS  ]	= (QUERYFLAGS & 0xff00) >> 8;
	query[QINDEX_FLAGS+1]	=  QUERYFLAGS & 0xff;
	query[QINDEX_NUMQUEST  ]= 0;
	query[QINDEX_NUMQUEST+1]= 1; // 1 standard query to be sent
	query[QINDEX_NUMANSW  ]	= 0;
	query[QINDEX_NUMANSW+1]	= 0;
	query[QINDEX_NUMAUTH  ]	= 0;
	query[QINDEX_NUMAUTH+1]	= 0;
	query[QINDEX_NUMADDIT  ]= 0;
	query[QINDEX_NUMADDIT+1]= 0;
	query[QINDEX_QUESTION]	= chars_to_next_dot(hostname,h);
	if ( h > 236 ) return 1;	// Hostnames longer than 236 chars are refused.
					// This is an arbitrary decision, SHOULD check
					// what the RFCs say about that
	for ( i = 0; i < h; ++i ) {
		// Compose the query section's hostname - replacing dots (and
		// preceding the string) with one-byte substring-length values
		// (for the immediately following substring) - \0 terminated
		query[QINDEX_QUESTION+i+1] = hostname[i];
		if ( hostname[i] == '.' )
			query[QINDEX_QUESTION+i+1] = chars_to_next_dot(hostname + i + 1, h - i - 1);
	}
	query[QINDEX_QTYPE+h  ]	= (QUERYTYPE_A & 0xff00) >> 8;
	query[QINDEX_QTYPE+h+1]	=  QUERYTYPE_A & 0xff;
					// First try an A query, if that
					// won't work, try CNAME
	printf ( "Resolving hostname [" );
	for ( i = 0; i < hnlength; ++i ) { printf ( "%c", hostname[i] ); }
	printf ("]" );
	for ( recursion = MAX_CNAME_RECURSION; recursion > 0; --recursion ) {
		printf ( ".." );
		// Try MAX_CNAME_RECURSION CNAME queries maximally, if then no
		// A record is found, assume the hostname to not be resolvable
		query[QINDEX_QUESTION+h+1] = 0;	// Marks the end of
						// the query string
		query[QINDEX_QCLASS+h  ]= (QUERYCLASS_INET & 0xff00) >> 8;
		query[QINDEX_QCLASS+h+1]=  QUERYCLASS_INET & 0xff;
		rx_qdrain();	// Clear NIC packet buffer -
				// there won't be anything of interest *now*.
		udp_transmit ( arptable[ARP_NAMESERVER].ipaddr.s_addr,
				UDP_PORT_DNS, UDP_PORT_DNS,
				hnlength + 18 + sizeof(struct iphdr) +
				sizeof(struct udphdr), querybuf );
		// If no answer comes in in a certain period of time, retry
		for (retry = 1; retry <= MAX_DNS_RETRIES; retry++) {
			timeout = rfc2131_sleep_interval(TIMEOUT, retry);
			i = await_reply ( await_dns,
				(query[QINDEX_QTYPE+h+1] << 8) +
				((unsigned char *)query)[QINDEX_ID+1],
				query, timeout);
			// The parameters given are
			//	bits 15...8 of integer = Query type  (low bits)
			//	bits  7...0 of integer = Query index (low bits)
			//	query + QINDEX_STORE_A points to the place
			//	where A records	shall be stored (4 bytes);
			//	query + 12(QINDEX_QUESTION) points to where
			//	a CNAME should go in case one is received
			if (i) break;
		}
		switch ( i ) {
		  case	RET_GOT_ADDR:	// Address successfully retrieved
			sprintf( deststring, "%@",
				  (long)query[QINDEX_STORE_A  ]           +
				( (long)query[QINDEX_STORE_A+1] * 256 ) +
				( (long)query[QINDEX_STORE_A+2] * 65536 )+
				( (long)query[QINDEX_STORE_A+3] * 16777216 ));
			printf ( " -> IP [%s]\n", deststring );
			return	RET_DNS_OK;
		  case	RET_RUN_CNAME_Q: // No A record found, try CNAME
			query[QINDEX_QTYPE+h  ]=(QUERYTYPE_CNAME & 0xff00)>> 8;
			query[QINDEX_QTYPE+h+1]= QUERYTYPE_CNAME & 0xff;
			break;
		  case	RET_RUN_NEXT_A:
			// Found a CNAME, now try A for the name it pointed to
			for ( i = 0; query[QINDEX_QUESTION+i] != 0;
					i += query[QINDEX_QUESTION+i] + 1 ) {;}
			h = i - 1;
			query[QINDEX_QTYPE+h  ]=(QUERYTYPE_A & 0xff00)>> 8;
			query[QINDEX_QTYPE+h+1]= QUERYTYPE_A & 0xff;
			break;
		  case	RET_CNAME_FAIL:
			// Neither A nor CNAME gave a usable result
			printf ("Host name cannot be resolved\n");
			return	RET_DNS_FAIL;
		  case	RET_NOSUCHNAME:
		  default:
			printf ( "Name resolution failed\n" );
			return	RET_DNS_FAIL;
		}
		query[QINDEX_ID  ] = 0;	// We will probably never have more
					// than 256 queries in one run
		query[QINDEX_ID+1]++;
	}
	// To deep recursion
	printf ( "CNAME recursion to deep - abort name resolver\n" );
	return	RET_DNS_FAIL;
}
#endif				/* DNS_RESOLVER */
