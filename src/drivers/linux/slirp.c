/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/in.h>
#include <ipxe/timer.h>
#include <ipxe/retry.h>
#include <ipxe/linux.h>
#include <ipxe/linux_api.h>
#include <ipxe/slirp.h>

/** @file
 *
 * Linux Slirp network driver
 *
 */

/** Maximum number of open file descriptors */
#define SLIRP_MAX_FDS 128

/** A Slirp network interface */
struct slirp_nic {
	/** The libslirp device object */
	struct Slirp *slirp;
	/** Polling file descriptor list */
	struct pollfd pollfds[SLIRP_MAX_FDS];
	/** Number of file descriptors */
	unsigned int numfds;
};

/** A Slirp alarm timer */
struct slirp_alarm {
	/** Slirp network interface */
	struct slirp_nic *slirp;
	/** Retry timer */
	struct retry_timer timer;
	/** Callback function */
	void ( __asmcall * callback ) ( void *opaque );
	/** Opaque value for callback function */
	void *opaque;
};

/** Default MAC address */
static const uint8_t slirp_default_mac[ETH_ALEN] =
	{ 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

/******************************************************************************
 *
 * Slirp interface
 *
 ******************************************************************************
 */

/**
 * Send packet
 *
 * @v buf		Data buffer
 * @v len		Length of data
 * @v device		Device opaque pointer
 * @ret len		Consumed length (or negative on error)
 */
static ssize_t __asmcall slirp_send_packet ( const void *buf, size_t len,
					     void *device ) {
	struct net_device *netdev = device;
	struct io_buffer *iobuf;

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( len );
	if ( ! iobuf )
		return -1;

	/* Populate I/O buffer */
	memcpy ( iob_put ( iobuf, len ), buf, len );

	/* Hand off to network stack */
	netdev_rx ( netdev, iobuf );

	return len;
}

/**
 * Print an error message
 *
 * @v msg		Error message
 * @v device		Device opaque pointer
 */
static void __asmcall slirp_guest_error ( const char *msg, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;

	DBGC ( slirp, "SLIRP %p error: %s\n", slirp, msg );
}

/**
 * Get virtual clock
 *
 * @v device		Device opaque pointer
 * @ret clock_ns	Clock time in nanoseconds
 */
static int64_t __asmcall slirp_clock_get_ns ( void *device __unused ) {
	int64_t time;

	time = currticks();
	return ( time * ( 1000000 / TICKS_PER_MS ) );
}

/**
 * Handle timer expiry
 *
 * @v timer		Retry timer
 * @v over		Failure indicator
 */
static void slirp_expired ( struct retry_timer *timer, int over __unused ) {
	struct slirp_alarm *alarm =
		container_of ( timer, struct slirp_alarm, timer );
	struct slirp_nic *slirp = alarm->slirp;

	/* Notify callback */
	DBGC ( slirp, "SLIRP %p timer fired\n", slirp );
	alarm->callback ( alarm->opaque );
}

/**
 * Create a new timer
 *
 * @v callback		Timer callback
 * @v opaque		Timer opaque pointer
 * @v device		Device opaque pointer
 * @ret timer		Timer
 */
static void * __asmcall
slirp_timer_new ( void ( __asmcall * callback ) ( void *opaque ),
		  void *opaque, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;
	struct slirp_alarm *alarm;

	/* Allocate timer */
	alarm = malloc ( sizeof ( *alarm ) );
	if ( ! alarm ) {
		DBGC ( slirp, "SLIRP %p could not allocate timer\n", slirp );
		return NULL;
	}

	/* Initialise timer */
	memset ( alarm, 0, sizeof ( *alarm ) );
	alarm->slirp = slirp;
	timer_init ( &alarm->timer, slirp_expired, NULL );
	alarm->callback = callback;
	alarm->opaque = opaque;
	DBGC ( slirp, "SLIRP %p timer %p has callback %p (%p)\n",
	       slirp, alarm, alarm->callback, alarm->opaque );

	return alarm;
}

/**
 * Delete a timer
 *
 * @v timer		Timer
 * @v device		Device opaque pointer
 */
static void __asmcall slirp_timer_free ( void *timer, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;
	struct slirp_alarm *alarm = timer;

	/* Ignore timers that failed to allocate */
	if ( ! alarm )
		return;

	/* Stop timer */
	stop_timer ( &alarm->timer );

	/* Free timer */
	free ( alarm );
	DBGC ( slirp, "SLIRP %p timer %p freed\n", slirp, alarm );
}

/**
 * Set timer expiry time
 *
 * @v timer		Timer
 * @v expire		Expiry time
 * @v device		Device opaque pointer
 */
static void __asmcall slirp_timer_mod ( void *timer, int64_t expire,
					void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;
	struct slirp_alarm *alarm = timer;
	int64_t timeout_ms;
	unsigned long timeout;

	/* Ignore timers that failed to allocate */
	if ( ! alarm )
		return;

	/* (Re)start timer */
	timeout_ms = ( expire - ( currticks() / TICKS_PER_MS ) );
	if ( timeout_ms < 0 )
		timeout_ms = 0;
	timeout = ( timeout_ms * TICKS_PER_MS );
	start_timer_fixed ( &alarm->timer, timeout );
	DBGC ( slirp, "SLIRP %p timer %p set for %ld ticks\n",
	       slirp, alarm, timeout );
}

/**
 * Register file descriptor for polling
 *
 * @v fd		File descriptor
 * @v device		Device opaque pointer
 */
static void __asmcall slirp_register_poll_fd ( int fd, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;

	DBGC ( slirp, "SLIRP %p registered FD %d\n", slirp, fd );
}

/**
 * Unregister file descriptor
 *
 * @v fd		File descriptor
 * @v device		Device opaque pointer
 */
static void __asmcall slirp_unregister_poll_fd ( int fd, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;

	DBGC ( slirp, "SLIRP %p unregistered FD %d\n", slirp, fd );
}

/**
 * Notify that new events are ready
 *
 * @v device		Device opaque pointer
 */
static void __asmcall slirp_notify ( void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;

	DBGC2 ( slirp, "SLIRP %p notified\n", slirp );
}

/** Slirp callbacks */
static struct slirp_callbacks slirp_callbacks = {
	.send_packet		= slirp_send_packet,
	.guest_error		= slirp_guest_error,
	.clock_get_ns		= slirp_clock_get_ns,
	.timer_new		= slirp_timer_new,
	.timer_free		= slirp_timer_free,
	.timer_mod		= slirp_timer_mod,
	.register_poll_fd	= slirp_register_poll_fd,
	.unregister_poll_fd	= slirp_unregister_poll_fd,
	.notify			= slirp_notify,
};

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int slirp_open ( struct net_device *netdev ) {
	struct slirp_nic *slirp = netdev->priv;

	/* Nothing to do */
	DBGC ( slirp, "SLIRP %p opened\n", slirp );

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void slirp_close ( struct net_device *netdev ) {
	struct slirp_nic *slirp = netdev->priv;

	/* Nothing to do */
	DBGC ( slirp, "SLIRP %p closed\n", slirp );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int slirp_transmit ( struct net_device *netdev,
			    struct io_buffer *iobuf ) {
	struct slirp_nic *slirp = netdev->priv;

	/* Transmit packet */
	linux_slirp_input ( slirp->slirp, iobuf->data, iob_len ( iobuf ) );
	netdev_tx_complete ( netdev, iobuf );

	return 0;
}

/**
 * Add polling file descriptor
 *
 * @v fd		File descriptor
 * @v events		Events of interest
 * @v device		Device opaque pointer
 * @ret index		File descriptor index
 */
static int __asmcall slirp_add_poll ( int fd, int events, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;
	struct pollfd *pollfd;
	unsigned int index;

	/* Fail if too many descriptors are registered */
	if ( slirp->numfds >= SLIRP_MAX_FDS ) {
		DBGC ( slirp, "SLIRP %p too many file descriptors\n", slirp );
		return -1;
	}

	/* Populate polling file descriptor */
	index = slirp->numfds++;
	pollfd = &slirp->pollfds[index];
	pollfd->fd = fd;
	pollfd->events = 0;
	if ( events & SLIRP_EVENT_IN )
		pollfd->events |= POLLIN;
	if ( events & SLIRP_EVENT_OUT )
		pollfd->events |= POLLOUT;
	if ( events & SLIRP_EVENT_PRI )
		pollfd->events |= POLLPRI;
	if ( events & SLIRP_EVENT_ERR )
		pollfd->events |= POLLERR;
	if ( events & SLIRP_EVENT_HUP )
		pollfd->events |= ( POLLHUP | POLLRDHUP );
	DBGCP ( slirp, "SLIRP %p polling FD %d event mask %#04x(%#04x)\n",
		slirp, fd, events, pollfd->events );

	return index;
}

/**
 * Get returned events for a file descriptor
 *
 * @v index		File descriptor index
 * @v device		Device opaque pointer
 * @ret events		Returned events
 */
static int __asmcall slirp_get_revents ( int index, void *device ) {
	struct net_device *netdev = device;
	struct slirp_nic *slirp = netdev->priv;
	int revents;
	int events;

	/* Ignore failed descriptors */
	if ( index < 0 )
		return 0;

	/* Collect events */
	revents = slirp->pollfds[index].revents;
	events = 0;
	if ( revents & POLLIN )
		events |= SLIRP_EVENT_IN;
	if ( revents & POLLOUT )
		events |= SLIRP_EVENT_OUT;
	if ( revents & POLLPRI )
		events |= SLIRP_EVENT_PRI;
	if ( revents & POLLERR )
		events |= SLIRP_EVENT_ERR;
	if ( revents & ( POLLHUP | POLLRDHUP ) )
		events |= SLIRP_EVENT_HUP;
	if ( events ) {
		DBGC2 ( slirp, "SLIRP %p polled FD %d events %#04x(%#04x)\n",
			slirp, slirp->pollfds[index].fd, events, revents );
	}

	return events;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void slirp_poll ( struct net_device *netdev ) {
	struct slirp_nic *slirp = netdev->priv;
	uint32_t timeout = 0;
	int ready;
	int error;

	/* Rebuild polling file descriptor list */
	slirp->numfds = 0;
	linux_slirp_pollfds_fill ( slirp->slirp, &timeout,
				   slirp_add_poll, netdev );

	/* Poll descriptors */
	ready = linux_poll ( slirp->pollfds, slirp->numfds, 0 );
	error = ( ready == -1 );
	linux_slirp_pollfds_poll ( slirp->slirp, error, slirp_get_revents,
				   netdev );

	/* Record polling errors */
	if ( error ) {
		DBGC ( slirp, "SLIRP %p poll failed: %s\n",
		       slirp, linux_strerror ( linux_errno ) );
		netdev_rx_err ( netdev, NULL, -ELINUX ( linux_errno ) );
	}
}

/** Network device operations */
static struct net_device_operations slirp_operations = {
	.open		= slirp_open,
	.close		= slirp_close,
	.transmit	= slirp_transmit,
	.poll		= slirp_poll,
};

/******************************************************************************
 *
 * Linux driver interface
 *
 ******************************************************************************
 */

/**
 * Probe device
 *
 * @v linux		Linux device
 * @v request		Device creation request
 * @ret rc		Return status code
 */
static int slirp_probe ( struct linux_device *linux,
			 struct linux_device_request *request ) {
	struct net_device *netdev;
	struct slirp_nic *slirp;
	struct slirp_config config;
	int rc;

	/* Allocate device */
	netdev = alloc_etherdev ( sizeof ( *slirp ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &slirp_operations );
	linux_set_drvdata ( linux, netdev );
	snprintf ( linux->dev.name, sizeof ( linux->dev.name ), "host" );
	netdev->dev = &linux->dev;
	memcpy ( netdev->hw_addr, slirp_default_mac, ETH_ALEN );
	slirp = netdev->priv;
	memset ( slirp, 0, sizeof ( *slirp ) );

	/* Apply requested settings */
	linux_apply_settings ( &request->settings,
			       netdev_settings ( netdev ) );

	/* Initialise default configuration (matching qemu) */
	memset ( &config, 0, sizeof ( config ) );
	config.version = 1;
	config.in_enabled = true;
	config.vnetwork.s_addr = htonl ( 0x0a000200 ); /* 10.0.2.0 */
	config.vnetmask.s_addr = htonl ( 0xffffff00 ); /* 255.255.255.0 */
	config.vhost.s_addr = htonl ( 0x0a000202 ); /* 10.0.2.2 */
	config.in6_enabled = true;
	config.vdhcp_start.s_addr = htonl ( 0x0a00020f ); /* 10.0.2.15 */
	config.vnameserver.s_addr = htonl ( 0x0a000203 ); /* 10.0.2.3 */

	/* Instantiate device */
	slirp->slirp = linux_slirp_new ( &config, &slirp_callbacks, netdev );
	if ( ! slirp->slirp ) {
		DBGC ( slirp, "SLIRP could not instantiate\n" );
		rc = -ENODEV;
		goto err_new;
	}

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	/* Set link up since there is no concept of link state */
	netdev_link_up ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register:
	linux_slirp_cleanup ( slirp->slirp );
 err_new:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove device
 *
 * @v linux		Linux device
 */
static void slirp_remove ( struct linux_device *linux ) {
	struct net_device *netdev = linux_get_drvdata ( linux );
	struct slirp_nic *slirp = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Shut down device */
	linux_slirp_cleanup ( slirp->slirp );

	/* Free network device */
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** Slirp driver */
struct linux_driver slirp_driver __linux_driver = {
	.name = "slirp",
	.probe = slirp_probe,
	.remove = slirp_remove,
	.can_probe = 1,
};
