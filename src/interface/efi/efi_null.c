/*
 * Copyright (C) 2020 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_null.h>

/** @file
 *
 * EFI null interfaces
 *
 */

/******************************************************************************
 *
 * Simple Network Protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_snp_start ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_stop ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_initialize ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			  UINTN extra_rx_bufsize __unused,
			  UINTN extra_tx_bufsize __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_reset ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
		     BOOLEAN ext_verify __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_shutdown ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_receive_filters ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			       UINT32 enable __unused,
			       UINT32 disable __unused,
			       BOOLEAN mcast_reset __unused,
			       UINTN mcast_count __unused,
			       EFI_MAC_ADDRESS *mcast __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_station_address ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			       BOOLEAN reset __unused,
			       EFI_MAC_ADDRESS *new __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_statistics ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			  BOOLEAN reset __unused, UINTN *stats_len __unused,
			  EFI_NETWORK_STATISTICS *stats __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_mcast_ip_to_mac ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			       BOOLEAN ipv6 __unused,
			       EFI_IP_ADDRESS *ip __unused,
			       EFI_MAC_ADDRESS *mac __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_nvdata ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
		      BOOLEAN read __unused, UINTN offset __unused,
		      UINTN len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_get_status ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			  UINT32 *interrupts __unused, VOID **txbuf __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_transmit ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
			UINTN ll_header_len __unused, UINTN len __unused,
			VOID *data __unused, EFI_MAC_ADDRESS *ll_src __unused,
			EFI_MAC_ADDRESS *ll_dest __unused,
			UINT16 *net_proto __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_snp_receive ( EFI_SIMPLE_NETWORK_PROTOCOL *snp __unused,
		       UINTN *ll_header_len __unused, UINTN *len __unused,
		       VOID *data __unused, EFI_MAC_ADDRESS *ll_src __unused,
		       EFI_MAC_ADDRESS *ll_dest __unused,
		       UINT16 *net_proto __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_SIMPLE_NETWORK_PROTOCOL efi_null_snp = {
	.Revision	= EFI_SIMPLE_NETWORK_PROTOCOL_REVISION,
	.Start		= efi_null_snp_start,
	.Stop		= efi_null_snp_stop,
	.Initialize	= efi_null_snp_initialize,
	.Reset		= efi_null_snp_reset,
	.Shutdown	= efi_null_snp_shutdown,
	.ReceiveFilters	= efi_null_snp_receive_filters,
	.StationAddress	= efi_null_snp_station_address,
	.Statistics	= efi_null_snp_statistics,
	.MCastIpToMac	= efi_null_snp_mcast_ip_to_mac,
	.NvData		= efi_null_snp_nvdata,
	.GetStatus	= efi_null_snp_get_status,
	.Transmit	= efi_null_snp_transmit,
	.Receive	= efi_null_snp_receive,
};

/**
 * Nullify SNP interface
 *
 * @v snp		SNP interface
 */
void efi_nullify_snp ( EFI_SIMPLE_NETWORK_PROTOCOL *snp ) {

	memcpy ( snp, &efi_null_snp,
		 offsetof ( typeof ( *snp ), WaitForPacket ) );
	snp->Mode->State = EfiSimpleNetworkStopped;
}

/******************************************************************************
 *
 * Network Interface Identification protocol
 *
 ******************************************************************************
 */

static EFIAPI VOID efi_null_undi_issue ( UINT64 cdb_phys ) {
	PXE_CDB *cdb = ( ( void * ) ( intptr_t ) cdb_phys );

	cdb->StatCode = PXE_STATCODE_UNSUPPORTED;
	cdb->StatFlags = PXE_STATFLAGS_COMMAND_FAILED;
}

static PXE_SW_UNDI efi_null_undi __attribute__ (( aligned ( 16 ) )) = {
	.Signature	= PXE_ROMID_SIGNATURE,
	.Len		= sizeof ( efi_null_undi ),
	.Rev		= PXE_ROMID_REV,
	.MajorVer	= PXE_ROMID_MAJORVER,
	.MinorVer	= PXE_ROMID_MINORVER,
	.Implementation	= PXE_ROMID_IMP_SW_VIRT_ADDR,
};

