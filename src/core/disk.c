#include "etherboot.h"
#include "disk.h"

#undef disk_disable

static int dummy(void *unused __unused)
{
	return (0);
}

static unsigned char disk_buffer[DISK_BUFFER_SIZE];
struct disk disk =
{
	{
		0,				/* dev.disable */
		{
			0,
			0,
			PCI_BUS_TYPE,
		},				/* dev.devid */
		0,				/* index */
		0,				/* type */
		PROBE_FIRST,			/* how_probe */
		PROBE_NONE,			/* to_probe */
		0,				/* failsafe */
		0,				/* type_index */
		{},				/* state */
	},
	(int (*)(struct disk *, sector_t ))dummy,		/* read */
	0 - 1, 		/* drive */
	0,		/* hw_sector_size */
	0,		/* sectors_per_read */
	0,		/* bytes */
	0,		/* sectors */
	0, 		/* sector */
	disk_buffer,	/* buffer */
	0,		/* priv */
	
	0,		/* disk_offset */
	0,		/* direction */
};


static int disk_read(
	struct disk *disk, unsigned char *buffer, sector_t sector)
{
	int result;
	sector_t base_sector;

	/* Note: I do not handle disk wrap around here! */

	/* Compute the start of the track cache */
	base_sector = sector;
	/* Support sectors_per_read > 1 only on small disks */
	if ((sizeof(sector_t) > sizeof(unsigned long)) &&
		(disk->sectors_per_read > 1)) {
		unsigned long offset;
		offset = ((unsigned long)sector) % disk->sectors_per_read;
		base_sector -= offset;
	}

	/* See if I need to update the track cache */
	if ((sector < disk->sector) ||
		sector >= disk->sector + (disk->bytes >> 9)) {
		twiddle();
		result = disk->read(disk, base_sector);
		if (result < 0)
			return result;
	}
	/* Service the request from the track cache */
	memcpy(buffer, disk->buffer + ((sector - base_sector)<<9), SECTOR_SIZE);
	return 0;
}
	
static int disk_read_sectors(
	struct disk *disk,
	unsigned char *buffer,
	sector_t base_sector, unsigned int sectors)
{
	sector_t sector = 0;
	unsigned long offset;
	int result = 0;

	for(offset = 0; offset < sectors; offset++) {
		sector = base_sector + offset;
		if (sector >= disk->sectors) {
			sector -= disk->sectors;
		}
		result = disk_read(disk, buffer + (offset << 9), sector);
		if (result < 0)
			break;
	}
	if (result < 0) {
		printf("disk read error at 0x%lx\n", sector);
	}
	return result;
}

static os_download_t probe_buffer(unsigned char *buffer, unsigned int len,
	int increment, unsigned int offset, unsigned int *roffset)
{
	os_download_t os_download;
	unsigned int end;
	end = 0;
	os_download = 0;
	if (increment > 0) {
		end = len - SECTOR_SIZE;
	}
	do {
		offset += increment;
		os_download = probe_image(buffer + offset, len - offset);
	} while(!os_download && (offset != end));
	*roffset = offset;
	return os_download;
}

static int load_image(
	struct disk *disk, 
	unsigned char *buffer, unsigned int buf_sectors,
	sector_t block, unsigned int offset,
	os_download_t os_download)
{
	sector_t skip_sectors;

	skip_sectors = 0;
	while(1) {
		skip_sectors = os_download(buffer + offset, 
			(buf_sectors << 9) - offset, 0);

		block += skip_sectors + buf_sectors;
		if (block >= disk->sectors) {
			block -= disk->sectors;
		}

		offset = 0;
		buf_sectors = 1;
		if (disk_read_sectors(disk, buffer, block, 1) < 0) {
			return 0;
		}
	}
	return -1;
}

int disk_probe(struct dev *dev)
{
	struct disk *disk = (struct disk *)dev;
	if (dev->how_probe == PROBE_NEXT) {
		disk->drive += 1;
	}
	return probe(dev);
}


