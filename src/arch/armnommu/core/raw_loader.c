/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifdef RAW_IMAGE
static unsigned long raw_load_addr;

int mach_boot(register unsigned long entry_point)
{
	void (*fnc)(void) = (void *) entry_point;
	// r0 = 0
	// r1 = 625 (machine nr. MACH_TYPE_P2001)
	(*fnc)();

	return 0; /* We should never reach this point ! */
}

static sector_t raw_download(unsigned char *data, unsigned int len, int eof)
{
	memcpy(phys_to_virt(raw_load_addr), data, len);
	raw_load_addr += len;
	if (!eof)
		return 0;

	done(1);
	printf("Starting program.\n");
	mach_boot(RAWADDR);
	printf("Bootsector returned?");
	longjmp(restart_etherboot, -2);
	return 1;
}

static os_download_t raw_probe(unsigned char *data __unused, unsigned int len __unused)
{
	printf("(RAW");
	// probe something here...
	printf(")... \n");

	//raw_load_addr = phys_to_virt(_end);
	raw_load_addr = RAWADDR;
	printf("Writing image to 0x%x\n", raw_load_addr);
	return raw_download;
}

#endif