/**
 * Nullify NII interface
 *
 * @v nii		NII interface
 */
void efi_nullify_nii ( EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL *nii ) {
	efi_null_undi.EntryPoint = ( ( intptr_t ) efi_null_undi_issue );
	nii->Id = ( ( intptr_t ) &efi_null_undi );
}

/******************************************************************************
 *
 * VLAN configuration protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_vlan_set ( EFI_VLAN_CONFIG_PROTOCOL *vcfg __unused,
		    UINT16 tag __unused, UINT8 priority __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_vlan_find ( EFI_VLAN_CONFIG_PROTOCOL *vcfg __unused,
		     UINT16 *filter __unused, UINT16 *count __unused,
		     EFI_VLAN_FIND_DATA **entries __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_vlan_remove ( EFI_VLAN_CONFIG_PROTOCOL *vcfg __unused,
		       UINT16 tag __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_VLAN_CONFIG_PROTOCOL efi_null_vlan = {
	.Set		= efi_null_vlan_set,
	.Find		= efi_null_vlan_find,
	.Remove		= efi_null_vlan_remove,
};

/**
 * Nullify VLAN configuration interface
 *
 * @v vcfg		VLAN configuration protocol
 */
void efi_nullify_vlan ( EFI_VLAN_CONFIG_PROTOCOL *vcfg ) {

	memcpy ( vcfg, &efi_null_vlan, sizeof ( *vcfg ) );
}

/******************************************************************************
 *
 * Component name protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_get_driver_name ( EFI_COMPONENT_NAME2_PROTOCOL *name2 __unused,
			   CHAR8 *language __unused,
			   CHAR16 **driver_name __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_get_controller_name ( EFI_COMPONENT_NAME2_PROTOCOL *name2 __unused,
			       EFI_HANDLE device __unused,
			       EFI_HANDLE child __unused,
			       CHAR8 *language __unused,
			       CHAR16 **controller_name __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_COMPONENT_NAME2_PROTOCOL efi_null_name2 = {
	.GetDriverName = efi_null_get_driver_name,
	.GetControllerName = efi_null_get_controller_name,
	.SupportedLanguages = "",
};

/**
 * Nullify Component Name Protocol interface
 *
 * @v name2		Component name protocol
 */
void efi_nullify_name2 ( EFI_COMPONENT_NAME2_PROTOCOL *name2 ) {

	memcpy ( name2, &efi_null_name2, sizeof ( *name2 ) );
}

/******************************************************************************
 *
 * Load file protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_load_file ( EFI_LOAD_FILE_PROTOCOL *load_file __unused,
		     EFI_DEVICE_PATH_PROTOCOL *path __unused,
		     BOOLEAN booting __unused, UINTN *len __unused,
		     VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

/**
 * Nullify Load File Protocol interface
 *
 * @v load_file		Load file protocol
 */
void efi_nullify_load_file ( EFI_LOAD_FILE_PROTOCOL *load_file ) {
	load_file->LoadFile = efi_null_load_file;
}

/******************************************************************************
 *
 * HII configuration access protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_hii_extract ( const EFI_HII_CONFIG_ACCESS_PROTOCOL *hii __unused,
		       EFI_STRING request __unused,
		       EFI_STRING *progress __unused,
		       EFI_STRING *results __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_hii_route ( const EFI_HII_CONFIG_ACCESS_PROTOCOL *hii __unused,
		     EFI_STRING config __unused,
		     EFI_STRING *progress __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_hii_callback ( const EFI_HII_CONFIG_ACCESS_PROTOCOL *hii __unused,
			EFI_BROWSER_ACTION action __unused,
			EFI_QUESTION_ID question_id __unused,
			UINT8 type __unused, EFI_IFR_TYPE_VALUE *value __unused,
			EFI_BROWSER_ACTION_REQUEST *action_request __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_HII_CONFIG_ACCESS_PROTOCOL efi_null_hii = {
	.ExtractConfig	= efi_null_hii_extract,
	.RouteConfig	= efi_null_hii_route,
	.Callback	= efi_null_hii_callback,
};

/**
 * Nullify HII configuration access protocol
 *
 * @v hii		HII configuration access protocol
 */
