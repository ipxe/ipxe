/*
 * Copyright (C) 2020 Google.
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

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/ethernet.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ipxe/efi/Protocol/ManagedNetwork.h>
#include <ipxe/efi/Protocol/ServiceBinding.h>

// An MNP NIC
struct mnp_nic {
  // Parent EFI device
  struct efi_device *efidev;

  // Managed network binding protocol
  EFI_SERVICE_BINDING_PROTOCOL *binding_interface;
  // Managed network protocol
  EFI_MANAGED_NETWORK_PROTOCOL *interface;

  // EFI child MNP handle
  // TODO(bur): Refactor holding a reference to the child device directly
  EFI_HANDLE child_device;

  // Maximum packet size
  // This is calculated as the sum of MediaHeaderSize and
  // MaxPacketSize, and may therefore be an overestimate
  size_t mtu;

  // Transmit token
  EFI_MANAGED_NETWORK_COMPLETION_TOKEN *txtok;
  // Transmit buffer, used to keep track of the current in-flight packet
  struct io_buffer *txbuf;

  // Receive token
  EFI_MANAGED_NETWORK_COMPLETION_TOKEN *rxtok;
};

// Transmit complete callback
static EFIAPI void mnp_tx_event(EFI_EVENT event __unused, void *context) {
  struct net_device *netdev = context;
  struct mnp_nic *mnp = netdev->priv;

  EFI_STATUS efirc;
  int rc = 0;
  if ((efirc = mnp->txtok->Status) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s tx event failed: %s\n", netdev->name, strerror(rc));
  }

  struct io_buffer *iobuf = mnp->txbuf;
  mnp->txbuf = NULL;
  netdev_tx_complete_err(netdev, iobuf, rc);
}

static int mnp_transmit(struct net_device *netdev, struct io_buffer *iobuf) {
  struct mnp_nic *mnp = netdev->priv;

  // Defer the packet if there is already a transmission in progress
  if (mnp->txbuf) {
    netdev_tx_defer(netdev, iobuf);
    return 0;
  }

  // Fill in the tx data
  // iobuf->data contains the ll header
  uint8_t header_len = netdev->ll_protocol->ll_header_len;
  mnp->txtok->Packet.TxData->DestinationAddress = NULL;
  mnp->txtok->Packet.TxData->SourceAddress = NULL;
  mnp->txtok->Packet.TxData->HeaderLength = header_len;
  mnp->txtok->Packet.TxData->DataLength = iob_len(iobuf) - header_len;
  mnp->txtok->Packet.TxData->FragmentCount = 1;
  mnp->txtok->Packet.TxData->FragmentTable[0].FragmentLength = iob_len(iobuf);
  mnp->txtok->Packet.TxData->FragmentTable[0].FragmentBuffer = iobuf->data;

  // Keep track of the current packet
  mnp->txbuf = iobuf;

  EFI_STATUS efirc;
  if ((efirc = mnp->interface->Transmit(mnp->interface, mnp->txtok)) !=
      EFI_SUCCESS) {
    int rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s cannot transmit: %s\n", netdev->name, strerror(rc));
    mnp->txbuf = NULL;
    return rc;
  }

  return 0;
}

static void recycle_and_receive(struct net_device *netdev) {
  struct mnp_nic *mnp = netdev->priv;
  EFI_STATUS efirc;
  int rc;

  EFI_MANAGED_NETWORK_RECEIVE_DATA *rxdata = mnp->rxtok->Packet.RxData;
  if (rxdata && rxdata->RecycleEvent) {
    EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
    if ((efirc = bs->SignalEvent(rxdata->RecycleEvent)) != EFI_SUCCESS) {
      rc = -EEFI(efirc);
      DBGC(netdev, "MNP %s signal rx recycle event failed: %s\n", netdev->name,
           strerror(rc));
    }
  }
  if ((efirc = mnp->interface->Receive(mnp->interface, mnp->rxtok)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s receive failed: %s\n", netdev->name, strerror(rc));
  }
}

// Receive complete callback
static EFIAPI void mnp_rx_event(EFI_EVENT event __unused, void *context) {
  struct net_device *netdev = context;
  struct mnp_nic *mnp = netdev->priv;

  if (mnp->rxtok->Status != EFI_SUCCESS) {
    int rc = -EEFI(mnp->rxtok->Status);
    DBGC(netdev, "MNP %s mnp_rx_event failed: %s\n", netdev->name,
         strerror(rc));
    netdev_rx_err(netdev, NULL, -EEFI(mnp->rxtok->Status));
    recycle_and_receive(netdev);
    return;
  }

  // Copy the packet an pass up the network stack
  EFI_MANAGED_NETWORK_RECEIVE_DATA *rxdata = mnp->rxtok->Packet.RxData;
  struct io_buffer *rxbuf = alloc_iob(rxdata->PacketLength);
  memcpy(rxbuf->data, rxdata->MediaHeader, rxdata->PacketLength);
  iob_put(rxbuf, rxdata->PacketLength);
  netdev_rx(netdev, rxbuf);

  recycle_and_receive(netdev);
}

static void mnp_poll(struct net_device *netdev __unused) {
  struct mnp_nic *mnp = netdev->priv;

  // Supplement the internal MNP polling
  EFI_STATUS efirc = mnp->interface->Poll(mnp->interface);
  if (!(efirc == EFI_SUCCESS || efirc == EFI_NOT_READY)) {
    int rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s poll failed: %s\n", netdev->name, strerror(rc));
  }
}

// Default MNP config with receive filters set
static EFI_MANAGED_NETWORK_CONFIG_DATA mnp_config = {
    .ReceivedQueueTimeoutValue = 10000,
    .TransmitQueueTimeoutValue = 10000,
    .ProtocolTypeFilter = 0,
    .EnableUnicastReceive = TRUE,
    .EnableMulticastReceive = TRUE,
    .EnableBroadcastReceive = TRUE,
    .EnablePromiscuousReceive = FALSE,
    .FlushQueuesOnReset = FALSE,
    .EnableReceiveTimestamps = FALSE,
    .DisableBackgroundPolling = FALSE,
};

static int txtok_alloc_init(struct net_device *netdev) {
  struct mnp_nic *mnp = netdev->priv;

  // Create the tx event used during this session
  EFI_STATUS efirc;
  int rc;
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  EFI_EVENT tx_event;
  if ((efirc = bs->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, mnp_tx_event,
                               netdev, &tx_event)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s create tx event failed: %s\n", netdev->name,
         strerror(rc));
    return rc;
  }

  // Allocate the token
  mnp->txtok = zalloc(sizeof(*mnp->txtok));
  if (!mnp->txtok) {
    DBGC(netdev, "MNP %s txtok alloc failed: out of resources\n", netdev->name);
    return -ENOMEM;
  }
  // Allocate the tx data
  mnp->txtok->Packet.TxData = zalloc(sizeof(*mnp->txtok->Packet.TxData));
  if (!mnp->txtok->Packet.TxData) {
    DBGC(netdev, "MNP %s tx data alloc failed: out of resources\n",
         netdev->name);
    return -ENOMEM;
  }
  mnp->txtok->Event = tx_event;
  mnp->txbuf = NULL;

  return 0;
}

static EFI_STATUS rxtok_alloc_init(struct net_device *netdev) {
  struct mnp_nic *mnp = netdev->priv;

  // Create the rx event used during this session
  EFI_STATUS efirc;
  int rc;
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  EFI_EVENT rx_event;
  if ((efirc = bs->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, mnp_rx_event,
                               netdev, &rx_event)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s create rx event failed: %s\n", netdev->name,
         strerror(rc));
    return efirc;
  }

  mnp->rxtok = zalloc(sizeof(*mnp->rxtok));
  mnp->rxtok->Event = rx_event;
  return EFI_SUCCESS;
}

// NOP function
static void mnp_nop() {}

// Weak link with efi_unload_apple_images from the "apple" driver
void efi_unload_apple_images(EFI_HANDLE handle)
    __attribute__((weak, alias("mnp_nop")));

static int mnp_open(struct net_device *netdev) {
  struct mnp_nic *mnp = netdev->priv;

  // TODO(bur): Find a less hacky way of accomplishing this.
  // Ideally we would call this from start or similar, but unloading images
  // from within the ConnectController contex seems bad. Calling it from
  // mnp_supported works, but also seems bad.
  // If linked with the "apple" driver, unload apple images bound to this device
  // Having dual UDP stacks causes TFTP confusion, so unload Apple's.
  efi_unload_apple_images(mnp->efidev->device);

  // Allocate and initialize the tokens
  // They will be reused throughout this session
  int rc;
  if ((rc = txtok_alloc_init(netdev)) != 0) return rc;
  if ((rc = rxtok_alloc_init(netdev)) != 0) return rc;

  // Open the NIC
  EFI_STATUS efirc;
  if ((efirc = mnp->interface->Configure(mnp->interface, &mnp_config)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s configure failed: %s\n", netdev->name, strerror(rc));
    return rc;
  }

  // Kick off the Receive loop
  if ((efirc = mnp->interface->Receive(mnp->interface, mnp->rxtok)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s receive failed: %s\n", netdev->name, strerror(rc));
    return rc;
  }

  return 0;
}

static void mnp_close(struct net_device *netdev) {
  struct mnp_nic *mnp = netdev->priv;

  // Close the NIC
  EFI_STATUS efirc;
  int rc;
  if ((efirc = mnp->interface->Configure(mnp->interface, NULL)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s configure failed: %s\n", netdev->name, strerror(rc));
    // Nothing we can do about this
  }

  // Close the token events
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  if ((efirc = bs->CloseEvent(mnp->txtok->Event)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s close tx event failed: %s\n", netdev->name,
         strerror(rc));
  }
  if ((efirc = bs->CloseEvent(mnp->rxtok->Event)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s close rx event failed: %s\n", netdev->name,
         strerror(rc));
  }

  // Discard transmit buffer, if applicable
  if (mnp->txbuf) {
    struct io_buffer *iobuf = mnp->txbuf;
    mnp->txbuf = NULL;
    netdev_tx_complete_err(netdev, iobuf, -ECANCELED);
  }

  // Free the tokens
  free(mnp->txtok->Packet.TxData);
  mnp->txtok->Packet.TxData = NULL;  // just because
  free(mnp->txtok);
  mnp->txtok = NULL;
  free(mnp->rxtok);
  mnp->rxtok = NULL;

  return;
}

// MNP network device operations
static struct net_device_operations mnp_operations = {
    .open = mnp_open,
    .close = mnp_close,
    .transmit = mnp_transmit,
    .poll = mnp_poll,
};

// Attach driver to device
int mnp_start(struct efi_device *efidev) {
  // Open MNP Binding protocol
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  EFI_HANDLE device = efidev->device;
  EFI_STATUS efirc;
  int rc;
  void *binding_interface = NULL;

  if ((efirc = bs->OpenProtocol(
           device, &efi_managed_network_service_binding_protocol_guid,
           &binding_interface, efi_image_handle, device,
           EFI_OPEN_PROTOCOL_BY_DRIVER | EFI_OPEN_PROTOCOL_EXCLUSIVE)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(device, "MNP %s open MNP binding protocol failed: %s\n",
         efi_handle_name(device), strerror(rc));
    DBGC_EFI_OPENERS(device, device,
                     &efi_managed_network_service_binding_protocol_guid);
    goto err_open_binding_protocol;
  }

  // Create child handle
  // This creates our own multiplexed MNP driver instance
  EFI_SERVICE_BINDING_PROTOCOL *mnp_binding = binding_interface;
  EFI_HANDLE child_device = NULL;
  if ((efirc = mnp_binding->CreateChild(mnp_binding, &child_device) !=
               EFI_SUCCESS)) {
    rc = -EEFI(efirc);
    DBGC(device, "MNP %s create child failed: %s\n", efi_handle_name(device),
         strerror(rc));
    goto err_create_child_protocol;
  }

  // Open MNP protocol
  void *interface = NULL;
  if ((efirc = bs->OpenProtocol(
           child_device, &efi_managed_network_protocol_guid, &interface,
           efi_image_handle, child_device,
           EFI_OPEN_PROTOCOL_BY_DRIVER | EFI_OPEN_PROTOCOL_EXCLUSIVE)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(child_device, "MNP %s open MNP protocol failed: %s\n",
         efi_handle_name(child_device), strerror(rc));
    DBGC_EFI_OPENERS(child_device, child_device,
                     &efi_managed_network_protocol_guid);
    goto err_open_protocol;
  }

  // Allocate and initialize the netdev structure
  struct net_device *netdev;
  struct mnp_nic *mnp;
  netdev = alloc_etherdev(sizeof(*mnp));
  if (!netdev) {
    rc = -ENOMEM;
    goto err_alloc;
  }
  netdev_init(netdev, &mnp_operations);
  // efidev->priv (->) netdev->priv (->) mnp
  mnp = netdev->priv;
  mnp->efidev = efidev;
  mnp->interface = interface;
  mnp->binding_interface = binding_interface;
  efidev_set_drvdata(efidev, netdev);

  // TODO(bur): Lookup child instead of storing it.
  mnp->child_device = child_device;

  // Populate underlying device information
  netdev->dev = zalloc(sizeof(*netdev->dev));
  efi_device_info(device, "MNP", netdev->dev);
  netdev->dev->driver_name = "MNP";
  netdev->dev->parent = &efidev->dev;

  // Open our instance of the NIC to read its MAC address
  if ((efirc = mnp->interface->Configure(mnp->interface, &mnp_config)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s cannot configure: %s\n", netdev->name, strerror(rc));
    goto err_config;
  }

  // Get the NIC's info
  EFI_SIMPLE_NETWORK_MODE mode;
  // Needed, for GetModeData to succeed.
  EFI_MANAGED_NETWORK_CONFIG_DATA mmnp_config;
  if ((efirc = mnp->interface->GetModeData(mnp->interface, &mmnp_config,
                                           &mode)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s cannot get mode: %s\n", netdev->name, strerror(rc));
    goto err_mode;
  }

  // Close our instance of the NIC
  if ((efirc = mnp->interface->Configure(mnp->interface, NULL)) !=
      EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(netdev, "MNP %s cannot configure: %s\n", netdev->name, strerror(rc));
    goto err_config;
  }

  // Populate network device parameters
  if (mode.MediaHeaderSize != netdev->ll_protocol->ll_header_len) {
    DBGC(device,
         "MNP %s has invalid media header length "
         "%d\n",
         efi_handle_name(device), mode.MediaHeaderSize);
    rc = -ENOTSUP;
    goto err_config;
  }
  mnp->mtu = mode.MaxPacketSize;
  if (mode.HwAddressSize != netdev->ll_protocol->hw_addr_len) {
    DBGC(device,
         "MNP %s has invalid hardware address length "
         "%d\n",
         efi_handle_name(device), mode.HwAddressSize);
    rc = -ENOTSUP;
    goto err_config;
  }
  memcpy(netdev->hw_addr, &mode.PermanentAddress,
         netdev->ll_protocol->hw_addr_len);
  if (mode.HwAddressSize != netdev->ll_protocol->ll_addr_len) {
    DBGC(device,
         "MNP %s has invalid link-layer address length "
         "%d\n",
         efi_handle_name(device), mode.HwAddressSize);
    rc = -ENOTSUP;
    goto err_config;
  }
  memcpy(netdev->ll_addr, &mode.CurrentAddress,
         netdev->ll_protocol->ll_addr_len);

  // Register network device
  if ((rc = register_netdev(netdev)) != 0) goto err_config;
  DBGC(device, "MNP %s registered as %s\n", efi_handle_name(device),
       netdev->name);

  // Always set the link to up.
  // Even when mode->MediaPresentSupported is true, mode->MediaPresent doesn't
  // seem to be updated during GetModeData(). Rebinding to the device does seem
  // to update mode->MediaPresent.
  netdev_link_up(netdev);
  return 0;

err_mode:
  mnp->interface->Configure(mnp->interface, NULL);
err_config:
  free(netdev->dev);
  netdev_nullify(netdev);
  netdev_put(netdev);
err_alloc:
  bs->CloseProtocol(device, &efi_managed_network_protocol_guid,
                    efi_image_handle, device);
err_open_protocol:
  mnp_binding->DestroyChild(mnp_binding, child_device);
err_create_child_protocol:
  bs->CloseProtocol(device, &efi_managed_network_service_binding_protocol_guid,
                    efi_image_handle, device);
err_open_binding_protocol:
  return rc;
}

// Detach driver from device
void mnp_stop(struct efi_device *efidev) {
  EFI_HANDLE device = efidev->device;
  struct net_device *netdev = efidev_get_drvdata(efidev);
  struct mnp_nic *mnp = netdev->priv;

  // Unregister network device
  // This will trigger mnp_close
  unregister_netdev(netdev);

  EFI_STATUS efirc;
  int rc;
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  if ((efirc = bs->CloseProtocol(
           mnp->child_device, &efi_managed_network_protocol_guid,
           efi_image_handle, mnp->child_device)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(device, "MNP %s could not close protocol: %s\n",
         efi_handle_name(device), strerror(rc));
    DBGC_EFI_OPENERS(mnp->child_device, mnp->child_device,
                     &efi_managed_network_protocol_guid);
    // Nothing we can do about this
  }

  if ((efirc = mnp->binding_interface->DestroyChild(
           mnp->binding_interface, mnp->child_device)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(device, "MNP %s could not destory child: %s\n",
         efi_handle_name(device), strerror(rc));
    // Nothing we can do about this
  }

  // Free network device
  free(netdev->dev);
  netdev_nullify(netdev);
  netdev_put(netdev);

  // Stop MNP protocol
  if ((efirc = bs->CloseProtocol(
           device, &efi_managed_network_service_binding_protocol_guid,
           efi_image_handle, device)) != EFI_SUCCESS) {
    rc = -EEFI(efirc);
    DBGC(device, "MNP %s could not close protocol: %s\n",
         efi_handle_name(device), strerror(rc));
    // Nothing we can do about this
  }

  return;
}

// Check to see if driver supports a device
static int mnp_supported(EFI_HANDLE device) {
  // Test for presence of managed network service binding protocol
  EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
  EFI_STATUS efirc;
  if ((efirc = bs->OpenProtocol(
           device, &efi_managed_network_service_binding_protocol_guid, NULL,
           efi_image_handle, device, EFI_OPEN_PROTOCOL_TEST_PROTOCOL)) !=
      EFI_SUCCESS) {
    DBGCP(device, "MNP %s is not an MNP device\n", efi_handle_name(device));
    return -EEFI(efirc);
  }

  DBGC(device, "MNP %s is an MNP device\n", efi_handle_name(device));
  return 0;
}

// EFI MNP driver
struct efi_driver mnp_driver __efi_driver(EFI_DRIVER_NORMAL) = {
    .name = "MNP",
    .supported = mnp_supported,
    .start = mnp_start,
    .stop = mnp_stop,
};
