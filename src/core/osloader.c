/**************************************************************************
OS loader

Author: Markus Gutschke (gutschk@math.uni-muenster.de)
  Date: Sep/95
Modifications: Ken Yap (for Etherboot/16)
  Doug Ambrisko (ELF and a.out support)
  Klaus Espenlaub (rewrote ELF and a.out (did it really work before?) support,
      added ELF Multiboot images).  Someone should merge the ELF and a.out
      loaders, as most of the code is now identical.  Maybe even NBI could be
      rewritten and merged into the generic loading framework.  This should
      save quite a few bytes of code if you have selected more than one format.
  Ken Yap (Jan 2001)
      Added support for linear entry addresses in tagged images,
      which allows a more efficient protected mode call instead of
      going to real mode and back. Also means entry addresses > 1 MB can
      be called.  Conditional on the LINEAR_EXEC_ADDR bit.
      Added support for Etherboot extension calls. Conditional on the
      TAGGED_PROGRAM_RETURNS bit. Implies LINEAR_EXEC_ADDR.
      Added support for non-MULTIBOOT ELF which also supports Etherboot
      extension calls. Conditional on the ELF_PROGRAM_RETURNS bit.

**************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"

struct os_entry_regs os_regs;

static struct ebinfo		loaderinfo = {
	VERSION_MAJOR, VERSION_MINOR,
	0
};

#define LOAD_DEBUG 0

static int prep_segment(unsigned long start, unsigned long mid, unsigned long end,
	unsigned long istart, unsigned long iend);
static unsigned long find_segment(unsigned long size, unsigned long align);
static sector_t dead_download ( unsigned char *data, unsigned int len, int eof);
static void done(int do_cleanup);

#if defined(IMAGE_FREEBSD) && defined(ELF_IMAGE)
static void elf_freebsd_probe(void);
static void elf_freebsd_fixup_segment(void);
static void elf_freebsd_find_segment_end(void);
static int elf_freebsd_debug_loader(unsigned int offset);
static void elf_freebsd_boot(unsigned long entry);
#else
#define elf_freebsd_probe() do {} while(0)
#define elf_freebsd_fixup_segment()  do {} while(0)
#define elf_freebsd_find_segment_end() do {} while(0)
#define elf_freebsd_debug_loader(off) (0)
#define elf_freebsd_boot(entry) do {} while(0)
#endif
#if defined(IMAGE_FREEBSD) && defined(AOUT_IMAGE)
static void aout_freebsd_probe(void);
static void aout_freebsd_boot(void);
#else
#define aout_freebsd_probe() do {} while(0)
#define aout_freebsd_boot() do {} while(0)
#endif

/**************************************************************************
dead_download - Restart etherboot if probe image fails
**************************************************************************/
static sector_t dead_download ( unsigned char *data __unused, unsigned int len __unused, int eof __unused) {
        longjmp(restart_etherboot, -2);
}

#ifdef	IMAGE_MULTIBOOT
#include "../arch/i386/core/multiboot_loader.c"
#else
#define multiboot_probe(data, len) do {} while(0)
#define multiboot_boot(entry) do {} while(0)
#endif


#ifdef WINCE_IMAGE
#include "../arch/i386/core/wince_loader.c"
#endif

#ifdef AOUT_IMAGE
#include "../arch/i386/core/aout_loader.c"
#endif

#ifdef TAGGED_IMAGE
#include "../arch/i386/core/tagged_loader.c"
#endif

#if defined(ELF_IMAGE) || defined(ELF64_IMAGE)
#include "elf_loader.c"
#endif

#if defined(COFF_IMAGE) 
#include "../arch/e1/core/coff_loader.c"
#endif

#ifdef IMAGE_FREEBSD
#include "../arch/i386/core/freebsd_loader.c"
#endif

#ifdef PXE_IMAGE
#include "../arch/i386/core/pxe_loader.c"
#endif

#ifdef RAW_IMAGE
#include "../arch/armnommu/core/raw_loader.c"
#endif

static void done(int do_cleanup)
{
#ifdef	SIZEINDICATOR
	printf("K ");
#endif
	printf("done\n");
	/* We may not want to do the cleanup: when booting a PXE
	 * image, for example, we need to leave the network card
	 * enabled, and it helps debugging if the serial console
	 * remains enabled.  The call the cleanup() will be triggered
	 * when the PXE stack is shut down.
	 */
	if ( do_cleanup ) {
		cleanup();
		arch_on_exit(0);
	}
}

static int prep_segment(unsigned long start, unsigned long mid, unsigned long end,
	unsigned long istart __unused, unsigned long iend __unused)
{
	unsigned fit, i;

#if LOAD_DEBUG
	printf ( "\nAbout to prepare segment [%lX,%lX)\n", start, end );
	sleep ( 3 );
#endif

	if (mid > end) {
		printf("filesz > memsz\n");
		return 0;
	}
	if ((end > virt_to_phys(_text)) && 
		(start < virt_to_phys(_end))) {
		printf("segment [%lX, %lX) overlaps etherboot [%lX, %lX)\n",
			start, end,
			virt_to_phys(_text), virt_to_phys(_end)
			);
		return 0;
	}
	if ((end > heap_ptr) && (start < heap_bot)) {
		printf("segment [%lX, %lX) overlaps heap [%lX, %lX)\n",
			start, end,
			heap_ptr, heap_bot
			);
		return 0;
	}
	fit = 0;
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM)
			continue;
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if ((start >= r_start) && (end <= r_end)) {
			fit = 1;
			break;
		}
	}
	if (!fit) {
		printf("\nsegment [%lX,%lX) does not fit in any memory region\n",
			start, end);
#if LOAD_DEBUG
		printf("Memory regions(%d):\n", meminfo.map_count);
		for(i = 0; i < meminfo.map_count; i++) {
			unsigned long long r_start, r_end;
			if (meminfo.map[i].type != E820_RAM)
				continue;
			r_start = meminfo.map[i].addr;
			r_end = r_start + meminfo.map[i].size;
			printf("[%X%X, %X%X) type %d\n", 
				(unsigned long)(r_start >> 32),
				(unsigned long)r_start,
				(unsigned long)(r_end >> 32),
				(unsigned long)r_end,
				meminfo.map[i].type);
		}
#endif
		return 0;
	}
