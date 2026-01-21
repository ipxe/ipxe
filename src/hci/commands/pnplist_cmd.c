/*
 * Copyright (C) 2025 iPXE Project.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <ipxe/netdevice.h>
#include <ipxe/pci.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/malloc.h>
#include <ipxe/settings.h>
#ifdef PLATFORM_efi
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_pci.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/Protocol/PciIo.h>
#endif

/** @file
 *
 * PNP device path listing command
 *
 */

/** "pnplist" options */
struct pnplist_options {
	/** Variable name to store output */
	char *store;
};

/** "pnplist" option list */
static struct option_descriptor pnplist_opts[] = {
	OPTION_DESC ( "store", 's', required_argument,
		      struct pnplist_options, store, parse_string ),
};

/** "pnplist" command descriptor */
static struct command_descriptor pnplist_cmd =
	COMMAND_DESC ( struct pnplist_options, pnplist_opts, 0, 0,
		       "[--store <variable>]" );

/**
 * Detect if device info appears to be abstracted by SNP or virtualization
 *
 * @v vendor		Vendor ID to check
 * @v device		Device ID to check  
 * @ret abstracted	True if this appears to be abstracted device info
 */
static int is_snp_abstracted ( uint16_t vendor, uint16_t device ) {
	/* SNP commonly uses these abstracted IDs */
	return ( vendor == 0x0102 && device == 0x000c );
}

/**
 * Try to get real PCI device information using EFI PCI I/O protocol
 *
 * This bypasses SNP abstraction by accessing the EFI handle associated with
 * the SNP device and using the EFI PCI I/O protocol to read real vendor/device IDs.
 *
 * @v netdev		Network device
 * @ret pci		PCI device, or NULL if not found
 * @ret need_free	Set to 1 if returned PCI device should be freed
 */
static struct pci_device * try_efi_pci_access ( struct net_device *netdev, int *need_free ) {
#ifdef PLATFORM_efi
	struct efi_snp_device *snpdev;
	struct efi_pci_device *efipci;
	struct pci_device *pci;
	EFI_HANDLE device_handle;
	int rc;

	/* Find SNP device for this network device */
	printf ( "DEBUG: netdev=%p name='%s' state=0x%x link_rc=%d\n", netdev, netdev->name, netdev->state, netdev->link_rc );
	snpdev = find_snpdev_by_netdev ( netdev );
	printf ( "DEBUG: snpdev=%p handle=%p parent=%p started=%d\n", snpdev, snpdev ? snpdev->handle : NULL, snpdev ? snpdev->parent : NULL, snpdev ? snpdev->started : 0 );
	if ( snpdev ) {
		
		/* Get the EFI device handle - this should be the PCI device */
		device_handle = snpdev->handle;
		
		/* Try to open PCI I/O protocol on the EFI device handle */
		efipci = malloc ( sizeof ( *efipci ) );
		if ( ! efipci ) {
			return NULL;
		}

		rc = efipci_info ( device_handle, efipci );
		printf ( "DEBUG: efipci_info returned rc=%d\n", rc );
		if ( rc == 0 ) {
			printf ( "DEBUG: Got real PCI device via EFI PCI I/O protocol - vendor=0x%04x device=0x%04x\n", efipci->pci.vendor, efipci->pci.device );
			
			/* Allocate a regular PCI device structure and copy the information */
			pci = malloc ( sizeof ( *pci ) );
			if ( pci ) {
				memcpy ( pci, &efipci->pci, sizeof ( *pci ) );
				*need_free = 1;
				free ( efipci );
				return pci;
			}
		} else {
			printf ( "DEBUG: Failed to get PCI device via EFI PCI I/O protocol\n" );
			free ( efipci );
		}
	} else {
		printf ( "DEBUG: SNP device not found for netdev\n" );
		*need_free = 0;
	}
#else
	/* Suppress unused parameter warning on non-EFI platforms */
	( void ) netdev;
#endif
	*need_free = 0;
	return NULL;
}

