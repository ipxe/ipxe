#include "image.h"
#include "memsizes.h"
#include "realmode.h"
#include "gateA20.h"
#include "osloader.h"
#include "etherboot.h"

/* An NBI image header */
struct imgheader {
	unsigned long magic;
	union {
		unsigned char length;
		unsigned long flags;
	};
	segoff_t location;
	union {
		segoff_t segoff;
		unsigned long linear;
	} execaddr;
} __attribute__ (( packed ));

/* NBI magic number */
#define NBI_MAGIC 0x1B031336UL

/* Interpretation of the "length" fields */
#define NBI_NONVENDOR_LENGTH(len)	( ( (len) & 0x0f ) << 2 )
#define NBI_VENDOR_LENGTH(len)		( ( (len) & 0xf0 ) >> 2 )
#define NBI_LENGTH(len) ( NBI_NONVENDOR_LENGTH(len) + NBI_VENDOR_LENGTH(len) )

/* Interpretation of the "flags" fields */
#define	NBI_PROGRAM_RETURNS(flags)	( (flags) & ( 1 << 8 ) )
#define	NBI_LINEAR_EXEC_ADDR(flags)	( (flags) & ( 1 << 31 ) )

/* NBI header length */
#define NBI_HEADER_LENGTH	512

/* An NBI segment header */
struct segheader {
	unsigned char length;
	unsigned char vendortag;
	unsigned char reserved;
	unsigned char flags;
	unsigned long loadaddr;
	unsigned long imglength;
	unsigned long memlength;
};

/* Interpretation of the "flags" fields */
#define NBI_LOADADDR_FLAGS(flags)	( (flags) & 0x03 )
#define NBI_LOADADDR_ABS		0x00
#define NBI_LOADADDR_AFTER		0x01
#define NBI_LOADADDR_END		0x02
#define NBI_LOADADDR_BEFORE		0x03
#define NBI_LAST_SEGHEADER(flags)	( (flags) & ( 1 << 2 ) )

/* Info passed to NBI image */
static struct ebinfo loaderinfo = {
	VERSION_MAJOR, VERSION_MINOR,
	0
};

/*
 * Determine whether or not this is a valid NBI image
 *
 */
static int nbi_probe ( physaddr_t start, off_t len, void **context ) {
	static struct imgheader imgheader;

	if ( (unsigned)len < sizeof ( imgheader ) ) {
		DBG ( "NBI image too small\n" );
		return 0;
	}

	copy_from_phys ( &imgheader, start, sizeof ( imgheader ) );

	if ( imgheader.magic != NBI_MAGIC )
		return 0;

	/* Record image context */
	DBG ( "NBI found valid image\n" );
	*context = &imgheader;
	return 1;
}

/*
 * Prepare a segment for an NBI image
 *
 */
static int nbi_prepare_segment ( physaddr_t dest, off_t imglen, off_t memlen,
				 physaddr_t src __unused ) {
	DBG ( "NBI preparing segment [%x,%x) (imglen %d memlen %d)\n",
	      dest, dest + memlen, imglen, memlen );
	return prep_segment ( dest, dest + imglen, dest + memlen );
}

/*
 * Load a segment for an NBI image
 *
 */
static int nbi_load_segment ( physaddr_t dest, off_t imglen,
			      off_t memlen __unused, physaddr_t src ) {
	DBG ( "NBI loading segment [%x,%x)\n", dest, dest + imglen );
	copy_phys_to_phys ( dest, src, imglen );
	return 1;
}

/*
 * Process segments of an NBI image
 *
 */
static int nbi_process_segments ( physaddr_t start, off_t len,
				  struct imgheader *imgheader,
				  int ( * process ) ( physaddr_t dest,
						      off_t imglen,
						      off_t memlen,
						      physaddr_t src ) ) {
	struct segheader sh;
	off_t offset = 0;
	off_t sh_off;
	physaddr_t dest;
	off_t dest_imglen, dest_memlen;
	
	/* Copy header to target location */
	dest = ( ( imgheader->location.segment << 4 ) +
		 imgheader->location.offset );
	dest_imglen = dest_memlen = NBI_HEADER_LENGTH;
	if ( ! process ( dest, dest_imglen, dest_memlen, start + offset ) )
		return 0;
	offset += dest_imglen;

	/* Process segments in turn */
	sh_off = NBI_LENGTH ( imgheader->length );
	do {
		/* Read segment header */
		copy_from_phys ( &sh, start + sh_off, sizeof ( sh ) );
		if ( sh.length == 0 ) {
			/* Avoid infinite loop? */
			DBG ( "NBI invalid segheader length 0\n" );
			return 0;
		}
		
		/* Calculate segment load address */
		switch ( NBI_LOADADDR_FLAGS ( sh.flags ) ) {
		case NBI_LOADADDR_ABS:
			dest = sh.loadaddr;
			break;
		case NBI_LOADADDR_AFTER:
			dest = dest + dest_memlen + sh.loadaddr;
			break;
		case NBI_LOADADDR_BEFORE:
			dest = dest - sh.loadaddr;
			break;
		case NBI_LOADADDR_END:
			/* Not correct according to the spec, but
			 * maintains backwards compatibility with
			 * previous versions of Etherboot.
			 */
			dest = ( meminfo.memsize * 1024 + 0x100000UL )
				- sh.loadaddr;
			break;
		default:
			DBG ( "NBI can't count up to three\n" );
			return 0;
		}

		/* Process this segment */
		dest_imglen = sh.imglength;
		dest_memlen = sh.memlength;
		if ( ! process ( dest, dest_imglen, dest_memlen,
				 start + offset ) )
			return 0;
		offset += dest_imglen;

		/* Next segheader */
		sh_off += NBI_LENGTH ( sh.length );
		if ( sh_off >= NBI_HEADER_LENGTH ) {
			DBG ( "NBI header overflow\n" );
			return 0;
		}

	} while ( ! NBI_LAST_SEGHEADER ( sh.flags ) );

	if ( offset != len ) {
		DBG ( "NBI length mismatch (file %d, metadata %d)\n",
		      len, offset );
		return 0;
	}

	return 1;
}

