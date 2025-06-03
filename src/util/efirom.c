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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define FILE_LICENCE(...) extern void __file_licence ( void )
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>

#define EFI_HOSTONLY
#include <ipxe/efi/Uefi.h>
#include <ipxe/efi/IndustryStandard/PeImage.h>
#include <ipxe/efi/IndustryStandard/Pci22.h>

#define eprintf(...) fprintf ( stderr, __VA_ARGS__ )

/* Round up ROM size */
#define ROM_SIZE( len ) ( ( (len) + 511 ) & ~511 )

/* Include the EDK2 compression code */
#include "eficompress.c"

/** Command-line options */
struct options {
	uint16_t vendor;
	uint16_t device;
	int compress;
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
 * Read information from PE headers
 *
 * @v pe		PE file
 * @ret machine		Machine type
 * @ret subsystem	EFI subsystem
 */
static void read_pe_info ( void *pe, uint16_t *machine,
			   uint16_t *subsystem ) {
	EFI_IMAGE_DOS_HEADER *dos;
	union {
		EFI_IMAGE_NT_HEADERS32 nt32;
		EFI_IMAGE_NT_HEADERS64 nt64;
	} *nt;

	/* Locate NT header */
	dos = pe;
	nt = ( pe + dos->e_lfanew );

	/* Parse out PE information */
	*machine = nt->nt32.FileHeader.Machine;
	switch ( *machine ) {
	case EFI_IMAGE_MACHINE_IA32:
	case EFI_IMAGE_MACHINE_ARMTHUMB_MIXED:
	case EFI_IMAGE_MACHINE_RISCV32:
		*subsystem = nt->nt32.OptionalHeader.Subsystem;
		break;
	case EFI_IMAGE_MACHINE_X64:
	case EFI_IMAGE_MACHINE_AARCH64:
	case EFI_IMAGE_MACHINE_LOONGARCH64:
	case EFI_IMAGE_MACHINE_RISCV64:
		*subsystem = nt->nt64.OptionalHeader.Subsystem;
		break;
	default:
		eprintf ( "Unrecognised machine type %04x\n", *machine );
		exit ( 1 );
	}
}

/**
 * Attempt to compress EFI data in-place
 *
 * @v data		Data to be compressed
 * @v max_len		Length of data
 * @ret len		Length after attempted compression
 */
static size_t efi_compress ( void *data, size_t max_len ) {
	void *tmp;
	UINT32 len;

	/* Allocate temporary buffer for compressed data */
	tmp = xmalloc ( max_len );

	/* Attempt compression */
	len = max_len;
	if ( ( EfiCompress ( data, max_len, tmp, &len ) == 0 ) &&
	     ( len < max_len ) ) {
		memcpy ( data, tmp, len );
	} else {
		len = max_len;
	}

	/* Free temporary buffer */
	free ( tmp );

	return len;
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
		uint8_t checksum;
	} *headers;
	struct stat pe_stat;
	size_t pe_size;
	size_t rom_size;
	size_t compressed_size;
	void *buf;
	void *payload;
	unsigned int i;
	uint16_t machine;
	uint16_t subsystem;
	uint8_t checksum;
	int compressed;

	/* Determine PE file size */
	if ( fstat ( fileno ( pe ), &pe_stat ) != 0 ) {
		eprintf ( "Could not stat PE file: %s\n",
			  strerror ( errno ) );
		exit ( 1 );
	}
	pe_size = pe_stat.st_size;

	/* Determine ROM file size */
	rom_size = ROM_SIZE ( sizeof ( *headers ) + pe_size );

	/* Allocate ROM buffer and read in PE file */
	buf = xmalloc ( rom_size );
	memset ( buf, 0, rom_size );
	headers = buf;
	payload = ( buf + sizeof ( *headers ) );
	if ( fread ( payload, pe_size, 1, pe ) != 1 ) {
		eprintf ( "Could not read PE file: %s\n",
			  strerror ( errno ) );
		exit ( 1 );
	}

	/* Parse PE headers */
	read_pe_info ( payload, &machine, &subsystem );

	/* Compress the image, if requested */
	if ( opts->compress ) {
		compressed_size = efi_compress ( payload, pe_size );
		rom_size = ROM_SIZE ( sizeof ( *headers ) + compressed_size );
		compressed = ( compressed_size < pe_size );
	} else {
		compressed = 0;
	}

	/* Construct ROM header */
	headers->rom.Signature = PCI_EXPANSION_ROM_HEADER_SIGNATURE;
	headers->rom.InitializationSize = ( rom_size / 512 );
	headers->rom.EfiSignature = EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE;
	headers->rom.EfiSubsystem = subsystem;
	headers->rom.EfiMachineType = machine;
	headers->rom.CompressionType =
		( compressed ? EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED : 0 );
	headers->rom.EfiImageHeaderOffset = sizeof ( *headers );
	headers->rom.PcirOffset =
		offsetof ( typeof ( *headers ), pci );
	headers->pci.Signature = PCI_DATA_STRUCTURE_SIGNATURE;
	headers->pci.VendorId = opts->vendor;
	headers->pci.DeviceId = opts->device;
	headers->pci.Length = sizeof ( headers->pci );
	headers->pci.ClassCode[2] = PCI_CLASS_NETWORK;
	headers->pci.ImageLength = ( rom_size / 512 );
	headers->pci.CodeType = 0x03; /* No constant in EFI headers? */
	headers->pci.Indicator = 0x80; /* No constant in EFI headers? */

	/* Fix image checksum */
	for ( i = 0, checksum = 0 ; i < rom_size ; i++ )
		checksum += *( ( uint8_t * ) buf + i );
	headers->checksum -= checksum;

	/* Write out ROM */
	if ( fwrite ( buf, rom_size, 1, rom ) != 1 ) {
		eprintf ( "Could not write ROM file: %s\n",
			  strerror ( errno ) );
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
			{ "compress", 0, NULL, 'c' },
			{ "help", 0, NULL, 'h' },
			{ 0, 0, 0, 0 }
		};

		if ( ( c = getopt_long ( argc, argv, "v:d:ch",
					 long_options,
					 &option_index ) ) == -1 ) {
			break;
		}

		switch ( c ) {
		case 'v':
			opts->vendor = strtoul ( optarg, &end, 16 );
			if ( *end || ( ! *optarg ) ) {
				eprintf ( "Invalid vendor \"%s\"\n", optarg );
				exit ( 2 );
			}
			break;
		case 'd':
			opts->device = strtoul ( optarg, &end, 16 );
			if ( *end || ( ! *optarg ) ) {
				eprintf ( "Invalid device \"%s\"\n", optarg );
				exit ( 2 );
			}
			break;
		case 'c':
			opts->compress = 1;
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
	struct options opts;
	int infile_index;
	const char *infile_name;
	const char *outfile_name;
	FILE *infile;
	FILE *outfile;

	/* Parse command-line arguments */
	memset ( &opts, 0, sizeof ( opts ) );
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
