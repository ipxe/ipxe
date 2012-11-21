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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/iobuf.h>
#include <ipxe/in.h>
#include <ipxe/pci.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_pci.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_snp.h>
#include <config/general.h>

/** EFI simple network protocol GUID */
static EFI_GUID efi_simple_network_protocol_guid
	= EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** EFI device path protocol GUID */
static EFI_GUID efi_device_path_protocol_guid
	= EFI_DEVICE_PATH_PROTOCOL_GUID;

/** EFI network interface identifier GUID */
static EFI_GUID efi_nii_protocol_guid
	= EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_GUID;

/** EFI network interface identifier GUID (extra special version) */
static EFI_GUID efi_nii31_protocol_guid = {
	/* At some point, it seems that someone decided to change the
	 * GUID.  Current EFI builds ignore the older GUID, older EFI
	 * builds ignore the newer GUID, so we have to expose both.
	 */
	0x1ACED566, 0x76ED, 0x4218,
	{ 0xBC, 0x81, 0x76, 0x7F, 0x1F, 0x97, 0x7A, 0x89 }
};

/** EFI component name protocol */
static EFI_GUID efi_component_name2_protocol_guid
	= EFI_COMPONENT_NAME2_PROTOCOL_GUID;

/** List of SNP devices */
static LIST_HEAD ( efi_snp_devices );

/**
 * Set EFI SNP mode based on iPXE net device parameters
 *
 * @v snp		SNP interface
 */
