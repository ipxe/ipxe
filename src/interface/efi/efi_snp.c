/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include <gpxe/in.h>
#include <gpxe/pci.h>
#include <gpxe/efi/efi.h>
#include <gpxe/efi/Protocol/DriverBinding.h>
#include <gpxe/efi/Protocol/PciIo.h>
#include <gpxe/efi/Protocol/SimpleNetwork.h>

/** @file
 *
 * gPXE EFI SNP interface
 *
 */

/** An SNP device */
struct efi_snp_device {
	/** The underlying gPXE network device */
	struct net_device *netdev;
	/** The SNP structure itself */
	EFI_SIMPLE_NETWORK_PROTOCOL snp;
	/** The SNP "mode" (parameters) */
	EFI_SIMPLE_NETWORK_MODE mode;
	/** Outstanding TX packet count (via "interrupt status")
	 *
	 * Used in order to generate TX completions.
	 */
	unsigned int tx_count_interrupts;
	/** Outstanding TX packet count (via "recycled tx buffers")
	 *
	 * Used in order to generate TX completions.
	 */
	unsigned int tx_count_txbufs;
	/** Outstanding RX packet count (via "interrupt status") */
	unsigned int rx_count_interrupts;
	/** Outstanding RX packet count (via WaitForPacket event) */
	unsigned int rx_count_events;
};

/** EFI simple network protocol GUID */
static EFI_GUID efi_simple_network_protocol_guid
	= EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** EFI driver binding protocol GUID */
static EFI_GUID efi_driver_binding_protocol_guid
	= EFI_DRIVER_BINDING_PROTOCOL_GUID;

/** EFI PCI I/O protocol GUID */
static EFI_GUID efi_pci_io_protocol_guid
	= EFI_PCI_IO_PROTOCOL_GUID;

/**
 * Set EFI SNP mode based on gPXE net device parameters
 *
 * @v snp		SNP interface
 */
static void efi_snp_set_mode ( struct efi_snp_device *snpdev ) {
	struct net_device *netdev = snpdev->netdev;
	EFI_SIMPLE_NETWORK_MODE *mode = &snpdev->mode;
	unsigned int ll_addr_len = netdev->ll_protocol->ll_addr_len;

	mode->HwAddressSize = ll_addr_len;
	mode->MediaHeaderSize = netdev->ll_protocol->ll_header_len;
	mode->MaxPacketSize = netdev->max_pkt_len;
	mode->ReceiveFilterMask = ( EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
				    EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
				    EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST );
	assert ( ll_addr_len <= sizeof ( mode->CurrentAddress ) );
	memcpy ( &mode->CurrentAddress, netdev->ll_addr, ll_addr_len );
	memcpy ( &mode->BroadcastAddress, netdev->ll_protocol->ll_broadcast,
		 ll_addr_len );
	memcpy ( &mode->PermanentAddress, netdev->ll_addr, ll_addr_len );
	mode->IfType = ntohs ( netdev->ll_protocol->ll_proto );
	mode->MacAddressChangeable = TRUE;
	mode->MediaPresentSupported = TRUE;
	mode->MediaPresent = ( netdev_link_ok ( netdev ) ? TRUE : FALSE );
}

/**
 * Poll net device and count received packets
 *
 * @v snpdev		SNP device
 */
static void efi_snp_poll ( struct efi_snp_device *snpdev ) {
	struct io_buffer *iobuf;
	unsigned int before = 0;
	unsigned int after = 0;
	unsigned int arrived;

	/* We have to report packet arrivals, and this is the easiest
	 * way to fake it.
	 */
	list_for_each_entry ( iobuf, &snpdev->netdev->rx_queue, list )
		before++;
	netdev_poll ( snpdev->netdev );
	list_for_each_entry ( iobuf, &snpdev->netdev->rx_queue, list )
		after++;
	arrived = ( after - before );

	snpdev->rx_count_interrupts += arrived;
	snpdev->rx_count_events += arrived;
}

