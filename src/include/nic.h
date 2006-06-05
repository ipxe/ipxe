 /*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#ifndef	NIC_H
#define NIC_H

#include <byteswap.h>
#include <gpxe/pci.h>
#include "dhcp.h"

typedef enum {
	DISABLE = 0,
	ENABLE,
	FORCE
} irq_action_t;

typedef enum duplex {
	HALF_DUPLEX = 1,
	FULL_DUPLEX
} duplex_t;

/*
 *	Structure returned from eth_probe and passed to other driver
 *	functions.
 */
struct nic {
	struct nic_operations	*nic_op;
	int			flags;	/* driver specific flags */
	unsigned char		*node_addr;
	unsigned char		*packet;
	unsigned int		packetlen;
	unsigned int		ioaddr;
	unsigned char		irqno;
	unsigned int		mbps;
	duplex_t		duplex;
	struct dhcp_dev_id	dhcp_dev_id;
	void			*priv_data;	/* driver private data */
};

struct nic_operations {
	int ( *connect ) ( struct nic * );
	int ( *poll ) ( struct nic *, int retrieve );
	void ( *transmit ) ( struct nic *, const char *,
			     unsigned int, unsigned int, const char * );
	void ( *irq ) ( struct nic *, irq_action_t );
};

extern struct nic nic;

static inline int eth_poll ( int retrieve ) {
	return nic.nic_op->poll ( &nic, retrieve );
}

static inline void eth_transmit ( const char *dest, unsigned int type,
				  unsigned int size, const void *packet ) {
	nic.nic_op->transmit ( &nic, dest, type, size, packet );
}

/*
 * Function prototypes
 *
 */
extern int dummy_connect ( struct nic *nic );
extern void dummy_irq ( struct nic *nic, irq_action_t irq_action );
extern int legacy_probe ( struct pci_device *pci,
			  const struct pci_device_id *id,
			  int ( * probe ) ( struct nic *nic,
					    struct pci_device *pci ),
			  void ( * disable ) ( struct nic *nic ) );
extern void legacy_remove ( struct pci_device *pci,
			    void ( * disable ) ( struct nic *nic ) );
extern void pci_fill_nic ( struct nic *nic, struct pci_device *pci );

#define PCI_DRIVER(_name,_ids,_class) 					\
	static int _name ## _legacy_probe ( struct pci_device *pci,	\
					    const struct pci_device_id *id ); \
	static void _name ## _legacy_remove ( struct pci_device *pci );	\
	struct pci_driver _name __pci_driver = {			\
		.ids = _ids,						\
		.id_count = sizeof ( _ids ) / sizeof ( _ids[0] ),	\
		.probe = _name ## _legacy_probe,			\
		.remove = _name ## _legacy_remove,			\
	};

#undef DRIVER
#define DRIVER(_unused1,_unused2,_unused3,_name,_probe,_disable)	\
	static int _name ## _legacy_probe ( struct pci_device *pci,	\
					    const struct pci_device_id *id ) {\
		return legacy_probe ( pci, id, _probe, _disable );	\
	}								\
	static void _name ## _legacy_remove ( struct pci_device *pci ) {\
		return legacy_remove ( pci, _disable );			\
	}

#endif	/* NIC_H */
