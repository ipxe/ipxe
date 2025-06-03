/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/dhcppkt.h>
#include <ipxe/init.h>
#include <ipxe/netdevice.h>
#include <ipxe/vlan.h>
#include <ipxe/uaccess.h>
#include <ipxe/cachedhcp.h>

/** @file
 *
 * Cached DHCP packet
 *
 */

/** A cached DHCP packet */
struct cached_dhcp_packet {
	/** Settings block name */
	const char *name;
	/** DHCP packet (if any) */
	struct dhcp_packet *dhcppkt;
	/** VLAN tag (if applicable) */
	unsigned int vlan;
	/** Flags */
	unsigned int flags;
};

/** Cached DHCP packet should be retained */
#define CACHEDHCP_RETAIN 0x0001

/** Cached DHCP packet has been used */
#define CACHEDHCP_USED 0x0002

/** Cached DHCPACK */
struct cached_dhcp_packet cached_dhcpack = {
	.name = DHCP_SETTINGS_NAME,
	.flags = CACHEDHCP_RETAIN,
};

/** Cached ProxyDHCPOFFER */
struct cached_dhcp_packet cached_proxydhcp = {
	.name = PROXYDHCP_SETTINGS_NAME,
};

/** Cached PXEBSACK */
struct cached_dhcp_packet cached_pxebs = {
	.name = PXEBS_SETTINGS_NAME,
};

/** List of cached DHCP packets */
static struct cached_dhcp_packet *cached_packets[] = {
	&cached_dhcpack,
	&cached_proxydhcp,
	&cached_pxebs,
};

/** Colour for debug messages */
#define colour &cached_dhcpack

/**
 * Free cached DHCP packet
 *
 * @v cache		Cached DHCP packet
 */
static void cachedhcp_free ( struct cached_dhcp_packet *cache ) {

	dhcppkt_put ( cache->dhcppkt );
	cache->dhcppkt = NULL;
}

/**
 * Apply cached DHCP packet settings
 *
 * @v cache		Cached DHCP packet
 * @v netdev		Network device, or NULL
 * @ret rc		Return status code
 */
static int cachedhcp_apply ( struct cached_dhcp_packet *cache,
			     struct net_device *netdev ) {
	struct settings *settings = NULL;
	struct ll_protocol *ll_protocol;
	const uint8_t *chaddr;
	uint8_t *hw_addr;
	uint8_t *ll_addr;
	size_t ll_addr_len;
	int rc;

	/* Do nothing if cache is empty or already in use */
	if ( ( ! cache->dhcppkt ) || ( cache->flags & CACHEDHCP_USED ) )
		return 0;
	chaddr = cache->dhcppkt->dhcphdr->chaddr;

	/* Handle association with network device, if specified */
	if ( netdev ) {
		hw_addr = netdev->hw_addr;
		ll_addr = netdev->ll_addr;
		ll_protocol = netdev->ll_protocol;
		ll_addr_len = ll_protocol->ll_addr_len;

		/* If cached packet's MAC address matches the network
		 * device's permanent MAC address, then assume that
		 * the permanent MAC address ought to be the network
		 * device's current link-layer address.
		 *
		 * This situation can arise when the PXE ROM does not
		 * understand the system-specific mechanism for
		 * overriding the MAC address, and so uses the
		 * permanent MAC address instead.  We choose to match
		 * this behaviour in order to minimise surprise.
		 */
		if ( memcmp ( hw_addr, chaddr, ll_addr_len ) == 0 ) {
			if ( memcmp ( hw_addr, ll_addr, ll_addr_len ) != 0 ) {
				DBGC ( colour, "CACHEDHCP %s resetting %s MAC "
				       "%s ", cache->name, netdev->name,
				       ll_protocol->ntoa ( ll_addr ) );
				DBGC ( colour, "-> %s\n",
				       ll_protocol->ntoa ( hw_addr ) );
			}
			memcpy ( ll_addr, hw_addr, ll_addr_len );
		}

		/* Do nothing unless cached packet's MAC address
		 * matches this network device.
		 */
		if ( memcmp ( ll_addr, chaddr, ll_addr_len ) != 0 ) {
			DBGC ( colour, "CACHEDHCP %s %s does not match %s\n",
			       cache->name, ll_protocol->ntoa ( chaddr ),
			       netdev->name );
			return 0;
		}

		/* Do nothing unless cached packet's VLAN tag matches
		 * this network device.
		 */
		if ( vlan_tag ( netdev ) != cache->vlan ) {
			DBGC ( colour, "CACHEDHCP %s VLAN %d does not match "
			       "%s\n", cache->name, cache->vlan,
			       netdev->name );
			return 0;
		}

		/* Use network device's settings block */
		settings = netdev_settings ( netdev );
		DBGC ( colour, "CACHEDHCP %s is for %s\n",
		       cache->name, netdev->name );
	}

	/* Register settings */
	if ( ( rc = register_settings ( &cache->dhcppkt->settings, settings,
					cache->name ) ) != 0 ) {
		DBGC ( colour, "CACHEDHCP %s could not register settings: %s\n",
		       cache->name, strerror ( rc ) );
		return rc;
	}

	/* Mark as used */
	cache->flags |= CACHEDHCP_USED;

	/* Free cached DHCP packet, if applicable */
	if ( ! ( cache->flags & CACHEDHCP_RETAIN ) )
		cachedhcp_free ( cache );

	return 0;
}

