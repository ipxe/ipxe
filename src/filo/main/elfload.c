/* ELF Boot loader
 * As we have seek, this implementation can be straightforward.
 * 2003-07 by SONE Takeshi
 */
#include <etherboot.h>
#include <elf.h>
#include <bits/elf_x.h>
#include <elf_boot.h>
#include <lib.h>
#include <sys_info.h>

#include <fs.h>
#define DEBUG_THIS DEBUG_ELFBOOT
#include <debug.h>

#if 1
//Use that in Etherboot
extern int elf_start(unsigned long __unused_i386, unsigned long entry, unsigned long param);
#define start_elf(x,y) elf_start(0, x, y)
#else
// original in filo
extern unsigned int start_elf(unsigned long entry_point, unsigned long param);
#endif

extern char _virt_start[], _end[];

static char *image_name, *image_version;

static int check_mem_ranges(struct sys_info *info,
	Elf_phdr *phdr, int phnum)
{
    int i, j;
    unsigned long start, end;
    unsigned long prog_start, prog_end;
#if 0
    struct memrange *mem;
#else 
    struct e820entry *mem;
#endif

    prog_start = virt_to_phys(&_virt_start);
    prog_end = virt_to_phys(&_end);

    for (i = 0; i < phnum; i++) {
	if (phdr[i].p_type != PT_LOAD)
	    continue;
	start = phdr[i].p_paddr;
	end = start + phdr[i].p_memsz;
	if (start < prog_start && end > prog_start)
	    goto conflict;
	if (start < prog_end && end > prog_end)
	    goto conflict;
#if 0
	for (j = 0; j < info->n_memranges; j++) {
	    mem = &info->memrange[j];
	    if (mem->base <= start && mem->base + mem->size >= end)
		break;
	}
	if (j >= info->n_memranges)
	    goto badseg;
#else
#define LB_MEM_RAM 1
        for (j = 0; j < meminfo.map_count; j++) {
            mem = &meminfo.map[j];
	    if (mem->type!=LB_MEM_RAM) continue;
            if (mem->addr <= start && mem->addr + mem->size >= end)
                break;
        }
        if (j >= meminfo.map_count)
            goto badseg;
#endif
    }
    return 1;

conflict:
    printf("%s occupies [%#lx-%#lx]\n", program_name, prog_start, prog_end);

badseg:
    printf("Segment %d [%#lx-%#lx] doesn't fit into memory\n", i, start, end-1);
    return 0;
}

static unsigned long process_image_notes(Elf_phdr *phdr, int phnum,
	unsigned short *sum_ptr)
{
    int i;
    char *buf = NULL;
    int retval = 0;
    unsigned long addr, end;
    Elf_Nhdr *nhdr;
    const char *name;
    void *desc;

    for (i = 0; i < phnum; i++) {
	if (phdr[i].p_type != PT_NOTE)
	    continue;
	buf = allot(phdr[i].p_filesz);
	file_seek(phdr[i].p_offset);
	if (file_read(buf, phdr[i].p_filesz) != phdr[i].p_filesz) {
	    printf("Can't read note segment\n");
	    goto out;
	}
	addr = (unsigned long) buf;
	end = addr + phdr[i].p_filesz;
	while (addr < end) {
	    nhdr = (Elf_Nhdr *) addr;
	    addr += sizeof(Elf_Nhdr);
	    name = (const char *) addr;
	    addr += (nhdr->n_namesz+3) & ~3;
	    desc = (void *) addr;
	    addr += (nhdr->n_descsz+3) & ~3;

	    if (nhdr->n_namesz==sizeof(ELF_NOTE_BOOT)
		    && memcmp(name, ELF_NOTE_BOOT, sizeof(ELF_NOTE_BOOT))==0) {
		if (nhdr->n_type == EIN_PROGRAM_NAME) {
		    image_name = calloc(1, nhdr->n_descsz + 1);
		    memcpy(image_name, desc, nhdr->n_descsz);
		}
		if (nhdr->n_type == EIN_PROGRAM_VERSION) {
		    image_version = calloc(1, nhdr->n_descsz + 1);
		    memcpy(image_version, desc, nhdr->n_descsz);
		}
		if (nhdr->n_type == EIN_PROGRAM_CHECKSUM) {
		    *sum_ptr = *(unsigned short *) desc;
		    debug("Image checksum: %04x\n", *sum_ptr);
		    /* Where in the file */
		    retval = phdr[i].p_offset
			+ (unsigned long) desc - (unsigned long) buf;
		}
	    }
	}
    }
out:
    if (buf)
	forget(buf);
    return retval;
}

