/*
 * Copyright (C) 2010 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/if_ether.h>
#include <ipxe/if_arp.h>
#include <ipxe/iobuf.h>
#include <ipxe/interface.h>
#include <ipxe/xfer.h>
#include <ipxe/netdevice.h>
#include <ipxe/features.h>
#include <ipxe/crc32.h>
#include <ipxe/fc.h>
#include <ipxe/fcoe.h>

/** @file
 *
 * FCoE protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "FCoE", DHCP_EB_FEATURE_FCOE, 1 );

/** An FCoE port */
struct fcoe_port {
	/** Reference count */
	struct refcnt refcnt;
	/** List of FCoE ports */
	struct list_head list;
	/** Transport interface */
	struct interface transport;
	/** Network device */
	struct net_device *netdev;
	/** FCoE forwarder MAC address */
	uint8_t fcf_ll_addr[ETH_ALEN];
};

/** List of FCoE ports */
static LIST_HEAD ( fcoe_ports );

struct net_protocol fcoe_protocol __net_protocol;

/** Default FCoE forwarded MAC address */
uint8_t fcoe_default_fcf_ll_addr[ETH_ALEN] =
	{ 0x0e, 0xfc, 0x00, 0xff, 0xff, 0xfe };

/**
 * Identify FCoE port by network device
 *
 * @v netdev		Network device
 * @ret fcoe		FCoE port, or NULL
 */
static struct fcoe_port * fcoe_demux ( struct net_device *netdev ) {
	struct fcoe_port *fcoe;

	list_for_each_entry ( fcoe, &fcoe_ports, list ) {
		if ( fcoe->netdev == netdev )
			return fcoe;
	}
	return NULL;
}