static void efi_snp_set_mode ( struct efi_snp_device *snpdev ) {
	struct net_device *netdev = snpdev->netdev;
	EFI_SIMPLE_NETWORK_MODE *mode = &snpdev->mode;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	unsigned int ll_addr_len = ll_protocol->ll_addr_len;

	mode->HwAddressSize = ll_addr_len;
	mode->MediaHeaderSize = ll_protocol->ll_header_len;
	mode->MaxPacketSize = netdev->max_pkt_len;
	mode->ReceiveFilterMask = ( EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
				    EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
				    EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST );
	assert ( ll_addr_len <= sizeof ( mode->CurrentAddress ) );
	memcpy ( &mode->CurrentAddress, netdev->ll_addr, ll_addr_len );
	memcpy ( &mode->BroadcastAddress, netdev->ll_broadcast, ll_addr_len );
	ll_protocol->init_addr ( netdev->hw_addr, &mode->PermanentAddress );
	mode->IfType = ntohs ( ll_protocol->ll_proto );
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
		snpdev, ( ( unsigned long ) extra_rx_bufsize ),
		( ( unsigned long ) extra_tx_bufsize ) );

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

	DBGC2 ( snpdev, "SNPDEV %p RECEIVE_FILTERS %08x&~%08x%s %ld mcast\n",
		snpdev, enable, disable, ( mcast_reset ? " reset" : "" ),
		( ( unsigned long ) mcast_count ) );
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
	if ( netdev_is_open ( snpdev->netdev ) ) {
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
		( read ? "read" : "write" ), ( ( unsigned long ) offset ),
		( ( unsigned long ) len ) );
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
		DBGC2 ( snpdev, " INTS:%02x", *interrupts );
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
	size_t payload_len;
	int rc;
	EFI_STATUS efirc;

	DBGC2 ( snpdev, "SNPDEV %p TRANSMIT %p+%lx", snpdev, data,
		( ( unsigned long ) len ) );
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
			       "%ld\n", snpdev,
			       ( ( unsigned long ) ll_header_len ) );
			efirc = EFI_INVALID_PARAMETER;
			goto err_sanity;
		}
		if ( len < ll_header_len ) {
			DBGC ( snpdev, "SNPDEV %p invalid packet length %ld\n",
			       snpdev, ( ( unsigned long ) len ) );
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
	payload_len = ( len - ll_protocol->ll_header_len );
	iobuf = alloc_iob ( MAX_LL_HEADER_LEN + ( ( payload_len > IOB_ZLEN ) ?
						  payload_len : IOB_ZLEN ) );
	if ( ! iobuf ) {
		DBGC ( snpdev, "SNPDEV %p TX could not allocate %ld-byte "
		       "buffer\n", snpdev, ( ( unsigned long ) len ) );
		efirc = EFI_DEVICE_ERROR;
		goto err_alloc_iob;
	}
	iob_reserve ( iobuf, ( MAX_LL_HEADER_LEN -
			       ll_protocol->ll_header_len ) );
	memcpy ( iob_put ( iobuf, len ), data, len );

	/* Create link-layer header, if specified */
	if ( ll_header_len ) {
		iob_pull ( iobuf, ll_protocol->ll_header_len );
		if ( ( rc = ll_protocol->push ( snpdev->netdev,
						iobuf, ll_dest, ll_src,
						htons ( *net_proto ) )) != 0 ){
			DBGC ( snpdev, "SNPDEV %p TX could not construct "
			       "header: %s\n", snpdev, strerror ( rc ) );
			efirc = RC_TO_EFIRC ( rc );
			goto err_ll_push;
		}
	}

	/* Transmit packet */
	if ( ( rc = netdev_tx ( snpdev->netdev, iob_disown ( iobuf ) ) ) != 0){
		DBGC ( snpdev, "SNPDEV %p TX could not transmit: %s\n",
		       snpdev, strerror ( rc ) );
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
	unsigned int iob_flags;
	int rc;
	EFI_STATUS efirc;

	DBGC2 ( snpdev, "SNPDEV %p RECEIVE %p(+%lx)", snpdev, data,
		( ( unsigned long ) *len ) );

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
	if ( ( rc = ll_protocol->pull ( snpdev->netdev, iobuf, &iob_ll_dest,
					&iob_ll_src, &iob_net_proto,
					&iob_flags ) ) != 0 ) {
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
	if ( ! netdev_is_open ( snpdev->netdev ) )
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

/******************************************************************************
 *
 * Component name protocol
 *
 ******************************************************************************
 */

/**
 * Look up driver name
 *
 * @v name2		Component name protocol
 * @v language		Language to use
 * @v driver_name	Driver name to fill in
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_get_driver_name ( EFI_COMPONENT_NAME2_PROTOCOL *name2,
			  CHAR8 *language __unused, CHAR16 **driver_name ) {
	struct efi_snp_device *snpdev =
		container_of ( name2, struct efi_snp_device, name2 );

	*driver_name = snpdev->driver_name;
	return 0;
}

/**
 * Look up controller name
 *
 * @v name2     		Component name protocol
 * @v device		Device
 * @v child		Child device, or NULL
 * @v language		Language to use
 * @v driver_name	Device name to fill in
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_get_controller_name ( EFI_COMPONENT_NAME2_PROTOCOL *name2,
			      EFI_HANDLE device __unused,
			      EFI_HANDLE child __unused,
			      CHAR8 *language __unused,
			      CHAR16 **controller_name ) {
	struct efi_snp_device *snpdev =
		container_of ( name2, struct efi_snp_device, name2 );

	*controller_name = snpdev->controller_name;
	return 0;
}

/******************************************************************************
 *
 * iPXE network driver
 *
 ******************************************************************************
 */

/**
 * Locate SNP device corresponding to network device
 *
 * @v netdev		Network device
 * @ret snp		SNP device, or NULL if not found
 */
static struct efi_snp_device * efi_snp_demux ( struct net_device *netdev ) {
	struct efi_snp_device *snpdev;

	list_for_each_entry ( snpdev, &efi_snp_devices, list ) {
		if ( snpdev->netdev == netdev )
			return snpdev;
	}
	return NULL;
}

/**
 * Create SNP device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int efi_snp_probe ( struct net_device *netdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_pci_device *efipci;
	struct efi_snp_device *snpdev;
	EFI_DEVICE_PATH_PROTOCOL *path_end;
	MAC_ADDR_DEVICE_PATH *macpath;
	size_t path_prefix_len = 0;
	EFI_STATUS efirc;
	int rc;

	/* Find EFI PCI device */
	efipci = efipci_find ( netdev->dev );
	if ( ! efipci ) {
		DBG ( "SNP skipping non-PCI device %s\n", netdev->name );
		rc = 0;
		goto err_no_pci;
	}

	/* Calculate device path prefix length */
	path_end = efi_devpath_end ( efipci->path );
	path_prefix_len = ( ( ( void * ) path_end ) -
			    ( ( void * ) efipci->path ) );

	/* Allocate the SNP device */
	snpdev = zalloc ( sizeof ( *snpdev ) + path_prefix_len +
			  sizeof ( *macpath ) );
	if ( ! snpdev ) {
		rc = -ENOMEM;
		goto err_alloc_snp;
	}
	snpdev->netdev = netdev_get ( netdev );
	snpdev->efipci = efipci;

	/* Sanity check */
	if ( netdev->ll_protocol->ll_addr_len > sizeof ( EFI_MAC_ADDRESS ) ) {
		DBGC ( snpdev, "SNPDEV %p cannot support link-layer address "
		       "length %d for %s\n", snpdev,
		       netdev->ll_protocol->ll_addr_len, netdev->name );
		rc = -ENOTSUP;
		goto err_ll_addr_len;
	}

	/* Populate the SNP structure */
	memcpy ( &snpdev->snp, &efi_snp_device_snp, sizeof ( snpdev->snp ) );
	snpdev->snp.Mode = &snpdev->mode;
	if ( ( efirc = bs->CreateEvent ( EVT_NOTIFY_WAIT, TPL_NOTIFY,
					 efi_snp_wait_for_packet, snpdev,
					 &snpdev->snp.WaitForPacket ) ) != 0 ){
		DBGC ( snpdev, "SNPDEV %p could not create event: %s\n",
		       snpdev, efi_strerror ( efirc ) );
		rc = EFIRC_TO_RC ( efirc );
		goto err_create_event;
	}

	/* Populate the SNP mode structure */
	snpdev->mode.State = EfiSimpleNetworkStopped;
	efi_snp_set_mode ( snpdev );

	/* Populate the NII structure */
	snpdev->nii.Revision =
		EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_REVISION;
	strncpy ( snpdev->nii.StringId, "iPXE",
		  sizeof ( snpdev->nii.StringId ) );

	/* Populate the component name structure */
	efi_snprintf ( snpdev->driver_name,
		       ( sizeof ( snpdev->driver_name ) /
			 sizeof ( snpdev->driver_name[0] ) ),
		       PRODUCT_SHORT_NAME " %s", netdev->dev->driver_name );
	efi_snprintf ( snpdev->controller_name,
		       ( sizeof ( snpdev->controller_name ) /
			 sizeof ( snpdev->controller_name[0] ) ),
		       PRODUCT_SHORT_NAME " %s (%s)",
		       netdev->name, netdev_addr ( netdev ) );
	snpdev->name2.GetDriverName = efi_snp_get_driver_name;
	snpdev->name2.GetControllerName = efi_snp_get_controller_name;
	snpdev->name2.SupportedLanguages = "en";

	/* Populate the device name */
	efi_snprintf ( snpdev->name, ( sizeof ( snpdev->name ) /
				       sizeof ( snpdev->name[0] ) ),
		       "%s", netdev->name );

	/* Populate the device path */
	memcpy ( &snpdev->path, efipci->path, path_prefix_len );
	macpath = ( ( ( void * ) &snpdev->path ) + path_prefix_len );
	path_end = ( ( void * ) ( macpath + 1 ) );
	memset ( macpath, 0, sizeof ( *macpath ) );
	macpath->Header.Type = MESSAGING_DEVICE_PATH;
	macpath->Header.SubType = MSG_MAC_ADDR_DP;
	macpath->Header.Length[0] = sizeof ( *macpath );
	memcpy ( &macpath->MacAddress, netdev->ll_addr,
		 sizeof ( macpath->MacAddress ) );
	macpath->IfType = ntohs ( netdev->ll_protocol->ll_proto );
	memset ( path_end, 0, sizeof ( *path_end ) );
	path_end->Type = END_DEVICE_PATH_TYPE;
	path_end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	path_end->Length[0] = sizeof ( *path_end );

	/* Install the SNP */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&snpdev->handle,
			&efi_simple_network_protocol_guid, &snpdev->snp,
			&efi_device_path_protocol_guid, &snpdev->path,
			&efi_nii_protocol_guid, &snpdev->nii,
			&efi_nii31_protocol_guid, &snpdev->nii,
			&efi_component_name2_protocol_guid, &snpdev->name2,
			NULL ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not install protocols: "
		       "%s\n", snpdev, efi_strerror ( efirc ) );
		rc = EFIRC_TO_RC ( efirc );
		goto err_install_protocol_interface;
	}

	/* Add as child of PCI device */
	if ( ( efirc = efipci_child_add ( efipci, snpdev->handle ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not become child of " PCI_FMT
		       ": %s\n", snpdev, PCI_ARGS ( &efipci->pci ),
		       efi_strerror ( efirc ) );
		rc = EFIRC_TO_RC ( efirc );
		goto err_efipci_child_add;
	}

	/* Install HII */
	if ( ( rc = efi_snp_hii_install ( snpdev ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not install HII: %s\n",
		       snpdev, strerror ( rc ) );
		goto err_hii_install;
	}

	/* Add to list of SNP devices */
	list_add ( &snpdev->list, &efi_snp_devices );

	DBGC ( snpdev, "SNPDEV %p installed for %s as device %p\n",
	       snpdev, netdev->name, snpdev->handle );
	return 0;

	efi_snp_hii_uninstall ( snpdev );
 err_hii_install:
	efipci_child_del ( efipci, snpdev->handle );
 err_efipci_child_add:
	bs->UninstallMultipleProtocolInterfaces (
			snpdev->handle,
			&efi_simple_network_protocol_guid, &snpdev->snp,
			&efi_device_path_protocol_guid, &snpdev->path,
			&efi_nii_protocol_guid, &snpdev->nii,
			&efi_nii31_protocol_guid, &snpdev->nii,
			&efi_component_name2_protocol_guid, &snpdev->name2,
			NULL );
 err_install_protocol_interface:
	bs->CloseEvent ( snpdev->snp.WaitForPacket );
 err_create_event:
 err_ll_addr_len:
	netdev_put ( netdev );
	free ( snpdev );
 err_alloc_snp:
 err_no_pci:
	return rc;
}

/**
 * Handle SNP device or link state change
 *
 * @v netdev		Network device
 */
static void efi_snp_notify ( struct net_device *netdev ) {
	struct efi_snp_device *snpdev;

	/* Locate SNP device */
	snpdev = efi_snp_demux ( netdev );
	if ( ! snpdev ) {
		DBG ( "SNP skipping non-SNP device %s\n", netdev->name );
		return;
	}

	/* Update link state */
	snpdev->mode.MediaPresent =
		( netdev_link_ok ( netdev ) ? TRUE : FALSE );
	DBGC ( snpdev, "SNPDEV %p link is %s\n", snpdev,
	       ( snpdev->mode.MediaPresent ? "up" : "down" ) );
}

/**
 * Destroy SNP device
 *
 * @v netdev		Network device
 */
static void efi_snp_remove ( struct net_device *netdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_snp_device *snpdev;

	/* Locate SNP device */
	snpdev = efi_snp_demux ( netdev );
	if ( ! snpdev ) {
		DBG ( "SNP skipping non-SNP device %s\n", netdev->name );
		return;
	}

	/* Uninstall the SNP */
	efi_snp_hii_uninstall ( snpdev );
	efipci_child_del ( snpdev->efipci, snpdev->handle );
	list_del ( &snpdev->list );
	bs->UninstallMultipleProtocolInterfaces (
			snpdev->handle,
			&efi_simple_network_protocol_guid, &snpdev->snp,
			&efi_device_path_protocol_guid, &snpdev->path,
			&efi_nii_protocol_guid, &snpdev->nii,
			&efi_nii31_protocol_guid, &snpdev->nii,
			&efi_component_name2_protocol_guid, &snpdev->name2,
			NULL );
	bs->CloseEvent ( snpdev->snp.WaitForPacket );
	netdev_put ( snpdev->netdev );
	free ( snpdev );
}

/** SNP driver */
struct net_driver efi_snp_driver __net_driver = {
	.name = "SNP",
	.probe = efi_snp_probe,
	.notify = efi_snp_notify,
	.remove = efi_snp_remove,
};