static int load_segments(Elf_phdr *phdr, int phnum,
	unsigned long checksum_offset)
{
    unsigned long bytes;
    unsigned int start_time, time;
    int i;
    int j;

    bytes = 0;
    start_time = currticks();
#if 0
    for (j = 0; j < phnum; j++) {
        if (phdr[j].p_type != PT_LOAD) 
            continue;
        debug("0 segment %d addr:%#x file:%#x mem:%#x, phdr%#x\n",
                j, phdr[j].p_paddr, phdr[j].p_filesz, phdr[j].p_memsz, virt_to_phys(&phdr[j]));
    }
#endif

    for (i = 0; i < phnum; i++) {
	if (phdr[i].p_type != PT_LOAD)
	    continue;
	debug("segment %d addr:%#x file:%#x mem:%#x phdr:%#x ",
		i, phdr[i].p_paddr, phdr[i].p_filesz, phdr[i].p_memsz, virt_to_phys(&phdr[i]));
	file_seek(phdr[i].p_offset);
	debug("loading... ");
	if (file_read(phys_to_virt(phdr[i].p_paddr), phdr[i].p_filesz)
		!= phdr[i].p_filesz) {
	    printf("Can't read program segment %d\n", i);
	    return 0;
	}
	bytes += phdr[i].p_filesz;
	debug("clearing... ");
	memset(phys_to_virt(phdr[i].p_paddr + phdr[i].p_filesz), 0, 
		phdr[i].p_memsz - phdr[i].p_filesz);
	if (phdr[i].p_offset <= checksum_offset
		&& phdr[i].p_offset + phdr[i].p_filesz >= checksum_offset+2) {
	    debug("clearing checksum... ");
	    memset(phys_to_virt(phdr[i].p_paddr + checksum_offset
			- phdr[i].p_offset), 0, 2);
	}
	debug("ok\n");

    }
    time = (currticks() - start_time)*1000/18;
    printf("Loaded %d bytes in %dms (%dKB/s)\n", bytes, time,
	    time? bytes/time : 0);
    return 1;
}

static int verify_image(Elf_ehdr *ehdr, Elf_phdr *phdr, int phnum,
	unsigned short image_sum)
{
    unsigned short sum, part_sum;
    unsigned long offset;
    int i;

    sum = 0;
    offset = 0;

    part_sum = ipchksum(ehdr, sizeof *ehdr);
    sum = add_ipchksums(offset, sum, part_sum);
    offset += sizeof *ehdr;

    part_sum = ipchksum(phdr, phnum * sizeof(*phdr));
    sum = add_ipchksums(offset, sum, part_sum);
    offset += phnum * sizeof(*phdr);

    for (i = 0; i < phnum; i++) {
	if (phdr[i].p_type != PT_LOAD)
	    continue;
	part_sum = ipchksum(phys_to_virt(phdr[i].p_paddr), phdr[i].p_memsz);
	sum = add_ipchksums(offset, sum, part_sum);
	offset += phdr[i].p_memsz;
    }

    if (sum != image_sum) {
	printf("Verify FAILED (image:%04x vs computed:%04x)\n",
		image_sum, sum);
	return 0;
    }
    return 1;
}

static inline unsigned const padded(unsigned s)
{
    return (s + 3) & ~3;
}

static Elf_Bhdr *add_boot_note(Elf_Bhdr *bhdr, const char *name,
	unsigned type, const char *desc, unsigned descsz)
{
    Elf_Nhdr nhdr;
    unsigned ent_size, new_size, pad;
    char *addr;

    if (!bhdr)
	return NULL;

    nhdr.n_namesz = name? strlen(name)+1 : 0;
    nhdr.n_descsz = descsz;
    nhdr.n_type = type;
    ent_size = sizeof(nhdr) + padded(nhdr.n_namesz) + padded(nhdr.n_descsz);
    if (bhdr->b_size + ent_size > 0xffff) {
	printf("Boot notes too big\n");
	forget(bhdr);
	return NULL;
    }
    if (bhdr->b_size + ent_size > bhdr->b_checksum) {
	do {
	    new_size = bhdr->b_checksum * 2;
	} while (new_size < bhdr->b_size + ent_size);
	if (new_size > 0xffff)
	    new_size = 0xffff;
	debug("expanding boot note size to %u\n", new_size);
	bhdr = realloc(bhdr, new_size);
	bhdr->b_checksum = new_size;
    }

    addr = (char *) bhdr;
    addr += bhdr->b_size;
    memcpy(addr, &nhdr, sizeof(nhdr));
    addr += sizeof(nhdr);

    memcpy(addr, name, nhdr.n_namesz);
    addr += nhdr.n_namesz;
    pad = padded(nhdr.n_namesz) - nhdr.n_namesz;
    memset(addr, 0, pad);
    addr += pad;

    memcpy(addr, desc, nhdr.n_descsz);
    addr += nhdr.n_descsz;
    pad = padded(nhdr.n_descsz) - nhdr.n_descsz;
    memset(addr, 0, pad);
    addr += pad;

    bhdr->b_size += ent_size;
    bhdr->b_records++;
    return bhdr;
}

