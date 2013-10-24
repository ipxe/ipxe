#ifndef _IPXE_PROXY_H
#define _IPXE_PROXY_H

/** @file
 *
 * HTTP Proxy
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

int is_proxy_set ( );
struct uri *get_proxy ( );
const char *proxied_uri_host ( struct uri *uri );
unsigned int proxied_uri_port ( struct uri *uri, unsigned int default_port );

#endif /* _IPXE_IP_H */