/**
 * Record cached DHCP packet
 *
 * @v cache		Cached DHCP packet
 * @v vlan		VLAN tag, if any
 * @v data		DHCPACK packet buffer
 * @v max_len		Maximum possible length
 * @ret rc		Return status code
 */
int cachedhcp_record ( struct cached_dhcp_packet *cache, unsigned int vlan,
		       const void *data, size_t max_len ) {
	struct dhcp_packet *dhcppkt;
	struct dhcp_packet *tmp;
	struct dhcphdr *dhcphdr;
	unsigned int i;
	size_t len;

	/* Free any existing cached packet */
	cachedhcp_free ( cache );

	/* Allocate and populate DHCP packet */
	dhcppkt = zalloc ( sizeof ( *dhcppkt ) + max_len );
	if ( ! dhcppkt ) {
		DBGC ( colour, "CACHEDHCP %s could not allocate copy\n",
		       cache->name );
		return -ENOMEM;
	}
	dhcphdr = ( ( ( void * ) dhcppkt ) + sizeof ( *dhcppkt ) );
	memcpy ( dhcphdr, data, max_len );
	dhcppkt_init ( dhcppkt, dhcphdr, max_len );

	/* Shrink packet to required length.  If reallocation fails,
	 * just continue to use the original packet and waste the
	 * unused space.
	 */
	len = dhcppkt_len ( dhcppkt );
	assert ( len <= max_len );
	tmp = realloc ( dhcppkt, ( sizeof ( *dhcppkt ) + len ) );
	if ( tmp )
		dhcppkt = tmp;

	/* Reinitialise packet at new address */
	dhcphdr = ( ( ( void * ) dhcppkt ) + sizeof ( *dhcppkt ) );
	dhcppkt_init ( dhcppkt, dhcphdr, len );

	/* Discard duplicate packets, since some PXE stacks (including
	 * iPXE itself) will report the DHCPACK packet as the PXEBSACK
	 * if no separate PXEBSACK exists.
	 */
	for ( i = 0 ; i < ( sizeof ( cached_packets ) /
			    sizeof ( cached_packets[0] ) ) ; i++ ) {
		tmp = cached_packets[i]->dhcppkt;
		if ( tmp && ( dhcppkt_len ( tmp ) == len ) &&
		     ( memcmp ( tmp->dhcphdr, dhcppkt->dhcphdr, len ) == 0 ) ) {
			DBGC ( colour, "CACHEDHCP %s duplicates %s\n",
			       cache->name, cached_packets[i]->name );
			dhcppkt_put ( dhcppkt );
			return -EEXIST;
		}
	}

	/* Store as cached packet */
	DBGC ( colour, "CACHEDHCP %s at %#08lx+%#zx/%#zx\n", cache->name,
	       virt_to_phys ( data ), len, max_len );
	cache->dhcppkt = dhcppkt;
	cache->vlan = vlan;

	return 0;
}

