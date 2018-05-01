#ifndef _IPXE_NETBIOS_H
#define _IPXE_NETBIOS_H

/** @file
 *
 * NetBIOS user names
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

extern const char * netbios_domain ( char **username );

/**
 * Restore NetBIOS [domain\]username
 *
 * @v domain		NetBIOS domain name
 * @v username		NetBIOS user name
 *
 * Restore the separator in a NetBIOS [domain\]username as split by
 * netbios_domain().
 */
static inline void netbios_domain_undo ( const char *domain, char *username ) {

	/* Restore separator, if applicable */
	if ( domain )
		username[-1] = '\\';
}

#endif /* _IPXE_NETBIOS_H */
