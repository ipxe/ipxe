/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 *
 * $Id$
 */

#ifndef CALLBACKS_ARCH_H
#define CALLBACKS_ARCH_H

/* Skip the definitions that won't make sense to the assembler */
#ifndef ASSEMBLY

/* Struct to hold general-purpose register values.  PUSHAL and POPAL
 * can work directly with this structure; do not change the order of
 * registers.
 */
typedef struct {
	union {
		uint16_t di;
		uint32_t edi;
	};
	union {
		uint16_t si;
		uint32_t esi;
	};
	union {
		uint16_t bp;
		uint32_t ebp;
	};
	union {
		uint16_t sp;
		uint32_t esp;
	};
	union {
		struct {
			uint8_t bl;
			uint8_t bh;
		} PACKED;
		uint16_t bx;
		uint32_t ebx;
	};
	union {
		struct {
			uint8_t dl;
			uint8_t dh;
		} PACKED;
		uint16_t dx;
		uint32_t edx;
	};
	union {
		struct {
			uint8_t cl;
			uint8_t ch;
		} PACKED;
		uint16_t cx;
		uint32_t ecx;
	};
	union {
		struct {
			uint8_t al;
			uint8_t ah;
		} PACKED;
		uint16_t ax;
		uint32_t eax;
	};
} regs_t;

/* Struct to hold segment register values.  Don't change the order;
 * many bits of assembly code rely on it.
 */
typedef struct {
	uint16_t cs;
	uint16_t ss;
	uint16_t ds;
	uint16_t es;
	uint16_t fs;
	uint16_t gs;
} PACKED seg_regs_t;

/* Struct for a GDT descriptor */
typedef struct {
		uint16_t limit;
		uint32_t address;
		uint16_t padding;
} PACKED gdt_descriptor_t;

/* Struct for a GDT entry.  Use GDT_SEGMENT() to fill it in.
 */
typedef struct {
	uint16_t limit_0_15;
	uint16_t base_0_15;
	uint8_t base_16_23;
	uint8_t accessed__type__sflag__dpl__present;
	uint8_t limit_16_19__avl__size__granularity;
	uint8_t base_24_31;
} PACKED gdt_segment_t;

#define GDT_SEGMENT(base,limit,type,sflag,dpl,avl,size,granularity) \
	( (gdt_segment_t) { \
		( (limit) & 0xffff ), \
		( (base) & 0xffff ), \
		( ( (base) >> 16 ) & 0xff ), \
		( ( 1 << 0 ) | ( (type) << 1 ) | \
		  ( (sflag) << 4 ) | ( (dpl) << 5 ) | ( 1 << 7 ) ), \
		( ( (limit) >> 16 ) | \
		  ( (avl) << 4 ) | ( (size) << 5 ) | ( (granularity) << 7 ) ),\
		( (base) >> 24 ) \
	} )
#define GDT_SEGMENT_BASE(gdt_segment) \
	( (gdt_segment)->base_0_15 | \
	  (gdt_segment)->base_16_23 << 16 | \
	  (gdt_segment)->base_24_31 << 24 )
#define GDT_SEGMENT_LIMIT(gdt_segment) \
	( (gdt_segment)->limit_0_15 | \
	  ( ( (gdt_segment)->limit_16_19__avl__size__granularity \
	      & 0xf ) << 16 ) )
#define GDT_SEGMENT_GRANULARITY(gdt_segment) \
	( ( (gdt_segment)->limit_16_19__avl__size__granularity \
	    & 0x80 ) >> 7 )
#define GDT_SEGMENT_TYPE(gdt_segment) \
	( ( (gdt_segment)->accessed__type__sflag__dpl__present & 0x0e ) >> 1 )
#define GDT_SEGMENT_SIZE(gdt_segment) \
	( ( (gdt_segment)->limit_16_19__avl__size__granularity \
	    & 0x60 ) >> 5 )

