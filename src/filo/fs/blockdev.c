#include <etherboot.h>

#include <lib.h>
#include <fs.h>

#define DEBUG_THIS DEBUG_BLOCKDEV
#include <debug.h>

#define NUM_CACHE 64
static unsigned char buf_cache[NUM_CACHE][512];
static unsigned long cache_sect[NUM_CACHE];

static char dev_name[256];

int dev_type = -1;
int dev_drive = -1;
unsigned long part_start;
unsigned long part_length;
int using_devsize;

static inline int has_pc_part_magic(unsigned char *sect)
{
    return sect[510]==0x55 && sect[511]==0xAA;
}

static inline int is_pc_extended_part(unsigned char type)
{
    return type==5 || type==0xf || type==0x85;
}

/* IBM-PC/MS-DOS style partitioning scheme */
static int open_pc_partition(int part, unsigned long *start_p,
	unsigned long *length_p)
{
    /* Layout of PC partition table */
    struct pc_partition {
	unsigned char boot;
	unsigned char head;
	unsigned char sector;
	unsigned char cyl;
	unsigned char type;
	unsigned char e_head;
	unsigned char e_sector;
	unsigned char e_cyl;
	unsigned char start_sect[4]; /* unaligned little endian */
	unsigned char nr_sects[4]; /* ditto */
    } *p;
    unsigned char buf[512];

    /* PC partition probe */
    if (!devread(0, 0, sizeof(buf), buf)) {
	debug("device read failed\n");
	return 0;
    }
    if (!has_pc_part_magic(buf)) {
	debug("pc partition magic number not found\n");
	//debug_hexdump(buf, 512);
	return PARTITION_UNKNOWN;
    }
    p = (struct pc_partition *) (buf + 0x1be);
    if (part < 4) {
	/* Primary partition */
	p += part;
	if (p->type==0 || is_pc_extended_part(p->type)) {
	    printf("Partition %d does not exist\n", part+1);
	    return 0;
	}
	*start_p = get_le32(p->start_sect);
	*length_p = get_le32(p->nr_sects);
	return 1;
    } else {
	/* Extended partition */
	int i;
	int cur_part;
	unsigned long ext_start, cur_table;
	/* Search for the extended partition
	 * which contains logical partitions */
	for (i = 0; i < 4; i++) {
	    if (is_pc_extended_part(p[i].type))
		break;
	}
	if (i >= 4) {
	    printf("Extended partition not found\n");
	    return 0;
	}
	debug("Extended partition at %d\n", i+1);
	/* Visit each logical partition labels */
	ext_start = get_le32(p[i].start_sect);
	cur_table = ext_start;
	cur_part = 4;
	for (;;) {
	    debug("cur_part=%d at %lu\n", cur_part, cur_table);
	    if (!devread(cur_table, 0, sizeof(buf), buf))
		return 0;
	    if (!has_pc_part_magic(buf)) {
		debug("no magic\n");
		break;
	    }

	    p = (struct pc_partition *) (buf + 0x1be);
	    /* First entry is the logical partition */
	    if (cur_part == part) {
		if (p->type==0) {
		    printf("Partition %d is empty\n", part+1);
		    return 0;
		}
		*start_p = cur_table + get_le32(p->start_sect);
		*length_p = get_le32(p->nr_sects);
		return 1;
	    }
	    /* Second entry is link to next partition */
	    if (!is_pc_extended_part(p[1].type)) {
		debug("no link\n");
		break;
	    }
	    cur_table = ext_start + get_le32(p[1].start_sect);

	    cur_part++;
	}
	printf("Logical partition %d not exist\n", part+1);
	return 0;
    }
}

static void flush_cache(void)
{
    int i;
    for (i = 0; i < NUM_CACHE; i++)
	cache_sect[i] = (unsigned long) -1;
}

static int parse_device_name(const char *name, int *type, int *drive,
	int *part, uint64_t *offset, uint64_t *length)
{
    *offset = *length = 0;

    if (memcmp(name, "hd", 2) == 0) {
	*type = DISK_IDE;
	name += 2;
	if (*name < 'a' || *name > 'z') {
	    printf("Invalid drive\n");
	    return 0;
	}
	*drive = *name - 'a';
	name++;
    } else if (memcmp(name, "mem", 3) == 0) {
	*type = DISK_MEM;
	name += 3;
	*drive = 0;
    } else if (memcmp(name, "ud", 2) == 0) {
        *type = DISK_USB;
        name += 2;
        if (*name < 'a' || *name > 'z') {
            printf("Invalid drive\n");
            return 0; 
        }
        *drive = *name - 'a';
        name++;
    } else {
	printf("Unknown device type\n");
	return 0;
    }

    *part = (int) simple_strtoull(name, (char **)&name, 0);

    if (*name == '@') {
	name++;
	*offset = strtoull_with_suffix(name, (char **)&name, 0);
	if (*name == ',')
	    *length = strtoull_with_suffix(name+1, (char **)&name, 0);
//	debug("offset=%#Lx length=%#Lx\n", *offset, *length);
    }

    if (*name != '\0') {
	printf("Can't parse device name\n");
	return 0;
    }

    return 1;
}

