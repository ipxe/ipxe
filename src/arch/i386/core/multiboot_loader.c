/* Multiboot support
 *
 * 2003-07-02 mmap fix and header probe by SONE Takeshi
 */

struct multiboot_mods {
	unsigned mod_start;
	unsigned mod_end;
	unsigned char *string;
	unsigned reserved;
};

struct multiboot_mmap {
	unsigned int size;
	unsigned int base_addr_low;
	unsigned int base_addr_high;
	unsigned int length_low;
	unsigned int length_high;
	unsigned int type;
};

/* The structure of a Multiboot 0.6 parameter block.  */
struct multiboot_info {
	unsigned int flags;
#define MULTIBOOT_MEM_VALID       0x01
#define MULTIBOOT_BOOT_DEV_VALID  0x02
#define MULTIBOOT_CMDLINE_VALID   0x04
#define MULTIBOOT_MODS_VALID      0x08
#define MULTIBOOT_AOUT_SYMS_VALID 0x10
#define MULTIBOOT_ELF_SYMS_VALID  0x20
#define MULTIBOOT_MMAP_VALID      0x40
	unsigned int memlower;
	unsigned int memupper;
	unsigned int bootdev;
	unsigned int cmdline;	/* physical address of the command line */
	unsigned mods_count;
	struct multiboot_mods *mods_addr;
	unsigned syms_num;
	unsigned syms_size;
	unsigned syms_addr;
	unsigned syms_shndx;
	unsigned mmap_length;
	unsigned  mmap_addr;
	/* The structure actually ends here, so I might as well put
	 * the ugly e820 parameters here...
	 */
	struct multiboot_mmap mmap[E820MAX];
};

/* Multiboot image header (minimal part) */
struct multiboot_header {
	unsigned int magic;
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
	unsigned int flags;
	unsigned int checksum;
};

static struct multiboot_header *mbheader;

static struct multiboot_info mbinfo;

static void multiboot_probe(unsigned char *data, int len)
{
    int offset;
    struct multiboot_header *h;

    /* Multiboot spec requires the header to be in first 8KB of the image */
    if (len > 8192)
	    len = 8192;

    for (offset = 0; offset < len; offset += 4) {
	    h = (struct multiboot_header *) (data + offset);
	    if (h->magic == MULTIBOOT_HEADER_MAGIC
			    && h->magic + h->flags + h->checksum == 0) {
		    printf("/Multiboot");
		    mbheader = h;
		    return;
	    }
    }
    mbheader = 0;
}

static inline void multiboot_boot(unsigned long entry)
{
	unsigned char cmdline[512], *c;
	int i;
	if (!mbheader)
		return;
	/* Etherboot limits the command line to the kernel name,
	 * default parameters and user prompted parameters.  All of
	 * them are shorter than 256 bytes.  As the kernel name and
	 * the default parameters come from the same BOOTP/DHCP entry
	 * (or if they don't, the parameters are empty), only two
	 * strings of the maximum size are possible.  Note this buffer
	 * can overrun if a stupid file name is chosen.  Oh well.  */
	c = cmdline;
	for (i = 0; KERNEL_BUF[i] != 0; i++) {
		switch (KERNEL_BUF[i]) {
		case ' ':
		case '\\':
		case '"':
			*c++ = '\\';
			break;
		default:
			break;
		}
		*c++ = KERNEL_BUF[i];
	}
	(void)sprintf(c, " -retaddr %#lX", virt_to_phys(xend32));

	mbinfo.flags = MULTIBOOT_MMAP_VALID | MULTIBOOT_MEM_VALID |MULTIBOOT_CMDLINE_VALID;
	mbinfo.memlower = meminfo.basememsize;
	mbinfo.memupper = meminfo.memsize;
	mbinfo.bootdev = 0;	/* not booted from disk */
	mbinfo.cmdline = virt_to_phys(cmdline);
	for (i = 0; i < (int) meminfo.map_count; i++) {
		mbinfo.mmap[i].size = sizeof(struct multiboot_mmap)
		    - sizeof(unsigned int);
		mbinfo.mmap[i].base_addr_low = 
		    (unsigned int) meminfo.map[i].addr;
		mbinfo.mmap[i].base_addr_high = 
		    (unsigned int) (meminfo.map[i].addr >> 32);
		mbinfo.mmap[i].length_low = 
		    (unsigned int) meminfo.map[i].size;
		mbinfo.mmap[i].length_high = 
		    (unsigned int) (meminfo.map[i].size >> 32);
		mbinfo.mmap[i].type = meminfo.map[i].type;
	}
	mbinfo.mmap_length = meminfo.map_count * sizeof(struct multiboot_mmap);
	mbinfo.mmap_addr = virt_to_phys(mbinfo.mmap);
	
	/* The Multiboot 0.6 spec requires all segment registers to be
	 * loaded with an unrestricted, writeable segment.
	 * xstart32 does this for us.
	 */
	
	/* Start the kernel, passing the Multiboot information record
	 * and the magic number.  */
	os_regs.eax = 0x2BADB002;
	os_regs.ebx = virt_to_phys(&mbinfo);
	xstart32(entry);
	longjmp(restart_etherboot, -2);
}
