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
 * Scan PCI bus for network controllers
 *
 * When the pci_device structure is invalid, scan the PCI bus to find
 * network controllers (class code 0x02xxxx) and return the one at the specified index.
 *
 * @v device_index	Index of the network device (0-based)
 * @ret pci		PCI device, or NULL if not found
 * @ret need_free	Set to 1 if returned PCI device should be freed
 */
static struct pci_device * scan_for_network_device ( int device_index, int *need_free ) {
	struct pci_device scan_pci;
	struct pci_device *pci;
	uint32_t busdevfn = 0;
	uint8_t base_class;
	int found_count = 0;
	int rc;
	
	printf ( "DEBUG: Scanning PCI bus for network controller #%d\n", device_index );
	
	/* Scan through all PCI devices */
	while ( ( rc = pci_find_next ( &scan_pci, &busdevfn ) ) == 0 ) {
		/* Get base class from the class field (base class is in bits 16-23) */
		base_class = ( scan_pci.class >> 16 ) & 0xff;
			
		/* Check if this is a network controller (class 0x02) */
		if ( base_class == PCI_CLASS_NETWORK ) {
			printf ( "DEBUG: Found network controller #%d - vendor=0x%04x device=0x%04x bus=%d slot=%d func=%d\n",
				 found_count, scan_pci.vendor, scan_pci.device,
				 PCI_BUS ( scan_pci.busdevfn ), PCI_SLOT ( scan_pci.busdevfn ), PCI_FUNC ( scan_pci.busdevfn ) );
			
			/* Check if this is the device we're looking for */
			if ( found_count == device_index ) {
				/* Allocate and copy the PCI device structure */
				pci = malloc ( sizeof ( *pci ) );
				if ( pci ) {
					memcpy ( pci, &scan_pci, sizeof ( *pci ) );
					*need_free = 1;
					printf ( "DEBUG: Selected network controller #%d for this netdev\n", device_index );
					return pci;
				}
			}
			
			found_count++;
		}
		
		busdevfn++;
	}
	
	printf ( "DEBUG: Network controller #%d not found (found %d total)\n", device_index, found_count );
	*need_free = 0;
	return NULL;
}

/**
 * Get the PCI device information for a network device
 *
 * @v netdev		Network device
 * @v device_index	Index of this network device in the list
 * @ret pci		PCI device, or NULL if not found
 * @ret need_free	Set to 1 if returned PCI device should be freed
 */
static struct pci_device * get_real_pci_device ( struct net_device *netdev, int device_index, int *need_free ) {
	struct device *dev = netdev->dev;
	struct pci_device *pci = NULL;
	
	*need_free = 0;
	
	/* Get direct PCI device access */
	printf ( "DEBUG: Getting PCI device for netdev '%s' (index %d)\n", netdev->name, device_index );
	if ( dev->desc.bus_type == BUS_TYPE_PCI ) {
		pci = container_of ( dev, struct pci_device, dev );
		printf ( "DEBUG: PCI device found - vendor=0x%04x device=0x%04x bus=%d slot=%d func=%d\n", pci->vendor, pci->device, PCI_BUS ( pci->busdevfn ), PCI_SLOT ( pci->busdevfn ), PCI_FUNC ( pci->busdevfn ) );
		
		/* Check if the PCI device structure has valid data */
		if ( pci->vendor == 0x0000 || pci->busdevfn == 0 ) {
			printf ( "DEBUG: PCI device structure appears invalid, scanning for network devices\n" );
			pci = scan_for_network_device ( device_index, need_free );
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
 * @v device_index	Index of this device in the network device list
 * @v buffer		Output buffer (or NULL to print to console)
 * @v len		Buffer length
 * @v used		Number of bytes used in buffer
 * @v store		Output in storage format (URL-encoded query string)
 * @ret rc		Return status code
 */
static int pnplist_show_device ( struct net_device *netdev, int device_index, char *buffer, size_t len, size_t *used, int store ) {
	struct pci_device *pci;
	uint16_t subsys_vendor = 0x0000, subsys_device = 0x0000;
	uint8_t revision = 0x00;
	int rc;
	int need_free = 0;
	size_t pos = 0;
	int n;

	printf ( "\nDEBUG: ==== Processing device '%s' ====\n", netdev->name );

	/* Get the real PCI device information */
	pci = get_real_pci_device ( netdev, device_index, &need_free );
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
	int device_index = 0;
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

			if ( ( rc = pnplist_show_device ( netdev, device_index, output + total_used,
							  output_len - total_used, &used, 1 ) ) != 0 ) {
				free ( output );
				return rc;
			}
			total_used += used;
			first = 0;
		} else {
			if ( ( rc = pnplist_show_device ( netdev, device_index, NULL, 0, &used, 0 ) ) != 0 )
				return rc;
		}
		device_index++;
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