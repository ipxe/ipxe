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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>
#include <ipxe/uuid.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/AbsolutePointer.h>
#include <ipxe/efi/Protocol/AcpiTable.h>
#include <ipxe/efi/Protocol/AdapterInformation.h>
#include <ipxe/efi/Protocol/AppleNetBoot.h>
#include <ipxe/efi/Protocol/Arp.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/BlockIo2.h>
#include <ipxe/efi/Protocol/BusSpecificDriverOverride.h>
#include <ipxe/efi/Protocol/ComponentName.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/ConsoleControl/ConsoleControl.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/efi/Protocol/DevicePathToText.h>
#include <ipxe/efi/Protocol/Dhcp4.h>
#include <ipxe/efi/Protocol/Dhcp6.h>
#include <ipxe/efi/Protocol/DiskIo.h>
#include <ipxe/efi/Protocol/Dns4.h>
#include <ipxe/efi/Protocol/Dns6.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/EapConfiguration.h>
#include <ipxe/efi/Protocol/GraphicsOutput.h>
#include <ipxe/efi/Protocol/HiiConfigAccess.h>
#include <ipxe/efi/Protocol/HiiFont.h>
#include <ipxe/efi/Protocol/Http.h>
#include <ipxe/efi/Protocol/Ip4.h>
#include <ipxe/efi/Protocol/Ip4Config.h>
#include <ipxe/efi/Protocol/Ip4Config2.h>
#include <ipxe/efi/Protocol/Ip6.h>
#include <ipxe/efi/Protocol/Ip6Config.h>
#include <ipxe/efi/Protocol/LoadFile.h>
#include <ipxe/efi/Protocol/LoadFile2.h>
#include <ipxe/efi/Protocol/LoadedImage.h>
#include <ipxe/efi/Protocol/ManagedNetwork.h>
#include <ipxe/efi/Protocol/Mtftp4.h>
#include <ipxe/efi/Protocol/Mtftp6.h>
#include <ipxe/efi/Protocol/NetworkInterfaceIdentifier.h>
#include <ipxe/efi/Protocol/PciIo.h>
#include <ipxe/efi/Protocol/PciRootBridgeIo.h>
#include <ipxe/efi/Protocol/PxeBaseCode.h>
#include <ipxe/efi/Protocol/Rng.h>
#include <ipxe/efi/Protocol/SerialIo.h>
#include <ipxe/efi/Protocol/ShimLock.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/Protocol/SimplePointer.h>
#include <ipxe/efi/Protocol/SimpleTextIn.h>
#include <ipxe/efi/Protocol/SimpleTextInEx.h>
#include <ipxe/efi/Protocol/SimpleTextOut.h>
#include <ipxe/efi/Protocol/Supplicant.h>
#include <ipxe/efi/Protocol/TcgService.h>
#include <ipxe/efi/Protocol/Tcg2Protocol.h>
#include <ipxe/efi/Protocol/Tcp4.h>
#include <ipxe/efi/Protocol/Tcp6.h>
#include <ipxe/efi/Protocol/Udp4.h>
#include <ipxe/efi/Protocol/Udp6.h>
#include <ipxe/efi/Protocol/UgaDraw.h>
#include <ipxe/efi/Protocol/UnicodeCollation.h>
#include <ipxe/efi/Protocol/UsbHostController.h>
#include <ipxe/efi/Protocol/Usb2HostController.h>
#include <ipxe/efi/Protocol/UsbIo.h>
#include <ipxe/efi/Protocol/VlanConfig.h>
#include <ipxe/efi/Protocol/WiFi2.h>
#include <ipxe/efi/Guid/Acpi.h>
#include <ipxe/efi/Guid/Fdt.h>
#include <ipxe/efi/Guid/FileInfo.h>
#include <ipxe/efi/Guid/FileSystemInfo.h>
#include <ipxe/efi/Guid/GlobalVariable.h>
#include <ipxe/efi/Guid/ImageAuthentication.h>
#include <ipxe/efi/Guid/SmBios.h>
#include <ipxe/efi/Guid/TlsAuthentication.h>

/** @file
 *
 * EFI GUIDs
 *
 */

