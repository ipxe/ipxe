#ifndef _IPXE_ACPI_H
#define _IPXE_ACPI_H

/** @file
 *
 * ACPI data structures
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/interface.h>
#include <ipxe/uaccess.h>

/**
 * An ACPI description header
 *
 * This is the structure common to the start of all ACPI system
 * description tables.
 */
struct acpi_description_header {
	/** ACPI signature (4 ASCII characters) */
	uint32_t signature;
	/** Length of table, in bytes, including header */
	uint32_t length;
	/** ACPI Specification minor version number */
	uint8_t revision;
	/** To make sum of entire table == 0 */
	uint8_t checksum;
	/** OEM identification */
	char oem_id[6];
	/** OEM table identification */
	char oem_table_id[8];
	/** OEM revision number */
	uint32_t oem_revision;
	/** ASL compiler vendor ID */
	char asl_compiler_id[4];
	/** ASL compiler revision number */
	uint32_t asl_compiler_revision;
} __attribute__ (( packed ));

/**
 * Build ACPI signature
 *
 * @v a			First character of ACPI signature
 * @v b			Second character of ACPI signature
 * @v c			Third character of ACPI signature
 * @v d			Fourth character of ACPI signature
 * @ret signature	ACPI signature
 */
#define ACPI_SIGNATURE( a, b, c, d ) \
	( ( (a) << 0 ) | ( (b) << 8 ) | ( (c) << 16 ) | ( (d) << 24 ) )

/** Root System Description Pointer signature */
#define RSDP_SIGNATURE { 'R', 'S', 'D', ' ', 'P', 'T', 'R', ' ' }

/** Root System Description Pointer */
struct acpi_rsdp {
	/** Signature */
	char signature[8];
	/** To make sum of entire table == 0 */
	uint8_t checksum;
	/** OEM identification */
	char oem_id[6];
	/** Revision */
	uint8_t revision;
	/** Physical address of RSDT */
	uint32_t rsdt;
} __attribute__ (( packed ));

/** EBDA RSDP length */
#define RSDP_EBDA_LEN 0x400

/** Fixed BIOS area RSDP start address */
#define RSDP_BIOS_START 0xe0000

/** Fixed BIOS area RSDP length */
#define RSDP_BIOS_LEN 0x20000

/** Stride at which to search for RSDP */
#define RSDP_STRIDE 16

/** Root System Description Table (RSDT) signature */
#define RSDT_SIGNATURE ACPI_SIGNATURE ( 'R', 'S', 'D', 'T' )

/** ACPI Root System Description Table (RSDT) */
struct acpi_rsdt {
	/** ACPI header */
	struct acpi_description_header acpi;
	/** ACPI table entries */
	uint32_t entry[0];
} __attribute__ (( packed ));

/** Fixed ACPI Description Table (FADT) signature */
#define FADT_SIGNATURE ACPI_SIGNATURE ( 'F', 'A', 'C', 'P' )

/** Fixed ACPI Description Table (FADT) */
struct acpi_fadt {
	/** ACPI header */
	struct acpi_description_header acpi;
	/** Physical address of FACS */
	uint32_t facs;
	/** Physical address of DSDT */
	uint32_t dsdt;
	/** Unused by iPXE */
	uint8_t unused[20];
	/** PM1a Control Register Block */
	uint32_t pm1a_cnt_blk;
	/** PM1b Control Register Block */
	uint32_t pm1b_cnt_blk;
} __attribute__ (( packed ));

/** ACPI PM1 Control Register (within PM1a_CNT_BLK or PM1A_CNT_BLK) */
#define ACPI_PM1_CNT 0
#define ACPI_PM1_CNT_SLP_TYP(x) ( (x) << 10 )	/**< Sleep type */
#define ACPI_PM1_CNT_SLP_EN ( 1 << 13 )		/**< Sleep enable */

/** Differentiated System Description Table (DSDT) signature */
#define DSDT_SIGNATURE ACPI_SIGNATURE ( 'D', 'S', 'D', 'T' )

/** Secondary System Description Table (SSDT) signature */
#define SSDT_SIGNATURE ACPI_SIGNATURE ( 'S', 'S', 'D', 'T' )

extern int acpi_describe ( struct interface *interface,
			   struct acpi_description_header *acpi, size_t len );
#define acpi_describe_TYPE( object_type )				\
	typeof ( int ( object_type,					\
		       struct acpi_description_header *acpi,		\
		       size_t len ) )

extern void acpi_fix_checksum ( struct acpi_description_header *acpi );
extern userptr_t acpi_find_rsdt ( userptr_t ebda );
extern userptr_t acpi_find ( userptr_t rsdt, uint32_t signature,
			     unsigned int index );
extern int acpi_sx ( userptr_t rsdt, uint32_t signature );

#endif /* _IPXE_ACPI_H */