/**
 * Get the actual PCI device information for a network device
 *
 * In SNP builds, network devices often present abstracted vendor/device IDs 
 * instead of the real hardware. This function detects abstraction and uses
 * EFI PCI I/O protocol to find the actual underlying network controller.
 *
 * @v netdev		Network device
 * @v device_index	Index of this device in the network device list
 * @ret pci		PCI device, or NULL if not found
 * @ret need_free	Set to 1 if returned PCI device should be freed
 */
static struct pci_device * get_real_pci_device ( struct net_device *netdev, int *need_free ) {
	struct device *dev = netdev->dev;
	struct pci_device *pci = NULL;
	
	*need_free = 0;
	
	/* First, try EFI PCI I/O protocol access to bypass SNP abstraction */
	printf ( "DEBUG: Attempting EFI PCI I/O access for netdev '%s'\n", netdev->name );
	pci = try_efi_pci_access ( netdev, need_free );
	if ( pci ) {
		printf ( "DEBUG: Got real PCI device via EFI - vendor=0x%04x device=0x%04x bus=%d slot=%d func=%d\n", pci->vendor, pci->device, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ), PCI_FUNC ( pci->busdevfn ) );
		return pci;
	}
	
	/* Fall back to direct PCI device access */
	printf ( "DEBUG: Falling back to direct PCI device access\n" );
	if ( dev->desc.bus_type == BUS_TYPE_PCI ) {
		pci = container_of ( dev, struct pci_device, dev );
		printf ( "DEBUG: Direct PCI device found - vendor=0x%04x device=0x%04x bus=%d slot=%d func=%d\n", pci->vendor, pci->device, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ), PCI_FUNC ( pci->busdevfn ) );
		
		/* Check if we have abstracted device information */
		if ( is_snp_abstracted ( pci->vendor, pci->device ) ) {
			printf ( "DEBUG: WARNING - Device appears to have SNP abstracted IDs (0x%04x:0x%04x)\n", pci->vendor, pci->device );
		}
		
		return pci;
	} else {
		printf ( "DEBUG: Device bus_type is not PCI (bus_type=%d)\n", dev->desc.bus_type );
	}

	return NULL;
}

/**
 * Display Windows-style PNP device path for a network device
 *
 * @v netdev		Network device
 * @v buffer		Output buffer (or NULL to print to console)
 * @v len		Buffer length
 * @v used		Number of bytes used in buffer
 * @v store		Output in storage format (URL-encoded query string)
 * @ret rc		Return status code
 */
