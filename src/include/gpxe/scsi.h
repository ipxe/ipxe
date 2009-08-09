#ifndef _GPXE_SCSI_H
#define _GPXE_SCSI_H

#include <stdint.h>
#include <gpxe/blockdev.h>
#include <gpxe/uaccess.h>
#include <gpxe/refcnt.h>

/** @file
 *
 * SCSI devices
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * @defgroup scsiops SCSI operation codes
 * @{
 */

#define SCSI_OPCODE_READ_10		0x28	/**< READ (10) */
#define SCSI_OPCODE_READ_16		0x88	/**< READ (16) */
#define SCSI_OPCODE_WRITE_10		0x2a	/**< WRITE (10) */
#define SCSI_OPCODE_WRITE_16		0x8a	/**< WRITE (16) */
#define SCSI_OPCODE_READ_CAPACITY_10	0x25	/**< READ CAPACITY (10) */
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

/** A SCSI "READ (10)" CDB */
struct scsi_cdb_read_10 {
	/** Opcode (0x28) */
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
	 * This is a logical block count, in big-endian order.
	 */
	uint16_t len;
	/** Control byte */
	uint8_t control;
} __attribute__ (( packed ));

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

/** A SCSI "WRITE (10)" CDB */
struct scsi_cdb_write_10 {
	/** Opcode (0x2a) */
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
	 * This is a logical block count, in big-endian order.
	 */
	uint16_t len;
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

/** A SCSI "READ CAPACITY (10)" CDB */
struct scsi_cdb_read_capacity_10 {
	/** Opcode (0x25) */
	uint8_t opcode;
	/** Reserved */
	uint8_t reserved_a;
	/** Logical block address
	 *
	 * Applicable only if the PMI bit is set.
	 */
	uint32_t lba;
	/** Reserved */
	uint8_t reserved_b[3];
	/** Control byte */
	uint8_t control;	
} __attribute__ (( packed ));

/** SCSI "READ CAPACITY (10)" parameter data */
struct scsi_capacity_10 {
	/** Maximum logical block number */
	uint32_t lba;
	/** Block length in bytes */
	uint32_t blksize;
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
	struct scsi_cdb_read_10 read10;
	struct scsi_cdb_read_16 read16;
	struct scsi_cdb_write_10 write10;
	struct scsi_cdb_write_16 write16;
	struct scsi_cdb_read_capacity_10 readcap10;
	struct scsi_cdb_read_capacity_16 readcap16;
	unsigned char bytes[16];
};

/** printf() format for dumping a scsi_cdb */
#define SCSI_CDB_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:" \
			"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

/** printf() parameters for dumping a scsi_cdb */
#define SCSI_CDB_DATA(cdb)						  \
	(cdb).bytes[0], (cdb).bytes[1], (cdb).bytes[2], (cdb).bytes[3],	  \
	(cdb).bytes[4], (cdb).bytes[5], (cdb).bytes[6], (cdb).bytes[7],	  \
	(cdb).bytes[8], (cdb).bytes[9], (cdb).bytes[10], (cdb).bytes[11], \
	(cdb).bytes[12], (cdb).bytes[13], (cdb).bytes[14], (cdb).bytes[15]

/** @} */

/** A SCSI command */
struct scsi_command {
	/** CDB for this command */
	union scsi_cdb cdb;
	/** Data-out buffer (may be NULL) */
	userptr_t data_out;
	/** Data-out buffer length
	 *
	 * Must be zero if @c data_out is NULL
	 */
	size_t data_out_len;
	/** Data-in buffer (may be NULL) */
	userptr_t data_in;
	/** Data-in buffer length
	 *
	 * Must be zero if @c data_in is NULL
	 */
	size_t data_in_len;
	/** SCSI status code */
	uint8_t status;
	/** SCSI sense response code */
	uint8_t sense_response;
	/** Command status code */
	int rc;
};

/** A SCSI LUN
 *
 * This is a four-level LUN as specified by SAM-2, in big-endian
 * order.
 */
struct scsi_lun {
	uint16_t u16[4];
}  __attribute__ (( packed ));

/** A SCSI device */
struct scsi_device {
	/** Block device interface */
	struct block_device blockdev;
	/**
	 * Issue SCSI command
	 *
	 * @v scsi		SCSI device
	 * @v command		SCSI command
	 * @ret rc		Return status code
	 *
	 * Note that a successful return status code indicates only
	 * that the SCSI command was issued.  The caller must check
	 * the status field in the command structure to see when the
	 * command completes and whether, for example, the device
	 * returned CHECK CONDITION or some other non-success status
	 * code.
	 */
	int ( * command ) ( struct scsi_device *scsi,
			    struct scsi_command *command );
	/** Backing device */
	struct refcnt *backend;
};

extern int scsi_detached_command ( struct scsi_device *scsi,
				   struct scsi_command *command );
extern int init_scsidev ( struct scsi_device *scsi );
extern int scsi_parse_lun ( const char *lun_string, struct scsi_lun *lun );

#endif /* _GPXE_SCSI_H */