/**
 * Change SNP state from "stopped" to "started"
 *
 * @v snp		SNP interface
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_start ( EFI_SIMPLE_NETWORK_PROTOCOL *snp ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );

	DBGC2 ( snpdev, "SNPDEV %p START\n", snpdev );

	snpdev->mode.State = EfiSimpleNetworkStarted;
	return 0;
}

/**
 * Change SNP state from "started" to "stopped"
 *
 * @v snp		SNP interface
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_stop ( EFI_SIMPLE_NETWORK_PROTOCOL *snp ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );

	DBGC2 ( snpdev, "SNPDEV %p STOP\n", snpdev );

	snpdev->mode.State = EfiSimpleNetworkStopped;
	return 0;
}

/**
 * Open the network device
 *
 * @v snp		SNP interface
 * @v extra_rx_bufsize	Extra RX buffer size, in bytes
 * @v extra_tx_bufsize	Extra TX buffer size, in bytes
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_initialize ( EFI_SIMPLE_NETWORK_PROTOCOL *snp,
		     UINTN extra_rx_bufsize, UINTN extra_tx_bufsize ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	int rc;

	DBGC2 ( snpdev, "SNPDEV %p INITIALIZE (%ld extra RX, %ld extra TX)\n",
		snpdev, extra_rx_bufsize, extra_tx_bufsize );

	if ( ( rc = netdev_open ( snpdev->netdev ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not open %s: %s\n",
		       snpdev, snpdev->netdev->name, strerror ( rc ) );
		return RC_TO_EFIRC ( rc );
	}

	snpdev->mode.State = EfiSimpleNetworkInitialized;
	return 0;
}

/**
 * Reset the network device
 *
 * @v snp		SNP interface
 * @v ext_verify	Extended verification required
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_reset ( EFI_SIMPLE_NETWORK_PROTOCOL *snp, BOOLEAN ext_verify ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	int rc;

	DBGC2 ( snpdev, "SNPDEV %p RESET (%s extended verification)\n",
		snpdev, ( ext_verify ? "with" : "without" ) );

	netdev_close ( snpdev->netdev );
	snpdev->mode.State = EfiSimpleNetworkStarted;

	if ( ( rc = netdev_open ( snpdev->netdev ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not reopen %s: %s\n",
		       snpdev, snpdev->netdev->name, strerror ( rc ) );
		return RC_TO_EFIRC ( rc );
	}

	snpdev->mode.State = EfiSimpleNetworkInitialized;
	return 0;
}

/**
 * Shut down the network device
 *
 * @v snp		SNP interface
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_shutdown ( EFI_SIMPLE_NETWORK_PROTOCOL *snp ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );

	DBGC2 ( snpdev, "SNPDEV %p SHUTDOWN\n", snpdev );

	netdev_close ( snpdev->netdev );
	snpdev->mode.State = EfiSimpleNetworkStarted;
	return 0;
}

/**
 * Manage receive filters
 *
 * @v snp		SNP interface
 * @v enable		Receive filters to enable
 * @v disable		Receive filters to disable
 * @v mcast_reset	Reset multicast filters
 * @v mcast_count	Number of multicast filters
 * @v mcast		Multicast filters
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_receive_filters ( EFI_SIMPLE_NETWORK_PROTOCOL *snp, UINT32 enable,
			  UINT32 disable, BOOLEAN mcast_reset,
			  UINTN mcast_count, EFI_MAC_ADDRESS *mcast ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	unsigned int i;

	DBGC2 ( snpdev, "SNPDEV %p RECEIVE_FILTERS %08lx&~%08lx%s %ld mcast\n",
		snpdev, enable, disable, ( mcast_reset ? " reset" : "" ),
		mcast_count );
	for ( i = 0 ; i < mcast_count ; i++ ) {
		DBGC2_HDA ( snpdev, i, &mcast[i],
			    snpdev->netdev->ll_protocol->ll_addr_len );
	}

	/* Lie through our teeth, otherwise MNP refuses to accept us */
	return 0;
}