/* TrEE protocol GUID definition in EDK2 headers is broken (missing braces) */
#define EFI_TREE_PROTOCOL_GUID						\
	{ 0x607f766c, 0x7455, 0x42be,					\
	  { 0x93, 0x0b, 0xe4, 0xd7, 0x6d, 0xb2, 0x72, 0x0f } }

/** Absolute pointer protocol GUID */
EFI_GUID efi_absolute_pointer_protocol_guid
	= EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;

/** ACPI table protocol GUID */
EFI_GUID efi_acpi_table_protocol_guid
	= EFI_ACPI_TABLE_PROTOCOL_GUID;

/** Adapter information protocol GUID */
EFI_GUID efi_adapter_information_protocol_guid
	= EFI_ADAPTER_INFORMATION_PROTOCOL_GUID;

/** Apple NetBoot protocol GUID */
EFI_GUID efi_apple_net_boot_protocol_guid
	= EFI_APPLE_NET_BOOT_PROTOCOL_GUID;

/** ARP protocol GUID */
EFI_GUID efi_arp_protocol_guid
	= EFI_ARP_PROTOCOL_GUID;

/** ARP service binding protocol GUID */
EFI_GUID efi_arp_service_binding_protocol_guid
	= EFI_ARP_SERVICE_BINDING_PROTOCOL_GUID;

/** Block I/O protocol GUID */
EFI_GUID efi_block_io_protocol_guid
	= EFI_BLOCK_IO_PROTOCOL_GUID;

/** Block I/O version 2 protocol GUID */
EFI_GUID efi_block_io2_protocol_guid
	= EFI_BLOCK_IO2_PROTOCOL_GUID;

/** Bus specific driver override protocol GUID */
EFI_GUID efi_bus_specific_driver_override_protocol_guid
	= EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID;

/** Component name protocol GUID */
EFI_GUID efi_component_name_protocol_guid
	= EFI_COMPONENT_NAME_PROTOCOL_GUID;

/** Component name 2 protocol GUID */
EFI_GUID efi_component_name2_protocol_guid
	= EFI_COMPONENT_NAME2_PROTOCOL_GUID;

/** Console control protocol GUID */
EFI_GUID efi_console_control_protocol_guid
	= EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

/** Device path protocol GUID */
EFI_GUID efi_device_path_protocol_guid
	= EFI_DEVICE_PATH_PROTOCOL_GUID;

/** DHCPv4 protocol GUID */
EFI_GUID efi_dhcp4_protocol_guid
	= EFI_DHCP4_PROTOCOL_GUID;

/** DHCPv4 service binding protocol GUID */
EFI_GUID efi_dhcp4_service_binding_protocol_guid
	= EFI_DHCP4_SERVICE_BINDING_PROTOCOL_GUID;

/** DHCPv6 protocol GUID */
EFI_GUID efi_dhcp6_protocol_guid
	= EFI_DHCP6_PROTOCOL_GUID;

/** DHCPv6 service binding protocol GUID */
EFI_GUID efi_dhcp6_service_binding_protocol_guid
	= EFI_DHCP6_SERVICE_BINDING_PROTOCOL_GUID;

/** Disk I/O protocol GUID */
EFI_GUID efi_disk_io_protocol_guid
	= EFI_DISK_IO_PROTOCOL_GUID;

/** DNSv4 protocol GUID */
EFI_GUID efi_dns4_protocol_guid
	= EFI_DNS4_PROTOCOL_GUID;

/** DNSv4 service binding protocol GUID */
EFI_GUID efi_dns4_service_binding_protocol_guid
	= EFI_DNS4_SERVICE_BINDING_PROTOCOL_GUID;

/** DNSv6 protocol GUID */
EFI_GUID efi_dns6_protocol_guid
	= EFI_DNS6_PROTOCOL_GUID;

/** DNSv6 service binding protocol GUID */
EFI_GUID efi_dns6_service_binding_protocol_guid
	= EFI_DNS6_SERVICE_BINDING_PROTOCOL_GUID;

/** Driver binding protocol GUID */
EFI_GUID efi_driver_binding_protocol_guid
	= EFI_DRIVER_BINDING_PROTOCOL_GUID;

/** EAP configuration protocol GUID */
EFI_GUID efi_eap_configuration_protocol_guid
	= EFI_EAP_CONFIGURATION_PROTOCOL_GUID;