/**
 * Transmit FCoE packet
 *
 * @v fcoe		FCoE port
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int fcoe_deliver ( struct fcoe_port *fcoe,
			  struct io_buffer *iobuf,
			  struct xfer_metadata *meta __unused ) {
	struct fc_frame_header *fchdr = iobuf->data;
	struct fcoe_header *fcoehdr;
	struct fcoe_footer *fcoeftr;
	uint32_t crc;
	int rc;

	/* Calculate CRC */
	crc = crc32_le ( ~((uint32_t)0), iobuf->data, iob_len ( iobuf ) );

	/* Create FCoE header */
	fcoehdr = iob_push ( iobuf, sizeof ( *fcoehdr ) );
	memset ( fcoehdr, 0, sizeof ( *fcoehdr ) );
	fcoehdr->sof = ( ( fchdr->seq_cnt == ntohs ( 0 ) ) ?
			 FCOE_SOF_I3 : FCOE_SOF_N3 );
	fcoeftr = iob_put ( iobuf, sizeof ( *fcoeftr ) );
	memset ( fcoeftr, 0, sizeof ( *fcoeftr ) );
	fcoeftr->crc = cpu_to_le32 ( crc ^ ~((uint32_t)0) );
	fcoeftr->eof = ( ( fchdr->f_ctl_es & FC_F_CTL_ES_END ) ?
			 FCOE_EOF_T : FCOE_EOF_N );

	/* Transmit packet */
	if ( ( rc = net_tx ( iob_disown ( iobuf ), fcoe->netdev, &fcoe_protocol,
			     fcoe->fcf_ll_addr ) ) != 0 ) {
		DBGC ( fcoe, "FCoE %s could not transmit: %s\n",
		       fcoe->netdev->name, strerror ( rc ) );
		goto done;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Allocate FCoE I/O buffer
 *
 * @v len		Payload length
 * @ret iobuf		I/O buffer, or NULL
 */
static struct io_buffer * fcoe_alloc_iob ( struct fcoe_port *fcoe __unused,
					   size_t len ) {
	struct io_buffer *iobuf;

	iobuf = alloc_iob ( MAX_LL_HEADER_LEN + sizeof ( struct fcoe_header ) +
			    len + sizeof ( struct fcoe_footer ) );
	if ( iobuf ) {
		iob_reserve ( iobuf, ( MAX_LL_HEADER_LEN +
				       sizeof ( struct fcoe_header ) ) );
	}
	return iobuf;
}

/**
 * Process incoming FCoE packets
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_source		Link-layer source address
 * @ret rc		Return status code
 */
static int fcoe_rx ( struct io_buffer *iobuf,
		     struct net_device *netdev,
		     const void *ll_source ) {
	struct fcoe_header *fcoehdr;
	struct fcoe_footer *fcoeftr;
	struct fcoe_port *fcoe;
	int rc;

	/* Identify FCoE port */
	if ( ( fcoe = fcoe_demux ( netdev ) ) == NULL ) {
		DBG ( "FCoE received frame for net device %s missing FCoE "
		      "port\n", netdev->name );
		rc = -ENOTCONN;
		goto done;
	}

	/* Sanity check */
	if ( iob_len ( iobuf ) < ( sizeof ( *fcoehdr ) + sizeof ( *fcoeftr ) )){
		DBGC ( fcoe, "FCoE %s received under-length frame (%zd "
		       "bytes)\n", fcoe->netdev->name, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto done;
	}

	/* Strip header and footer */
	fcoehdr = iobuf->data;
	iob_pull ( iobuf, sizeof ( *fcoehdr ) );
	fcoeftr = ( iobuf->data + iob_len ( iobuf ) - sizeof ( *fcoeftr ) );
	iob_unput ( iobuf, sizeof ( *fcoeftr ) );

	/* Validity checks */
	if ( fcoehdr->version != FCOE_FRAME_VER ) {
		DBGC ( fcoe, "FCoE %s received unsupported frame version "
		       "%02x\n", fcoe->netdev->name, fcoehdr->version );
		rc = -EPROTONOSUPPORT;
		goto done;
	}
	if ( ! ( ( fcoehdr->sof == FCOE_SOF_I3 ) ||
		 ( fcoehdr->sof == FCOE_SOF_N3 ) ) ) {
		DBGC ( fcoe, "FCoE %s received unsupported start-of-frame "
		       "delimiter %02x\n", fcoe->netdev->name, fcoehdr->sof );
		rc = -EINVAL;
		goto done;
	}
	if ( ( le32_to_cpu ( fcoeftr->crc ) ^ ~((uint32_t)0) ) !=
	     crc32_le ( ~((uint32_t)0), iobuf->data, iob_len ( iobuf ) ) ) {
		DBGC ( fcoe, "FCoE %s received invalid CRC\n",
		       fcoe->netdev->name );
		rc = -EINVAL;
		goto done;
	}
	if ( ! ( ( fcoeftr->eof == FCOE_EOF_N ) ||
		 ( fcoeftr->eof == FCOE_EOF_T ) ) ) {
		DBGC ( fcoe, "FCoE %s received unsupported end-of-frame "
		       "delimiter %02x\n", fcoe->netdev->name, fcoeftr->eof );
		rc = -EINVAL;
		goto done;
	}

	/* Record FCF address */
	memcpy ( &fcoe->fcf_ll_addr, ll_source, sizeof ( fcoe->fcf_ll_addr ) );

	/* Hand off via transport interface */
	if ( ( rc = xfer_deliver_iob ( &fcoe->transport,
				       iob_disown ( iobuf ) ) ) != 0 ) {
		DBGC ( fcoe, "FCoE %s could not deliver frame: %s\n",
		       fcoe->netdev->name, strerror ( rc ) );
		goto done;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Check FCoE flow control window
 *
 * @v fcoe		FCoE port
 * @ret len		Length of window
 */
static size_t fcoe_window ( struct fcoe_port *fcoe ) {
	struct net_device *netdev = fcoe->netdev;

	return ( ( netdev_is_open ( netdev ) && netdev_link_ok ( netdev ) ) ?
		 ~( ( size_t ) 0 ) : 0 );
}

/**
 * Close FCoE port
 *
 * @v fcoe		FCoE port
 * @v rc		Reason for close
 */
static void fcoe_close ( struct fcoe_port *fcoe, int rc ) {

	intf_shutdown ( &fcoe->transport, rc );
	netdev_put ( fcoe->netdev );
	list_del ( &fcoe->list );
	ref_put ( &fcoe->refcnt );
}

/** FCoE transport interface operations */
static struct interface_operation fcoe_transport_op[] = {
	INTF_OP ( xfer_deliver, struct fcoe_port *, fcoe_deliver ),
	INTF_OP ( xfer_alloc_iob, struct fcoe_port *, fcoe_alloc_iob ),
	INTF_OP ( xfer_window, struct fcoe_port *, fcoe_window ),
	INTF_OP ( intf_close, struct fcoe_port *, fcoe_close ),
};

/** FCoE transport interface descriptor */
static struct interface_descriptor fcoe_transport_desc =
	INTF_DESC ( struct fcoe_port, transport, fcoe_transport_op );

/**
 * Create FCoE port
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int fcoe_probe ( struct net_device *netdev ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct fcoe_port *fcoe;
	union fcoe_name node_wwn;
	union fcoe_name port_wwn;
	int rc;

	/* Sanity check */
	if ( ll_protocol->ll_proto != htons ( ARPHRD_ETHER ) ) {
		/* Not an error; simply skip this net device */
		DBG ( "FCoE skipping non-Ethernet device %s\n", netdev->name );
		rc = 0;
		goto err_non_ethernet;
	}
	assert ( ll_protocol->ll_addr_len == sizeof ( fcoe->fcf_ll_addr ) );

	/* Allocate and initialise structure */
	fcoe = zalloc ( sizeof ( *fcoe ) );
	if ( ! fcoe ) {
		rc = -ENOMEM;
		goto err_zalloc;
	}
	ref_init ( &fcoe->refcnt, NULL );
	intf_init ( &fcoe->transport, &fcoe_transport_desc, &fcoe->refcnt );
	fcoe->netdev = netdev_get ( netdev );

	/* Construct node and port names */
	node_wwn.fcoe.authority = htons ( FCOE_AUTHORITY_IEEE );
	memcpy ( &node_wwn.fcoe.mac, netdev->ll_addr,
		 sizeof ( node_wwn.fcoe.mac ) );
	port_wwn.fcoe.authority = htons ( FCOE_AUTHORITY_IEEE_EXTENDED );
	memcpy ( &port_wwn.fcoe.mac, netdev->ll_addr,
		 sizeof ( port_wwn.fcoe.mac ) );

	/* Construct initial FCF address */
	memcpy ( &fcoe->fcf_ll_addr, &fcoe_default_fcf_ll_addr,
		 sizeof ( fcoe->fcf_ll_addr ) );

	DBGC ( fcoe, "FCoE %s is %s", fcoe->netdev->name,
	       fc_ntoa ( &node_wwn.fc ) );
	DBGC ( fcoe, " port %s\n", fc_ntoa ( &port_wwn.fc ) );

	/* Attach Fibre Channel port */
	if ( ( rc = fc_port_open ( &fcoe->transport, &node_wwn.fc,
				   &port_wwn.fc ) ) != 0 )
		goto err_fc_create;

	/* Transfer reference to port list */
	list_add ( &fcoe->list, &fcoe_ports );
	return 0;

 err_fc_create:
	netdev_put ( fcoe->netdev );
 err_zalloc:
 err_non_ethernet:
	return rc;
}

/**
 * Handle FCoE port device or link state change
 *
 * @v netdev		Network device
 */
static void fcoe_notify ( struct net_device *netdev ) {
	struct fcoe_port *fcoe;

	/* Sanity check */
	if ( ( fcoe = fcoe_demux ( netdev ) ) == NULL ) {
		DBG ( "FCoE notification for net device %s missing FCoE "
		      "port\n", netdev->name );
		return;
	}

	/* Send notification of potential window change */
	xfer_window_changed ( &fcoe->transport );
}

/**
 * Destroy FCoE port
 *
 * @v netdev		Network device
 */
static void fcoe_remove ( struct net_device *netdev ) {
	struct fcoe_port *fcoe;

	/* Sanity check */
	if ( ( fcoe = fcoe_demux ( netdev ) ) == NULL ) {
		DBG ( "FCoE removal of net device %s missing FCoE port\n",
		      netdev->name );
		return;
	}

	/* Close FCoE device */
	fcoe_close ( fcoe, 0 );
}

/** FCoE driver */
struct net_driver fcoe_driver __net_driver = {
	.name = "FCoE",
	.probe = fcoe_probe,
	.notify = fcoe_notify,
	.remove = fcoe_remove,
};

/** FCoE protocol */
struct net_protocol fcoe_protocol __net_protocol = {
	.name = "FCoE",
	.net_proto = htons ( ETH_P_FCOE ),
	.rx = fcoe_rx,
};