/**
 * Set station address
 *
 * @v snp		SNP interface
 * @v reset		Reset to permanent address
 * @v new		New station address
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_station_address ( EFI_SIMPLE_NETWORK_PROTOCOL *snp, BOOLEAN reset,
			  EFI_MAC_ADDRESS *new ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	struct ll_protocol *ll_protocol = snpdev->netdev->ll_protocol;

	DBGC2 ( snpdev, "SNPDEV %p STATION_ADDRESS %s\n", snpdev,
		( reset ? "reset" : ll_protocol->ntoa ( new ) ) );

	/* Set the MAC address */
	if ( reset )
		new = &snpdev->mode.PermanentAddress;
	memcpy ( snpdev->netdev->ll_addr, new, ll_protocol->ll_addr_len );

	/* MAC address changes take effect only on netdev_open() */
	if ( snpdev->netdev->state & NETDEV_OPEN ) {
		DBGC ( snpdev, "SNPDEV %p MAC address changed while net "
		       "devive open\n", snpdev );
	}

	return 0;
}

/**
 * Get (or reset) statistics
 *
 * @v snp		SNP interface
 * @v reset		Reset statistics
 * @v stats_len		Size of statistics table
 * @v stats		Statistics table
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_statistics ( EFI_SIMPLE_NETWORK_PROTOCOL *snp, BOOLEAN reset,
		     UINTN *stats_len, EFI_NETWORK_STATISTICS *stats ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	EFI_NETWORK_STATISTICS stats_buf;

	DBGC2 ( snpdev, "SNPDEV %p STATISTICS%s", snpdev,
		( reset ? " reset" : "" ) );

	/* Gather statistics */
	memset ( &stats_buf, 0, sizeof ( stats_buf ) );
	stats_buf.TxGoodFrames = snpdev->netdev->tx_stats.good;
	stats_buf.TxDroppedFrames = snpdev->netdev->tx_stats.bad;
	stats_buf.TxTotalFrames = ( snpdev->netdev->tx_stats.good +
				    snpdev->netdev->tx_stats.bad );
	stats_buf.RxGoodFrames = snpdev->netdev->rx_stats.good;
	stats_buf.RxDroppedFrames = snpdev->netdev->rx_stats.bad;
	stats_buf.RxTotalFrames = ( snpdev->netdev->rx_stats.good +
				    snpdev->netdev->rx_stats.bad );
	if ( *stats_len > sizeof ( stats_buf ) )
		*stats_len = sizeof ( stats_buf );
	if ( stats )
		memcpy ( stats, &stats_buf, *stats_len );

	/* Reset statistics if requested to do so */
	if ( reset ) {
		memset ( &snpdev->netdev->tx_stats, 0,
			 sizeof ( snpdev->netdev->tx_stats ) );
		memset ( &snpdev->netdev->rx_stats, 0,
			 sizeof ( snpdev->netdev->rx_stats ) );
	}

	return 0;
}

/**
 * Convert multicast IP address to MAC address
 *
 * @v snp		SNP interface
 * @v ipv6		Address is IPv6
 * @v ip		IP address
 * @v mac		MAC address
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_mcast_ip_to_mac ( EFI_SIMPLE_NETWORK_PROTOCOL *snp, BOOLEAN ipv6,
			  EFI_IP_ADDRESS *ip, EFI_MAC_ADDRESS *mac ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	struct ll_protocol *ll_protocol = snpdev->netdev->ll_protocol;
	const char *ip_str;
	int rc;

	ip_str = ( ipv6 ? "(IPv6)" /* FIXME when we have inet6_ntoa() */ :
		   inet_ntoa ( *( ( struct in_addr * ) ip ) ) );
	DBGC2 ( snpdev, "SNPDEV %p MCAST_IP_TO_MAC %s\n", snpdev, ip_str );

	/* Try to hash the address */
	if ( ( rc = ll_protocol->mc_hash ( ( ipv6 ? AF_INET6 : AF_INET ),
					   ip, mac ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not hash %s: %s\n",
		       snpdev, ip_str, strerror ( rc ) );
		return RC_TO_EFIRC ( rc );
	}

	return 0;
}

