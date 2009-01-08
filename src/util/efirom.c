/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <gpxe/efi/efi.h>
#include <gpxe/efi/IndustryStandard/PeImage.h>
#include <gpxe/efi/IndustryStandard/Pci22.h>

#define eprintf(...) fprintf ( stderr, __VA_ARGS__ )

/** Command-line options */
struct options {
	uint16_t vendor;
	uint16_t device;
};

/**
 * Allocate memory
 *
 * @v len		Length of memory to allocate
 * @ret ptr		Pointer to allocated memory
 */
static void * xmalloc ( size_t len ) {
	void *ptr;

	ptr = malloc ( len );
	if ( ! ptr ) {
		eprintf ( "Could not allocate %zd bytes\n", len );
		exit ( 1 );
	}

	return ptr;
}

/**
 * Get file size
 *
 * @v file		File
 * @v len		File size
 */
static size_t file_size ( FILE *file ) {
	ssize_t len;

	if ( fseek ( file, 0, SEEK_END ) != 0 ) {
		eprintf ( "Could not seek: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
	len = ftell ( file );
	if ( len < 0 ) {
		eprintf ( "Could not determine file size: %s\n",
			  strerror ( errno ) );
		exit ( 1 );
	}
	return len;
}

/**
 * Copy file
 *
 * @v in		Input file
 * @v out		Output file
 * @v len		Length to copy
 */
static void file_copy ( FILE *in, FILE *out, size_t len ) {
	char buf[4096];
	size_t frag_len;

	while ( len ) {
		frag_len = len;
		if ( frag_len > sizeof ( buf ) )
			frag_len = sizeof ( buf );
		if ( fread ( buf, frag_len, 1, in ) != 1 ) {
			eprintf ( "Could not read: %s\n",
				  strerror ( errno ) );
			exit ( 1 );
		}
		if ( fwrite ( buf, frag_len, 1, out ) != 1 ) {
			eprintf ( "Could not write: %s\n",
				  strerror ( errno ) );
			exit ( 1 );
		}
		len -= frag_len;
	}
}

/**
 * Read information from PE headers
 *
 * @v pe		PE file
 * @ret machine		Machine type
 * @ret subsystem	EFI subsystem
 */
static void read_pe_info ( FILE *pe, uint16_t *machine,
			   uint16_t *subsystem ) {
	EFI_IMAGE_DOS_HEADER dos;
	union {
		EFI_IMAGE_NT_HEADERS32 nt32;
		EFI_IMAGE_NT_HEADERS64 nt64;
	} nt;

	/* Read DOS header */
	if ( fseek ( pe, 0, SEEK_SET ) != 0 ) {
		eprintf ( "Could not seek: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
	if ( fread ( &dos, sizeof ( dos ), 1, pe ) != 1 ) {
		eprintf ( "Could not read: %s\n", strerror ( errno ) );
		exit ( 1 );
	}

	/* Read NT header */
	if ( fseek ( pe, dos.e_lfanew, SEEK_SET ) != 0 ) {
		eprintf ( "Could not seek: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
	if ( fread ( &nt, sizeof ( nt ), 1, pe ) != 1 ) {
		eprintf ( "Could not read: %s\n", strerror ( errno ) );
		exit ( 1 );
	}

	/* Locate NT header */
	*machine = nt.nt32.FileHeader.Machine;
	switch ( *machine ) {
	case EFI_IMAGE_MACHINE_IA32:
		*subsystem = nt.nt32.OptionalHeader.Subsystem;
		break;
	case EFI_IMAGE_MACHINE_X64:
		*subsystem = nt.nt64.OptionalHeader.Subsystem;
		break;
	default:
		eprintf ( "Unrecognised machine type %04x\n", *machine );
		exit ( 1 );
	}
}

/**
 * Convert EFI image to ROM image
 *
 * @v pe		EFI file
 * @v rom		ROM file
 */
static void make_efi_rom ( FILE *pe, FILE *rom, struct options *opts ) {
	struct {
		EFI_PCI_EXPANSION_ROM_HEADER rom;
		PCI_DATA_STRUCTURE pci __attribute__ (( aligned ( 4 ) ));
	} headers;
	size_t pe_size;
	size_t rom_size;
	unsigned int rom_size_sectors;

	/* Determine output file size */
	pe_size = file_size ( pe );
	rom_size = ( pe_size + sizeof ( headers ) );
	rom_size_sectors = ( ( rom_size + 511 ) / 512 );

	/* Construct ROM header */
	memset ( &headers, 0, sizeof ( headers ) );
	headers.rom.Signature = PCI_EXPANSION_ROM_HEADER_SIGNATURE;
	headers.rom.InitializationSize = rom_size_sectors;
	headers.rom.EfiSignature = EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE;
	read_pe_info ( pe, &headers.rom.EfiMachineType,
		       &headers.rom.EfiSubsystem );
	headers.rom.EfiImageHeaderOffset = sizeof ( headers );
	headers.rom.PcirOffset =
		offsetof ( typeof ( headers ), pci );
	headers.pci.Signature = PCI_DATA_STRUCTURE_SIGNATURE;
	headers.pci.VendorId = opts->vendor;
	headers.pci.DeviceId = opts->device;
	headers.pci.Length = sizeof ( headers.pci );
	headers.pci.ClassCode[0] = PCI_CLASS_NETWORK;
	headers.pci.ImageLength = rom_size_sectors;
	headers.pci.CodeType = 0x03; /* No constant in EFI headers? */
	headers.pci.Indicator = 0x80; /* No constant in EFI headers? */

	/* Write out ROM header */
	if ( fwrite ( &headers, sizeof ( headers ), 1, rom ) != 1 ) {
		eprintf ( "Could not write headers: %s\n",
			  strerror ( errno ) );
		exit ( 1 );
	}

	/* Write out payload */
	if ( fseek ( pe, 0, SEEK_SET ) != 0 ) {
		eprintf ( "Could not seek: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
	file_copy ( pe, rom, pe_size );

	/* Round up to 512-byte boundary */
	if ( ftruncate ( fileno ( rom ), ( rom_size_sectors * 512 ) ) != 0 ) {
		eprintf ( "Could not set length: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
}

/**
 * Print help
 *
 * @v program_name	Program name
 */
static void print_help ( const char *program_name ) {
	eprintf ( "Syntax: %s [--vendor=VVVV] [--device=DDDD] "
		  "infile outfile\n", program_name );
}

/**
 * Parse command-line options
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v opts		Options structure to populate
 */
static int parse_options ( const int argc, char **argv,
			   struct options *opts ) {
	char *end;
	int c;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "vendor", required_argument, NULL, 'v' },
			{ "device", required_argument, NULL, 'd' },
			{ "help", 0, NULL, 'h' },
			{ 0, 0, 0, 0 }
		};

		if ( ( c = getopt_long ( argc, argv, "v:d:h",
					 long_options,
					 &option_index ) ) == -1 ) {
			break;
		}

		switch ( c ) {
		case 'v':
			opts->vendor = strtoul ( optarg, &end, 16 );
			if ( *end ) {
				eprintf ( "Invalid vendor \"%s\"\n", optarg );
				exit ( 2 );
			}
			break;
		case 'd':
			opts->device = strtoul ( optarg, &end, 16 );
			if ( *end ) {
				eprintf ( "Invalid device \"%s\"\n", optarg );
				exit ( 2 );
			}
			break;
		case 'h':
			print_help ( argv[0] );
			exit ( 0 );
		case '?':
		default:
			exit ( 2 );
		}
	}
	return optind;
}

int main ( int argc, char **argv ) {
	struct options opts = {
	};
	unsigned int infile_index;
	const char *infile_name;
	const char *outfile_name;
	FILE *infile;
	FILE *outfile;

	/* Parse command-line arguments */
	infile_index = parse_options ( argc, argv, &opts );
	if ( argc != ( infile_index + 2 ) ) {
		print_help ( argv[0] );
		exit ( 2 );
	}
	infile_name = argv[infile_index];
	outfile_name = argv[infile_index + 1];

	/* Open input and output files */
	infile = fopen ( infile_name, "r" );
	if ( ! infile ) {
		eprintf ( "Could not open %s for reading: %s\n",
			  infile_name, strerror ( errno ) );
		exit ( 1 );
	}
	outfile = fopen ( outfile_name, "w" );
	if ( ! outfile ) {
		eprintf ( "Could not open %s for writing: %s\n",
			  outfile_name, strerror ( errno ) );
		exit ( 1 );
	}

	/* Convert file */
	make_efi_rom ( infile, outfile, &opts );

	fclose ( outfile );
	fclose ( infile );

	return 0;
}
