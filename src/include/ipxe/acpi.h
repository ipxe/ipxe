#ifndef _IPXE_ACPI_H
#define _IPXE_ACPI_H

/** @file
 *
 * ACPI data structures
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <byteswap.h>
#include <ipxe/refcnt.h>
#include <ipxe/list.h>
#include <ipxe/interface.h>
#include <ipxe/tables.h>
#include <ipxe/api.h>
#include <config/general.h>

/** An ACPI small resource descriptor header */
struct acpi_small_resource {
	/** Tag byte */
	uint8_t tag;
} __attribute__ (( packed ));

/** ACPI small resource length mask */
#define ACPI_SMALL_LEN_MASK 0x03

/** An ACPI end resource descriptor */
#define ACPI_END_RESOURCE 0x78

/** An ACPI end resource descriptor */
struct acpi_end_resource {
	/** Header */
	struct acpi_small_resource hdr;
	/** Checksum */
	uint8_t checksum;
} __attribute__ (( packed ));

/** An ACPI large resource descriptor header */
struct acpi_large_resource {
	/** Tag byte */
	uint8_t tag;
	/** Length of data items */
	uint16_t len;
} __attribute__ (( packed ));

/** ACPI large resource flag */
#define ACPI_LARGE 0x80

/** An ACPI QWORD address space resource descriptor */
#define ACPI_QWORD_ADDRESS_SPACE_RESOURCE 0x8a

/** An ACPI QWORD address space resource descriptor */
struct acpi_qword_address_space_resource {
	/** Header */
	struct acpi_large_resource hdr;
	/** Resource type */
	uint8_t type;
	/** General flags */
	uint8_t general;
	/** Type-specific flags */
	uint8_t specific;
	/** Granularity */
	uint64_t granularity;
	/** Minimum address */
	uint64_t min;
	/** Maximum address */
	uint64_t max;
	/** Translation offset */
	uint64_t offset;
	/** Length */
	uint64_t len;
} __attribute__ (( packed ));

/** A memory address space type */
#define ACPI_ADDRESS_TYPE_MEM 0x00

/** A bus number address space type */
#define ACPI_ADDRESS_TYPE_BUS 0x02

/** An ACPI resource descriptor */
union acpi_resource {
	/** Tag byte */
	uint8_t tag;
	/** Small resource descriptor */
	struct acpi_small_resource small;
	/** End resource descriptor */
	struct acpi_end_resource end;
	/** Large resource descriptor */
	struct acpi_large_resource large;
	/** QWORD address space resource descriptor */
	struct acpi_qword_address_space_resource qword;
};

/**
 * Get ACPI resource tag
 *
 * @v res		ACPI resource descriptor
 * @ret tag		Resource tag
 */
static inline unsigned int acpi_resource_tag ( union acpi_resource *res ) {

	return ( ( res->tag & ACPI_LARGE ) ?
		 res->tag : ( res->tag & ~ACPI_SMALL_LEN_MASK ) );
}

/**
 * Get length of ACPI small resource descriptor
 *
 * @v res		Small resource descriptor
 * @ret len		Length of descriptor
 */
static inline size_t acpi_small_len ( struct acpi_small_resource *res ) {

	return ( sizeof ( *res ) + ( res->tag & ACPI_SMALL_LEN_MASK ) );
}

/**
 * Get length of ACPI large resource descriptor
 *
 * @v res		Large resource descriptor
 * @ret len		Length of descriptor
 */
static inline size_t acpi_large_len ( struct acpi_large_resource *res ) {

	return ( sizeof ( *res ) + le16_to_cpu ( res->len ) );
}

/**
 * Get length of ACPI resource descriptor
 *
 * @v res		ACPI resource descriptor
 * @ret len		Length of descriptor
 */
static inline size_t acpi_resource_len ( union acpi_resource *res ) {

	return ( ( res->tag & ACPI_LARGE ) ?
		 acpi_large_len ( &res->large ) :
		 acpi_small_len ( &res->small ) );
}

/**
 * Get next ACPI resource descriptor
 *
 * @v res		ACPI resource descriptor
 * @ret next		Next ACPI resource descriptor
 */
static inline union acpi_resource *
acpi_resource_next ( union acpi_resource *res ) {

	return ( ( ( void * ) res ) + acpi_resource_len ( res ) );
}

/**
 * An ACPI description header
 *
 * This is the structure common to the start of all ACPI system
 * description tables.
 */