void efi_nullify_hii ( EFI_HII_CONFIG_ACCESS_PROTOCOL *hii ) {

	memcpy ( hii, &efi_null_hii, sizeof ( *hii ) );
}

/******************************************************************************
 *
 * Block I/O protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_block_reset ( EFI_BLOCK_IO_PROTOCOL *block __unused,
		       BOOLEAN verify __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_block_read ( EFI_BLOCK_IO_PROTOCOL *block __unused,
		      UINT32 media __unused, EFI_LBA lba __unused,
		      UINTN len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_block_write ( EFI_BLOCK_IO_PROTOCOL *block __unused,
		       UINT32 media __unused, EFI_LBA lba __unused,
		       UINTN len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_block_flush ( EFI_BLOCK_IO_PROTOCOL *block __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_BLOCK_IO_MEDIA efi_null_block_media;

static EFI_BLOCK_IO_PROTOCOL efi_null_block = {
	.Revision = EFI_BLOCK_IO_INTERFACE_REVISION,
	.Media = &efi_null_block_media,
	.Reset = efi_null_block_reset,
	.ReadBlocks = efi_null_block_read,
	.WriteBlocks = efi_null_block_write,
	.FlushBlocks = efi_null_block_flush,
};

/**
 * Nullify block I/O protocol
 *
 * @v block		Block I/O protocol
 */
void efi_nullify_block ( EFI_BLOCK_IO_PROTOCOL *block ) {

	memcpy ( block, &efi_null_block, sizeof ( *block ) );
}

