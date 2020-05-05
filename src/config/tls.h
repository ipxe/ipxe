#ifndef CONFIG_TLS_H
#define CONFIG_TLS_H

/** @file
 *
 * TLS configuration
 *
 * These options affect the operation of the behaviour of the
 * TLS implementation.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#define	TLS_FRAGMENTATION_ENABLED	/* If the TLS implementation should
					 * request a maximum fragment length */
#define TLS_REQUESTED_MAX_FRAGMENT_LENGTH TLS_MAX_FRAGMENT_LENGTH_4096 /* Which fragment 
									* length should 
									* be requested */

#include <config/named.h>
#include NAMED_CONFIG(tls.h)
#include <config/local/tls.h>
#include LOCAL_NAMED_CONFIG(tls.h)

#endif /* CONFIG_TLS_H */
