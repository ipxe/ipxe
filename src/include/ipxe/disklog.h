#ifndef _IPXE_DISKLOG_H
#define _IPXE_DISKLOG_H

/** @file
 *
 * Disk log console
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#ifndef ASSEMBLY

#include <stdint.h>
#include <ipxe/console.h>

/** Disk log partition header */
struct disklog_header {
	/** Magic signature */
	char magic[10];
} __attribute__ (( packed ));

/** A disk log */
struct disklog {
	/** Console device */
	struct console_driver *console;
	/** Disk log operations */
	struct disklog_operations *op;
	/** Logical block data buffer */
	uint8_t *buffer;
	/** Logical block size */
	size_t blksize;
	/** Current logical block index */
	uint64_t lba;
	/** Maximum logical block index */
	uint64_t max_lba;
	/** Current offset within logical block */
	unsigned int offset;
	/** Current number of unwritten characters */
	unsigned int unwritten;
};

/** Disk log operations */
struct disklog_operations {
	/**
	 * Write current logical block
	 *
	 * @ret rc		Return status code
	 */
	int ( * write ) ( void );
};

/**
 * Initialise disk log console
 *
 * @v disklog		Disk log
 * @v op		Disk log operations
 * @v console		Console device
 * @v buffer		Data buffer
 * @v blksize		Logical block size
 * @v lba		Starting logical block index
 * @v max_lba		Maximum logical block index
 */
static inline __attribute__ (( always_inline )) void
disklog_init ( struct disklog *disklog, struct disklog_operations *op,
	       struct console_driver *console, void *buffer, size_t blksize,
	       uint64_t lba, uint64_t max_lba ) {

	disklog->op = op;
	disklog->console = console;
	disklog->buffer = buffer;
	disklog->blksize = blksize;
	disklog->lba = lba;
	disklog->max_lba = max_lba;
}

extern int disklog_open ( struct disklog *disklog );
extern void disklog_putchar ( struct disklog *disklog, int character );

#endif /* ASSEMBLY */

/** Disk log partition type */
#define DISKLOG_PARTITION_TYPE 0xe0

/** Disk log partition magic signature */
#define DISKLOG_MAGIC "iPXE LOG\n\n"

/** Maximum number of outstanding unwritten characters */
#define DISKLOG_MAX_UNWRITTEN 64

#endif /* _IPXE_DISKLOG_H */