int devopen(const char *name, int *reopen)
{
    int type, drive, part;
    uint64_t offset, length;
    uint32_t disk_size = 0;

    /* Don't re-open the device that's already open */
    if (strcmp(name, dev_name) == 0) {
	debug("already open\n");
	*reopen = 1;
	return 1;
    }
    *reopen = 0;

    if (!parse_device_name(name, &type, &drive, &part, &offset, &length)) {
	debug("failed to parse device name: %s\n", name);
	return 0;
    }

    /* Do simple sanity check first */
    if (offset & 0x1ff) {
	printf("Device offset must be multiple of 512\n");
	return 0;
    }
    if (length & 0x1ff) {
	debugx("WARNING: length is rounded up to multiple of 512\n");
	length = (length + 0x1ff) & ~0x1ff;
    }

    switch (type) {
#ifdef IDE_DISK
    case DISK_IDE:
	if (ide_probe(drive) != 0) {
	    debug("failed to open ide\n");
	    return 0;
	}
	disk_size = (uint32_t) -1; /* FIXME */
	break;
#endif
    case DISK_MEM:
	disk_size = 1 << (32 - 9); /* 4GB/512-byte */
	break;
#ifdef USB_DISK
    case DISK_USB:
        if (usb_probe(drive) != 0) {
            debug("failed to open usb\n");
            return 0;
        }
        disk_size = (uint32_t) -1; /* FIXME */
        break;
#endif
    default:
	printf("Unknown device type %d\n", type);
	return 0;
    }

    if (dev_type != type || dev_drive != drive)
	flush_cache();

    /* start with whole disk */
    dev_type = type;
    dev_drive = drive;
    part_start = 0;
    part_length = disk_size;
    using_devsize = 1;

    if (part != 0) {
	/* partition is specified */
	int ret;
	ret = open_pc_partition(part - 1, &part_start, &part_length);
	if (ret == PARTITION_UNKNOWN) {
	    ret = open_eltorito_image(part - 1, &part_start, &part_length);
	    if (ret == PARTITION_UNKNOWN) {
		printf("Unrecognized partitioning scheme\n");
		return 0;
	    }
	}
	if (ret == 0) {
	    debug("can't open partition %d\n", part);
	    return 0;
	}

	debug("Partition %d start %lu length %lu\n", part,
		part_start, part_length);
    }

    if (offset) {
	if (offset >= (uint64_t) part_length << 9) {
	    printf("Device offset is too high\n");
	    return 0;
	}
	part_start += offset >> 9;
	part_length -= offset >> 9;
	debug("after offset: start %lu, length %lu\n", part_start, part_length);
    }

    if (length) {
	if (length > (uint64_t) part_length << 9) {
	    printf("Specified length exceeds the size of device\n");
	    return 0;
	}
	part_length = length >> 9;
	debug("after length: length %lu\n", part_length);
	using_devsize = 0;
    }

    strncpy(dev_name, name, sizeof(dev_name)-1);

    return 1;
}

/* Read a sector from opened device with simple/stupid buffer cache */
static void *read_sector(unsigned long sector)
{
    unsigned int hash;
    void *buf;
    int i;

    /* If reading memory, just return the memory as the buffer */
    if (dev_type == DISK_MEM) {
	unsigned long phys = sector << 9;
	//debug("mem: %#lx\n", phys);
	return phys_to_virt(phys);
    }

    /* Search in the cache */
    hash = sector % NUM_CACHE;
    buf = buf_cache[hash];
    if (cache_sect[hash] != sector) {
	cache_sect[hash] = (unsigned long) -1;
	switch (dev_type) {
#ifdef IDE_DISK
	case DISK_IDE:
	    if (ide_read(dev_drive, sector, buf) != 0)
		goto readerr;
	    break;
#endif
#ifdef USB_DISK
        case DISK_USB:
            if (usb_read(dev_drive, sector, buf) != 0)
                goto readerr;
            break;
#endif
	default:
	    printf("read_sector: device not open\n");
	    return 0;
	}
	cache_sect[hash] = sector;
    }
#if 0	
    printf("in read_sector:\n");
    for(i=0;i<128;i++) {
             if((i%4)==0) printf("\n %08x:",i*4);
             printf(" %08x ",(uint32_t)*((uint32_t *)buf+i));
       }
#endif

    return buf;

readerr:
    printf("Disk read error dev_type=%d drive=%d sector=%x\n",
	    dev_type, dev_drive, sector);
    dev_name[0] = '\0'; /* force re-open the device next time */
    return 0;
}

int devread(unsigned long sector, unsigned long byte_offset,
	unsigned long byte_len, void *buf)
{
    char *sector_buffer;
    char *dest = buf;
    unsigned long len;
    int i;

    sector += byte_offset >> 9;
    byte_offset &= 0x1ff;

    if (sector + ((byte_len + 0x1ff) >> 9) > part_length) {
	printf("Attempt to read out of device/partition\n");
	debug("sector=%x part_length=%x byte_len=%x\n",
		sector, part_length, byte_len);
	return 0;
    }

    while (byte_len > 0) {
	sector_buffer = read_sector(part_start + sector);
	if (!sector_buffer) {
	    debug("read sector failed\n");
	    return 0;
	}
	len = 512 - byte_offset;
	if (len > byte_len)
	    len = byte_len;
	memcpy(dest, sector_buffer + byte_offset, len);
	sector++;
	byte_offset = 0;
	byte_len -= len;
	dest += len;
    }
	
    return 1;
}
