/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ipxe/console.h>
#include <ipxe/netdevice.h>
#include <ipxe/device.h>
#include <ipxe/job.h>
#include <ipxe/monojob.h>
#include <ipxe/nap.h>
#include <usr/ifmgmt.h>

/** @file
 *
 * Network interface management
 *
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
int ifopen ( struct net_device *netdev ) {
	int rc;

	if ( ( rc = netdev_open ( netdev ) ) != 0 ) {
		printf ( "Could not open %s: %s\n",
			 netdev->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
void ifclose ( struct net_device *netdev ) {
	netdev_close ( netdev );
}

/**
 * Print network device error breakdown
 *
 * @v stats		Network device statistics
 * @v prefix		Message prefix
 */
static void ifstat_errors ( struct net_device_stats *stats,
			    const char *prefix ) {
	unsigned int i;

	for ( i = 0 ; i < ( sizeof ( stats->errors ) /
			    sizeof ( stats->errors[0] ) ) ; i++ ) {
		if ( stats->errors[i].count )
			printf ( "  [%s: %d x \"%s\"]\n", prefix,
				 stats->errors[i].count,
				 strerror ( stats->errors[i].rc ) );
	}
}

/**
 * Print status of network device
 *
 * @v netdev		Network device
 */
void ifstat ( struct net_device *netdev ) {
	printf ( "%s: %s using %s on %s (%s)\n"
		 "  [Link:%s, TX:%d TXE:%d RX:%d RXE:%d]\n",
		 netdev->name, netdev_addr ( netdev ),
		 netdev->dev->driver_name, netdev->dev->name,
		 ( netdev_is_open ( netdev ) ? "open" : "closed" ),
		 ( netdev_link_ok ( netdev ) ? "up" : "down" ),
		 netdev->tx_stats.good, netdev->tx_stats.bad,
		 netdev->rx_stats.good, netdev->rx_stats.bad );
	if ( ! netdev_link_ok ( netdev ) ) {
		printf ( "  [Link status: %s]\n",
			 strerror ( netdev->link_rc ) );
	}
	ifstat_errors ( &netdev->tx_stats, "TXE" );
	ifstat_errors ( &netdev->rx_stats, "RXE" );
}

/** Network device poller */
struct ifpoller {
	/** Job control interface */
	struct interface job;
	/** Network device */
	struct net_device *netdev;
	/**
	 * Check progress
	 *
	 * @v ifpoller		Network device poller
	 * @ret ongoing_rc	Ongoing job status code (if known)
	 */
	int ( * progress ) ( struct ifpoller *ifpoller );
};

/**
 * Report network device poller progress
 *
 * @v ifpoller		Network device poller
 * @v progress		Progress report to fill in
 * @ret ongoing_rc	Ongoing job status code (if known)
 */
static int ifpoller_progress ( struct ifpoller *ifpoller,
			       struct job_progress *progress __unused ) {

	/* Reduce CPU utilisation */
	cpu_nap();

	/* Hand off to current progress checker */
	return ifpoller->progress ( ifpoller );
}

/** Network device poller operations */
static struct interface_operation ifpoller_job_op[] = {
	INTF_OP ( job_progress, struct ifpoller *, ifpoller_progress ),
};

/** Network device poller descriptor */
static struct interface_descriptor ifpoller_job_desc =
	INTF_DESC ( struct ifpoller, job, ifpoller_job_op );

/**
 * Poll network device until completion
 *
 * @v netdev		Network device
 * @v timeout		Timeout period, in ticks
 * @v progress		Method to check progress
 * @ret rc		Return status code
 */
static int ifpoller_wait ( struct net_device *netdev, unsigned long timeout,
			   int ( * progress ) ( struct ifpoller *ifpoller ) ) {
	static struct ifpoller ifpoller = {
		.job = INTF_INIT ( ifpoller_job_desc ),
	};

	ifpoller.netdev = netdev;
	ifpoller.progress = progress;
	intf_plug_plug ( &monojob, &ifpoller.job );
	return monojob_wait ( "", timeout );
}

/**
 * Check link-up progress
 *
 * @v ifpoller		Network device poller
 * @ret ongoing_rc	Ongoing job status code (if known)
 */
static int iflinkwait_progress ( struct ifpoller *ifpoller ) {
	struct net_device *netdev = ifpoller->netdev;
	int ongoing_rc = netdev->link_rc;

	/* Terminate successfully if link is up */
	if ( ongoing_rc == 0 )
		intf_close ( &ifpoller->job, 0 );

	/* Otherwise, report link status as ongoing job status */
	return ongoing_rc;
}

/**
 * Wait for link-up, with status indication
 *
 * @v netdev		Network device
 * @v timeout		Timeout period, in ticks
 */
int iflinkwait ( struct net_device *netdev, unsigned long timeout ) {
	int rc;

	/* Ensure device is open */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;

	/* Return immediately if link is already up */
	netdev_poll ( netdev );
	if ( netdev_link_ok ( netdev ) )
		return 0;

	/* Wait for link-up */
	printf ( "Waiting for link-up on %s", netdev->name );
	return ifpoller_wait ( netdev, timeout, iflinkwait_progress );
}