int disk_load_configuration(struct dev *dev)
{
	/* Start with the very simplest possible disk configuration */
	struct disk *disk = (struct disk *)dev;
	disk->direction = (dev->failsafe)?-1:1;
	disk->disk_offset = 0;
	return 0;
}

int disk_load(struct dev *dev)
{
	struct disk *disk = (struct disk *)dev;
	/* 16K == 8K in either direction from the start of the disk */
	static unsigned char buffer[32*SECTOR_SIZE]; 
	os_download_t os_download;
	unsigned int offset;
	unsigned int len;
	unsigned int buf_sectors;
	volatile sector_t block;
	volatile int inc, increment;
	int i;
	int result;
	jmp_buf real_restart;


	printf("Searching for image...\n");
	result = 0;
	/* Only check for 16byte aligned images */
	increment = (disk->direction < 0)?-16:16;
	/* Load a buffer, and see if it contains the start of an image
	 * we can boot from disk.
	 */
	len = sizeof(buffer);
	buf_sectors = sizeof(buffer) / SECTOR_SIZE;
	inc = increment;
	block = (disk->disk_offset) >> 9;
	if (buf_sectors/2 > block) {
		block = (disk->sectors - (buf_sectors/2)) + block;
	}
	/* let probe buffer assume offset always needs to be incremented */
	offset = (len/2 + ((disk->disk_offset) & 0x1ff)) - inc;

	/* Catch longjmp so if this image fails to load, I start looking
	 * for the next image where I left off looking for this image.
	 */
	memcpy(&real_restart, &restart_etherboot, sizeof(jmp_buf));
	i = setjmp(restart_etherboot);
	if ((i != 0) && (i != -2)) {
		memcpy(&restart_etherboot, &real_restart, sizeof(jmp_buf));
		longjmp(restart_etherboot, i);
	}
	/* Read the canidate sectors into the buffer */
	if (disk_read_sectors(disk, buffer, block, buf_sectors) < 0) {
		result = -1;
		goto out;
	}
	if (inc == increment) {
		os_download = probe_buffer(buffer, len, inc, offset, &offset);
		if (os_download)
			goto load_image;
		inc = -inc;
	}
	os_download = probe_buffer(buffer, len, inc, offset, &offset);
	if (!os_download) {
		result = -1;
		goto out;
	}
 load_image:
	printf("Loading image...\n");
	result = load_image(disk, buffer, buf_sectors, block, offset, os_download);
 out:
	memcpy(&restart_etherboot, &real_restart, sizeof(jmp_buf));
	return result;
}

int url_file(const char *name,
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int) __unused)
{
	unsigned int drive;
	unsigned long  disk_offset;
	int direction;
	int type;

	disk_offset = 0;
	direction = 1;
	if (memcmp(name, "disk", 4) == 0) {
		type = DISK_DRIVER;
		name += 4;
	}
	else if (memcmp(name, "floppy", 6) == 0) {
		type = FLOPPY_DRIVER;
		name += 6;
	}
	else {
		printf("Unknown device type\n");
		return 0;
	}
	drive = strtoul(name, &name, 10);
	if ((name[0] == '+') || (name[0] == '-')) {
		direction = (name[0] == '-')? -1 : 1;
		name++;
		disk_offset = strtoul(name, &name, 10);
	}
	if (name[0]) {
		printf("Junk '%s' at end of disk url\n", name);
		return 0;
	}
	memset(&disk, 0, sizeof(disk));
	disk.buffer = disk_buffer;
	disk.drive = 0;
	disk.dev.how_probe = PROBE_FIRST;
	disk.dev.type = type;
	do {
		disk_disable();
		disk.dev.how_probe = disk_probe(&disk.dev);
		if (disk.dev.how_probe == PROBE_FAILED) {
			printf("Not that many drives\n");
			return 0;
		}
	} while(disk.drive < drive);
	disk.direction = direction;
	disk.disk_offset = disk_offset;
	
	return disk_load(&disk.dev);
}

void disk_disable(void)
{
	disable(&disk.dev);
}