static inline Elf_Bhdr *add_note_string(Elf_Bhdr *bhdr, const char *name,
	unsigned type, const char *desc)
{
    return add_boot_note(bhdr, name, type, desc, strlen(desc) + 1);
}

static Elf_Bhdr *build_boot_notes(struct sys_info *info, const char *cmdline)
{
    Elf_Bhdr *bhdr;

    bhdr = allot(256);
    bhdr->b_signature = ELF_BHDR_MAGIC;
    bhdr->b_size = sizeof *bhdr;
    bhdr->b_checksum = 256; /* XXX cache the current buffer size here */
    bhdr->b_records = 0;

    if (info->firmware)
	bhdr = add_note_string(bhdr, NULL, EBN_FIRMWARE_TYPE, info->firmware);
    bhdr = add_note_string(bhdr, NULL, EBN_BOOTLOADER_NAME, program_name);
    bhdr = add_note_string(bhdr, NULL, EBN_BOOTLOADER_VERSION, program_version);
    if (cmdline)
	bhdr = add_note_string(bhdr, NULL, EBN_COMMAND_LINE, cmdline);
    if (!bhdr)
	return bhdr;
    bhdr->b_checksum = 0;
    bhdr->b_checksum = ipchksum(bhdr, bhdr->b_size);
    return bhdr;
}

int elf_load(struct sys_info *info, const char *filename, const char *cmdline)
{
    Elf_ehdr ehdr;
    Elf_phdr *phdr = NULL;
    unsigned long phdr_size;
    unsigned long checksum_offset;
    unsigned short checksum;
    Elf_Bhdr *boot_notes = NULL;
    int retval = -1;
    int image_retval;

    image_name = image_version = 0;

    if (!file_open(filename))
	goto out;

    if (file_read(&ehdr, sizeof ehdr) != sizeof ehdr) {
	debug("Can't read ELF header\n");
	retval = LOADER_NOT_SUPPORT;
	goto out;
    }

    if (ehdr.e_ident[EI_MAG0] != ELFMAG0
	    || ehdr.e_ident[EI_MAG1] != ELFMAG1
	    || ehdr.e_ident[EI_MAG2] != ELFMAG2
	    || ehdr.e_ident[EI_MAG3] != ELFMAG3
	    || ehdr.e_ident[EI_CLASS] != ARCH_ELF_CLASS
	    || ehdr.e_ident[EI_DATA] != ARCH_ELF_DATA
	    || ehdr.e_ident[EI_VERSION] != EV_CURRENT
	    || ehdr.e_type != ET_EXEC
	    || !ARCH_ELF_MACHINE_OK(ehdr.e_machine)
	    || ehdr.e_version != EV_CURRENT
	    || ehdr.e_phentsize != sizeof(Elf_phdr)) {
	debug("Not a bootable ELF image\n");
	retval = LOADER_NOT_SUPPORT;
	goto out;
    }

    phdr_size = ehdr.e_phnum * sizeof *phdr;
    phdr = allot(phdr_size);//hack LYH otherwise some one clear the last entry 
    file_seek(ehdr.e_phoff);
    if (file_read(phdr, phdr_size) != phdr_size) {
	printf("Can't read program header\n");
	goto out;
    }

    if (!check_mem_ranges(info, phdr, ehdr.e_phnum))
	goto out;

    checksum_offset = process_image_notes(phdr, ehdr.e_phnum, &checksum);

    printf("Loading %s", image_name ? image_name : "image");
    if (image_version)
	printf(" version %s", image_version);
    printf("...\n");

    if (!load_segments(phdr, ehdr.e_phnum, checksum_offset))
	goto out;

    if (checksum_offset) {
	if (!verify_image(&ehdr, phdr, ehdr.e_phnum, checksum))
	    goto out;
    }

    boot_notes = build_boot_notes(info, cmdline);

    debug("current time: %x\n", currticks());

    debug("entry point is %#x\n", ehdr.e_entry);
    printf("Jumping to entry point...\n");

    image_retval = start_elf(ehdr.e_entry, virt_to_phys(boot_notes));
#if 0
    console_init();
#endif

    printf("Image returned with return value %#x\n", image_retval);
    retval = 0;

out:
    if (phdr)
	forget(phdr);
    if (boot_notes)
	forget(boot_notes);
    if (image_name)
	forget(image_name);
    if (image_version)
	forget(image_version);
    return retval;
}