/** Graphics output protocol GUID */
EFI_GUID efi_graphics_output_protocol_guid
	= EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

/** HII configuration access protocol GUID */
EFI_GUID efi_hii_config_access_protocol_guid
	= EFI_HII_CONFIG_ACCESS_PROTOCOL_GUID;

/** HII font protocol GUID */
EFI_GUID efi_hii_font_protocol_guid
	= EFI_HII_FONT_PROTOCOL_GUID;

/** HTTP protocol GUID */
EFI_GUID efi_http_protocol_guid
	= EFI_HTTP_PROTOCOL_GUID;

/** HTTP service binding protocol GUID */
EFI_GUID efi_http_service_binding_protocol_guid
	= EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID;

/** IPv4 protocol GUID */
EFI_GUID efi_ip4_protocol_guid
	= EFI_IP4_PROTOCOL_GUID;

/** IPv4 configuration protocol GUID */
EFI_GUID efi_ip4_config_protocol_guid
	= EFI_IP4_CONFIG_PROTOCOL_GUID;

/** IPv4 configuration 2 protocol GUID */
EFI_GUID efi_ip4_config2_protocol_guid
	= EFI_IP4_CONFIG2_PROTOCOL_GUID;

/** IPv4 service binding protocol GUID */
EFI_GUID efi_ip4_service_binding_protocol_guid
	= EFI_IP4_SERVICE_BINDING_PROTOCOL_GUID;

/** IPv6 protocol GUID */
EFI_GUID efi_ip6_protocol_guid
	= EFI_IP6_PROTOCOL_GUID;

/** IPv6 configuration protocol GUID */
EFI_GUID efi_ip6_config_protocol_guid
	= EFI_IP6_CONFIG_PROTOCOL_GUID;

/** IPv6 service binding protocol GUID */
EFI_GUID efi_ip6_service_binding_protocol_guid
	= EFI_IP6_SERVICE_BINDING_PROTOCOL_GUID;

/** Load file protocol GUID */
EFI_GUID efi_load_file_protocol_guid
	= EFI_LOAD_FILE_PROTOCOL_GUID;

/** Load file 2 protocol GUID */
EFI_GUID efi_load_file2_protocol_guid
	= EFI_LOAD_FILE2_PROTOCOL_GUID;

/** Loaded image protocol GUID */
EFI_GUID efi_loaded_image_protocol_guid
	= EFI_LOADED_IMAGE_PROTOCOL_GUID;

/** Loaded image device path protocol GUID */
EFI_GUID efi_loaded_image_device_path_protocol_guid
	= EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;

/** Managed network protocol GUID */
EFI_GUID efi_managed_network_protocol_guid
	= EFI_MANAGED_NETWORK_PROTOCOL_GUID;

/** Managed network service binding protocol GUID */
EFI_GUID efi_managed_network_service_binding_protocol_guid
	= EFI_MANAGED_NETWORK_SERVICE_BINDING_PROTOCOL_GUID;

/** MTFTPv4 protocol GUID */
EFI_GUID efi_mtftp4_protocol_guid
	= EFI_MTFTP4_PROTOCOL_GUID;

/** MTFTPv4 service binding protocol GUID */
EFI_GUID efi_mtftp4_service_binding_protocol_guid
	= EFI_MTFTP4_SERVICE_BINDING_PROTOCOL_GUID;

/** MTFTPv6 protocol GUID */
EFI_GUID efi_mtftp6_protocol_guid
	= EFI_MTFTP6_PROTOCOL_GUID;

/** MTFTPv6 service binding protocol GUID */
EFI_GUID efi_mtftp6_service_binding_protocol_guid
	= EFI_MTFTP6_SERVICE_BINDING_PROTOCOL_GUID;

/** Network interface identifier protocol GUID (old version) */
EFI_GUID efi_nii_protocol_guid
	= EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_GUID;

/** Network interface identifier protocol GUID (new version) */
EFI_GUID efi_nii31_protocol_guid
	= EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_GUID_31;

/** PCI I/O protocol GUID */
EFI_GUID efi_pci_io_protocol_guid
	= EFI_PCI_IO_PROTOCOL_GUID;