/**
 * Read or write non-volatile storage
 *
 * @v snp		SNP interface
 * @v read		Operation is a read
 * @v offset		Starting offset within NVRAM
 * @v len		Length of data buffer
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_nvdata ( EFI_SIMPLE_NETWORK_PROTOCOL *snp, BOOLEAN read,
		 UINTN offset, UINTN len, VOID *data ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );

	DBGC2 ( snpdev, "SNPDEV %p NVDATA %s %lx+%lx\n", snpdev,
		( read ? "read" : "write" ), offset, len );
	if ( ! read )
		DBGC2_HDA ( snpdev, offset, data, len );

	return EFI_UNSUPPORTED;
}

/**
 * Read interrupt status and TX recycled buffer status
 *
 * @v snp		SNP interface
 * @v interrupts	Interrupt status, or NULL
 * @v txbufs		Recycled transmit buffer address, or NULL
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_get_status ( EFI_SIMPLE_NETWORK_PROTOCOL *snp,
		     UINT32 *interrupts, VOID **txbufs ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );

	DBGC2 ( snpdev, "SNPDEV %p GET_STATUS", snpdev );

	/* Poll the network device */
	efi_snp_poll ( snpdev );

	/* Interrupt status.  In practice, this seems to be used only
	 * to detect TX completions.
	 */
	if ( interrupts ) {
		*interrupts = 0;
		/* Report TX completions once queue is empty; this
		 * avoids having to add hooks in the net device layer.
		 */
		if ( snpdev->tx_count_interrupts &&
		     list_empty ( &snpdev->netdev->tx_queue ) ) {
			*interrupts |= EFI_SIMPLE_NETWORK_TRANSMIT_INTERRUPT;
			snpdev->tx_count_interrupts--;
		}
		/* Report RX */
		if ( snpdev->rx_count_interrupts ) {
			*interrupts |= EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT;
			snpdev->rx_count_interrupts--;
		}
		DBGC2 ( snpdev, " INTS:%02lx", *interrupts );
	}

	/* TX completions.  It would be possible to design a more
	 * idiotic scheme for this, but it would be a challenge.
	 * According to the UEFI header file, txbufs will be filled in
	 * with a list of "recycled transmit buffers" (i.e. completed
	 * TX buffers).  Observant readers may care to note that
	 * *txbufs is a void pointer.  Precisely how a list of
	 * completed transmit buffers is meant to be represented as an
	 * array of voids is left as an exercise for the reader.
	 *
	 * The only users of this interface (MnpDxe/MnpIo.c and
	 * PxeBcDxe/Bc.c within the EFI dev kit) both just poll until
	 * seeing a non-NULL result return in txbufs.  This is valid
	 * provided that they do not ever attempt to transmit more
	 * than one packet concurrently (and that TX never times out).
	 */
	if ( txbufs ) {
		if ( snpdev->tx_count_txbufs &&
		     list_empty ( &snpdev->netdev->tx_queue ) ) {
			*txbufs = "Which idiot designed this API?";
			snpdev->tx_count_txbufs--;
		} else {
			*txbufs = NULL;
		}
		DBGC2 ( snpdev, " TX:%s", ( *txbufs ? "some" : "none" ) );
	}

	DBGC2 ( snpdev, "\n" );
	return 0;
}

