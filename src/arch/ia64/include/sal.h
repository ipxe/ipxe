#ifndef IA64_SAL_H
#define IA64_SAL_H

struct fptr {
	unsigned long entry;
	unsigned long gp;
};
extern struct fptr sal_entry;
extern struct fptr pal_entry;
extern int parse_sal_system_table(void *table);

#define	SAL_FREQ_BASE_PLATFORM        0
#define	SAL_FREQ_BASE_INTERVAL_TIMER  1
#define	SAL_FREQ_BASE_REALTIME_CLOCK  2

long sal_freq_base (unsigned long which, unsigned long *ticks_per_second,
	unsigned long *drift_info);

#define PCI_SAL_ADDRESS(seg, bus, dev, fn, reg) \
	((unsigned long)(seg << 24) | (unsigned long)(bus << 16) | \
	 (unsigned long)(dev << 11) | (unsigned long)(fn << 8) | \
	 (unsigned long)(reg))

long sal_pci_config_read (
	unsigned long pci_config_addr, unsigned long size, unsigned long *value);
long sal_pci_config_write (
	unsigned long pci_config_addr, unsigned long size, unsigned long value);

#endif /* IA64_SAL_H */
