/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ARM_BITS_ELF_H
#define ARM_BITS_ELF_H

/* ELF Defines for the current architecture */
#define	EM_CURRENT	EM_ARM
#define ELFDATA_CURRENT	ELFDATA2LSB

#define ELF_CHECK_ARCH(x) \
	((x).e_machine == EM_CURRENT)

#endif /* ARM_BITS_ELF_H */
