#ifdef LINUXBIOS

#include "etherboot.h"
#include "dev.h"
#include "linuxbios_tables.h"

struct meminfo meminfo;
static int lb_failsafe = 1;
static unsigned lb_boot[MAX_BOOT_ENTRIES];
static unsigned lb_boot_index;
static struct cmos_entries lb_countdown;
static struct cmos_checksum lb_checksum;

#undef DEBUG_LINUXBIOS

static void set_base_mem_k(struct meminfo *info, unsigned mem_k)
{
	if ((mem_k <= 640) && (info->basememsize <= mem_k)) {
		info->basememsize = mem_k;
	}
}
static void set_high_mem_k(struct meminfo *info, unsigned mem_k)
{
	/* Shave off a megabyte before playing */
	if (mem_k < 1024) {
		return;
	}
	mem_k -= 1024;
	if (info->memsize <= mem_k) {
		info->memsize = mem_k;
	}
}

#define for_each_lbrec(head, rec) \
	for(rec = (struct lb_record *)(((char *)head) + sizeof(*head)); \
		(((char *)rec) < (((char *)head) + sizeof(*head) + head->table_bytes))  && \
		(rec->size >= 1) && \
		((((char *)rec) + rec->size) <= (((char *)head) + sizeof(*head) + head->table_bytes)); \
		rec = (struct lb_record *)(((char *)rec) + rec->size)) 
		

#define for_each_crec(tbl, rec) \
	for(rec = (struct lb_record *)(((char *)tbl) + tbl->header_length); \
		(((char *)rec) < (((char *)tbl) + tbl->size))  && \
		(rec->size >= 1) && \
		((((char *)rec) + rec->size) <= (((char *)tbl) + tbl->size)); \
		rec = (struct lb_record *)(((char *)rec) + rec->size)) 
		


static void read_lb_memory(
	struct meminfo *info, struct lb_memory *mem)
{
	int i;
	int entries;
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	for(i = 0; (i < entries); i++) {
		if (info->map_count < E820MAX) {
			info->map[info->map_count].addr = mem->map[i].start;
			info->map[info->map_count].size = mem->map[i].size;
			info->map[info->map_count].type = mem->map[i].type;
			info->map_count++;
		}
		switch(mem->map[i].type) {
		case LB_MEM_RAM:
		{
			unsigned long long end;
			unsigned long mem_k;
			end = mem->map[i].start + mem->map[i].size;
#if defined(DEBUG_LINUXBIOS)
			printf("lb: %X%X - %X%X (ram)\n",
				(unsigned long)(mem->map[i].start >>32), 
				(unsigned long)(mem->map[i].start & 0xFFFFFFFF), 
				(unsigned long)(end >> 32), 
				(unsigned long)(end & 0xFFFFFFFF));
#endif /* DEBUG_LINUXBIOS */
			end >>= 10;
			mem_k = end;
			if (end & 0xFFFFFFFF00000000ULL) {
				mem_k = 0xFFFFFFFF;
			}
			set_base_mem_k(info, mem_k);
			set_high_mem_k(info, mem_k);
			break;
		}
		case LB_MEM_RESERVED:
		default:
#if defined(DEBUG_LINUXBIOS)
		{
			unsigned long long end;
			end = mem->map[i].start + mem->map[i].size;
			printf("lb: %X%X - %X%X (reserved)\n",
				(unsigned long)(mem->map[i].start >>32), 
				(unsigned long)(mem->map[i].start & 0xFFFFFFFF), 
				(unsigned long)(end >> 32), 
				(unsigned long)(end & 0xFFFFFFFF));
		}
#endif /* DEBUG_LINUXBIOS */
			break;
		}
	}
}

static unsigned cmos_read(unsigned offset, unsigned int size)
{
	unsigned addr, old_addr;
	unsigned value;
	
	addr = offset/8;

	old_addr = inb(0x70);
	outb(addr | (old_addr &0x80), 0x70);
	value = inb(0x71);
	outb(old_addr, 0x70);

	value >>= offset & 0x7;
	value &= ((1 << size) - 1);
	
	return value;
}

static unsigned cmos_read_checksum(void)
{
	unsigned sum = 
		(cmos_read(lb_checksum.location, 8) << 8) |
		cmos_read(lb_checksum.location +8, 8);
	return sum & 0xffff;
}