/******************************************************************************
 *
 * PXE base code protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_pxe_start ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
		     BOOLEAN use_ipv6 __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_stop ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_dhcp ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
		    BOOLEAN sort __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_discover ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			UINT16 type __unused, UINT16 *layer __unused,
			BOOLEAN bis __unused,
			EFI_PXE_BASE_CODE_DISCOVER_INFO *info __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_mtftp ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
		     EFI_PXE_BASE_CODE_TFTP_OPCODE opcode __unused,
		     VOID *data __unused, BOOLEAN overwrite __unused,
		     UINT64 *len __unused, UINTN *blksize __unused,
		     EFI_IP_ADDRESS *ip __unused, UINT8 *filename __unused,
		     EFI_PXE_BASE_CODE_MTFTP_INFO *info __unused,
		     BOOLEAN callback __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_udp_write ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			 UINT16 flags __unused,
			 EFI_IP_ADDRESS *dest_ip __unused,
			 EFI_PXE_BASE_CODE_UDP_PORT *dest_port __unused,
			 EFI_IP_ADDRESS *gateway __unused,
			 EFI_IP_ADDRESS *src_ip __unused,
			 EFI_PXE_BASE_CODE_UDP_PORT *src_port __unused,
			 UINTN *hdr_len __unused, VOID *hdr __unused,
			 UINTN *len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_udp_read ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			UINT16 flags __unused,
			EFI_IP_ADDRESS *dest_ip __unused,
			EFI_PXE_BASE_CODE_UDP_PORT *dest_port __unused,
			EFI_IP_ADDRESS *src_ip __unused,
			EFI_PXE_BASE_CODE_UDP_PORT *src_port __unused,
			UINTN *hdr_len __unused, VOID *hdr __unused,
			UINTN *len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_set_ip_filter ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			     EFI_PXE_BASE_CODE_IP_FILTER *filter __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_arp ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
		   EFI_IP_ADDRESS *ip __unused,
		   EFI_MAC_ADDRESS *mac __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_set_parameters ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			      BOOLEAN *autoarp __unused,
			      BOOLEAN *sendguid __unused, UINT8 *ttl __unused,
			      UINT8 *tos __unused,
			      BOOLEAN *callback __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_set_station_ip ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			      EFI_IP_ADDRESS *ip __unused,
			      EFI_IP_ADDRESS *netmask __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_pxe_set_packets ( EFI_PXE_BASE_CODE_PROTOCOL *pxe __unused,
			   BOOLEAN *dhcpdisc_ok __unused,
			   BOOLEAN *dhcpack_ok __unused,
			   BOOLEAN *proxyoffer_ok __unused,
			   BOOLEAN *pxebsdisc_ok __unused,
			   BOOLEAN *pxebsack_ok __unused,
			   BOOLEAN *pxebsbis_ok __unused,
			   EFI_PXE_BASE_CODE_PACKET *dhcpdisc __unused,
			   EFI_PXE_BASE_CODE_PACKET *dhcpack __unused,
			   EFI_PXE_BASE_CODE_PACKET *proxyoffer __unused,
			   EFI_PXE_BASE_CODE_PACKET *pxebsdisc __unused,
			   EFI_PXE_BASE_CODE_PACKET *pxebsack __unused,
			   EFI_PXE_BASE_CODE_PACKET *pxebsbis __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_PXE_BASE_CODE_PROTOCOL efi_null_pxe = {
	.Revision	= EFI_PXE_BASE_CODE_PROTOCOL_REVISION,
	.Start		= efi_null_pxe_start,
	.Stop		= efi_null_pxe_stop,
	.Dhcp		= efi_null_pxe_dhcp,
	.Discover	= efi_null_pxe_discover,
	.Mtftp		= efi_null_pxe_mtftp,
	.UdpWrite	= efi_null_pxe_udp_write,
	.UdpRead	= efi_null_pxe_udp_read,
	.SetIpFilter	= efi_null_pxe_set_ip_filter,
	.Arp		= efi_null_pxe_arp,
	.SetParameters	= efi_null_pxe_set_parameters,
	.SetStationIp	= efi_null_pxe_set_station_ip,
	.SetPackets	= efi_null_pxe_set_packets,
};

/**
 * Nullify PXE base code protocol
 *
 * @v pxe		PXE base code protocol
 */
void efi_nullify_pxe ( EFI_PXE_BASE_CODE_PROTOCOL *pxe ) {

	memcpy ( pxe, &efi_null_pxe, offsetof ( typeof ( *pxe ), Mode ) );
	pxe->Mode->Started = FALSE;
}

/******************************************************************************
 *
 * Apple Net Boot protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_apple_dhcp ( EFI_APPLE_NET_BOOT_PROTOCOL *apple __unused,
		      UINTN *len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_apple_bsdp ( EFI_APPLE_NET_BOOT_PROTOCOL *apple __unused,
		      UINTN *len __unused, VOID *data __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_APPLE_NET_BOOT_PROTOCOL efi_null_apple = {
	.GetDhcpResponse = efi_null_apple_dhcp,
	.GetBsdpResponse = efi_null_apple_bsdp,
};

/**
 * Nullify Apple Net Boot protocol
 *
 * @v apple		Apple Net Boot protocol
 */
void efi_nullify_apple ( EFI_APPLE_NET_BOOT_PROTOCOL *apple ) {

	memcpy ( apple, &efi_null_apple, sizeof ( *apple ) );
}

/******************************************************************************
 *
 * USB I/O Protocol
 *
 ******************************************************************************
 */