/**
 * Start packet transmission
 *
 * @v snp		SNP interface
 * @v ll_header_len	Link-layer header length, if to be filled in
 * @v len		Length of data buffer
 * @v data		Data buffer
 * @v ll_src		Link-layer source address, if specified
 * @v ll_dest		Link-layer destination address, if specified
 * @v net_proto		Network-layer protocol (in host order)
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_transmit ( EFI_SIMPLE_NETWORK_PROTOCOL *snp,
		   UINTN ll_header_len, UINTN len, VOID *data,
		   EFI_MAC_ADDRESS *ll_src, EFI_MAC_ADDRESS *ll_dest,
		   UINT16 *net_proto ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	struct ll_protocol *ll_protocol = snpdev->netdev->ll_protocol;
	struct io_buffer *iobuf;
	int rc;
	EFI_STATUS efirc;

	DBGC2 ( snpdev, "SNPDEV %p TRANSMIT %p+%lx", snpdev, data, len );
	if ( ll_header_len ) {
		if ( ll_src ) {
			DBGC2 ( snpdev, " src %s",
				ll_protocol->ntoa ( ll_src ) );
		}
		if ( ll_dest ) {
			DBGC2 ( snpdev, " dest %s",
				ll_protocol->ntoa ( ll_dest ) );
		}
		if ( net_proto ) {
			DBGC2 ( snpdev, " proto %04x", *net_proto );
		}
	}
	DBGC2 ( snpdev, "\n" );

	/* Sanity checks */
	if ( ll_header_len ) {
		if ( ll_header_len != ll_protocol->ll_header_len ) {
			DBGC ( snpdev, "SNPDEV %p TX invalid header length "
			       "%ld\n", snpdev, ll_header_len );
			efirc = EFI_INVALID_PARAMETER;
			goto err_sanity;
		}
		if ( len < ll_header_len ) {
			DBGC ( snpdev, "SNPDEV %p invalid packet length %ld\n",
			       snpdev, len );
			efirc = EFI_BUFFER_TOO_SMALL;
			goto err_sanity;
		}
		if ( ! ll_dest ) {
			DBGC ( snpdev, "SNPDEV %p TX missing destination "
			       "address\n", snpdev );
			efirc = EFI_INVALID_PARAMETER;
			goto err_sanity;
		}
		if ( ! net_proto ) {
			DBGC ( snpdev, "SNPDEV %p TX missing network "
			       "protocol\n", snpdev );
			efirc = EFI_INVALID_PARAMETER;
			goto err_sanity;
		}
		if ( ! ll_src )
			ll_src = &snpdev->mode.CurrentAddress;
	}

	/* Allocate buffer */
	iobuf = alloc_iob ( len );
	if ( ! iobuf ) {
		DBGC ( snpdev, "SNPDEV %p TX could not allocate %ld-byte "
		       "buffer\n", snpdev, len );
		efirc = EFI_DEVICE_ERROR;
		goto err_alloc_iob;
	}
	memcpy ( iob_put ( iobuf, len ), data, len );

	/* Create link-layer header, if specified */
	if ( ll_header_len ) {
		iob_pull ( iobuf, ll_header_len );
		if ( ( rc = ll_protocol->push ( iobuf, ll_dest, ll_src,
						htons ( *net_proto ) )) != 0 ){
			DBGC ( snpdev, "SNPDEV %p TX could not construct "
			       "header: %s\n", snpdev, strerror ( rc ) );
			efirc = RC_TO_EFIRC ( rc );
			goto err_ll_push;
		}
	}

	/* Transmit packet */
	if ( ( rc = netdev_tx ( snpdev->netdev, iobuf ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p TX could not transmit: %s\n",
		       snpdev, strerror ( rc ) );
		iobuf = NULL;
		efirc = RC_TO_EFIRC ( rc );
		goto err_tx;
	}

	/* Record transmission as outstanding */
	snpdev->tx_count_interrupts++;
	snpdev->tx_count_txbufs++;

	return 0;

 err_tx:
 err_ll_push:
	free_iob ( iobuf );
 err_alloc_iob:
 err_sanity:
	return efirc;
}