static int cmos_valid(void)
{
	unsigned i;
	unsigned sum, old_sum;
	sum = 0;
	if ((lb_checksum.tag != LB_TAG_OPTION_CHECKSUM) || 
		(lb_checksum.type != CHECKSUM_PCBIOS) ||
		(lb_checksum.size != sizeof(lb_checksum))) {
		return 0;
	}
	for(i = lb_checksum.range_start; i <= lb_checksum.range_end; i+= 8) {
		sum += cmos_read(i, 8);
	}
	sum = (~sum)&0x0ffff;
	old_sum = cmos_read_checksum();
	return sum == old_sum;
}

static void cmos_write(unsigned offset, unsigned int size, unsigned setting)
{
	unsigned addr, old_addr;
	unsigned value, mask, shift;
	unsigned sum;
	
	addr = offset/8;
	
	shift = offset & 0x7;
	mask = ((1 << size) - 1) << shift;
	setting = (setting << shift) & mask;

	old_addr = inb(0x70);
	sum = cmos_read_checksum();
	sum = (~sum) & 0xffff;

	outb(addr | (old_addr &0x80), 0x70);
	value = inb(0x71);
	sum -= value;
	value &= ~mask;
	value |= setting;
	sum += value;
	outb(value, 0x71);

	sum = (~sum) & 0x0ffff;
	outb((lb_checksum.location/8) | (old_addr & 0x80), 0x70);
	outb((sum >> 8) & 0xff, 0x71);
	outb(((lb_checksum.location +8)/8) | (old_addr & 0x80), 0x70);
	outb(sum & 0xff, 0x71);

	outb(old_addr, 0x70);

	return;
}

static void read_linuxbios_values(struct meminfo *info,
	struct lb_header *head)
{
	/* Read linuxbios tables... */
	struct lb_record *rec;
	memset(lb_boot, 0, sizeof(lb_boot));
	for_each_lbrec(head, rec) {
		switch(rec->tag) {
		case LB_TAG_MEMORY:
		{
			struct lb_memory *mem;
			mem = (struct lb_memory *) rec;
			read_lb_memory(info, mem); 
			break;
		}
		case LB_TAG_CMOS_OPTION_TABLE:
		{
			struct cmos_option_table *tbl;
			struct lb_record *crec;
			struct cmos_entries *entry;
			tbl = (struct cmos_option_table *)rec;
			for_each_crec(tbl, crec) {
				/* Pick off the checksum entry and keep it */
				if (crec->tag == LB_TAG_OPTION_CHECKSUM) {
					memcpy(&lb_checksum, crec, sizeof(lb_checksum));
					continue;
				}
				if (crec->tag != LB_TAG_OPTION)
					continue;
				entry = (struct cmos_entries *)crec;
				if ((entry->bit < 112) || (entry->bit > 1020))
					continue;
				/* See if LinuxBIOS came up in fallback or normal mode */
				if (memcmp(entry->name, "last_boot", 10) == 0) {
					lb_failsafe = cmos_read(entry->bit, entry->length) == 0;
					continue;
				}
				/* Find where the boot countdown is */
				if (memcmp(entry->name, "boot_countdown", 15) == 0) {
					lb_countdown =  *entry;
					continue;
				}
				/* Find the default boot index */
				if (memcmp(entry->name, "boot_index", 11) == 0) {
					lb_boot_index = cmos_read(entry->bit, entry->length);
					continue;
				}
				/* Now filter for the boot order options */
				if (entry->length != 4)
					continue;
				if (entry->config != 'e')
					continue;
				if (memcmp(entry->name, "boot_first", 11) == 0) {
					lb_boot[0] = cmos_read(entry->bit, entry->length);
				}
				else if (memcmp(entry->name, "boot_second", 12) == 0) {
					lb_boot[1] = cmos_read(entry->bit, entry->length);
				}
				else if (memcmp(entry->name, "boot_third", 11) == 0) {
					lb_boot[2] = cmos_read(entry->bit, entry->length);
				}
			}
			break;
		}
		default: 
			break;
		};
	}
}