static EFI_STATUS EFIAPI
efi_null_usb_control_transfer ( EFI_USB_IO_PROTOCOL *usbio __unused,
				EFI_USB_DEVICE_REQUEST *packet __unused,
				EFI_USB_DATA_DIRECTION direction __unused,
				UINT32 timeout __unused, VOID *data __unused,
				UINTN len __unused, UINT32 *status __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_bulk_transfer ( EFI_USB_IO_PROTOCOL *usbio __unused,
			     UINT8 endpoint __unused, VOID *data __unused,
			     UINTN *len __unused, UINTN timeout __unused,
			     UINT32 *status __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_sync_interrupt_transfer ( EFI_USB_IO_PROTOCOL *usbio __unused,
				       UINT8 endpoint __unused,
				       VOID *data __unused,
				       UINTN *len __unused,
				       UINTN timeout __unused,
				       UINT32 *status __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_async_interrupt_transfer ( EFI_USB_IO_PROTOCOL *usbio __unused,
					UINT8 endpoint __unused,
					BOOLEAN start __unused,
					UINTN interval __unused,
					UINTN len __unused,
					EFI_ASYNC_USB_TRANSFER_CALLBACK
					callback __unused,
					VOID *context __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_isochronous_transfer ( EFI_USB_IO_PROTOCOL *usbio __unused,
				    UINT8 endpoint __unused,
				    VOID *data __unused, UINTN len __unused,
				    UINT32 *status __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_async_isochronous_transfer ( EFI_USB_IO_PROTOCOL *usbio __unused,
					  UINT8 endpoint __unused,
					  VOID *data __unused,
					  UINTN len __unused,
					  EFI_ASYNC_USB_TRANSFER_CALLBACK
					  callback __unused,
					  VOID *context __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_get_device_descriptor ( EFI_USB_IO_PROTOCOL *usbio __unused,
				     EFI_USB_DEVICE_DESCRIPTOR
				     *efidesc __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_get_config_descriptor ( EFI_USB_IO_PROTOCOL *usbio __unused,
				     EFI_USB_CONFIG_DESCRIPTOR
				     *efidesc __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_get_interface_descriptor ( EFI_USB_IO_PROTOCOL *usbio __unused,
					EFI_USB_INTERFACE_DESCRIPTOR
					*efidesc __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_get_endpoint_descriptor ( EFI_USB_IO_PROTOCOL *usbio __unused,
				       UINT8 index __unused,
				       EFI_USB_ENDPOINT_DESCRIPTOR
				       *efidesc __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_get_string_descriptor ( EFI_USB_IO_PROTOCOL *usbio __unused,
				     UINT16 language __unused,
				     UINT8 index __unused,
				     CHAR16 **string __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_get_supported_languages ( EFI_USB_IO_PROTOCOL *usbio __unused,
				       UINT16 **languages __unused,
				       UINT16 *len __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
efi_null_usb_port_reset ( EFI_USB_IO_PROTOCOL *usbio __unused ) {
	return EFI_UNSUPPORTED;
}

static EFI_USB_IO_PROTOCOL efi_null_usbio = {
	.UsbControlTransfer	     = efi_null_usb_control_transfer,
	.UsbBulkTransfer	     = efi_null_usb_bulk_transfer,
	.UsbAsyncInterruptTransfer   = efi_null_usb_async_interrupt_transfer,
	.UsbSyncInterruptTransfer    = efi_null_usb_sync_interrupt_transfer,
	.UsbIsochronousTransfer	     = efi_null_usb_isochronous_transfer,
	.UsbAsyncIsochronousTransfer = efi_null_usb_async_isochronous_transfer,
	.UsbGetDeviceDescriptor	     = efi_null_usb_get_device_descriptor,
	.UsbGetConfigDescriptor	     = efi_null_usb_get_config_descriptor,
	.UsbGetInterfaceDescriptor   = efi_null_usb_get_interface_descriptor,
	.UsbGetEndpointDescriptor    = efi_null_usb_get_endpoint_descriptor,
	.UsbGetStringDescriptor	     = efi_null_usb_get_string_descriptor,
	.UsbGetSupportedLanguages    = efi_null_usb_get_supported_languages,
	.UsbPortReset		     = efi_null_usb_port_reset,
};

/**
 * Nullify USB I/O protocol
 *
 * @v usbio		USB I/O protocol
 */
void efi_nullify_usbio ( EFI_USB_IO_PROTOCOL *usbio ) {

	memcpy ( usbio, &efi_null_usbio, sizeof ( *usbio ) );
}