/**
 * Receive packet
 *
 * @v snp		SNP interface
 * @v ll_header_len	Link-layer header length, if to be filled in
 * @v len		Length of data buffer
 * @v data		Data buffer
 * @v ll_src		Link-layer source address, if specified
 * @v ll_dest		Link-layer destination address, if specified
 * @v net_proto		Network-layer protocol (in host order)
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_receive ( EFI_SIMPLE_NETWORK_PROTOCOL *snp,
		  UINTN *ll_header_len, UINTN *len, VOID *data,
		  EFI_MAC_ADDRESS *ll_src, EFI_MAC_ADDRESS *ll_dest,
		  UINT16 *net_proto ) {
	struct efi_snp_device *snpdev =
		container_of ( snp, struct efi_snp_device, snp );
	struct ll_protocol *ll_protocol = snpdev->netdev->ll_protocol;
	struct io_buffer *iobuf;
	const void *iob_ll_dest;
	const void *iob_ll_src;
	uint16_t iob_net_proto;
	int rc;
	EFI_STATUS efirc;

	DBGC2 ( snpdev, "SNPDEV %p RECEIVE %p(+%lx)", snpdev, data, *len );

	/* Poll the network device */
	efi_snp_poll ( snpdev );

	/* Dequeue a packet, if one is available */
	iobuf = netdev_rx_dequeue ( snpdev->netdev );
	if ( ! iobuf ) {
		DBGC2 ( snpdev, "\n" );
		efirc = EFI_NOT_READY;
		goto out_no_packet;
	}
	DBGC2 ( snpdev, "+%zx\n", iob_len ( iobuf ) );

	/* Return packet to caller */
	memcpy ( data, iobuf->data, iob_len ( iobuf ) );
	*len = iob_len ( iobuf );

	/* Attempt to decode link-layer header */
	if ( ( rc = ll_protocol->pull ( iobuf, &iob_ll_dest, &iob_ll_src,
					&iob_net_proto ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not parse header: %s\n",
		       snpdev, strerror ( rc ) );
		efirc = RC_TO_EFIRC ( rc );
		goto out_bad_ll_header;
	}

	/* Return link-layer header parameters to caller, if required */
	if ( ll_header_len )
		*ll_header_len = ll_protocol->ll_header_len;
	if ( ll_src )
		memcpy ( ll_src, iob_ll_src, ll_protocol->ll_addr_len );
	if ( ll_dest )
		memcpy ( ll_dest, iob_ll_dest, ll_protocol->ll_addr_len );
	if ( net_proto )
		*net_proto = ntohs ( iob_net_proto );

	efirc = 0;

 out_bad_ll_header:
	free_iob ( iobuf );
out_no_packet:
	return efirc;
}

/**
 * Poll event
 *
 * @v event		Event
 * @v context		Event context
 */
static VOID EFIAPI efi_snp_wait_for_packet ( EFI_EVENT event,
					     VOID *context ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_snp_device *snpdev = context;

	DBGCP ( snpdev, "SNPDEV %p WAIT_FOR_PACKET\n", snpdev );

	/* Do nothing unless the net device is open */
	if ( ! ( snpdev->netdev->state & NETDEV_OPEN ) )
		return;

	/* Poll the network device */
	efi_snp_poll ( snpdev );

	/* Fire event if packets have been received */
	if ( snpdev->rx_count_events != 0 ) {
		DBGC2 ( snpdev, "SNPDEV %p firing WaitForPacket event\n",
			snpdev );
		bs->SignalEvent ( event );
		snpdev->rx_count_events--;
	}
}

/** SNP interface */
static EFI_SIMPLE_NETWORK_PROTOCOL efi_snp_device_snp = {
	.Revision	= EFI_SIMPLE_NETWORK_PROTOCOL_REVISION,
	.Start		= efi_snp_start,
	.Stop		= efi_snp_stop,
	.Initialize	= efi_snp_initialize,
	.Reset		= efi_snp_reset,
	.Shutdown	= efi_snp_shutdown,
	.ReceiveFilters	= efi_snp_receive_filters,
	.StationAddress	= efi_snp_station_address,
	.Statistics	= efi_snp_statistics,
	.MCastIpToMac	= efi_snp_mcast_ip_to_mac,
	.NvData		= efi_snp_nvdata,
	.GetStatus	= efi_snp_get_status,
	.Transmit	= efi_snp_transmit,
	.Receive	= efi_snp_receive,
};

/**
 * Locate net device corresponding to EFI device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @ret netdev		Net device, or NULL if not found
 */
