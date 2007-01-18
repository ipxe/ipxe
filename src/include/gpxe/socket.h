#ifndef _GPXE_SOCKET_H
#define _GPXE_SOCKET_H

/** @file
 *
 * Socket addresses
 *
 */

/* Network address family numbers */
#define AF_INET		1
#define AF_INET6	2

/** A socket address family */
typedef uint16_t sa_family_t;

/** Length of a @c struct @c sockaddr */
#define SA_LEN 32

/**
 * Generalized socket address structure
 *
 * This contains the fields common to socket addresses for all address
 * families.
 */
struct sockaddr {
	/** Socket address family
	 *
	 * This is an AF_XXX constant.
	 */
        sa_family_t sa_family;
	/** Padding
	 *
	 * This ensures that a struct @c sockaddr_tcpip is large
	 * enough to hold a socket address for any TCP/IP address
	 * family.
	 */
	char pad[ SA_LEN - sizeof ( sa_family_t ) ];
};

#endif /* _GPXE_SOCKET_H */
