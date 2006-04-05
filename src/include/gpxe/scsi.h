#ifndef _SCSI_H
#define _SCSI_H

#include <stdint.h>

struct scsi_cdb_read_10 {
	/** Opcode */
	uint8_t opcode;
	/** Flags */
	uint8_t flags;
	/** Start address
	 *
	 * This is a logical block number, in big-endian order.
	 */
	uint32_t lba;
	/** Group number */
	uint8_t group;
	/** Transfer length
	 *
	 * This is a logical block count.
	 */
	uint16_t len;
	/** Control byte */
	uint8_t control;
} __attribute__ (( packed ));

#define SCSI_OPCODE_READ_10 0x28

union scsi_cdb {
	struct scsi_cdb_read_10 read_10;
	char bytes[16];
};

#endif /* _SCSI_H */
