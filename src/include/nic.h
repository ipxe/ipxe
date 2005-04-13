 /*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#ifndef	NIC_H
#define NIC_H

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
	struct nic_operations *nic_op;
	int		flags;	/* driver specific flags */
	unsigned char	*node_addr;
	unsigned char	*packet;
	unsigned int	packetlen;
	unsigned int	ioaddr;
	unsigned char	irqno;
	unsigned int	mbps;
	duplex_t	duplex;
	void		*priv_data;	/* driver can hang private data here */
};

struct nic_operations {
	int ( *connect ) ( struct nic * );
	int ( *poll ) ( struct nic *, int retrieve );
	void ( *transmit ) ( struct nic *, const char *,
			     unsigned int, unsigned int, const char * );
	void ( *irq ) ( struct nic *, irq_action_t );
	void ( *disable ) ( struct nic * );
};

/*
 * Function prototypes
 *
 */
struct dev;
extern struct nic * nic_device ( struct dev * dev );
extern int dummy_connect ( struct nic *nic );
extern int dummy_irq ( struct nic *nic );

/*
 * Functions that implicitly operate on the current boot device
 *
 * "nic" always points to &dev.nic
 */

extern struct nic *nic;

static inline int eth_connect ( void ) {
	return nic->nic_op->connect ( nic );
}

static inline int eth_poll ( int retrieve ) {
	return nic->nic_op->poll ( nic, retrieve );
}

static inline void eth_transmit ( const char *dest, unsigned int type,
				  unsigned int size, const void *packet ) {
	nic->nic_op->transmit ( nic, dest, type, size, packet );
}

static inline void eth_irq ( irq_action_t action ) {
	nic->nic_op->irq ( nic, action );
}

/* Should be using disable() rather than eth_disable() */
static inline void eth_disable ( void ) __attribute__ (( deprecated ));
static inline void eth_disable ( void ) {
	nic->nic_op->disable ( nic );
}

/* dev.h needs declarations from nic.h */
#include "dev.h"
/* to get global "dev" */
#include "main.h"

#endif	/* NIC_H */