/**
 * Cached DHCP packet early startup function
 *
 */
static void cachedhcp_startup_early ( void ) {

	/* Apply cached ProxyDHCPOFFER, if any */
	cachedhcp_apply ( &cached_proxydhcp, NULL );
	cachedhcp_free ( &cached_proxydhcp );

	/* Apply cached PXEBSACK, if any */
	cachedhcp_apply ( &cached_pxebs, NULL );
	cachedhcp_free ( &cached_pxebs );
}

/**
 * Cache DHCP packet late startup function
 *
 */
static void cachedhcp_startup_late ( void ) {

	/* Clear retention flag */
	cached_dhcpack.flags &= ~CACHEDHCP_RETAIN;

	/* Free cached DHCPACK, if used by a network device */
	if ( cached_dhcpack.flags & CACHEDHCP_USED )
		cachedhcp_free ( &cached_dhcpack );

	/* Report unclaimed DHCPACK, if any.  Do not free yet, since
	 * it may still be claimed by a dynamically created device
	 * such as a VLAN device.
	 */
	if ( cached_dhcpack.dhcppkt ) {
		DBGC ( colour, "CACHEDHCP %s unclaimed\n",
		       cached_dhcpack.name );
	}
}

/**
 * Cached DHCP packet shutdown function
 *
 * @v booting		System is shutting down for OS boot
 */
static void cachedhcp_shutdown ( int booting __unused ) {

	/* Free cached DHCPACK, if any */
	if ( cached_dhcpack.dhcppkt ) {
		DBGC ( colour, "CACHEDHCP %s never claimed\n",
		       cached_dhcpack.name );
	}
	cachedhcp_free ( &cached_dhcpack );
}

/** Cached DHCP packet early startup function */
struct startup_fn cachedhcp_early_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "cachedhcp1",
	.startup = cachedhcp_startup_early,
};

/** Cached DHCP packet late startup function */
struct startup_fn cachedhcp_late_fn __startup_fn ( STARTUP_LATE ) = {
	.name = "cachedhcp2",
	.startup = cachedhcp_startup_late,
	.shutdown = cachedhcp_shutdown,
};

/**
 * Apply cached DHCPACK to network device, if applicable
 *
 * @v netdev		Network device
 * @v priv		Private data
 * @ret rc		Return status code
 */
static int cachedhcp_probe ( struct net_device *netdev, void *priv __unused ) {

	/* Apply cached DHCPACK to network device, if applicable */
	return cachedhcp_apply ( &cached_dhcpack, netdev );
}

/** Cached DHCP packet network device driver */
struct net_driver cachedhcp_driver __net_driver = {
	.name = "cachedhcp",
	.probe = cachedhcp_probe,
};

/**
 * Recycle cached DHCPACK
 *
 * @v netdev		Network device
 * @v priv		Private data
 */
void cachedhcp_recycle ( struct net_device *netdev ) {
	struct cached_dhcp_packet *cache = &cached_dhcpack;
	struct settings *settings;

	/* Return DHCPACK to cache, if applicable */
	settings = find_child_settings ( netdev_settings ( netdev ),
					 cache->name );
	if ( cache->dhcppkt && ( settings == &cache->dhcppkt->settings ) ) {
		DBGC ( colour, "CACHEDHCP %s recycled from %s\n",
		       cache->name, netdev->name );
		assert ( cache->flags & CACHEDHCP_USED );
		unregister_settings ( settings );
		cache->flags &= ~CACHEDHCP_USED;
	}
}