/** PCI root bridge I/O protocol GUID */
EFI_GUID efi_pci_root_bridge_io_protocol_guid
	= EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;

/** PXE base code protocol GUID */
EFI_GUID efi_pxe_base_code_protocol_guid
	= EFI_PXE_BASE_CODE_PROTOCOL_GUID;

/** Random number generator protocol GUID */
EFI_GUID efi_rng_protocol_guid
	= EFI_RNG_PROTOCOL_GUID;

/** Serial I/O protocol GUID */
EFI_GUID efi_serial_io_protocol_guid
	= EFI_SERIAL_IO_PROTOCOL_GUID;

/** Shim lock protocol GUID */
EFI_GUID efi_shim_lock_protocol_guid
	= EFI_SHIM_LOCK_PROTOCOL_GUID;

/** Simple file system protocol GUID */
EFI_GUID efi_simple_file_system_protocol_guid
	= EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

/** Simple network protocol GUID */
EFI_GUID efi_simple_network_protocol_guid
	= EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** Simple pointer protocol GUID */
EFI_GUID efi_simple_pointer_protocol_guid
	= EFI_SIMPLE_POINTER_PROTOCOL_GUID;

/** Simple text input protocol GUID */
EFI_GUID efi_simple_text_input_protocol_guid
	= EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;

/** Simple text input extension protocol GUID */
EFI_GUID efi_simple_text_input_ex_protocol_guid
	= EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;

/** Simple text output protocol GUID */
EFI_GUID efi_simple_text_output_protocol_guid
	= EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;

/** Supplicant protocol GUID */
EFI_GUID efi_supplicant_protocol_guid
	= EFI_SUPPLICANT_PROTOCOL_GUID;

/** TCG protocol GUID */
EFI_GUID efi_tcg_protocol_guid
	= EFI_TCG_PROTOCOL_GUID;

/** TCG2 protocol GUID */
EFI_GUID efi_tcg2_protocol_guid
	= EFI_TCG2_PROTOCOL_GUID;

/** TCPv4 protocol GUID */
EFI_GUID efi_tcp4_protocol_guid
	= EFI_TCP4_PROTOCOL_GUID;

/** TCPv4 service binding protocol GUID */
EFI_GUID efi_tcp4_service_binding_protocol_guid
	= EFI_TCP4_SERVICE_BINDING_PROTOCOL_GUID;

/** TCPv6 protocol GUID */
EFI_GUID efi_tcp6_protocol_guid
	= EFI_TCP6_PROTOCOL_GUID;

/** TCPv6 service binding protocol GUID */
EFI_GUID efi_tcp6_service_binding_protocol_guid
	= EFI_TCP6_SERVICE_BINDING_PROTOCOL_GUID;

/** TrEE protocol GUID */
EFI_GUID efi_tree_protocol_guid
	= EFI_TREE_PROTOCOL_GUID;

/** UDPv4 protocol GUID */
EFI_GUID efi_udp4_protocol_guid
	= EFI_UDP4_PROTOCOL_GUID;

/** UDPv4 service binding protocol GUID */
EFI_GUID efi_udp4_service_binding_protocol_guid
	= EFI_UDP4_SERVICE_BINDING_PROTOCOL_GUID;

/** UDPv6 protocol GUID */
EFI_GUID efi_udp6_protocol_guid
	= EFI_UDP6_PROTOCOL_GUID;

/** UDPv6 service binding protocol GUID */
EFI_GUID efi_udp6_service_binding_protocol_guid
	= EFI_UDP6_SERVICE_BINDING_PROTOCOL_GUID;

/** UGA draw protocol GUID */
EFI_GUID efi_uga_draw_protocol_guid
	= EFI_UGA_DRAW_PROTOCOL_GUID;

/** Unicode collation protocol GUID */
EFI_GUID efi_unicode_collation_protocol_guid
	= EFI_UNICODE_COLLATION_PROTOCOL_GUID;

/** USB host controller protocol GUID */
EFI_GUID efi_usb_hc_protocol_guid
	= EFI_USB_HC_PROTOCOL_GUID;

/** USB2 host controller protocol GUID */
EFI_GUID efi_usb2_hc_protocol_guid
	= EFI_USB2_HC_PROTOCOL_GUID;

