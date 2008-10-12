#ifndef _EFI_H
#define _EFI_H

/** @file
 *
 * EFI API
 *
 * The intention is to include near-verbatim copies of the EFI headers
 * required by gPXE.  This is achieved using the import.pl script in
 * this directory.  Run the import script to update the local copies
 * of the headers:
 *
 *     ./import.pl /path/to/edk2/edk2
 *
 * where /path/to/edk2/edk2 is the path to your local checkout of the
 * EFI Development Kit.
 *
 * Note that import.pl will modify any #include lines in each imported
 * header to reflect its new location within the gPXE tree.  It will
 * also tidy up the file by removing carriage return characters and
 * trailing whitespace.
 *
 *
 * At the time of writing, there are a few other modifications to
 * these headers that are present in my personal edk2 tree, that are
 * not yet committed back to the main edk2 repository.  These
 * modifications are fixes for compilation on case-dependent
 * filesystems, compilation under -mrtd and -mregparm=3, etc.
 */

/* Include the top-level EFI header file */
#include <gpxe/efi/Uefi.h>

/* Reset any trailing #pragma pack directives */
#pragma pack()

#include <gpxe/tables.h>
#include <gpxe/uuid.h>

/** An EFI protocol used by gPXE */
struct efi_protocol {
	union {
		/** EFI protocol GUID */
		EFI_GUID guid;
		/** UUID structure understood by gPXE */
		union uuid uuid;
	} u;
	/** Variable containing pointer to protocol structure */
	void **protocol;
};

/** Declare an EFI protocol used by gPXE */
#define __efi_protocol \
	__table ( struct efi_protocol, efi_protocols, 01 )

/** Declare an EFI protocol to be required by gPXE
 *
 * @v _protocol		EFI protocol name
 * @v _ptr		Pointer to protocol instance
 */
#define EFI_REQUIRE_PROTOCOL( _protocol, _ptr )				     \
	struct efi_protocol __ ## _protocol __efi_protocol = {		     \
		.u.guid = _protocol ## _GUID,				     \
		.protocol = ( ( void ** ) ( void * )			     \
			      ( ( (_ptr) == ( ( _protocol ** ) NULL ) ) ?    \
				(_ptr) : (_ptr) ) ),			     \
	}

/** Convert a gPXE status code to an EFI status code
 *
 * FIXME: actually perform some kind of conversion.  gPXE error codes
 * will be detected as EFI error codes; both have the top bit set, and
 * the success return code is zero for both.  Anything that just
 * reports a numerical error will be OK, anything attempting to
 * interpret the value or to display a text equivalent will be
 * screwed.
 */
#define RC_TO_EFIRC( rc ) (rc)

/** Convert an EFI status code to a gPXE status code
 *
 * FIXME: as above
 */
#define EFIRC_TO_RC( efirc ) (efirc)

extern EFI_HANDLE efi_image_handle;
extern EFI_SYSTEM_TABLE *efi_systab;

#endif /* _EFI_H */
