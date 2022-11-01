#ifndef _IPXE_FDT_H
#define _IPXE_FDT_H

/** @file
 *
 * Flattened Device Tree
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

struct net_device;

/** Device tree header */
struct fdt_header {
	/** Magic signature */
	uint32_t magic;
	/** Total size of device tree */
	uint32_t totalsize;
	/** Offset to structure block */
	uint32_t off_dt_struct;
	/** Offset to strings block */
	uint32_t off_dt_strings;
	/** Offset to memory reservation block */
	uint32_t off_mem_rsvmap;
	/** Version of this data structure */
	uint32_t version;
	/** Lowest version to which this structure is compatible */
	uint32_t last_comp_version;
	/** Physical ID of the boot CPU */
	uint32_t boot_cpuid_phys;
	/** Length of string block */
	uint32_t size_dt_strings;
	/** Length of structure block */
	uint32_t size_dt_struct;
} __attribute__ (( packed ));

/** Magic signature */
#define FDT_MAGIC 0xd00dfeed

/** Expected device tree version */
#define FDT_VERSION 16

/** Device tree token */
typedef uint32_t fdt_token_t;

/** Begin node token */
#define FDT_BEGIN_NODE 0x00000001

/** End node token */
#define FDT_END_NODE 0x00000002

/** Property token */
#define FDT_PROP 0x00000003

/** Property fragment */
struct fdt_prop {
	/** Data length */
	uint32_t len;
	/** Name offset */
	uint32_t name_off;
} __attribute__ (( packed ));

/** NOP token */
#define FDT_NOP 0x00000004

/** End of structure block */
#define FDT_END 0x00000009

/** Alignment of structure block */
#define FDT_STRUCTURE_ALIGN ( sizeof ( fdt_token_t ) )

/** A device tree */
struct fdt {
	/** Tree data */
	union {
		/** Tree header */
		const struct fdt_header *hdr;
		/** Raw data */
		const void *raw;
	};
	/** Length of tree */
	size_t len;
	/** Offset to structure block */
	unsigned int structure;
	/** Length of structure block */
	size_t structure_len;
	/** Offset to strings block */
	unsigned int strings;
	/** Length of strings block */
	size_t strings_len;
};

extern int fdt_path ( const char *path, unsigned int *offset );
extern int fdt_alias ( const char *name, unsigned int *offset );
extern const char * fdt_string ( unsigned int offset, const char *name );
extern int fdt_mac ( unsigned int offset, struct net_device *netdev );
extern int register_fdt ( const struct fdt_header *hdr );

#endif /* _IPXE_FDT_H */