/** USB I/O protocol GUID */
EFI_GUID efi_usb_io_protocol_guid
	= EFI_USB_IO_PROTOCOL_GUID;

/** VLAN configuration protocol GUID */
EFI_GUID efi_vlan_config_protocol_guid
	= EFI_VLAN_CONFIG_PROTOCOL_GUID;

/** WiFi 2 protocol GUID */
EFI_GUID efi_wifi2_protocol_guid
	= EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_GUID;

/** ACPI 1.0 table GUID */
EFI_GUID efi_acpi_10_table_guid
	= ACPI_10_TABLE_GUID;

/** ACPI 2.0 table GUID */
EFI_GUID efi_acpi_20_table_guid
	= EFI_ACPI_20_TABLE_GUID;

/** FDT table GUID */
EFI_GUID efi_fdt_table_guid
	= FDT_TABLE_GUID;

/** SMBIOS table GUID */
EFI_GUID efi_smbios_table_guid
	= SMBIOS_TABLE_GUID;

/** SMBIOS3 table GUID */
EFI_GUID efi_smbios3_table_guid
	= SMBIOS3_TABLE_GUID;

/** X.509 certificate GUID */
EFI_GUID efi_cert_x509_guid = EFI_CERT_X509_GUID;

/** File information GUID */
EFI_GUID efi_file_info_id = EFI_FILE_INFO_ID;

/** File system information GUID */
EFI_GUID efi_file_system_info_id = EFI_FILE_SYSTEM_INFO_ID;

/** Global variable GUID */
EFI_GUID efi_global_variable = EFI_GLOBAL_VARIABLE;

/** TLS CA certificate variable GUID */
EFI_GUID efi_tls_ca_certificate_guid = EFI_TLS_CA_CERTIFICATE_GUID;

/** HttpBootDxe module GUID */
static EFI_GUID efi_http_boot_dxe_guid = {
	0xecebcb00, 0xd9c8, 0x11e4,
	{ 0xaf, 0x3d, 0x8c, 0xdc, 0xd4, 0x26, 0xc9, 0x73 }
};

/** IScsiDxe module GUID */
static EFI_GUID efi_iscsi_dxe_guid = {
	0x86cddf93, 0x4872, 0x4597,
	{ 0x8a, 0xf9, 0xa3, 0x5a, 0xe4, 0xd3, 0x72, 0x5f }
};

/** Old IScsi4Dxe module GUID */
static EFI_GUID efi_iscsi4_dxe_guid = {
	0x4579b72d, 0x7ec4, 0x4dd4,
	{ 0x84, 0x86, 0x08, 0x3c, 0x86, 0xb1, 0x82, 0xa7 }
};

/** UefiPxeBcDxe module GUID */
static EFI_GUID efi_uefi_pxe_bc_dxe_guid = {
	0xb95e9fda, 0x26de, 0x48d2,
	{ 0x88, 0x07, 0x1f, 0x91, 0x07, 0xac, 0x5e, 0x3a }
};

/** VlanConfigDxe module GUID */
static EFI_GUID efi_vlan_config_dxe_guid = {
	0xe4f61863, 0xfe2c, 0x4b56,
	{ 0xa8, 0xf4, 0x08, 0x51, 0x9b, 0xc4, 0x39, 0xdf }
};

/** WiFiConnectionMgrDxe module GUID */
static EFI_GUID efi_wifi_connection_mgr_dxe_guid = {
	0x99b7c019, 0x4789, 0x4829,
	{ 0xa7, 0xbd, 0x0d, 0x4b, 0xaa, 0x62, 0x28, 0x72 }
};

/** A well-known GUID */
struct efi_well_known_guid {
	/** GUID */
	EFI_GUID *guid;
	/** Name */
	const char *name;
};

