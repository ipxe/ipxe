/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * MNP NIC driver
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/cachedhcp.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_service.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/mnpnet.h>
#include <ipxe/efi/Protocol/ManagedNetwork.h>

/** An MNP transmit or receive token */
struct mnp_token {
	/** MNP completion token */
	EFI_MANAGED_NETWORK_COMPLETION_TOKEN token;
	/** Token is owned by MNP */
	int busy;
};

/** An MNP NIC */
struct mnp_nic {
	/** EFI device */
	struct efi_device *efidev;
	/** Managed network protocol */
	EFI_MANAGED_NETWORK_PROTOCOL *mnp;
	/** Generic device */
	struct device dev;

	/** Transmit token */
	struct mnp_token tx;
	/** Transmit descriptor */
	EFI_MANAGED_NETWORK_TRANSMIT_DATA txdata;
	/** Transmit I/O buffer */
	struct io_buffer *txbuf;

	/** Receive token */
	struct mnp_token rx;
};

/**
 * Transmit or receive token event
 *
 * @v event		Event
 * @v context		Event context
 */
static VOID EFIAPI mnpnet_event ( EFI_EVENT event __unused, VOID *context ) {
	struct mnp_token *token = context;

	/* Sanity check */
	assert ( token->busy );

	/* Mark token as no longer owned by MNP */
	token->busy = 0;
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int mnpnet_transmit ( struct net_device *netdev,
			     struct io_buffer *iobuf ) {
	struct mnp_nic *mnp = netdev->priv;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing if shutdown is in progress */
	if ( efi_shutdown_in_progress )
		return -ECANCELED;

	/* Defer the packet if there is already a transmission in progress */
	if ( mnp->txbuf ) {
		netdev_tx_defer ( netdev, iobuf );
		return 0;
	}

	/* Construct transmit token */
	mnp->txdata.DataLength =
		( iob_len ( iobuf ) - ll_protocol->ll_header_len );
	mnp->txdata.HeaderLength = ll_protocol->ll_header_len;
	mnp->txdata.FragmentCount = 1;
	mnp->txdata.FragmentTable[0].FragmentLength = iob_len ( iobuf );
	mnp->txdata.FragmentTable[0].FragmentBuffer = iobuf->data;
	mnp->tx.token.Packet.TxData = &mnp->txdata;

	/* Record as in use */
	mnp->tx.busy = 1;

	/* Transmit packet */
	if ( ( efirc = mnp->mnp->Transmit ( mnp->mnp, &mnp->tx.token ) ) != 0 ){
		rc = -EEFI ( efirc );
		DBGC ( mnp, "MNP %s could not transmit: %s\n",
		       netdev->name, strerror ( rc ) );
		mnp->tx.busy = 0;
		return rc;
	}

	/* Record I/O buffer */
	mnp->txbuf = iobuf;

	return 0;
}

/**
 * Refill receive token
 *
 * @v netdev		Network device
 */
static void mnpnet_refill_rx ( struct net_device *netdev ) {
	struct mnp_nic *mnp = netdev->priv;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing if receive token is still in use */
	if ( mnp->rx.busy )
		return;

	/* Mark as in use */
	mnp->rx.busy = 1;

	/* Queue receive token */
	if ( ( efirc = mnp->mnp->Receive ( mnp->mnp, &mnp->rx.token ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( mnp, "MNP %s could not receive: %s\n",
		       netdev->name, strerror ( rc ) );
		/* Wait for next refill */
		mnp->rx.busy = 0;
		return;
	}
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void mnpnet_poll_tx ( struct net_device *netdev ) {
	struct mnp_nic *mnp = netdev->priv;
	struct io_buffer *iobuf;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing if transmit token is still in use */
	if ( mnp->tx.busy )
		return;

	/* Do nothing unless we have a completion */
	if ( ! mnp->txbuf )
		return;

	/* Get completion status */
	efirc = mnp->tx.token.Status;
	rc = ( efirc ? -EEFI ( efirc ) : 0 );

	/* Complete transmission */
	iobuf = mnp->txbuf;
	mnp->txbuf = NULL;
	netdev_tx_complete_err ( netdev, iobuf, rc );
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void mnpnet_poll_rx ( struct net_device *netdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct mnp_nic *mnp = netdev->priv;
	EFI_MANAGED_NETWORK_RECEIVE_DATA *rxdata;
	struct io_buffer *iobuf;
	size_t len;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing unless we have a completion */
	if ( mnp->rx.busy )
		return;
	rxdata = mnp->rx.token.Packet.RxData;

	/* Get completion status */
	if ( ( efirc = mnp->rx.token.Status ) != 0 ) {
		rc = -EEFI ( efirc );
		netdev_rx_err ( netdev, NULL, rc );
		goto recycle;
	}

	/* Allocate and fill I/O buffer */
	len = rxdata->PacketLength;
	iobuf = alloc_iob ( len );
	if ( ! iobuf ) {
		netdev_rx_err ( netdev, NULL, -ENOMEM );
		goto recycle;
	}
	memcpy ( iob_put ( iobuf, len ), rxdata->MediaHeader, len );

	/* Hand off to network stack */
	netdev_rx ( netdev, iobuf );

 recycle:
	/* Recycle token */
	bs->SignalEvent ( rxdata->RecycleEvent );
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void mnpnet_poll ( struct net_device *netdev ) {
	struct mnp_nic *mnp = netdev->priv;

	/* Do nothing if shutdown is in progress */
	if ( efi_shutdown_in_progress )
		return;

	/* Poll interface */
	mnp->mnp->Poll ( mnp->mnp );

	/* Process any transmit completions */
	mnpnet_poll_tx ( netdev );

	/* Process any receive completions */
	mnpnet_poll_rx ( netdev );

	/* Refill receive token */
	mnpnet_refill_rx ( netdev );
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int mnpnet_open ( struct net_device *netdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	static EFI_MANAGED_NETWORK_CONFIG_DATA config = {
		.EnableUnicastReceive = TRUE,
		.EnableMulticastReceive = TRUE,
		.EnableBroadcastReceive = TRUE,
		.EnablePromiscuousReceive = TRUE,
		.FlushQueuesOnReset = TRUE,
		.DisableBackgroundPolling = TRUE,
	};
	struct mnp_nic *mnp = netdev->priv;
	EFI_STATUS efirc;
	int rc;

	/* Create transmit event */
	if ( ( efirc = bs->CreateEvent ( EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
					 mnpnet_event, &mnp->tx,
					 &mnp->tx.token.Event ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( mnp, "MNP %s could not create TX event: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_tx_event;
	}

	/* Create receive event */
	if ( ( efirc = bs->CreateEvent ( EVT_NOTIFY_SIGNAL, TPL_NOTIFY,
					 mnpnet_event, &mnp->rx,
					 &mnp->rx.token.Event ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( mnp, "MNP %s could not create RX event: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_rx_event;
	}

	/* Configure MNP */
	if ( ( efirc = mnp->mnp->Configure ( mnp->mnp, &config ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( mnp, "MNP %s could not configure: %s\n",
		       netdev->name, strerror ( rc ) );
		goto err_configure;
	}

	/* Refill receive token */
	mnpnet_refill_rx ( netdev );

	return 0;

	mnp->mnp->Configure ( mnp->mnp, NULL );
 err_configure:
	bs->CloseEvent ( mnp->rx.token.Event );
 err_rx_event:
	bs->CloseEvent ( mnp->tx.token.Event );
 err_tx_event:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void mnpnet_close ( struct net_device *netdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct mnp_nic *mnp = netdev->priv;

	/* Reset MNP (unless whole system shutdown is in progress) */
	if ( ! efi_shutdown_in_progress )
		mnp->mnp->Configure ( mnp->mnp, NULL );

	/* Close events */
	bs->CloseEvent ( mnp->rx.token.Event );
	bs->CloseEvent ( mnp->tx.token.Event );

	/* Reset tokens */
	mnp->tx.busy = 0;
	mnp->rx.busy = 0;

	/* Discard any incomplete I/O buffer */
	if ( mnp->txbuf ) {
		netdev_tx_complete_err ( netdev, mnp->txbuf, -ECANCELED );
		mnp->txbuf = NULL;
	}
}

/** MNP network device operations */
static struct net_device_operations mnpnet_operations = {
	.open = mnpnet_open,
	.close = mnpnet_close,
	.transmit = mnpnet_transmit,
	.poll = mnpnet_poll,
};

/**
 * Attach driver to device
 *
 * @v efidev		EFI device
 * @ret rc		Return status code
 */
int mnpnet_start ( struct efi_device *efidev ) {
	EFI_HANDLE device = efidev->device;
	EFI_GUID *binding = &efi_managed_network_service_binding_protocol_guid;
	EFI_SIMPLE_NETWORK_MODE mode;
	struct net_device *netdev;
	struct mnp_nic *mnp;
	EFI_STATUS efirc;
	int rc;

	/* Allocate and initalise structure */
	netdev = alloc_etherdev ( sizeof ( *mnp ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &mnpnet_operations );
	mnp = netdev->priv;
	mnp->efidev = efidev;
	efidev_set_drvdata ( efidev, netdev );

	/* Populate underlying device information */
	efi_device_info ( device, "MNP", &mnp->dev );
	mnp->dev.driver_name = "MNP";
	mnp->dev.parent = &efidev->dev;
	list_add ( &mnp->dev.siblings, &efidev->dev.children );
	INIT_LIST_HEAD ( &mnp->dev.children );
	netdev->dev = &mnp->dev;

	/* Create MNP child */
	if ( ( rc = efi_service_add ( device, binding,
				      &efidev->child ) ) != 0 ) {
		DBGC ( mnp, "MNP %s could not create child: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_service;
	}

	/* Open MNP protocol */
	if ( ( rc = efi_open_by_driver ( efidev->child,
					 &efi_managed_network_protocol_guid,
					 &mnp->mnp ) ) != 0 ) {
		DBGC ( mnp, "MNP %s could not open MNP protocol: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_open;
	}

	/* Get configuration */
	efirc = mnp->mnp->GetModeData ( mnp->mnp, NULL, &mode );
	if ( ( efirc != 0 ) && ( efirc != EFI_NOT_STARTED ) ) {
		rc = -EEFI ( efirc );
		DBGC ( mnp, "MNP %s could not get mode data: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_mode;
	}

	/* Populate network device parameters */
	if ( mode.HwAddressSize != netdev->ll_protocol->hw_addr_len ) {
		DBGC ( device, "MNP %s has invalid hardware address length "
		       "%d\n", efi_handle_name ( device ), mode.HwAddressSize );
		rc = -ENOTSUP;
		goto err_hw_addr_len;
	}
	memcpy ( netdev->hw_addr, &mode.PermanentAddress,
		 netdev->ll_protocol->hw_addr_len );
	if ( mode.HwAddressSize != netdev->ll_protocol->ll_addr_len ) {
		DBGC ( device, "MNP %s has invalid link-layer address length "
		       "%d\n", efi_handle_name ( device ), mode.HwAddressSize );
		rc = -ENOTSUP;
		goto err_ll_addr_len;
	}
	memcpy ( netdev->ll_addr, &mode.CurrentAddress,
		 netdev->ll_protocol->ll_addr_len );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;
	DBGC ( mnp, "MNP %s registered as %s\n",
	       efi_handle_name ( device ), netdev->name );

	/* Mark as link up: we don't handle link state */
	netdev_link_up ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register:
 err_ll_addr_len:
 err_hw_addr_len:
 err_mode:
	efi_close_by_driver ( efidev->child,
			      &efi_managed_network_protocol_guid );
 err_open:
	efi_service_del ( device, binding, efidev->child );
 err_service:
	list_del ( &mnp->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Detach driver from device
 *
 * @v efidev		EFI device
  */
void mnpnet_stop ( struct efi_device *efidev ) {
	EFI_GUID *binding = &efi_managed_network_service_binding_protocol_guid;
	struct net_device *netdev = efidev_get_drvdata ( efidev );
	struct mnp_nic *mnp = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Close MNP protocol */
	efi_close_by_driver ( efidev->child,
			      &efi_managed_network_protocol_guid );

	/* Remove MNP child (unless whole system shutdown is in progress) */
	if ( ! efi_shutdown_in_progress )
		efi_service_del ( efidev->device, binding, efidev->child );

	/* Free network device */
	list_del ( &mnp->dev.siblings );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/**
 * Create temporary MNP network device
 *
 * @v handle		MNP service binding handle
 * @v netdev		Network device to fill in
 * @ret rc		Return status code
 */
int mnptemp_create ( EFI_HANDLE handle, struct net_device **netdev ) {
	struct efi_device *efidev;
	int rc;

	/* Create temporary EFI device */
	efidev = efidev_alloc ( handle );
	if ( ! efidev ) {
		DBGC ( handle, "MNP %s could not create temporary device\n",
		       efi_handle_name ( handle ) );
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Start temporary network device */
	if ( ( rc = mnpnet_start ( efidev ) ) != 0 ) {
		DBGC ( handle, "MNP %s could not start MNP: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_start;
	}

	/* Fill in network device */
	*netdev = efidev_get_drvdata ( efidev );

	return 0;

	mnpnet_stop ( efidev );
 err_start:
	efidev_free ( efidev );
 err_alloc:
	return rc;
}

/**
 * Destroy temporary MNP network device
 *
 * @v netdev		Network device
 */
void mnptemp_destroy ( struct net_device *netdev ) {
	struct mnp_nic *mnp = netdev->priv;
	struct efi_device *efidev = mnp->efidev;

	/* Recycle any cached DHCP packet */
	cachedhcp_recycle ( netdev );

	/* Stop temporary network device */
	mnpnet_stop ( efidev );

	/* Free temporary EFI device */
	efidev_free ( efidev );
}
