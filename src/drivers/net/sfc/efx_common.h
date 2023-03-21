/**************************************************************************
 *
 * GPL common net driver for Solarflare network cards
 *
 * Written by Michael Brown <mbrown@fensystems.co.uk>
 *
 * Copyright Fen Systems Ltd. 2005
 * Copyright Level 5 Networks Inc. 2005
 * Copyright Solarflare Communications Inc. 2013-2017
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 *
 ***************************************************************************/
#ifndef EFX_COMMON_H
#define EFX_COMMON_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define __packed    __attribute__((__packed__))
#define __force     /*nothing*/

typedef uint16_t    __le16;
typedef uint32_t    __le32;
typedef uint64_t    __le64;

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct{int: -!!(e); }))
#define BUILD_BUG_ON(e) ((void)BUILD_BUG_ON_ZERO(e))

#include <stdbool.h>
#include <ipxe/io.h>
#include <ipxe/netdevice.h>
#include "efx_bitfield.h"
#include "mcdi.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/**************************************************************************
 *
 * Hardware data structures and sizing
 *
 ***************************************************************************/
typedef efx_qword_t efx_rx_desc_t;
typedef efx_qword_t efx_tx_desc_t;
typedef efx_qword_t efx_event_t;

#define EFX_BUF_ALIGN		4096
#define EFX_RXD_SIZE		512
#define EFX_RXD_MASK            (EFX_RXD_SIZE - 1)
#define EFX_TXD_SIZE		512
#define EFX_TXD_MASK            (EFX_TXD_SIZE - 1)
#define EFX_EVQ_SIZE		512
#define EFX_EVQ_MASK            (EFX_EVQ_SIZE - 1)

/* There is space for 512 rx descriptors available. This number can be
 * anything between 1 and 512 in powers of 2. This value will affect the
 * network performance. During a test we were able to push 239 descriptors
 * before we ran out of space.
 */
#define EFX_NUM_RX_DESC		64
#define EFX_NUM_RX_DESC_MASK    (EFX_NUM_RX_DESC - 1)

/* The packet size is usually 1500 bytes hence we choose 1600 as the buf size,
 * which is (1500+metadata)
 */
#define EFX_RX_BUF_SIZE		1600

/* Settings for the state field in efx_nic.
 */
#define EFX_STATE_POLLING	1

typedef unsigned long long dma_addr_t;

/** A buffer table allocation backing a tx dma, rx dma or eventq */
struct efx_special_buffer {
	dma_addr_t dma_addr;
	int id;
};

/** A transmit queue */
struct efx_tx_queue {
	/* The hardware ring */
	efx_tx_desc_t *ring;

	/* The software ring storing io_buffers. */
	struct io_buffer *buf[EFX_TXD_SIZE];

	/* The buffer table reservation pushed to hardware */
	struct efx_special_buffer entry;

	/* Software descriptor write ptr */
	unsigned int write_ptr;

	/* Hardware descriptor read ptr */
	unsigned int read_ptr;
};

/** A receive queue */
struct efx_rx_queue {
	/* The hardware ring */
	efx_rx_desc_t *ring;

	/* The software ring storing io_buffers */
	struct io_buffer *buf[EFX_NUM_RX_DESC];

	/* The buffer table reservation pushed to hardware */
	struct efx_special_buffer entry;

	/* Descriptor write ptr, into both the hardware and software rings */
	unsigned int write_ptr;

	/* Hardware completion ptr */
	unsigned int read_ptr;

	/* The value of RX_CONT in the previous RX event */
	unsigned int rx_cont_prev;
};

/** An event queue */
struct efx_ev_queue {
	/* The hardware ring to push to hardware.
	 * Must be the first entry in the structure.
	 */
	efx_event_t *ring;

	/* The buffer table reservation pushed to hardware */
	struct efx_special_buffer entry;

	/* Pointers into the ring */
	unsigned int read_ptr;
};

/* Hardware revisions */
enum efx_revision {
	EFX_HUNTINGTON,
};

/** Hardware access */
struct efx_nic {
	struct net_device *netdev;
	enum efx_revision revision;
	const struct efx_nic_type *type;

	int port;
	u32 state;

	/** Memory and IO base */
	void *membase;
	unsigned long mmio_start;
	unsigned long mmio_len;

	/* Buffer table allocation head */
	int buffer_head;

	/* Queues */
	struct efx_rx_queue rxq;
	struct efx_tx_queue txq;
	struct efx_ev_queue evq;

	unsigned int rx_prefix_size;

	/** INT_REG_KER */
	int int_en;
	efx_oword_t int_ker __aligned;

	/* Set to true if firmware supports the workaround for bug35388 */
	bool workaround_35388;

};


/** Efx device type definition */
struct efx_nic_type {
	int (*mcdi_rpc)(struct efx_nic *efx, unsigned int cmd,
			const efx_dword_t *inbuf, size_t inlen,
			efx_dword_t *outbuf, size_t outlen,
			size_t *outlen_actual, bool quiet);
};

extern const struct efx_nic_type hunt_nic_type;

#define EFX_MAC_FRAME_LEN(_mtu)					\
	(((_mtu)						\
	  + /* EtherII already included */			\
	  + 4 /* FCS */						\
	  /* No VLAN supported */				\
	  + 16 /* bug16772 */					\
	  + 7) & ~7)

/*******************************************************************************
 *
 *
 * Hardware API
 *
 *
 ******************************************************************************/
static inline void _efx_writel(struct efx_nic *efx, uint32_t value,
			       unsigned int reg)
{
	writel((value), (efx)->membase + (reg));
}

static inline uint32_t _efx_readl(struct efx_nic *efx, unsigned int reg)
{
	return readl((efx)->membase + (reg));
}

#define efx_writel_table(efx, value, index, reg)		\
	efx_writel(efx, value, (reg) + ((index) * reg##_STEP))

#define efx_writel_page(efx, value, index, reg)			\
	efx_writel(efx, value, (reg) + ((index) * 0x2000))

/* Hardware access */
extern void efx_writel(struct efx_nic *efx, efx_dword_t *value,
		       unsigned int reg);
extern void efx_readl(struct efx_nic *efx, efx_dword_t *value,
		      unsigned int reg);

/* Initialisation */
extern void efx_probe(struct net_device *netdev, enum efx_revision rev);
extern void efx_remove(struct net_device *netdev);

#endif /* EFX_COMMON_H */