#define GDT_TYPE_DATA			(0x0)
#define GDT_TYPE_STACK			(0x2)
#define GDT_TYPE_WRITEABLE		(0x1)
#define GDT_TYPE_CODE			(0x6)
#define GDT_TYPE_EXEC_ONLY_CODE		(0x4)
#define GDT_TYPE_CONFORMING		(0x1)
#define GDT_SFLAG_SYSTEM		(0)
#define GDT_SFLAG_NORMAL		(1)
#define GDT_AVL_NORMAL			(0)
#define GDT_SIZE_16BIT			(0x0)
#define GDT_SIZE_32BIT			(0x2)
#define GDT_SIZE_64BIT			(0x1)
#define GDT_SIZE_UNKNOWN		(0x3)
#define GDT_GRANULARITY_SMALL		(0)
#define GDT_GRANULARITY_LARGE		(1)
#define GDT_SEGMENT_NORMAL(base,limit,type,size,granularity) \
	GDT_SEGMENT ( base, limit, type, \
		      GDT_SFLAG_NORMAL, 0, GDT_AVL_NORMAL, \
		      size, granularity )

/* Protected mode code segment */
#define GDT_SEGMENT_PMCS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_CODE | GDT_TYPE_CONFORMING, \
	GDT_SIZE_32BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_PMCS_PHYS GDT_SEGMENT_PMCS(0)
/* Protected mode data segment */
#define GDT_SEGMENT_PMDS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_DATA | GDT_TYPE_WRITEABLE, \
	GDT_SIZE_32BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_PMDS_PHYS GDT_SEGMENT_PMDS(0)
/* Real mode code segment */
/* Not sure if there's any reason to use GDT_TYPE_EXEC_ONLY_CODE
 * instead of just GDT_TYPE_CODE, but that's what our old GDT did and
 * it worked, so I'm not changing it.
 */
#define GDT_SEGMENT_RMCS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xffff, GDT_TYPE_EXEC_ONLY_CODE | GDT_TYPE_CONFORMING, \
	GDT_SIZE_16BIT, GDT_GRANULARITY_SMALL )
/* Real mode data segment */
#define GDT_SEGMENT_RMDS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xffff, GDT_TYPE_DATA | GDT_TYPE_WRITEABLE, \
	GDT_SIZE_16BIT, GDT_GRANULARITY_SMALL )
/* Long mode code segment */
#define GDT_SEGMENT_LMCS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_CODE | GDT_TYPE_CONFORMING, \
	GDT_SIZE_64BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_LMCS_PHYS GDT_SEGMENT_LMCS(0)
/* Long mode data segment */
/* AFIACT, GDT_SIZE_64BIT applies only to code segments */
#define GDT_SEGMENT_LMDS(base) GDT_SEGMENT_NORMAL ( \
	base, 0xfffff, GDT_TYPE_DATA | GDT_TYPE_WRITEABLE, \
	GDT_SIZE_32BIT, GDT_GRANULARITY_LARGE )
#define GDT_SEGMENT_LMDS_PHYS GDT_SEGMENT_LMDS(0)

/* Template for creating GDT structures (including segment register
 * lists), suitable for passing as parameters to external_call().
 */
#define GDT_STRUCT_t(num_segments) \
	struct { \
		gdt_descriptor_t descriptor; \
		gdt_segment_t segments[num_segments]; \
	} PACKED
/* And utility function for filling it in */
#define GDT_ADJUST(structure) { \
	(structure)->descriptor.address = \
		virt_to_phys(&((structure)->descriptor.limit)); \
	(structure)->descriptor.limit = \
		sizeof((structure)->segments) + 8 - 1; \
	(structure)->descriptor.padding = 0; \
}

/* Data passed in to in_call() by assembly wrapper.
 */
typedef struct {
	regs_t		regs;
	seg_regs_t	seg_regs;
	gdt_descriptor_t gdt_desc;
	uint32_t	flags;
	struct {
		uint32_t offset;
		uint32_t segment;
	} ret_addr;
} PACKED i386_pm_in_call_data_t;

typedef struct {
	seg_regs_t	seg_regs;
	union {
		uint16_t	pad;
		uint16_t	prefix_sp;
	};
	uint16_t	flags;
	struct {
		uint16_t offset;
		uint16_t segment;
	} ret_addr;
	uint32_t orig_opcode;
} PACKED i386_rm_in_call_data_t;

typedef struct {
	i386_pm_in_call_data_t *pm;
	i386_rm_in_call_data_t *rm;
} i386_in_call_data_t;
#define in_call_data_t i386_in_call_data_t

/* Function prototypes
 */
extern int install_rm_callback_interface ( void *address, size_t available );

#endif /* ASSEMBLY */

#define RM_IN_CALL	(0)
#define RM_IN_CALL_FAR	(2)

#endif /* CALLBACKS_ARCH_H */
