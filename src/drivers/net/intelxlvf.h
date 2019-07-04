#ifndef _INTELXLVF_H
#define _INTELXLVF_H

/** @file
 *
 * Intel 40 Gigabit Ethernet virtual function network card driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include "intelxl.h"

/** BAR size */
#define INTELXLVF_BAR_SIZE 0x10000

/** Transmit Queue Tail Register */
#define INTELXLVF_QTX_TAIL 0x00000

/** Receive Queue Tail Register */
#define INTELXLVF_QRX_TAIL 0x02000

/** VF Interrupt Zero Dynamic Control Register */
#define INTELXLVF_VFINT_DYN_CTL0 0x5c00

/** VF Admin Queue register block */
#define INTELXLVF_ADMIN 0x6000

/** Admin Command Queue Base Address Low Register (offset) */
#define INTELXLVF_ADMIN_CMD_BAL 0x1c00

/** Admin Command Queue Base Address High Register (offset) */
#define INTELXLVF_ADMIN_CMD_BAH 0x1800

/** Admin Command Queue Length Register (offset) */
#define INTELXLVF_ADMIN_CMD_LEN 0x0800

/** Admin Command Queue Head Register (offset) */
#define INTELXLVF_ADMIN_CMD_HEAD 0x0400

/** Admin Command Queue Tail Register (offset) */
#define INTELXLVF_ADMIN_CMD_TAIL 0x2400

/** Admin Event Queue Base Address Low Register (offset) */
#define INTELXLVF_ADMIN_EVT_BAL 0x0c00

/** Admin Event Queue Base Address High Register (offset) */
#define INTELXLVF_ADMIN_EVT_BAH 0x0000

/** Admin Event Queue Length Register (offset) */
#define INTELXLVF_ADMIN_EVT_LEN 0x2000

/** Admin Event Queue Head Register (offset) */
#define INTELXLVF_ADMIN_EVT_HEAD 0x1400

/** Admin Event Queue Tail Register (offset) */
#define INTELXLVF_ADMIN_EVT_TAIL 0x1000

/** Maximum time to wait for a VF admin request to complete */
#define INTELXLVF_ADMIN_MAX_WAIT_MS 2000

/** VF Reset Status Register */
#define INTELXLVF_VFGEN_RSTAT 0x8800
#define INTELXLVF_VFGEN_RSTAT_VFR_STATE(x) ( (x) & 0x3 )
#define INTELXLVF_VFGEN_RSTAT_VFR_STATE_ACTIVE 0x2

/** Maximum time to wait for reset to complete */
#define INTELXLVF_RESET_MAX_WAIT_MS 1000

/**
 * Initialise descriptor ring
 *
 * @v ring		Descriptor ring
 * @v count		Number of descriptors
 * @v len		Length of a single descriptor
 * @v tail		Tail register offset
 */
static inline __attribute__ (( always_inline)) void
intelxlvf_init_ring ( struct intelxl_ring *ring, unsigned int count,
		      size_t len, unsigned int tail ) {

	ring->len = ( count * len );
	ring->tail = tail;
}

#endif /* _INTELXLVF_H */
