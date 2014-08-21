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

#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/BusSpecificDriverOverride.h>
#include <ipxe/efi/Protocol/ComponentName.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/efi/Protocol/DevicePathToText.h>
#include <ipxe/efi/Protocol/DiskIo.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/GraphicsOutput.h>
#include <ipxe/efi/Protocol/LoadFile.h>
#include <ipxe/efi/Protocol/LoadFile2.h>
#include <ipxe/efi/Protocol/LoadedImage.h>
#include <ipxe/efi/Protocol/PciIo.h>
#include <ipxe/efi/Protocol/PciRootBridgeIo.h>
#include <ipxe/efi/Protocol/PxeBaseCode.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/Protocol/TcgService.h>

/** @file
 *
 * EFI GUIDs
 *
 */

/** Block I/O protocol GUID */
EFI_GUID efi_block_io_protocol_guid
	= EFI_BLOCK_IO_PROTOCOL_GUID;

/** Bus specific driver override protocol GUID */
EFI_GUID efi_bus_specific_driver_override_protocol_guid
	= EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID;

/** Component name protocol GUID */
EFI_GUID efi_component_name_protocol_guid
	= EFI_COMPONENT_NAME_PROTOCOL_GUID;

/** Component name 2 protocol GUID */
EFI_GUID efi_component_name2_protocol_guid
	= EFI_COMPONENT_NAME2_PROTOCOL_GUID;

/** Device path protocol GUID */
EFI_GUID efi_device_path_protocol_guid
	= EFI_DEVICE_PATH_PROTOCOL_GUID;

/** Disk I/O protocol GUID */
EFI_GUID efi_disk_io_protocol_guid
	= EFI_DISK_IO_PROTOCOL_GUID;

/** Driver binding protocol GUID */
EFI_GUID efi_driver_binding_protocol_guid
	= EFI_DRIVER_BINDING_PROTOCOL_GUID;

/** Graphics output protocol GUID */
EFI_GUID efi_graphics_output_protocol_guid
	= EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

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

/** PCI I/O protocol GUID */
EFI_GUID efi_pci_io_protocol_guid
	= EFI_PCI_IO_PROTOCOL_GUID;

/** PCI root bridge I/O protocol GUID */
EFI_GUID efi_pci_root_bridge_io_protocol_guid
	= EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;

/** PXE base code protocol GUID */
EFI_GUID efi_pxe_base_code_protocol_guid
	= EFI_PXE_BASE_CODE_PROTOCOL_GUID;

/** Simple file system protocol GUID */
EFI_GUID efi_simple_file_system_protocol_guid
	= EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

/** Simple network protocol GUID */
EFI_GUID efi_simple_network_protocol_guid
	= EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** TCG protocol GUID */
EFI_GUID efi_tcg_protocol_guid
	= EFI_TCG_PROTOCOL_GUID;
