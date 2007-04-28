#ifndef _GPXE_SOCKET_H
#define _GPXE_SOCKET_H

/** @file
 *
 * Socket addresses
 *
 */

/**
 * @defgroup commdomains Communication domains
 *
 * @{
 */
#define PF_INET		1	/**< IPv4 Internet protocols */
#define PF_INET6	2	/**< IPv6 Internet protocols */
/** @} */

/**
 * Name communication domain
 *
 * @v domain		Communication domain (e.g. PF_INET)
 * @ret name		Name of communication domain
 */
static inline __attribute__ (( always_inline )) const char *
socket_domain_name ( int domain ) {
	switch ( domain ) {
	case PF_INET:		return "PF_INET";
	case PF_INET6:		return "PF_INET6";
	default:		return "PF_UNKNOWN";
	}
}

/**
 * @defgroup commtypes Communication types
 *
 * @{
 */
#define SOCK_STREAM	1	/**< Connection-based, reliable streams */
#define SOCK_DGRAM	2	/**< Connectionless, unreliable streams */
/** @} */

/**
 * Name communication type
 *
 * @v type		Communication type (e.g. SOCK_STREAM)
 * @ret name		Name of communication type
 */
static inline __attribute__ (( always_inline )) const char *
socket_type_name ( int type ) {
	switch ( type ) {
	case SOCK_STREAM:	return "SOCK_STREAM";
	case SOCK_DGRAM:	return "SOCK_DGRAM";
	default:		return "SOCK_UNKNOWN";
	}
}

/**
 * @defgroup addrfam Address families
 *
 * @{
 */
#define AF_INET		1	/**< IPv4 Internet addresses */
#define AF_INET6	2	/**< IPv6 Internet addresses */
/** @} */

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
