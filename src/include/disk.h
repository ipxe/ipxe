#ifndef DISK_H
#define DISK_H

#include "dev.h"

/*
 *	Structure returned from disk_probe and passed to other driver
 *	functions.
 */
struct disk
{
	struct dev    dev;  /* This must come first */
	int	      (*read)(struct disk *, sector_t sector);
	unsigned int  drive;
	unsigned long hw_sector_size;   /* The hardware sector size for dealing
				         * with partition tables and the like.
				         * Must be >= 512
				         */
	unsigned int  sectors_per_read; /* The number of 512 byte sectors
					 * returned by each read call. 
				         * All I/O must be aligned to this size.
				         */
	unsigned int  bytes;	        /* The number of bytes in the read buffer. */
	sector_t      sectors;	        /* The number of sectors on the drive.  */
	sector_t      sector;	        /* The first sector in the driver buffer  */
	unsigned char *buffer;	        /* The data read from the drive */
	void	      *priv;	        /* driver can hang private data here */

	unsigned long disk_offset;
	int           direction;
};

extern struct disk disk;
extern int url_file(const char *name,
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int));

extern int disk_probe(struct dev *dev);
extern int disk_load_configuration(struct dev *dev);
extern int disk_load(struct dev *dev);
extern void disk_disable(void);


#ifndef DOWNLOAD_PROTO_DISK
#define disk_disable()	do { } while(0)
#endif

#define SECTOR_SIZE 512
#define SECTOR_SHIFT 9

/* Maximum block_size that may be set. */
#define DISK_BUFFER_SIZE (18 * SECTOR_SIZE)

#endif /* DISK_H */