struct acpi_header {
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
 * Transcribe ACPI table signature (for debugging)
 *
 * @v signature		ACPI table signature
 * @ret name		ACPI table signature name
 */
static inline const char * acpi_name ( uint32_t signature ) {
	static union {
		uint32_t signature;
		char name[5];
	} u;

	u.signature = cpu_to_le32 ( signature );
	return u.name;
}

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

/** Root System Description Table (RSDT) signature */
#define RSDT_SIGNATURE ACPI_SIGNATURE ( 'R', 'S', 'D', 'T' )

/** ACPI Root System Description Table (RSDT) */
struct acpi_rsdt {
	/** ACPI header */
	struct acpi_header acpi;
	/** ACPI table entries */
	uint32_t entry[0];
} __attribute__ (( packed ));

/** Fixed ACPI Description Table (FADT) signature */
#define FADT_SIGNATURE ACPI_SIGNATURE ( 'F', 'A', 'C', 'P' )

/** Fixed ACPI Description Table (FADT) */
struct acpi_fadt {
	/** ACPI header */
	struct acpi_header acpi;
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
	/** PM2 Control Register Block */
	uint32_t pm2_cnt_blk;
	/** PM Timer Control Register Block */
	uint32_t pm_tmr_blk;
} __attribute__ (( packed ));

/** ACPI PM1 Control Register (within PM1a_CNT_BLK or PM1A_CNT_BLK) */
#define ACPI_PM1_CNT 0
#define ACPI_PM1_CNT_SLP_TYP(x) ( (x) << 10 )	/**< Sleep type */
#define ACPI_PM1_CNT_SLP_EN ( 1 << 13 )		/**< Sleep enable */

/** ACPI PM Timer Register (within PM_TMR_BLK) */
#define ACPI_PM_TMR 0

/** Differentiated System Description Table (DSDT) signature */
#define DSDT_SIGNATURE ACPI_SIGNATURE ( 'D', 'S', 'D', 'T' )

/** Secondary System Description Table (SSDT) signature */
#define SSDT_SIGNATURE ACPI_SIGNATURE ( 'S', 'S', 'D', 'T' )

/** An ACPI descriptor (used to construct ACPI tables) */
struct acpi_descriptor {
	/** Reference count of containing object */
	struct refcnt *refcnt;
	/** Table model */
	struct acpi_model *model;
	/** List of ACPI descriptors for this model */
	struct list_head list;
};

/**
 * Initialise ACPI descriptor
 *
 * @v desc		ACPI descriptor
 * @v model		Table model
 * @v refcnt		Reference count
 */
static inline __attribute__ (( always_inline )) void
acpi_init ( struct acpi_descriptor *desc, struct acpi_model *model,
	    struct refcnt *refcnt ) {

	desc->refcnt = refcnt;
	desc->model = model;
	INIT_LIST_HEAD ( &desc->list );
}

/** An ACPI table model */
struct acpi_model {
	/** List of descriptors */
	struct list_head descs;
	/**
	 * Check if ACPI descriptor is complete
	 *
	 * @v desc		ACPI descriptor
	 * @ret rc		Return status code
	 */
	int ( * complete ) ( struct acpi_descriptor *desc );
	/**
	 * Install ACPI tables
	 *
	 * @v install		Installation method
	 * @ret rc		Return status code
	 */
	int ( * install ) ( int ( * install ) ( struct acpi_header *acpi ) );
};

/** ACPI models */
#define ACPI_MODELS __table ( struct acpi_model, "acpi_models" )

/** Declare an ACPI model */
#define __acpi_model __table_entry ( ACPI_MODELS, 01 )

/**
 * Calculate static inline ACPI API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define ACPI_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( ACPI_PREFIX_ ## _subsys, _api_func )

/**
 * Provide an ACPI API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_ACPI( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( ACPI_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline ACPI API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_ACPI_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( ACPI_PREFIX_ ## _subsys, _api_func )

extern const struct acpi_header * acpi_find_via_rsdt ( uint32_t signature,
						       unsigned int index );

/* Include all architecture-independent ACPI API headers */
#include <ipxe/null_acpi.h>
#include <ipxe/efi/efi_acpi.h>
#include <ipxe/linux/linux_acpi.h>

/* Include all architecture-dependent ACPI API headers */
#include <bits/acpi.h>

/**
 * Locate ACPI root system description table
 *
 * @ret rsdt		ACPI root system description table, or NULL
 */
const struct acpi_rsdt * acpi_find_rsdt ( void );

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
const struct acpi_header * acpi_find ( uint32_t signature,
				       unsigned int index );

extern struct acpi_descriptor *
acpi_describe ( struct interface *interface );
#define acpi_describe_TYPE( object_type )				\
	typeof ( struct acpi_descriptor * ( object_type ) )

extern const struct acpi_header * ( * acpi_finder ) ( uint32_t signature,
						      unsigned int index );

extern void acpi_fix_checksum ( struct acpi_header *acpi );
extern const struct acpi_header * acpi_table ( uint32_t signature,
					       unsigned int index );
extern int acpi_extract ( uint32_t signature, void *data,
			  int ( * extract ) ( const struct acpi_header *zsdt,
					      size_t len, size_t offset,
					      void *data ) );
extern void acpi_add ( struct acpi_descriptor *desc );
extern void acpi_del ( struct acpi_descriptor *desc );
extern int acpi_install ( int ( * install ) ( struct acpi_header *acpi ) );

#endif /* _IPXE_ACPI_H */