static struct net_device *
efi_snp_netdev ( EFI_DRIVER_BINDING_PROTOCOL *driver, EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_PCI_IO_PROTOCOL *pci;
		void *interface;
	} u;
	UINTN pci_segment, pci_bus, pci_dev, pci_fn;
	unsigned int pci_busdevfn;
	struct net_device *netdev = NULL;
	EFI_STATUS efirc;

	/* See if device is a PCI device */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_pci_io_protocol_guid,
					  &u.interface,
					  driver->DriverBindingHandle,
					  device,
					  EFI_OPEN_PROTOCOL_BY_DRIVER )) !=0 ){
		DBGCP ( driver, "SNPDRV %p device %p is not a PCI device\n",
			driver, device );
		goto out_no_pci_io;
	}

	/* Get PCI bus:dev.fn address */
	if ( ( efirc = u.pci->GetLocation ( u.pci, &pci_segment, &pci_bus,
					    &pci_dev, &pci_fn ) ) != 0 ) {
		DBGC ( driver, "SNPDRV %p device %p could not get PCI "
		       "location: %lx\n", driver, device, efirc );
		goto out_no_pci_location;
	}
	DBGCP ( driver, "SNPDRV %p device %p is PCI %04lx:%02lx:%02lx.%lx\n",
		driver, device, pci_segment, pci_bus, pci_dev, pci_fn );

	/* Look up corresponding network device */
	pci_busdevfn = PCI_BUSDEVFN ( pci_bus, PCI_DEVFN ( pci_dev, pci_fn ) );
	if ( ( netdev = find_netdev_by_location ( BUS_TYPE_PCI,
						  pci_busdevfn ) ) == NULL ) {
		DBGCP ( driver, "SNPDRV %p device %p is not a gPXE network "
			"device\n", driver, device );
		goto out_no_netdev;
	}
	DBGC ( driver, "SNPDRV %p device %p is %s\n",
	       driver, device, netdev->name );

 out_no_netdev:
 out_no_pci_location:
	bs->CloseProtocol ( device, &efi_pci_io_protocol_guid,
			    driver->DriverBindingHandle, device );
 out_no_pci_io:
	return netdev;
}

/**
 * Locate SNP corresponding to EFI device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @ret snp		EFI SNP, or NULL if not found
 */
static struct efi_snp_device *
efi_snp_snpdev ( EFI_DRIVER_BINDING_PROTOCOL *driver, EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_SIMPLE_NETWORK_PROTOCOL *snp;
		void *interface;
	} u;
	struct efi_snp_device *snpdev;
	EFI_STATUS efirc;

	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_network_protocol_guid,
					  &u.interface,
					  driver->DriverBindingHandle,
					  device,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL))!=0){
		DBGC ( driver, "SNPDRV %p device %p could not locate SNP: "
		       "%lx\n", driver, device, efirc );
		return NULL;
	}

	snpdev =  container_of ( u.snp, struct efi_snp_device, snp );
	DBGCP ( driver, "SNPDRV %p device %p is SNPDEV %p\n",
		driver, device, snpdev );
	return snpdev;
}

