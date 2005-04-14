#ifndef DEV_H
#define DEV_H

#include "stdint.h"

/* Device types */
#include "nic.h"

/* Need to check the packing of this struct if Etherboot is ported */
struct dev_id {
	uint16_t	vendor_id;
	uint16_t	device_id;
	uint8_t		bus_type;
#define	PCI_BUS_TYPE	1
#define	ISA_BUS_TYPE	2
#define MCA_BUS_TYPE	3
} __attribute__ ((packed));

/* Dont use sizeof, that will include the padding */
#define	DEV_ID_SIZE	8

struct dev {
	struct dev_operations *dev_op;
	const char *name;
	struct dev_id	devid;	/* device ID string (sent to DHCP server) */
	/* Pointer to bus information for device.  Whatever sets up
	 * the struct dev must make sure that this points to a buffer
	 * large enough for the required struct <bus>_device.
	 */
	void *bus;
	/* All possible device types */
	union {
		struct nic	nic;
	};
};

/*
 * Macro to help create a common symbol with enough space for any
 * struct <bus>_device.
 *
 * Use as e.g. DEV_BUS(struct pci_device);
 */
#define DEV_BUS(datatype,symbol) datatype symbol __asm__ ( "_dev_bus" );

struct dev_operations {
	void ( *disable ) ( struct dev * );
	void ( *print_info ) ( struct dev * );
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
static inline void print_info ( struct dev *dev ) {
	dev->dev_op->print_info ( dev );
}
static inline int load_configuration ( struct dev *dev ) {
	return dev->dev_op->load_configuration ( dev );
}
static inline int load ( struct dev *dev ) {
	return dev->dev_op->load ( dev );
}

#endif /* DEV_H */
