/*
 * Copyright 2003 Yannis Mitsos and George Thanos 
 * {gmitsos@gthanos}@telecom.ntua.gr
 * Released under GPL2, see the file COPYING in the top directory
 * COFF loader is based on the source code of the ELF loader.
 *
 */
#include "coff.h"

#define COFF_DEBUG 0

typedef struct {
	COFF_filehdr coff32;
	COFF_opthdr	opthdr32;
	union {
		COFF_scnhdr scnhdr32[1];
		unsigned char dummy[1024];
	} p;
	unsigned long curaddr;
	signed int segment;		/* current segment number, -1 for none */
	unsigned int loc;		/* start offset of current block */
	unsigned int skip;		/* padding to be skipped to current segment */
	unsigned long toread;	/* remaining data to be read in the segment */
}coff_state;

coff_state cstate;

static sector_t coff32_download(unsigned char *data, unsigned int len, int eof);
static inline os_download_t coff_probe(unsigned char *data, unsigned int len)
{
	unsigned long phdr_size;

	if (len < (sizeof(cstate.coff32)+ sizeof(cstate.opthdr32))) {
		return 0;
	}
	memcpy(&cstate.coff32, data, (sizeof(cstate.coff32)+sizeof(cstate.opthdr32)));

	if ((cstate.coff32.f_magic != EM_E1) ||
	  	(cstate.opthdr32.magic != O_MAGIC)){
		return 0;
	}
	printf("(COFF");
	printf(")... \n");

	if (cstate.coff32.f_opthdr == 0){
		printf("No optional header in COFF file, cannot find the entry point\n");
                return dead_download;
	}

	phdr_size = cstate.coff32.f_nscns * sizeof(cstate.p.scnhdr32);
	if (sizeof(cstate.coff32) +  cstate.coff32.f_opthdr + phdr_size > len) {
		printf("COFF header outside first block\n");
                return dead_download;
	}

	memcpy(&cstate.p.scnhdr32, data + (sizeof(cstate.coff32) +  cstate.coff32.f_opthdr), phdr_size);

	/* Check for Etherboot related limitations.  Memory
	 * between _text and _end is not allowed.
	 * Reasons: the Etherboot code/data area.
	 */
	for (cstate.segment = 0; cstate.segment < cstate.coff32.f_nscns; cstate.segment++) {
		unsigned long start, mid, end, istart, iend;

		if ((cstate.p.scnhdr32[cstate.segment].s_flags != S_TYPE_TEXT) && 
			(cstate.p.scnhdr32[cstate.segment].s_flags != S_TYPE_DATA) && 
			(cstate.p.scnhdr32[cstate.segment].s_flags != S_TYPE_BSS)){ /* Do we realy need to check the BSS section ? */
#ifdef COFF_DEBUG
				printf("Section <%s> in not a loadable section \n",cstate.p.scnhdr32[cstate.segment].s_name);
#endif
			continue;
		}

	start = cstate.p.scnhdr32[cstate.segment].s_paddr;
	mid = start + cstate.p.scnhdr32[cstate.segment].s_size;
	end = start + cstate.p.scnhdr32[cstate.segment].s_size;

	/* Do we need the following variables ? */
	istart = 0x8000;
	iend = 0x8000;

		if (!prep_segment(start, mid, end, istart, iend)) {
                	return dead_download;
		}
}
	cstate.segment = -1;
	cstate.loc = 0;
	cstate.skip = 0;
	cstate.toread = 0;
	return coff32_download;
}

extern int mach_boot(unsigned long entry_point);
static sector_t coff32_download(unsigned char *data, unsigned int len, int eof)
{
	unsigned long skip_sectors = 0;
	unsigned int offset;	/* working offset in the current data block */
	int i;

	offset = 0;
	do {
		if (cstate.segment != -1) {
			if (cstate.skip) {
				if (cstate.skip >= len - offset) {
					cstate.skip -= len - offset;
					break;
				}
				offset += cstate.skip;
				cstate.skip = 0;
			}
			
			if (cstate.toread) {
				unsigned int cplen;
				cplen = len - offset;
				if (cplen >= cstate.toread) {
					cplen = cstate.toread;
				}
				memcpy(phys_to_virt(cstate.curaddr), data+offset, cplen);
				cstate.curaddr += cplen;
				cstate.toread -= cplen;
				offset += cplen;
				if (cstate.toread)
					break;
			}
		}
	
		/* Data left, but current segment finished - look for the next
		 * segment (in file offset order) that needs to be loaded. 
		 * We can only seek forward, so select the program headers,
		 * in the correct order.
		 */
		cstate.segment = -1;
		for (i = 0; i < cstate.coff32.f_nscns; i++) {

			if ((cstate.p.scnhdr32[i].s_flags != S_TYPE_TEXT) && 
			(cstate.p.scnhdr32[i].s_flags != S_TYPE_DATA))
				continue;
			if (cstate.p.scnhdr32[i].s_size == 0)
				continue;
			if (cstate.p.scnhdr32[i].s_scnptr < cstate.loc + offset)
				continue;	/* can't go backwards */
			if ((cstate.segment != -1) &&
				(cstate.p.scnhdr32[i].s_scnptr >= cstate.p.scnhdr32[cstate.segment].s_scnptr))
				continue;	/* search minimum file offset */
			cstate.segment = i;
		}

		if (cstate.segment == -1) {
			/* No more segments to be loaded, so just start the
			 * kernel.  This saves a lot of network bandwidth if
			 * debug info is in the kernel but not loaded.  */
			goto coff_startkernel;
			break;
		}
		cstate.curaddr = cstate.p.scnhdr32[cstate.segment].s_paddr;
		cstate.skip	   = cstate.p.scnhdr32[cstate.segment].s_scnptr - (cstate.loc + offset);
		cstate.toread  = cstate.p.scnhdr32[cstate.segment].s_size;
#if COFF_DEBUG
		printf("PHDR %d, size %#lX, curaddr %#lX\n",
			cstate.segment, cstate.toread, cstate.curaddr);
#endif
	} while (offset < len);

	cstate.loc += len + (cstate.skip & ~0x1ff);
	skip_sectors = cstate.skip >> 9;
	cstate.skip &= 0x1ff;
	
	if (eof) {
		unsigned long entry;
coff_startkernel:
		entry = cstate.opthdr32.entry;
		done();
		mach_boot(entry);
	}
	return skip_sectors;
}
