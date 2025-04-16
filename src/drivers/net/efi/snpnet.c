/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/vsprintf.h>
#include <ipxe/timer.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/efi_snp.h>
#include "snpnet.h"

/** @file
 *
 * SNP NIC driver
 *
 */

/** An SNP NIC */
struct snp_nic {
	/** EFI device */
	struct efi_device *efidev;
	/** Simple network protocol */
	EFI_SIMPLE_NETWORK_PROTOCOL *snp;
	/** Generic device */
	struct device dev;

	/** Maximum packet size
	 *
	 * This is calculated as the sum of MediaHeaderSize and
	 * MaxPacketSize, and may therefore be an overestimate.
	 */
	size_t mtu;

	/** Current transmit buffer */
	struct io_buffer *txbuf;
	/** Current receive buffer */
	struct io_buffer *rxbuf;
};

/** Maximum number of received packets per poll */
#define SNP_RX_QUOTA 4

/** Maximum initialisation retry count */
#define SNP_INITIALIZE_RETRY_MAX 10

/** Delay between each initialisation retry */
#define SNP_INITIALIZE_RETRY_DELAY_MS 10

/** Additional padding for receive buffers
 *
 * Some SNP implementations seem to require additional space in the
 * allocated receive buffers, otherwise full-length packets will be
 * silently dropped.
 *
 * The EDK2 MnpDxe driver happens to allocate an additional 8 bytes of
 * padding (4 for a VLAN tag, 4 for the Ethernet frame checksum).
 * Match this behaviour since drivers are very likely to have been
 * tested against MnpDxe.
 */
#define SNP_RX_PAD 8

/** An SNP interface patch to inhibit shutdown for insomniac devices */
struct snp_insomniac_patch {
	/** Original Shutdown() method */
	EFI_SIMPLE_NETWORK_SHUTDOWN shutdown;
	/** Original Stop() method */
	EFI_SIMPLE_NETWORK_STOP stop;
};

/**
 * Format SNP MAC address (for debugging)
 *
 * @v mac		MAC address
 * @v len		Length of MAC address
 * @ret text		MAC address as text
 */
static const char * snpnet_mac_text ( EFI_MAC_ADDRESS *mac, size_t len ) {
	static char buf[ sizeof ( *mac ) * 3 /* "xx:" or "xx\0" */ ];
	size_t used = 0;
	unsigned int i;

	for ( i = 0 ; i < len ; i++ ) {
		used += ssnprintf ( &buf[used], ( sizeof ( buf ) - used ),
				    "%s%02x", ( used ? ":" : "" ),
				    mac->Addr[i] );
	}
	return buf;
}

/**
 * Dump SNP mode information (for debugging)
 *
 * @v netdev		Network device
 */
static void snpnet_dump_mode ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	EFI_SIMPLE_NETWORK_MODE *mode = snp->snp->Mode;
	size_t mac_len = mode->HwAddressSize;
	unsigned int i;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_EXTRA )
		return;

	DBGC2 ( snp, "SNP %s st %d type %d hdr %d pkt %d rxflt %#x/%#x%s "
		"nvram %d acc %d mcast %d/%d\n", netdev->name, mode->State,
		mode->IfType, mode->MediaHeaderSize, mode->MaxPacketSize,
		mode->ReceiveFilterSetting, mode->ReceiveFilterMask,
		( mode->MultipleTxSupported ? " multitx" : "" ),
		mode->NvRamSize, mode->NvRamAccessSize,
		mode->MCastFilterCount, mode->MaxMCastFilterCount );
	DBGC2 ( snp, "SNP %s hw %s", netdev->name,
		snpnet_mac_text ( &mode->PermanentAddress, mac_len ) );
	DBGC2 ( snp, " addr %s%s",
		snpnet_mac_text ( &mode->CurrentAddress, mac_len ),
		( mode->MacAddressChangeable ? "" : "(f)" ) );
	DBGC2 ( snp, " bcast %s\n",
		snpnet_mac_text ( &mode->BroadcastAddress, mac_len ) );
	for ( i = 0 ; i < mode->MCastFilterCount ; i++ ) {
		DBGC2 ( snp, "SNP %s mcast %s\n", netdev->name,
			snpnet_mac_text ( &mode->MCastFilter[i], mac_len ) );
	}
	DBGC2 ( snp, "SNP %s media %s\n", netdev->name,
		( mode->MediaPresentSupported ?
		  ( mode->MediaPresent ? "present" : "not present" ) :
		  "presence not supported" ) );
}