static int pnplist_show_device ( struct net_device *netdev, char *buffer, size_t len, size_t *used, int store ) {
	struct pci_device *pci;
	uint16_t subsys_vendor = 0x0000, subsys_device = 0x0000;
	uint8_t revision = 0x00;
	int rc;
	int need_free = 0;
	size_t pos = 0;
	int n;

	printf ( "\nDEBUG: ==== Processing device '%s' ====\n", netdev->name );

	/* Get the real PCI device information */
	pci = get_real_pci_device ( netdev, &need_free );
	if ( ! pci ) {
		printf ( "DEBUG: Could not identify PCI device for '%s', skipping\n" , netdev->name );
		return 0; /* Skip devices we can't identify */
	}
	printf ( "DEBUG: Successfully obtained PCI device (need_free=%d)\n", need_free );

	/* If vendor/device IDs are zero, try reading directly from config space */
	if ( pci->vendor == 0x0000 || pci->device == 0x0000 ) {
		uint16_t vendor_from_cfg = 0x0000, device_from_cfg = 0x0000;
		
		printf ( "DEBUG: Vendor/Device IDs are zero, attempting to read from PCI config space\n" );
		
		/* Read vendor ID from config space */
		rc = pci_read_config_word ( pci, PCI_VENDOR_ID, &vendor_from_cfg );
		printf ( "DEBUG: Read vendor ID from config: rc=%d value=0x%04x\n", rc, vendor_from_cfg );
		
		/* Read device ID from config space */
		rc = pci_read_config_word ( pci, PCI_DEVICE_ID, &device_from_cfg );
		printf ( "DEBUG: Read device ID from config: rc=%d value=0x%04x\n", rc, device_from_cfg );
		
		/* If we successfully read valid IDs, update the pci structure */
		if ( vendor_from_cfg != 0x0000 && device_from_cfg != 0x0000 &&
		     vendor_from_cfg != 0xFFFF && device_from_cfg != 0xFFFF ) {
			printf ( "DEBUG: Successfully recovered vendor=0x%04x device=0x%04x from config space\n",
				 vendor_from_cfg, device_from_cfg );
			pci->vendor = vendor_from_cfg;
			pci->device = device_from_cfg;
		} else {
			printf ( "DEBUG: WARNING - Could not recover valid vendor/device IDs from config space\n" );
		}
	}

	/* Try to read subsystem vendor ID */
	rc = pci_read_config_word ( pci, PCI_SUBSYSTEM_VENDOR_ID, &subsys_vendor );
	printf ( "DEBUG: Read subsystem vendor ID: rc=%d value=0x%04x\n", rc, subsys_vendor );
	if ( rc != 0 ) {
		subsys_vendor = 0x0000;
	}

	/* Try to read subsystem device ID */
	rc = pci_read_config_word ( pci, PCI_SUBSYSTEM_ID, &subsys_device );
	printf ( "DEBUG: Read subsystem device ID: rc=%d value=0x%04x\n", rc, subsys_device );
	if ( rc != 0 ) {
		subsys_device = 0x0000;
	}

	/* Try to read revision */
	rc = pci_read_config_byte ( pci, PCI_REVISION, &revision );
	printf ( "DEBUG: Read revision: rc=%d value=0x%02x\n", rc, revision );
	if ( rc != 0 ) {
		revision = 0x00;
	}

	/* Display PNP device path in Windows format */
	printf ( "DEBUG: Final values - vendor=0x%04x device=0x%04x subsys_vendor=0x%04x subsys_device=0x%04x rev=0x%02x\n", pci->vendor, pci->device, subsys_vendor, subsys_device, revision );
	if ( ( subsys_vendor == 0x0000 ) || ( subsys_device == 0x0000 ) ||
	     ( subsys_vendor == 0xFFFF ) || ( subsys_device == 0xFFFF ) ) {
		printf ( "DEBUG: Using fallback subsystem IDs (invalid subsystem)\n" );
		/* No valid subsystem ID - use vendor and device ID as fallback */
		if ( buffer ) {
			if ( store ) {
				/* URL-encoded query string format */
				n = snprintf ( buffer + pos, ( pos < len ) ? ( len - pos ) : 0,
					       "%s_ven=0x%04x&%s_dev=0x%04x&%s_subsys_ven=0x%04x&%s_subsys_dev=0x%04x&%s_rev=0x%02x&%s_bus_loc=%02x:%02x.%x",
					       netdev->name, pci->vendor,
					       netdev->name, pci->device,
					       netdev->name, pci->vendor, /* Fallback */
					       netdev->name, pci->device, /* Fallback */
					       netdev->name, revision,
					       netdev->name, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ), PCI_FUNC ( pci->busdevfn ) );
			} else {
				n = snprintf ( buffer + pos, ( pos < len ) ? ( len - pos ) : 0,
					       "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X\\%X&%X&%X&%X\n",
					       pci->vendor, pci->device, pci->device, pci->vendor,
					       revision, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ),
					       PCI_FUNC ( pci->busdevfn ), pci->busdevfn );
			}
			if ( n > 0 )
				pos += n;
		} else {
			printf ( "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X\\%X&%X&%X&%X\n",
				 pci->vendor, pci->device, pci->device, pci->vendor,
				 revision, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ),
				 PCI_FUNC ( pci->busdevfn ), pci->busdevfn );
		}
	} else {
		printf ( "DEBUG: Using actual subsystem IDs\n" );
		/* Use actual subsystem IDs */
		if ( buffer ) {
			if ( store ) {
				/* URL-encoded query string format */
				n = snprintf ( buffer + pos, ( pos < len ) ? ( len - pos ) : 0,
					       "%s_ven=0x%04x&%s_dev=0x%04x&%s_subsys_ven=0x%04x&%s_subsys_dev=0x%04x&%s_rev=0x%02x&%s_bus_loc=%02x:%02x.%x",
					       netdev->name, pci->vendor,
					       netdev->name, pci->device,
					       netdev->name, subsys_vendor,
					       netdev->name, subsys_device,
					       netdev->name, revision,
					       netdev->name, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ), PCI_FUNC ( pci->busdevfn ) );
			} else {
				n = snprintf ( buffer + pos, ( pos < len ) ? ( len - pos ) : 0,
					       "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X\\%X&%X&%X&%X\n",
					       pci->vendor, pci->device, subsys_device, subsys_vendor,
					       revision, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ),
					       PCI_FUNC ( pci->busdevfn ), pci->busdevfn );
			}
			if ( n > 0 )
				pos += n;
		} else {
			printf ( "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X\\%X&%X&%X&%X\n",
				 pci->vendor, pci->device, subsys_device, subsys_vendor,
				 revision, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ),
				 PCI_FUNC ( pci->busdevfn ), pci->busdevfn );
		}
	}

	/* Free allocated PCI device structure if needed */
	if ( need_free ) {
		free ( pci );
	}

	*used = pos;
	return 0;
}

