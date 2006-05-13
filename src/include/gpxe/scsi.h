#ifndef _GPXE_SCSI_H
#define _GPXE_SCSI_H

#include <stdint.h>
#include <gpxe/blockdev.h>

/**
 * @defgroup scsiops SCSI operation codes
 * @{
 */

#define SCSI_OPCODE_READ_16		0x88	/**< READ (16) */
#define SCSI_OPCODE_WRITE_16		0x8a	/**< WRITE (16) */
#define SCSI_OPCODE_SERVICE_ACTION_IN	0x9e	/**< SERVICE ACTION IN */
#define SCSI_SERVICE_ACTION_READ_CAPACITY_16 0x10 /**< READ CAPACITY (16) */

/** @} */

/**
 * @defgroup scsiflags SCSI flags
 * @{
 */

#define SCSI_FL_FUA_NV		0x02	/**< Force unit access to NVS */
#define SCSI_FL_FUA		0x08	/**< Force unit access */
#define SCSI_FL_DPO		0x10	/**< Disable cache page out */

/** @} */

/**
 * @defgroup scsicdbs SCSI command data blocks
 * @{
 */

/** A SCSI "READ (16)" CDB */
struct scsi_cdb_read_16 {
	/** Opcode (0x88) */
	uint8_t opcode;
	/** Flags */
	uint8_t flags;
	/** Start address
	 *
	 * This is a logical block number, in big-endian order.
	 */
	uint64_t lba;
	/** Transfer length
	 *
	 * This is a logical block count, in big-endian order.
	 */
	uint32_t len;
	/** Group number */
	uint8_t group;
	/** Control byte */
	uint8_t control;
} __attribute__ (( packed ));

/** A SCSI "WRITE (16)" CDB */
struct scsi_cdb_write_16 {
	/** Opcode (0x8a) */
	uint8_t opcode;
	/** Flags */
	uint8_t flags;
	/** Start address
	 *
	 * This is a logical block number, in big-endian order.
	 */
	uint64_t lba;
	/** Transfer length
	 *
	 * This is a logical block count, in big-endian order.
	 */
	uint32_t len;
	/** Group number */
	uint8_t group;
	/** Control byte */
	uint8_t control;
} __attribute__ (( packed ));

/** A SCSI "READ CAPACITY (16)" CDB */
struct scsi_cdb_read_capacity_16 {
	/** Opcode (0x9e) */
	uint8_t opcode;
	/** Service action */
	uint8_t service_action;
	/** Logical block address
	 *
	 * Applicable only if the PMI bit is set.
	 */
	uint64_t lba;
	/** Transfer length
	 *
	 * This is the size of the data-in buffer, in bytes.
	 */
	uint32_t len;
	/** Reserved */
	uint8_t reserved;
	/** Control byte */
	uint8_t control;
} __attribute__ (( packed ));

/** SCSI "READ CAPACITY (16)" parameter data */
struct scsi_capacity_16 {
	/** Maximum logical block number */
	uint64_t lba;
	/** Block length in bytes */
	uint32_t blksize;
	/** Reserved */
	uint8_t reserved[20];
} __attribute__ (( packed ));

/** A SCSI Command Data Block */
union scsi_cdb {
	struct scsi_cdb_read_16 read16;
	struct scsi_cdb_write_16 write16;
	struct scsi_cdb_read_capacity_16 readcap16;
	char bytes[16];
};

/** @} */

/** A SCSI command */
struct scsi_command {
	/** CDB for this command */
	union scsi_cdb cdb;
	/** Data-out buffer (may be NULL) */
	const void *data_out;
	/** Data-out buffer length
	 *
	 * Must be zero if @c data_out is NULL
	 */
	size_t data_out_len;
	/** Data-in buffer (may be NULL) */
	void *data_in;
	/** Data-in buffer length
	 *
	 * Must be zero if @c data_in is NULL
	 */
	size_t data_in_len;
};

/** A SCSI device */
struct scsi_device {
	/** Block device interface */
	struct block_device blockdev;
	/** Logical unit number (LUN)
	 *
	 * This is a four-level LUN as specified by SAM-2, in
	 * big-endian order.
	 */
	uint64_t lun;
	/**
	 * Issue SCSI command
	 *
	 * @v scsi		SCSI device
	 * @v command		SCSI command
	 * @ret rc		Return status code
	 */
	int ( * command ) ( struct scsi_device *scsi,
			    struct scsi_command *command );
};

extern int init_scsidev ( struct scsi_device *scsi );

#endif /* _GPXE_SCSI_H */