#if LOAD_DEBUG
	/* Zap the whole lot.  Do this so that if we're treading on
	 * anything, it shows up now, when the debug message is
	 * visible, rather than when we're partway through downloading
	 * the file.
	 *
	 * If you see an entire screen full of exclamation marks, then
	 * you've almost certainly written all over the display RAM.
	 * This is likely to happen if the status of the A20 line gets
	 * screwed up.  Of course, if this happens, it's a good bet
	 * that you've also trashed the whole of low memory, so expect
	 * interesting things to happen...
	 */
	memset(phys_to_virt(start), '!', mid - start);
#endif
	/* Zero the bss */
	if (end > mid) {
		memset(phys_to_virt(mid), 0, end - mid);
	}
	return 1;
}

static unsigned long find_segment(unsigned long size, unsigned long align)
{
	unsigned i;
	/* Verify I have a power of 2 alignment */
	if (align & (align - 1)) {
		return ULONG_MAX;
	}
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM)
			continue;
		if ((meminfo.map[i].addr + meminfo.map[i].size) > ULONG_MAX) {
			continue;
		}
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		/* Don't allow the segment to overlap etherboot */
		if ((r_end > virt_to_phys(_text)) && (r_start < virt_to_phys(_text))) {
			r_end = virt_to_phys(_text);
		}
		if ((r_start > virt_to_phys(_text)) && (r_start < virt_to_phys(_end))) {
			r_start = virt_to_phys(_end);
		}
		/* Don't allow the segment to overlap the heap */
		if ((r_end > heap_ptr) && (r_start < heap_ptr)) {
			r_end = heap_ptr;
		}
		if ((r_start > heap_ptr) && (r_start < heap_bot)) {
			r_start = heap_ptr;
		}
		r_start = (r_start + align - 1) & ~(align - 1);
		if ((r_end >= r_start) && ((r_end - r_start) >= size)) {
			return r_start;
		}
	}
	/* I did not find anything :( */
	return ULONG_MAX;
}

/**************************************************************************
PROBE_IMAGE - Detect image file type
**************************************************************************/
os_download_t probe_image(unsigned char *data, unsigned int len)
{
	os_download_t os_download = 0;
#ifdef AOUT_IMAGE
	if (!os_download) os_download = aout_probe(data, len);
#endif
#ifdef ELF_IMAGE
	if (!os_download) os_download = elf32_probe(data, len);
#endif
#ifdef ELF64_IMAGE
	if (!os_download) os_download = elf64_probe(data, len);
#endif
#ifdef COFF_IMAGE
    if (!os_download) os_download = coff_probe(data, len);
#endif
#ifdef WINCE_IMAGE
	if (!os_download) os_download = wince_probe(data, len);
#endif
#ifdef TAGGED_IMAGE
	if (!os_download) os_download = tagged_probe(data, len);
#endif
/* PXE_IMAGE must always be last */
#ifdef PXE_IMAGE
	if (!os_download) os_download = pxe_probe(data, len);
#endif
#ifdef RAW_IMAGE
	if (!os_download) os_download = raw_probe(data, len);
#endif
	return os_download;
}

/**************************************************************************
LOAD_BLOCK - Try to load file
**************************************************************************/
int load_block(unsigned char *data, unsigned int block, unsigned int len, int eof)
{
	static os_download_t os_download;
	static sector_t skip_sectors;
	static unsigned int skip_bytes;
#ifdef	SIZEINDICATOR
	static int rlen = 0;

	if (block == 1)
	{
		rlen=len;
		printf("XXXX");
	}
	if (!(block % 4) || eof) {
		int size;
		size = ((block-1) * rlen + len) / 1024;

		putchar('\b');
		putchar('\b');
		putchar('\b');
		putchar('\b');

		putchar('0' + (size/1000)%10);
		putchar('0' + (size/100)%10);
		putchar('0' + (size/10)%10);
		putchar('0' + (size/1)%10);
	}
#endif
	if (block == 1)
	{
		skip_sectors = 0;
		skip_bytes = 0;
		os_download = probe_image(data, len);
		if (!os_download) {
			printf("error: not a valid image\n");
#if 0
			printf("block: %d len: %d\n", block, len);
			printf("%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx\n",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);
#endif
			return 0;
		}
	} /* end of block zero processing */

	/* Either len is greater or the skip is greater */
	if ((skip_sectors > (len >> 9)) ||
		((skip_sectors == (len >> 9)) && (skip_bytes >= (len & 0x1ff)))) {
		/* If I don't have enough bytes borrow them from skip_sectors */
		if (skip_bytes < len) {
			skip_sectors -= (len - skip_bytes + 511) >> 9;
			skip_bytes += (len - skip_bytes + 511) & ~0x1ff;
		}
		skip_bytes -= len;
	}
	else {
		len -= (skip_sectors << 9) + skip_bytes;
		data += (skip_sectors << 9) + skip_bytes;
	}
	skip_sectors = os_download(data, len, eof);
	skip_bytes = 0;
	
	return 1;
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */

