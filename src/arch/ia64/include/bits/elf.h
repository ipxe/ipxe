#ifndef IA64_BITS_ELF_H
#define IA64_BITS_ELF_H

/* ELF Defines for the current architecture */
#define	EM_CURRENT	EM_IA_64
#define ELFDATA_CURRENT	ELFDATA2LSB

#define ELF_CHECK_ARCH(x) \
	((x).e_machine == EM_CURRENT)

#endif /* IA64_BITS_ELF_H */