/*
 * Load an NBI image into memory
 *
 */
static int nbi_load ( physaddr_t start, off_t len, void *context ) {
	struct imgheader *imgheader = context;

	/* If we don't have enough data give up */
	if ( len < NBI_HEADER_LENGTH )
		return 0;
	
	DBG ( "NBI placing header at %hx:%hx\n",
	      imgheader->location.segment, imgheader->location.offset );

	/* NBI files can have overlaps between segments; the bss of
	 * one segment may overlap the initialised data of another.  I
	 * assume this is a design flaw, but there are images out
	 * there that we need to work with.  We therefore do two
	 * passes: first to initialise the segments, then to copy the
	 * data.  This avoids zeroing out already-copied data.
	 */
	if ( ! nbi_process_segments ( start, len, imgheader,
				      nbi_prepare_segment ) )
		return 0;
	if ( ! nbi_process_segments ( start, len, imgheader,
				      nbi_load_segment ) )
		return 0;

	return 1;
}

/*
 * Boot a 16-bit NBI image
 *
 */
static int nbi_boot16 ( struct imgheader *imgheader ) {
	uint16_t basemem_bootp;
	int discard_D, discard_S, discard_b;

	DBG ( "NBI executing 16-bit image at %hx:%hx\n",
	      imgheader->execaddr.segoff.segment,
	      imgheader->execaddr.segoff.offset );

	gateA20_unset();

	basemem_bootp = BASEMEM_PARAMETER_INIT ( bootp_data );
	REAL_EXEC ( rm_xstart16,
		    "pushw %%ds\n\t"	/* far pointer to bootp data copy */
		    "pushw %%bx\n\t"
		    "pushl %%esi\n\t"	/* location */
		    "pushw %%cs\n\t"	/* lcall execaddr */
		    "call 1f\n\t"
		    "jmp 2f\n\t"
		    "\n1:\n\t"
		    "pushl %%edi\n\t"
		    "lret\n\t"
		    "\n2:\n\t"
		    "addw $8,%%sp\n\t",	/* pop location and bootp ptr */
		    3,
		    OUT_CONSTRAINTS ( "=D" ( discard_D ), "=S" ( discard_S ),
				      "=b" ( discard_b ) ),
		    IN_CONSTRAINTS ( "D" ( imgheader->execaddr.segoff ),
				     "S" ( imgheader->location ),
				     "b" ( basemem_bootp ) ),
		    CLOBBER ( "eax", "ecx", "edx", "ebp" ) );
	BASEMEM_PARAMETER_DONE ( bootp_data );
	
	return 0;
}

/*
 * Boot a 32-bit NBI image
 *
 */
static int nbi_boot32 ( struct imgheader *imgheader ) {
	int rc = 0;

	DBG ( "NBI executing 32-bit image at %x\n",
	      imgheader->execaddr.linear );

	/* no gateA20_unset for PM call */
	rc = xstart32 ( imgheader->execaddr.linear,
			virt_to_phys ( &loaderinfo ),
			( ( imgheader->location.segment << 4 ) +
			  imgheader->location.offset ),
			virt_to_phys ( &bootp_data ) );
	printf ( "Secondary program returned %d\n", rc );
	if ( ! NBI_PROGRAM_RETURNS ( imgheader->flags ) ) {
		/* We shouldn't have returned */
		rc = 0;
	}

	return rc;
}

/*
 * Boot a loaded NBI image
 *
 */
static int nbi_boot ( void *context ) {
	struct imgheader *imgheader = context;

	if ( NBI_LINEAR_EXEC_ADDR ( imgheader->flags ) ) {
		return nbi_boot32 ( imgheader );
	} else {
		return nbi_boot16 ( imgheader );
	}
}

static struct image nbi_image __image = {
	.name	= "NBI",
	.probe	= nbi_probe,
	.load	= nbi_load,
	.boot	= nbi_boot,
};
