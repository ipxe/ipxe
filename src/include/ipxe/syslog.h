#ifndef _IPXE_SYSLOG_H
#define _IPXE_SYSLOG_H

/** @file
 *
 * Syslog protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <syslog.h>
#include <ipxe/lineconsole.h>
#include <ipxe/interface.h>

/** Syslog server port */
#define SYSLOG_PORT 514

/** Syslog line buffer size
 *
 * This is a policy decision
 */
#define SYSLOG_BUFSIZE 128

/** Syslog default facility
 *
 * This is a policy decision
 */
#define SYSLOG_DEFAULT_FACILITY 0 /* kernel */

/** Syslog default severity
 *
 * This is a policy decision
 */
#define SYSLOG_DEFAULT_SEVERITY LOG_INFO

/** Syslog priority */
#define SYSLOG_PRIORITY( facility, severity ) ( 8 * (facility) + (severity) )

/** A syslog console */
struct syslog {
	/** Line console */
	struct line_console line;
	/** Data transfer interface */
	struct interface xfer;
	/** Console driver */
	struct console_driver *console;
	/** Line terminator */
	const char *terminator;
	/** Log severity */
	unsigned int severity;
	/** Recursion guard */
	int entered;
	/** Show (but do not reset) buffer contents */
	int show;
};

extern struct ansiesc_handler syslog_handlers[];
extern struct interface_descriptor syslog_datagram_desc;
extern struct interface_descriptor syslog_stream_desc;

extern void syslog_write ( struct syslog *syslog, int character );

#endif /* _IPXE_SYSLOG_H */
