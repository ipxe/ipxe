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
static unsigned int mbimgoffset, mboffset;
static unsigned char mbbuffer[12];

static struct multiboot_info mbinfo;

static void multiboot_init(void)
{
	mbheader = NULL;
	mbimgoffset = 0;
	mboffset = 0;
}

/* Remember this probing function is actually different from the usual probing
 * functions, since the Multiboot header is somewhere in the first 8KB of the
 * image and it is byte aligned, but there is not much more known about how to
 * find it.  In the Etherboot context the most complicated issue is that the
 * image has to be processed block-by-block, with unknown block size and no
 * guarantees about block alignment with respect to the image.  */
static void multiboot_peek(unsigned char *data, int len)
{
    struct multiboot_header *h;

	/* If we have already searched the first 8KB of the image or if we have
	 * already found a valid Multiboot header, skip this code.  */
    if ((mboffset == 12) || (mbimgoffset >= 8192))
		return;

	if (mbimgoffset + len >= 8192)
	    len = 8192 - mbimgoffset;

	/* This piece of code is pretty stupid, since it always copies data, even
	 * if it is word aligned.  This shouldn't matter too much on platforms that
	 * use the Multiboot spec, since the processors are usually reasonably fast
	 * and this code is only executed for the first 8KB of the image.  Feel
	 * free to improve it, but be prepared to write quite a lot of code that
	 * deals with non-aligned data with respect to the image to load.  */
	while (len > 0) {
		mbimgoffset++;
		memcpy(mbbuffer + mboffset, data, 1);
		mboffset++;
		data++;
		len--;
		if (mboffset == 4) {
			/* Accumulated a word into the buffer.  */
			h = (struct multiboot_header *)mbbuffer;
			if (h->magic != MULTIBOOT_HEADER_MAGIC) {
				/* Wrong magic, this cannot be the start of the header.  */
				mboffset = 0;
			}
		} else if (mboffset == 12) {
			/* Accumulated the minimum header data into the buffer.  */
			h = (struct multiboot_header *)mbbuffer;
			if (h->magic + h->flags + h->checksum != 0) {
				/* Checksum error, not a valid header.  Check for a possible
				 * header starting in the current flag/checksum field.  */
				if (h->flags == MULTIBOOT_HEADER_MAGIC) {
					mboffset -= 4;
					memmove(mbbuffer, mbbuffer + 4, mboffset);
				} else if (h->checksum == MULTIBOOT_HEADER_MAGIC) {
					mboffset -= 8;
					memmove(mbbuffer, mbbuffer + 8, mboffset);
				} else {
					mboffset = 0;
				}
			} else {
			    printf("Multiboot... ");
			    mbheader = h;
				if ((h->flags & 0xfffc) != 0) {
					printf("\nERROR: Unsupported Multiboot requirements flags\n");
					longjmp(restart_etherboot, -2);
				}
				break;
			}
		}
	}
	mbimgoffset += len;
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
	for (i = 0; KERNEL_BUF[i] != '\0'; i++) {
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
	if (addparam != NULL) {
		*c++ = ' ';
		memcpy(c, addparam, addparamlen);
		c += addparamlen;
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
	/* A Multiboot kernel by default never returns - there is nothing in the
	 * specification about what happens to the boot loader after the kernel has
	 * been started.  Thus if the kernel returns it is definitely aware of the
	 * semantics involved (i.e. the "-retaddr" parameter).  Do not treat this
	 * as an error, but restart with a fresh DHCP request in order to activate
	 * the menu again in case one is used.  */
	longjmp(restart_etherboot, 2);
}
