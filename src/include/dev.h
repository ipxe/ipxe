#ifndef DEV_H
#define DEV_H

#include "stdint.h"
#include "nic.h"
#include "pci.h"

/* Need to check the packing of this struct if Etherboot is ported */
struct dev_id {
	uint16_t	vendor_id;
	uint16_t	device_id;
	uint8_t		bus_type;
#define	PCI_BUS_TYPE	1
#define	ISA_BUS_TYPE	2
} __attribute__ ((packed));

/* Dont use sizeof, that will include the padding */
#define	DEV_ID_SIZE	8

struct dev {
	struct dev_operations *dev_op;
	const char *name;
	struct dev_id	devid;	/* device ID string (sent to DHCP server) */
	/* All possible bus types */
	union {
		struct pci_device pci;
	};
	/* All possible device types */
	union {
		struct nic	nic;
	};
};

struct dev_operations {
	void ( *disable ) ( struct dev * );
	int ( *load_configuration ) ( struct dev * );
	int ( *load ) ( struct dev * );
};

struct boot_driver {
	char *name;
	int (*probe) ( struct dev * );
};

#define BOOT_DRIVER( driver_name, probe_func )				      \
	static struct boot_driver boot_driver				      \
	    __attribute__ ((used,__section__(".boot_drivers"))) = {	      \
		.name = driver_name,					      \
		.probe = probe_func,					      \
	};

/* Functions in dev.c */
extern void print_drivers ( void );
extern int probe ( struct dev *dev );
extern void disable ( struct dev *dev );
static inline int load_configuration ( struct dev *dev ) {
	return dev->dev_op->load_configuration ( dev );
}
static inline int load ( struct dev *dev ) {
	return dev->dev_op->load ( dev );
}

#endif /* DEV_H */