/** Well-known GUIDs */
static struct efi_well_known_guid efi_well_known_guids[] = {
	{ &efi_absolute_pointer_protocol_guid,
	  "AbsolutePointer" },
	{ &efi_acpi_10_table_guid,
	  "Acpi10" },
	{ &efi_acpi_20_table_guid,
	  "Acpi20" },
	{ &efi_acpi_table_protocol_guid,
	  "AcpiTable" },
	{ &efi_adapter_information_protocol_guid,
	  "AdapterInfo" },
	{ &efi_apple_net_boot_protocol_guid,
	  "AppleNetBoot" },
	{ &efi_arp_protocol_guid,
	  "Arp" },
	{ &efi_arp_service_binding_protocol_guid,
	  "ArpSb" },
	{ &efi_block_io_protocol_guid,
	  "BlockIo" },
	{ &efi_block_io2_protocol_guid,
	  "BlockIo2" },
	{ &efi_bus_specific_driver_override_protocol_guid,
	  "BusSpecificDriverOverride" },
	{ &efi_cert_x509_guid,
	  "CertX509" },
	{ &efi_component_name_protocol_guid,
	  "ComponentName" },
	{ &efi_component_name2_protocol_guid,
	  "ComponentName2" },
	{ &efi_console_control_protocol_guid,
	  "ConsoleControl" },
	{ &efi_device_path_protocol_guid,
	  "DevicePath" },
	{ &efi_driver_binding_protocol_guid,
	  "DriverBinding" },
	{ &efi_dhcp4_protocol_guid,
	  "Dhcp4" },
	{ &efi_dhcp4_service_binding_protocol_guid,
	  "Dhcp4Sb" },
	{ &efi_dhcp6_protocol_guid,
	  "Dhcp6" },
	{ &efi_dhcp6_service_binding_protocol_guid,
	  "Dhcp6Sb" },
	{ &efi_disk_io_protocol_guid,
	  "DiskIo" },
	{ &efi_dns4_protocol_guid,
	  "Dns4" },
	{ &efi_dns4_service_binding_protocol_guid,
	  "Dns4Sb" },
	{ &efi_dns6_protocol_guid,
	  "Dns6" },
	{ &efi_dns6_service_binding_protocol_guid,
	  "Dns6Sb" },
	{ &efi_eap_configuration_protocol_guid,
	  "EapConfig" },
	{ &efi_fdt_table_guid,
	  "Fdt" },
	{ &efi_global_variable,
	  "GlobalVar" },
	{ &efi_graphics_output_protocol_guid,
	  "GraphicsOutput" },
	{ &efi_hii_config_access_protocol_guid,
	  "HiiConfigAccess" },
	{ &efi_hii_font_protocol_guid,
	  "HiiFont" },
	{ &efi_http_boot_dxe_guid,
	  "HttpBootDxe" },
	{ &efi_http_protocol_guid,
	  "Http" },
	{ &efi_http_service_binding_protocol_guid,
	  "HttpSb" },
	{ &efi_ip4_protocol_guid,
	  "Ip4" },
	{ &efi_ip4_config_protocol_guid,
	  "Ip4Config" },
	{ &efi_ip4_config2_protocol_guid,
	  "Ip4Config2" },
	{ &efi_ip4_service_binding_protocol_guid,
	  "Ip4Sb" },
	{ &efi_ip6_protocol_guid,
	  "Ip6" },
	{ &efi_ip6_config_protocol_guid,
	  "Ip6Config" },
	{ &efi_ip6_service_binding_protocol_guid,
	  "Ip6Sb" },
	{ &efi_iscsi_dxe_guid,
	  "IScsiDxe" },
	{ &efi_iscsi4_dxe_guid,
	  "IScsi4Dxe" },
	{ &efi_load_file_protocol_guid,
	  "LoadFile" },
	{ &efi_load_file2_protocol_guid,
	  "LoadFile2" },
	{ &efi_loaded_image_protocol_guid,
	  "LoadedImage" },
	{ &efi_loaded_image_device_path_protocol_guid,
	  "LoadedImageDevicePath"},
	{ &efi_managed_network_protocol_guid,
	  "ManagedNetwork" },
	{ &efi_managed_network_service_binding_protocol_guid,
	  "ManagedNetworkSb" },
	{ &efi_mtftp4_protocol_guid,
	  "Mtftp4" },
	{ &efi_mtftp4_service_binding_protocol_guid,
	  "Mtftp4Sb" },
	{ &efi_mtftp6_protocol_guid,
	  "Mtftp6" },
	{ &efi_mtftp6_service_binding_protocol_guid,
	  "Mtftp6Sb" },
	{ &efi_nii_protocol_guid,
	  "Nii" },
	{ &efi_nii31_protocol_guid,
	  "Nii31" },
	{ &efi_pci_io_protocol_guid,
	  "PciIo" },
	{ &efi_pci_root_bridge_io_protocol_guid,
	  "PciRootBridgeIo" },
	{ &efi_pxe_base_code_protocol_guid,
	  "PxeBaseCode" },
	{ &efi_rng_protocol_guid,
	  "Rng" },
	{ &efi_serial_io_protocol_guid,
	  "SerialIo" },
	{ &efi_shim_lock_protocol_guid,
	  "ShimLock" },
	{ &efi_simple_file_system_protocol_guid,
	  "SimpleFileSystem" },
	{ &efi_simple_network_protocol_guid,
	  "SimpleNetwork" },
	{ &efi_simple_pointer_protocol_guid,
	  "SimplePointer" },
	{ &efi_simple_text_input_protocol_guid,
	  "SimpleTextInput" },
	{ &efi_simple_text_input_ex_protocol_guid,
	  "SimpleTextInputEx" },
	{ &efi_simple_text_output_protocol_guid,
	  "SimpleTextOutput" },
	{ &efi_smbios_table_guid,
	  "Smbios" },
	{ &efi_smbios3_table_guid,
	  "Smbios3" },
	{ &efi_supplicant_protocol_guid,
	  "Supplicant" },
	{ &efi_tcg_protocol_guid,
	  "Tcg" },
	{ &efi_tcg2_protocol_guid,
	  "Tcg2" },
	{ &efi_tcp4_protocol_guid,
	  "Tcp4" },
	{ &efi_tcp4_service_binding_protocol_guid,
	  "Tcp4Sb" },
	{ &efi_tcp6_protocol_guid,
	  "Tcp6" },
	{ &efi_tcp6_service_binding_protocol_guid,
	  "Tcp6Sb" },
	{ &efi_tls_ca_certificate_guid,
	  "TlsCaCert" },
	{ &efi_tree_protocol_guid,
	  "TrEE" },
	{ &efi_udp4_protocol_guid,
	  "Udp4" },
	{ &efi_udp4_service_binding_protocol_guid,
	  "Udp4Sb" },
	{ &efi_udp6_protocol_guid,
	  "Udp6" },
	{ &efi_udp6_service_binding_protocol_guid,
	  "Udp6Sb" },
	{ &efi_uefi_pxe_bc_dxe_guid,
	  "UefiPxeBcDxe" },
	{ &efi_uga_draw_protocol_guid,
	  "UgaDraw" },
	{ &efi_unicode_collation_protocol_guid,
	  "UnicodeCollation" },
	{ &efi_usb_hc_protocol_guid,
	  "UsbHc" },
	{ &efi_usb2_hc_protocol_guid,
	  "Usb2Hc" },
	{ &efi_usb_io_protocol_guid,
	  "UsbIo" },
	{ &efi_vlan_config_protocol_guid,
	  "VlanConfig" },
	{ &efi_vlan_config_dxe_guid,
	  "VlanConfigDxe" },
	{ &efi_wifi2_protocol_guid,
	  "Wifi2" },
	{ &efi_wifi_connection_mgr_dxe_guid,
	  "WiFiConnectionMgrDxe" },
};

/**
 * Convert GUID to a printable string
 *
 * @v guid		GUID
 * @ret string		Printable string
 */
const __attribute__ (( pure )) char * efi_guid_ntoa ( CONST EFI_GUID *guid ) {
	union {
		union uuid uuid;
		EFI_GUID guid;
	} u;
	unsigned int i;

	/* Sanity check */
	if ( ! guid )
		return NULL;

	/* Check for a match against well-known GUIDs */
	for ( i = 0 ; i < ( sizeof ( efi_well_known_guids ) /
			    sizeof ( efi_well_known_guids[0] ) ) ; i++ ) {
		if ( memcmp ( guid, efi_well_known_guids[i].guid,
			      sizeof ( *guid ) ) == 0 ) {
			return efi_well_known_guids[i].name;
		}
	}

	/* Convert GUID to standard endianness */
	memcpy ( &u.guid, guid, sizeof ( u.guid ) );
	uuid_mangle ( &u.uuid );
	return uuid_ntoa ( &u.uuid );
}