static unsigned long count_lb_records(void *start, unsigned long length)
{
	struct lb_record *rec;
	void *end;
	unsigned long count;
	count = 0;
	end = ((char *)start) + length;
	for(rec = start; ((void *)rec < end) &&
		((signed long)rec->size <= (end - (void *)rec)); 
		rec = (void *)(((char *)rec) + rec->size)) {
		count++;
	}
	return count;
}

static int find_lb_table(void *start, void *end, struct lb_header **result)
{
	unsigned char *ptr;

	/* For now be stupid.... */
	for(ptr = start; virt_to_phys(ptr) < virt_to_phys(end); ptr += 16) {
		struct lb_header *head = (struct lb_header *)ptr;
		if (	(head->signature[0] != 'L') || 
			(head->signature[1] != 'B') ||
			(head->signature[2] != 'I') ||
			(head->signature[3] != 'O')) {
			continue;
		}
		if (head->header_bytes != sizeof(*head))
			continue;
#if defined(DEBUG_LINUXBIOS)
		printf("Found canidate at: %X\n", virt_to_phys(head));
#endif
		if (ipchksum((uint16_t *)head, sizeof(*head)) != 0) 
			continue;
#if defined(DEBUG_LINUXBIOS)
		printf("header checksum o.k.\n");
#endif
		if (ipchksum((uint16_t *)(ptr + sizeof(*head)), head->table_bytes) !=
			head->table_checksum) {
			continue;
		}
#if defined(DEBUG_LINUXBIOS)
		printf("table checksum o.k.\n");
#endif
		if (count_lb_records(ptr + sizeof(*head), head->table_bytes) !=
			head->table_entries) {
			continue;
		}
#if defined(DEBUG_LINUXBIOS)
		printf("record count o.k.\n");
#endif
		*result = head;
		return 1;
	};
	return 0;
}

void get_memsizes(void)
{
	struct lb_header *lb_table;
	int found;
#if defined(DEBUG_LINUXBIOS)
	printf("\nSearching for linuxbios tables...\n");
#endif /* DEBUG_LINUXBIOS */
	found = 0;
	meminfo.basememsize = 0;
	meminfo.memsize = 0;
	meminfo.map_count = 0;
	/* This code is specific to linuxBIOS but could
	 * concievably be extended to work under a normal bios.
	 * but size is important...
	 */
	if (!found) {
		found = find_lb_table(phys_to_virt(0x00000), phys_to_virt(0x01000), &lb_table);
	}
	if (!found) {
		found = find_lb_table(phys_to_virt(0xf0000), phys_to_virt(0x100000), &lb_table);
	}
	if (found) {
#if defined (DEBUG_LINUXBIOS)
		printf("Found LinuxBIOS table at: %X\n", virt_to_phys(lb_table));
#endif
		read_linuxbios_values(&meminfo, lb_table);
	}

#if defined(DEBUG_LINUXBIOS)
	printf("base_mem_k = %d high_mem_k = %d\n", 
		meminfo.basememsize, meminfo.memsize);
#endif /* DEBUG_LINUXBIOS */
	
}

unsigned long get_boot_order(unsigned long order, unsigned *index)
{
	static int again;
	static int checksum_valid;
	static unsigned boot_count;
	int i;

	if (!lb_failsafe && !again) {
		/* Decrement the boot countdown the first time through */
		checksum_valid = cmos_valid();
		boot_count = cmos_read(lb_countdown.bit, lb_countdown.length);
		if (boot_count > 0) {
			cmos_write(lb_countdown.bit, lb_countdown.length, boot_count -1);
		}
		again = 1;
	}
	if (lb_failsafe || !checksum_valid) {
		/* When LinuxBIOS is in failsafe mode, or there is an
		 * invalid cmos checksum ignore all cmos options 
		 */
		return order;
	}
	for(i = 0; i < MAX_BOOT_ENTRIES; i++) {
		unsigned long boot;
		boot = order >> (i*BOOT_BITS) & BOOT_MASK;
		boot = lb_boot[i] & BOOT_TYPE_MASK;
		if (boot >= BOOT_NOTHING) {
			boot = BOOT_NOTHING;
		}
		if (boot_count == 0) {
			boot |= BOOT_FAILSAFE;
		}
		order &= ~(BOOT_MASK << (i * BOOT_BITS));
		order |= (boot << (i*BOOT_BITS));
	}
	*index = lb_boot_index;
	return order;
}
#endif /* LINUXBIOS */