/**
 * "pnplist" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int pnplist_exec ( int argc, char **argv ) {
	struct pnplist_options opts;
	struct net_device *netdev;
	char *output = NULL;
	size_t output_len = 0;
	size_t total_used = 0;
	size_t used;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &pnplist_cmd, &opts ) ) != 0 )
		return rc;

	/* If storing to a variable, allocate output buffer */
	if ( opts.store ) {
		/* Initial buffer size - will grow if needed */
		output_len = 1024;
		output = malloc ( output_len );
		if ( ! output )
			return -ENOMEM;
		output[0] = '\0';
		total_used = 0;
	}

	int first = 1;
	/* Iterate through all network devices */
	for_each_netdev ( netdev ) {
		if ( opts.store ) {
			/* Check if we need more buffer space */
			while ( total_used + 512 >= output_len ) {
				char *new_output;
				output_len *= 2;
				new_output = realloc ( output, output_len );
				if ( ! new_output ) {
					free ( output );
					return -ENOMEM;
				}
				output = new_output;
			}

			if ( ! first ) {
				output[total_used++] = '&';
				output[total_used] = '\0';
			}

			if ( ( rc = pnplist_show_device ( netdev, output + total_used,
							  output_len - total_used, &used, 1 ) ) != 0 ) {
				free ( output );
				return rc;
			}
			total_used += used;
			first = 0;
		} else {
			if ( ( rc = pnplist_show_device ( netdev, NULL, 0, &used, 0 ) ) != 0 )
				return rc;
		}
	}

	/* Store output to variable if requested */
	if ( opts.store ) {
		struct named_setting setting;
		printf ( "\nDEBUG: Storing %d bytes to variable '%s'\n", ( int ) total_used, opts.store );
		/* Parse setting name */
		if ( ( rc = parse_autovivified_setting ( opts.store, &setting ) ) != 0 ) {
			printf ( "Could not parse setting name \"%s\": %s\n",
				 opts.store, strerror ( rc ) );
			free ( output );
			return rc;
		}
		/* Apply default type if necessary */
		if ( ! setting.setting.type )
			setting.setting.type = &setting_type_string;
		/* Store setting */
		rc = storef_setting ( setting.settings, &setting.setting, output );
		if ( rc != 0 ) {
			printf ( "Could not store to variable \"%s\": %s\n",
				 opts.store, strerror ( rc ) );
		}
		free ( output );
		return rc;
	}

	return 0;
}

/** PNP list command */
COMMAND ( pnplist, pnplist_exec );