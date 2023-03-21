#ifndef _IPXE_SLIRP_H
#define _IPXE_SLIRP_H

/** @file
 *
 * Linux Slirp network driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdbool.h>

/** Ready to be read */
#define SLIRP_EVENT_IN 0x01

/** Ready to be written */
#define SLIRP_EVENT_OUT 0x02

/** Exceptional condition */
#define SLIRP_EVENT_PRI 0x04

/** Error condition */
#define SLIRP_EVENT_ERR 0x08

/** Hang up */
#define SLIRP_EVENT_HUP 0x10

/** Slirp device configuration */
struct slirp_config {
	/** Configuration version */
	uint32_t version;
	/** Restrict to host loopback connections only */
	int restricted;
	/** IPv4 is enabled */
	bool in_enabled;
	/** IPv4 network */
	struct in_addr vnetwork;
	/** IPv4 netmask */
	struct in_addr vnetmask;
	/** IPv4 host server address */
	struct in_addr vhost;
	/** IPv6 is enabled */
	bool in6_enabled;
	/** IPv6 prefix */
	struct in6_addr vprefix_addr6;
	/** IPv6 prefix length */
	uint8_t vprefix_len;
	/** IPv6 host server address */
	struct in6_addr vhost6;
	/** Client hostname */
	const char *vhostname;
	/** TFTP server name */
	const char *tftp_server_name;
	/** TFTP path prefix */
	const char *tftp_path;
	/** Boot filename */
	const char *bootfile;
	/** DHCPv4 start address */
	struct in_addr vdhcp_start;
	/** DNS IPv4 address */
	struct in_addr vnameserver;
	/** DNS IPv6 address */
	struct in_addr vnameserver6;
	/** DNS search list */
	const char **vdnssearch;
	/** Domain name */
	const char *vdomainname;
	/** Interface MTU */
	size_t if_mtu;
	/** Interface MRU */
	size_t if_mru;
	/** Disable host loopback connections */
	bool disable_host_loopback;
	/** Enable emulation (apparently unsafe) */
	bool enable_emu;
};

/** Slirp device callbacks */
struct slirp_callbacks {
	/**
	 * Send packet
	 *
	 * @v buf		Data buffer
	 * @v len		Length of data
	 * @v device		Device opaque pointer
	 * @ret len		Consumed length (or negative on error)
	 */
	ssize_t ( __asmcall * send_packet ) ( const void *buf, size_t len,
					      void *device );
	/**
	 * Print an error message
	 *
	 * @v msg		Error message
	 * @v device		Device opaque pointer
	 */
	void ( __asmcall * guest_error ) ( const char *msg, void *device );
	/**
	 * Get virtual clock
	 *
	 * @v device		Device opaque pointer
	 * @ret clock_ns	Clock time in nanoseconds
	 */
	int64_t ( __asmcall * clock_get_ns ) ( void *device );
	/**
	 * Create a new timer
	 *
	 * @v callback		Timer callback
	 * @v opaque		Timer opaque pointer
	 * @v device		Device opaque pointer
	 * @ret timer		Timer
	 */
	void * ( __asmcall * timer_new ) ( void ( __asmcall * callback )
					   ( void *opaque ),
					   void *opaque, void *device );
	/**
	 * Delete a timer
	 *
	 * @v timer		Timer
	 * @v device		Device opaque pointer
	 */
	void ( __asmcall * timer_free ) ( void *timer, void *device );
	/**
	 * Set timer expiry time
	 *
	 * @v timer		Timer
	 * @v expire		Expiry time
	 * @v device		Device opaque pointer
	 */
	void ( __asmcall * timer_mod ) ( void *timer, int64_t expire,
					 void *device );
	/**
	 * Register file descriptor for polling
	 *
	 * @v fd		File descriptor
	 * @v device		Device opaque pointer
	 */
	void ( __asmcall * register_poll_fd ) ( int fd, void *device );
	/**
	 * Unregister file descriptor
	 *
	 * @v fd		File descriptor
	 * @v device		Device opaque pointer
	 */
	void ( __asmcall * unregister_poll_fd ) ( int fd, void *device );
	/**
	 * Notify that new events are ready
	 *
	 * @v device		Device opaque pointer
	 */
	void ( __asmcall * notify ) ( void *device );
};

#endif /* _IPXE_SLIRP_H */
