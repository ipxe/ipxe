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

#endif /* _EFI_H */
