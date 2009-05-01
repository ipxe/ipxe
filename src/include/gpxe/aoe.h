#ifndef _GPXE_AOE_H
#define _GPXE_AOE_H

/** @file
 *
 * AoE protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <gpxe/list.h>
#include <gpxe/if_ether.h>
#include <gpxe/retry.h>
#include <gpxe/ata.h>

/** An AoE config command */
struct aoecfg {
	/** AoE Queue depth */
	uint16_t bufcnt;
	/** ATA target firmware version */
	uint16_t fwver;
	/** ATA target sector count */
	uint8_t scnt;
	/** AoE config string subcommand */
	uint8_t aoeccmd;
	/** AoE config string length */
	uint16_t cfglen;
	/** AoE config string */
	uint8_t data[0];
} __attribute__ (( packed ));

/** An AoE ATA command */
struct aoeata {
	/** AoE command flags */
	uint8_t aflags;
	/** ATA error/feature register */
	uint8_t err_feat;
	/** ATA sector count register */
	uint8_t count;
	/** ATA command/status register */
	uint8_t cmd_stat;
	/** Logical block address, in little-endian order */
	union {
		uint64_t u64;
		uint8_t bytes[6];
	} lba;
	/** Data payload */
	uint8_t data[0];
} __attribute__ (( packed ));

#define AOE_FL_EXTENDED	0x40	/**< LBA48 extended addressing */
#define AOE_FL_DEV_HEAD	0x10	/**< Device/head flag */
#define AOE_FL_ASYNC	0x02	/**< Asynchronous write */
#define AOE_FL_WRITE	0x01	/**< Write command */

/** An AoE command */
union aoecmd {
	/** Config command */
	struct aoecfg cfg;
	/** ATA command */
	struct aoeata ata;
};

/** An AoE header */
struct aoehdr {
	/** Protocol version number and flags */
	uint8_t ver_flags;
	/** Error code */
	uint8_t error;
	/** Major device number, in network byte order */
	uint16_t major;
	/** Minor device number */
	uint8_t minor;
	/** Command number */
	uint8_t command;
	/** Tag, in network byte order */
	uint32_t tag;
	/** Payload */
	union aoecmd cmd[0];
} __attribute__ (( packed ));

#define AOE_VERSION	0x10	/**< Version 1 */
#define AOE_VERSION_MASK 0xf0	/**< Version part of ver_flags field */

#define AOE_FL_RESPONSE	0x08	/**< Message is a response */
#define AOE_FL_ERROR	0x04	/**< Command generated an error */

#define AOE_MAJOR_BROADCAST 0xffff
#define AOE_MINOR_BROADCAST 0xff

#define AOE_CMD_ATA	0x00	/**< Issue ATA command */
#define AOE_CMD_CONFIG	0x01	/**< Query Config Information */

#define AOE_TAG_MAGIC	0xebeb0000

#define AOE_ERR_BAD_COMMAND	1 /**< Unrecognised command code */
#define AOE_ERR_BAD_PARAMETER	2 /**< Bad argument parameter */
#define AOE_ERR_UNAVAILABLE	3 /**< Device unavailable */
#define AOE_ERR_CONFIG_EXISTS	4 /**< Config string present */
#define AOE_ERR_BAD_VERSION	5 /**< Unsupported version */

/** An AoE session */
struct aoe_session {
	/** Reference counter */
	struct refcnt refcnt;

	/** List of all AoE sessions */
	struct list_head list;

	/** Network device */
	struct net_device *netdev;

	/** Major number */
	uint16_t major;
	/** Minor number */
	uint8_t minor;
	/** Target MAC address */
	uint8_t target[ETH_ALEN];

	/** Tag for current AoE command */
	uint32_t tag;

	/** Current AOE command */
	uint8_t aoe_cmd_type;
	/** Current ATA command */
	struct ata_command *command;
	/** Overall status of current ATA command */
	unsigned int status;
	/** Byte offset within command's data buffer */
	unsigned int command_offset;
	/** Return status code for command */
	int rc;

	/** Retransmission timer */
	struct retry_timer timer;
};

#define AOE_STATUS_ERR_MASK	0x0f /**< Error portion of status code */ 
#define AOE_STATUS_PENDING	0x80 /**< Command pending */

/** Maximum number of sectors per packet */
#define AOE_MAX_COUNT 2

extern void aoe_detach ( struct ata_device *ata );
extern int aoe_attach ( struct ata_device *ata, struct net_device *netdev,
			const char *root_path );

#endif /* _GPXE_AOE_H */