/**
 * Check to see if driver supports a device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v child		Path to child device, if any
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_driver_supported ( EFI_DRIVER_BINDING_PROTOCOL *driver,
			   EFI_HANDLE device,
			   EFI_DEVICE_PATH_PROTOCOL *child ) {
	struct net_device *netdev;

	DBGCP ( driver, "SNPDRV %p DRIVER_SUPPORTED %p (%p)\n",
		driver, device, child );

	netdev = efi_snp_netdev ( driver, device );
	return ( netdev ? 0 : EFI_UNSUPPORTED );
}

/**
 * Attach driver to device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v child		Path to child device, if any
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_driver_start ( EFI_DRIVER_BINDING_PROTOCOL *driver,
		       EFI_HANDLE device,
		       EFI_DEVICE_PATH_PROTOCOL *child ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_snp_device *snpdev;
	struct net_device *netdev;
	EFI_STATUS efirc;

	DBGCP ( driver, "SNPDRV %p DRIVER_START %p (%p)\n",
		driver, device, child );

	/* Allocate the SNP device */
	snpdev = zalloc ( sizeof ( *snpdev ) );
	if ( ! snpdev ) {
		efirc = EFI_OUT_OF_RESOURCES;
		goto err_alloc_snp;
	}

	/* Identify the net device */
	netdev = efi_snp_netdev ( driver, device );
	if ( ! netdev ) {
		DBGC ( snpdev, "SNPDEV %p cannot find netdev for device %p\n",
		       snpdev, device );
		efirc = EFI_UNSUPPORTED;
		goto err_no_netdev;
	}
	snpdev->netdev = netdev_get ( netdev );

	/* Sanity check */
	if ( netdev->ll_protocol->ll_addr_len > sizeof ( EFI_MAC_ADDRESS ) ) {
		DBGC ( snpdev, "SNPDEV %p cannot support link-layer address "
		       "length %zd for %s\n", snpdev,
		       netdev->ll_protocol->ll_addr_len, netdev->name );
		efirc = EFI_INVALID_PARAMETER;
		goto err_ll_addr_len;
	}

	/* Populate the SNP structure */
	memcpy ( &snpdev->snp, &efi_snp_device_snp, sizeof ( snpdev->snp ) );
	snpdev->snp.Mode = &snpdev->mode;
	if ( ( efirc = bs->CreateEvent ( EVT_NOTIFY_WAIT, TPL_NOTIFY,
					 efi_snp_wait_for_packet, snpdev,
					 &snpdev->snp.WaitForPacket ) ) != 0 ){
		DBGC ( snpdev, "SNPDEV %p could not create event: %lx\n",
		       snpdev, efirc );
		goto err_create_event;
	}

	/* Populate the SNP mode structure */
	snpdev->mode.State = EfiSimpleNetworkStopped;
	efi_snp_set_mode ( snpdev );

	/* Install the SNP */
	if ( ( efirc = bs->InstallProtocolInterface ( &device,
				&efi_simple_network_protocol_guid,
				EFI_NATIVE_INTERFACE, &snpdev->snp ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not install protocol: %lx\n",
		       snpdev, efirc );
		goto err_install_protocol_interface;
	}

	DBGC ( snpdev, "SNPDEV %p installed for %s on device %p\n",
	       snpdev, netdev->name, device );
	return 0;

	bs->UninstallProtocolInterface ( device,
					 &efi_simple_network_protocol_guid,
					 &snpdev->snp );
 err_install_protocol_interface:
	bs->CloseEvent ( snpdev->snp.WaitForPacket );
 err_create_event:
 err_ll_addr_len:
	netdev_put ( netdev );
 err_no_netdev:
	free ( snpdev );
 err_alloc_snp:
	return efirc;
}

/**
 * Detach driver from device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v num_children	Number of child devices
 * @v children		List of child devices
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_driver_stop ( EFI_DRIVER_BINDING_PROTOCOL *driver,
		      EFI_HANDLE device,
		      UINTN num_children,
		      EFI_HANDLE *children ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_snp_device *snpdev;

	DBGCP ( driver, "SNPDRV %p DRIVER_STOP %p (%ld %p)\n",
		driver, device, num_children, children );

	/* Locate SNP device */
	snpdev = efi_snp_snpdev ( driver, device );
	if ( ! snpdev ) {
		DBGC ( driver, "SNPDRV %p device %p could not find SNPDEV\n",
		       driver, device );
		return EFI_DEVICE_ERROR;
	}

	/* Uninstall the SNP */
	bs->UninstallProtocolInterface ( device,
					 &efi_simple_network_protocol_guid,
					 &snpdev->snp );
	bs->CloseEvent ( snpdev->snp.WaitForPacket );
	netdev_put ( snpdev->netdev );
	free ( snpdev );
	return 0;
}

/** EFI SNP driver binding */
static EFI_DRIVER_BINDING_PROTOCOL efi_snp_binding = {
	efi_snp_driver_supported,
	efi_snp_driver_start,
	efi_snp_driver_stop,
	0x10,
	NULL,
	NULL
};

/**
 * Install EFI SNP driver
 *
 * @ret rc		Return status code
 */
int efi_snp_install ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DRIVER_BINDING_PROTOCOL *driver = &efi_snp_binding;
	EFI_STATUS efirc;

	driver->ImageHandle = efi_image_handle;
	if ( ( efirc = bs->InstallProtocolInterface (
					&driver->DriverBindingHandle,
					&efi_driver_binding_protocol_guid,
					EFI_NATIVE_INTERFACE,
					driver ) ) != 0 ) {
		DBGC ( driver, "SNPDRV %p could not install driver binding: "
		       "%lx\n", driver, efirc );
		return EFIRC_TO_RC ( efirc );
	}

	DBGC ( driver, "SNPDRV %p driver binding installed as %p\n",
	       driver, driver->DriverBindingHandle );
	return 0;
}