/**
 * Check link state
 *
 * @v netdev		Network device
 */
static void snpnet_check_link ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	EFI_SIMPLE_NETWORK_MODE *mode = snp->snp->Mode;

	/* Do nothing unless media presence detection is supported */
	if ( ! mode->MediaPresentSupported )
		return;

	/* Report any link status change */
	if ( mode->MediaPresent && ( ! netdev_link_ok ( netdev ) ) ) {
		netdev_link_up ( netdev );
	} else if ( ( ! mode->MediaPresent ) && netdev_link_ok ( netdev ) ) {
		netdev_link_down ( netdev );
	}
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int snpnet_transmit ( struct net_device *netdev,
			     struct io_buffer *iobuf ) {
	struct snp_nic *snp = netdev->priv;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing if shutdown is in progress */
	if ( efi_shutdown_in_progress )
		return -ECANCELED;

	/* Defer the packet if there is already a transmission in progress */
	if ( snp->txbuf ) {
		netdev_tx_defer ( netdev, iobuf );
		return 0;
	}

	/* Pad to minimum Ethernet length, to work around underlying
	 * drivers that do not correctly handle frame padding
	 * themselves.
	 */
	iob_pad ( iobuf, ETH_ZLEN );

	/* Transmit packet */
	if ( ( efirc = snp->snp->Transmit ( snp->snp, 0, iob_len ( iobuf ),
					    iobuf->data, NULL, NULL,
					    NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( snp, "SNP %s could not transmit: %s\n",
		       netdev->name, strerror ( rc ) );
		return rc;
	}
	snp->txbuf = iobuf;

	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void snpnet_poll_tx ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	struct io_buffer *iobuf;
	UINT32 irq;
	VOID *txbuf;
	EFI_STATUS efirc;
	int rc;

	/* Get status */
	txbuf = NULL;
	if ( ( efirc = snp->snp->GetStatus ( snp->snp, &irq, &txbuf ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( snp, "SNP %s could not get status: %s\n",
		       netdev->name, strerror ( rc ) );
		netdev_rx_err ( netdev, NULL, rc );
		return;
	}

	/* Do nothing unless we have a completion */
	if ( ! txbuf )
		return;

	/* Sanity check */
	if ( ! snp->txbuf ) {
		DBGC ( snp, "SNP %s reported spurious TX completion\n",
		       netdev->name );
		netdev_tx_err ( netdev, NULL, -EPIPE );
		return;
	}

	/* Complete transmission */
	iobuf = snp->txbuf;
	snp->txbuf = NULL;
	netdev_tx_complete ( netdev, iobuf );
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void snpnet_poll_rx ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	UINTN len;
	unsigned int quota;
	EFI_STATUS efirc;
	int rc;

	/* Retrieve up to SNP_RX_QUOTA packets */
	for ( quota = SNP_RX_QUOTA ; quota ; quota-- ) {

		/* Allocate buffer, if required */
		if ( ! snp->rxbuf ) {
			snp->rxbuf = alloc_iob ( snp->mtu + SNP_RX_PAD );
			if ( ! snp->rxbuf ) {
				/* Leave for next poll */
				break;
			}
		}

		/* Receive packet */
		len = iob_tailroom ( snp->rxbuf );
		if ( ( efirc = snp->snp->Receive ( snp->snp, NULL, &len,
						   snp->rxbuf->data, NULL,
						   NULL, NULL ) ) != 0 ) {

			/* EFI_NOT_READY is just the usual "no packet"
			 * status indication; ignore it.
			 */
			if ( efirc == EFI_NOT_READY )
				break;

			/* Anything else is an error */
			rc = -EEFI ( efirc );
			DBGC ( snp, "SNP %s could not receive: %s\n",
			       netdev->name, strerror ( rc ) );
			netdev_rx_err ( netdev, NULL, rc );
			break;
		}

		/* Hand off to network stack */
		iob_put ( snp->rxbuf, len );
		netdev_rx ( netdev, snp->rxbuf );
		snp->rxbuf = NULL;
	}
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void snpnet_poll ( struct net_device *netdev ) {

	/* Do nothing if shutdown is in progress */
	if ( efi_shutdown_in_progress )
		return;

	/* Process any TX completions */
	snpnet_poll_tx ( netdev );

	/* Process any RX completions */
	snpnet_poll_rx ( netdev );

	/* Check for link state changes */
	snpnet_check_link ( netdev );
}

/**
 * Set receive filters
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int snpnet_rx_filters ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	UINT32 filters[] = {
		snp->snp->Mode->ReceiveFilterMask,
		( EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
		  EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
		  EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST ),
		( EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
		  EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST |
		  EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST ),
		( EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
		  EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST ),
		( EFI_SIMPLE_NETWORK_RECEIVE_UNICAST ),
	};
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Try possible receive filters in turn */
	for ( i = 0; i < ( sizeof ( filters ) / sizeof ( filters[0] ) ); i++ ) {
		efirc = snp->snp->ReceiveFilters ( snp->snp, filters[i],
				EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST, TRUE,
				0, NULL );
		if ( efirc == 0 )
			return 0;
		rc = -EEFI ( efirc );
		DBGC ( snp, "SNP %s could not set receive filters %#02x (have "
		       "%#02x): %s\n", netdev->name, filters[i],
		       snp->snp->Mode->ReceiveFilterSetting, strerror ( rc ) );
	}

	return rc;
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int snpnet_open ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	EFI_MAC_ADDRESS *mac = ( ( void * ) netdev->ll_addr );
	EFI_SIMPLE_NETWORK_MODE *mode = snp->snp->Mode;
	EFI_STATUS efirc;
	unsigned int retry;
	int rc;

	/* Try setting MAC address (before initialising) */
	if ( ( efirc = snp->snp->StationAddress ( snp->snp, FALSE, mac ) ) !=0){
		rc = -EEFI ( efirc );
		DBGC ( snp, "SNP %s could not set station address before "
		       "initialising: %s\n", netdev->name, strerror ( rc ) );
		/* Ignore error */
	}

	/* Initialise NIC, retrying multiple times if link stays down */
	for ( retry = 0 ; ; ) {

		/* Initialise NIC, if not already initialised */
		if ( ( mode->State != EfiSimpleNetworkInitialized ) &&
		     ( ( efirc = snp->snp->Initialize ( snp->snp,
							0, 0 ) ) != 0 ) ) {
			rc = -EEFI ( efirc );
			snpnet_dump_mode ( netdev );
			DBGC ( snp, "SNP %s could not initialise: %s\n",
			       netdev->name, strerror ( rc ) );
			return rc;
		}

		/* Stop if we have link up (or no link detection capability) */
		if ( ( ! mode->MediaPresentSupported ) || mode->MediaPresent )
			break;

		/* Stop if we have exceeded our retry count.  This is
		 * not a failure; it is plausible that we genuinely do
		 * not have link up.
		 */
		if ( ++retry >= SNP_INITIALIZE_RETRY_MAX )
			break;
		DBGC ( snp, "SNP %s retrying initialisation (retry %d)\n",
		       netdev->name, retry );

		/* Delay to allow time for link to establish */
		mdelay ( SNP_INITIALIZE_RETRY_DELAY_MS );

		/* Shut down and retry (unless device is insomniac);
		 * this is sometimes necessary in order to persuade
		 * the underlying SNP driver to actually update the
		 * link state.
		 */
		if ( ( ! netdev_insomniac ( netdev ) ) &&
		     ( ( efirc = snp->snp->Shutdown ( snp->snp ) ) != 0 ) ) {
			rc = -EEFI ( efirc );
			snpnet_dump_mode ( netdev );
			DBGC ( snp, "SNP %s could not shut down: %s\n",
			       netdev->name, strerror ( rc ) );
			return rc;
		}
	}

	/* Try setting MAC address (after initialising) */
	if ( ( efirc = snp->snp->StationAddress ( snp->snp, FALSE, mac ) ) !=0){
		rc = -EEFI ( efirc );
		DBGC ( snp, "SNP %s could not set station address after "
		       "initialising: %s\n", netdev->name, strerror ( rc ) );
		/* Ignore error */
	}

	/* Set receive filters */
	if ( ( rc = snpnet_rx_filters ( netdev ) ) != 0 ) {
		/* Ignore error */
	}

	/* Dump mode information (for debugging) */
	snpnet_dump_mode ( netdev );

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void snpnet_close ( struct net_device *netdev ) {
	struct snp_nic *snp = netdev->priv;
	EFI_STATUS efirc;
	int rc;

	/* Shut down NIC (unless whole system shutdown is in progress,
	 * or device is insomniac).
	 */
	if ( ( ! efi_shutdown_in_progress ) &&
	     ( ! netdev_insomniac ( netdev ) ) &&
	     ( ( efirc = snp->snp->Shutdown ( snp->snp ) ) != 0 ) ) {
		rc = -EEFI ( efirc );
		DBGC ( snp, "SNP %s could not shut down: %s\n",
		       netdev->name, strerror ( rc ) );
		/* Nothing we can do about this */
	}

	/* Discard transmit buffer, if applicable */
	if ( snp->txbuf ) {
		netdev_tx_complete_err ( netdev, snp->txbuf, -ECANCELED );
		snp->txbuf = NULL;
	}

	/* Discard receive buffer, if applicable */
	if ( snp->rxbuf ) {
		free_iob ( snp->rxbuf );
		snp->rxbuf = NULL;
	}
}

/** SNP network device operations */
static struct net_device_operations snpnet_operations = {
	.open = snpnet_open,
	.close = snpnet_close,
	.transmit = snpnet_transmit,
	.poll = snpnet_poll,
};

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @v protocol		Protocol GUID
 * @ret rc		Return status code
 */
int snpnet_supported ( EFI_HANDLE device, EFI_GUID *protocol ) {
	EFI_HANDLE parent;
	int rc;

	/* Check that this is not a device we are providing ourselves */
	if ( find_snpdev ( device ) != NULL ) {
		DBGCP ( device, "HANDLE %s is provided by this binary\n",
			efi_handle_name ( device ) );
		return -ENOTTY;
	}

	/* Test for presence of protocol */
	if ( ( rc = efi_test ( device, protocol ) ) != 0 ) {
		DBGCP ( device, "HANDLE %s is not a %s device\n",
			efi_handle_name ( device ),
			efi_guid_ntoa ( protocol ) );
		return rc;
	}

	/* Check that there are no instances of this protocol further
	 * up this device path.
	 */
	if ( ( rc = efi_locate_device ( device, protocol,
					&parent, 1 ) ) == 0 ) {
		DBGC2 ( device, "HANDLE %s has %s-supporting parent ",
			efi_handle_name ( device ),
			efi_guid_ntoa ( protocol ) );
		DBGC2 ( device, "%s\n", efi_handle_name ( parent ) );
		return -ENOTTY;
	}

	DBGC ( device, "HANDLE %s is a %s device\n",
	       efi_handle_name ( device ), efi_guid_ntoa ( protocol ) );
	return 0;
}

/**
 * Check if device must be insomniac
 *
 * @v device		EFI device handle
 * @v is_insomniac	Device must be insomniac
 */
static int snpnet_is_insomniac ( EFI_HANDLE device ) {
	int rc;

	/* Check for wireless devices
	 *
	 * The UEFI model for wireless network configuration is
	 * somewhat underdefined.  At the time of writing, the EDK2
	 * "UEFI WiFi Connection Manager" driver provides only one way
	 * to configure wireless network credentials, which is to
	 * enter them interactively via an HII form.  Credentials are
	 * not stored (or exposed via any protocol interface), and so
	 * any temporary disconnection from the wireless network will
	 * inevitably leave the interface in an unusable state that
	 * cannot be recovered without user intervention.
	 *
	 * Experimentation shows that at least some wireless network
	 * drivers will disconnect from the wireless network when the
	 * SNP Shutdown() method is called, or if the device is not
	 * polled sufficiently frequently to maintain its association
	 * to the network.  We therefore inhibit calls to Shutdown()
	 * and Stop() for any such SNP protocol interfaces, and mark
	 * our network device as insomniac so that it will be polled
	 * even when closed.
	 */
	if ( ( rc = efi_test ( device, &efi_wifi2_protocol_guid ) ) == 0 ) {
		DBGC ( device, "SNP %s is wireless: assuming insomniac\n",
		       efi_handle_name ( device ) );
		return 1;
	}

	return 0;
}

/**
 * Ignore shutdown attempt
 *
 * @v snp		SNP interface
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
snpnet_do_nothing ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused ) {

	return 0;
}

/**
 * Patch SNP protocol interface to prevent shutdown
 *
 * @v device		EFI device handle
 * @v patch		Interface patch
 * @ret rc		Return status code
 */
static int snpnet_insomniac_patch ( EFI_HANDLE device,
				    struct snp_insomniac_patch *patch ) {
	EFI_SIMPLE_NETWORK_PROTOCOL *interface;
	int rc;

	/* Open interface for ephemeral use */
	if ( ( rc = efi_open ( device, &efi_simple_network_protocol_guid,
			       &interface ) ) != 0 ) {
		DBGC ( device, "SNP %s cannot open SNP protocol for patching: "
		       "%s\n", efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Record original Shutdown() and Stop() methods */
	patch->shutdown = interface->Shutdown;
	patch->stop = interface->Stop;

	/* Inhibit other UEFI drivers' calls to Shutdown() and Stop()
	 *
	 * This is necessary since disconnecting the MnpDxe driver
	 * will attempt to shut down the SNP device, which would leave
	 * us with an unusable device.
	 */
	interface->Shutdown = snpnet_do_nothing;
	interface->Stop = snpnet_do_nothing;
	DBGC ( device, "SNP %s patched to inhibit shutdown\n",
	       efi_handle_name ( device ) );

	return 0;
}

/**
 * Restore patched SNP protocol interface
 *
 * @v device		EFI device handle
 * @v patch		Interface patch to fill in
 * @ret rc		Return status code
 */
static int snpnet_insomniac_restore ( EFI_HANDLE device,
				      struct snp_insomniac_patch *patch ) {
	EFI_SIMPLE_NETWORK_PROTOCOL *interface;
	int rc;

	/* Avoid returning uninitialised data on error */
	memset ( patch, 0, sizeof ( *patch ) );

	/* Open interface for ephemeral use */
	if ( ( rc = efi_open ( device, &efi_simple_network_protocol_guid,
			       &interface ) ) != 0 ) {
		DBGC ( device, "SNP %s cannot open patched SNP protocol: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Restore original Shutdown() and Stop() methods, if possible */
	if ( interface->Shutdown == snpnet_do_nothing )
		interface->Shutdown = patch->shutdown;
	if ( interface->Stop == snpnet_do_nothing )
		interface->Stop = patch->stop;

	/* Check that original methods were restored (by us or others) */
	if ( ( interface->Shutdown != patch->shutdown ) ||
	     ( interface->Stop != patch->stop ) ) {
		DBGC ( device, "SNP %s could not restore patched SNP "
		       "protocol\n", efi_handle_name ( device ) );
		return -EBUSY;
	}

	return 0;
}

/**
 * Exclude existing drivers
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
int snpnet_exclude ( EFI_HANDLE device ) {
	EFI_GUID *protocol = &efi_simple_network_protocol_guid;
	struct snp_insomniac_patch patch;
	int insomniac;
	int rc;

	/* Check if this is a device that must not ever be shut down */
	insomniac = snpnet_is_insomniac ( device );

	/* Inhibit calls to Shutdown() and Stop(), if applicable */
	if ( insomniac &&
	     ( ( rc = snpnet_insomniac_patch ( device, &patch ) ) != 0 ) ) {
		goto err_patch;
	}

	/* Exclude existing SNP drivers */
	if ( ( rc = efi_driver_exclude ( device, protocol ) ) != 0 ) {
		DBGC ( device, "SNP %s could not exclude drivers: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_exclude;
	}

 err_exclude:
	if ( insomniac )
		snpnet_insomniac_restore ( device, &patch );
 err_patch:
	return rc;
}

/**
 * Attach driver to device
 *
 * @v efidev		EFI device
 * @ret rc		Return status code
 */
int snpnet_start ( struct efi_device *efidev ) {
	EFI_HANDLE device = efidev->device;
	EFI_SIMPLE_NETWORK_PROTOCOL *interface;
	EFI_SIMPLE_NETWORK_MODE *mode;
	struct net_device *netdev;
	struct snp_nic *snp;
	EFI_STATUS efirc;
	int rc;

	/* Open SNP protocol */
	if ( ( rc = efi_open_by_driver ( device,
					 &efi_simple_network_protocol_guid,
					 &interface ) ) != 0 ) {
		DBGC ( device, "SNP %s cannot open SNP protocol: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		DBGC_EFI_OPENERS ( device, device,
				   &efi_simple_network_protocol_guid );
		goto err_open_protocol;
	}

	/* Allocate and initialise structure */
	netdev = alloc_etherdev ( sizeof ( *snp ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &snpnet_operations );
	snp = netdev->priv;
	snp->efidev = efidev;
	snp->snp = interface;
	mode = snp->snp->Mode;
	efidev_set_drvdata ( efidev, netdev );

	/* Populate underlying device information */
	efi_device_info ( device, "SNP", &snp->dev );
	snp->dev.driver_name = "SNP";
	snp->dev.parent = &efidev->dev;
	list_add ( &snp->dev.siblings, &efidev->dev.children );
	INIT_LIST_HEAD ( &snp->dev.children );
	netdev->dev = &snp->dev;

	/* Check if device is insomniac */
	if ( snpnet_is_insomniac ( device ) )
		netdev->state |= NETDEV_INSOMNIAC;

	/* Bring to the correct state for a closed interface */
	if ( ( mode->State == EfiSimpleNetworkStopped ) &&
	     ( ( efirc = snp->snp->Start ( snp->snp ) ) != 0 ) ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "SNP %s could not start: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_start;
	}
	if ( ( mode->State == EfiSimpleNetworkInitialized ) &&
	     ( ! netdev_insomniac ( netdev ) ) &&
	     ( ( efirc = snp->snp->Shutdown ( snp->snp ) ) != 0 ) ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "SNP %s could not shut down: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_shutdown;
	}

	/* Populate network device parameters */
	if ( mode->HwAddressSize != netdev->ll_protocol->hw_addr_len ) {
		DBGC ( device, "SNP %s has invalid hardware address length "
		       "%d\n", efi_handle_name ( device ), mode->HwAddressSize);
		rc = -ENOTSUP;
		goto err_hw_addr_len;
	}
	memcpy ( netdev->hw_addr, &mode->PermanentAddress,
		 netdev->ll_protocol->hw_addr_len );
	if ( mode->HwAddressSize != netdev->ll_protocol->ll_addr_len ) {
		DBGC ( device, "SNP %s has invalid link-layer address length "
		       "%d\n", efi_handle_name ( device ), mode->HwAddressSize);
		rc = -ENOTSUP;
		goto err_ll_addr_len;
	}
	memcpy ( netdev->ll_addr, &mode->CurrentAddress,
		 netdev->ll_protocol->ll_addr_len );
	snp->mtu = ( snp->snp->Mode->MaxPacketSize +
		     snp->snp->Mode->MediaHeaderSize );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;
	DBGC ( device, "SNP %s registered as %s\n",
	       efi_handle_name ( device ), netdev->name );

	/* Set initial link state */
	if ( snp->snp->Mode->MediaPresentSupported ) {
		snpnet_check_link ( netdev );
	} else {
		netdev_link_up ( netdev );
	}

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_ll_addr_len:
 err_hw_addr_len:
 err_shutdown:
 err_start:
	list_del ( &snp->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	efi_close_by_driver ( device, &efi_simple_network_protocol_guid );
 err_open_protocol:
	return rc;
}

/**
 * Detach driver from device
 *
 * @v efidev		EFI device
  */
void snpnet_stop ( struct efi_device *efidev ) {
	struct net_device *netdev = efidev_get_drvdata ( efidev );
	struct snp_nic *snp = netdev->priv;
	EFI_HANDLE device = efidev->device;
	EFI_STATUS efirc;
	int rc;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Stop SNP protocol (unless whole system shutdown is in progress) */
	if ( ( ! efi_shutdown_in_progress ) &&
	     ( ( efirc = snp->snp->Stop ( snp->snp ) ) != 0 ) ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "SNP %s could not stop: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		/* Nothing we can do about this */
	}

	/* Free network device */
	list_del ( &snp->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );

	/* Close SNP protocol */
	efi_close_by_driver ( device, &efi_simple_network_protocol_guid );
}
