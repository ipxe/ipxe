/*
 * Copyright (C) 2018 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include <ipxe/pcibridge.h>
#include <ipxe/version.h>
#include "ena.h"

/** @file
 *
 * Amazon ENA network driver
 *
 */

/**
 * Get direction name (for debugging)
 *
 * @v direction		Direction
 * @ret name		Direction name
 */
static const char * ena_direction ( unsigned int direction ) {

	switch ( direction ) {
	case ENA_SQ_TX:		return "TX";
	case ENA_SQ_RX:		return "RX";
	default:		return "<UNKNOWN>";
	}
}

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Wait for reset operation to be acknowledged
 *
 * @v ena		ENA device
 * @v expected		Expected reset state
 * @ret rc		Return status code
 */
static int ena_reset_wait ( struct ena_nic *ena, uint32_t expected ) {
	uint32_t stat;
	unsigned int i;

	/* Wait for reset to complete */
	for ( i = 0 ; i < ENA_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check if device is ready */
		stat = readl ( ena->regs + ENA_STAT );
		if ( ( stat & ENA_STAT_RESET ) == expected )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( ena, "ENA %p timed out waiting for reset status %#08x "
	       "(got %#08x)\n", ena, expected, stat );
	return -ETIMEDOUT;
}

/**
 * Reset hardware
 *
 * @v ena		ENA device
 * @ret rc		Return status code
 */
static int ena_reset ( struct ena_nic *ena ) {
	int rc;

	/* Trigger reset */
	writel ( ENA_CTRL_RESET, ( ena->regs + ENA_CTRL ) );

	/* Wait for reset to take effect */
	if ( ( rc = ena_reset_wait ( ena, ENA_STAT_RESET ) ) != 0 )
		return rc;

	/* Clear reset */
	writel ( 0, ( ena->regs + ENA_CTRL ) );

	/* Wait for reset to clear */
	if ( ( rc = ena_reset_wait ( ena, 0 ) ) != 0 )
		return rc;

	return 0;
}

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/**
 * Set queue base address
 *
 * @v ena		ENA device
 * @v offset		Register offset
 * @v address		Base address
 */
static inline void ena_set_base ( struct ena_nic *ena, unsigned int offset,
				  void *base ) {
	physaddr_t phys = virt_to_bus ( base );

	/* Program base address registers */
	writel ( ( phys & 0xffffffffUL ),
		 ( ena->regs + offset + ENA_BASE_LO ) );
	if ( sizeof ( phys ) > sizeof ( uint32_t ) ) {
		writel ( ( ( ( uint64_t ) phys ) >> 32 ),
			 ( ena->regs + offset + ENA_BASE_HI ) );
	} else {
		writel ( 0, ( ena->regs + offset + ENA_BASE_HI ) );
	}
}

/**
 * Set queue capabilities
 *
 * @v ena		ENA device
 * @v offset		Register offset
 * @v count		Number of entries
 * @v size		Size of each entry
 */
static inline __attribute__ (( always_inline )) void
ena_set_caps ( struct ena_nic *ena, unsigned int offset, unsigned int count,
	       size_t size ) {

	/* Program capabilities register */
	writel ( ENA_CAPS ( count, size ), ( ena->regs + offset ) );
}

/**
 * Clear queue capabilities
 *
 * @v ena		ENA device
 * @v offset		Register offset
 */
static inline __attribute__ (( always_inline )) void
ena_clear_caps ( struct ena_nic *ena, unsigned int offset ) {

	/* Clear capabilities register */
	writel ( 0, ( ena->regs + offset ) );
}

/**
 * Create admin queues
 *
 * @v ena		ENA device
 * @ret rc		Return status code
 */
static int ena_create_admin ( struct ena_nic *ena ) {
	size_t aq_len = ( ENA_AQ_COUNT * sizeof ( ena->aq.req[0] ) );
	size_t acq_len = ( ENA_ACQ_COUNT * sizeof ( ena->acq.rsp[0] ) );
	int rc;

	/* Allocate admin completion queue */
	ena->acq.rsp = malloc_phys ( acq_len, acq_len );
	if ( ! ena->acq.rsp ) {
		rc = -ENOMEM;
		goto err_alloc_acq;
	}
	memset ( ena->acq.rsp, 0, acq_len );

	/* Allocate admin queue */
	ena->aq.req = malloc_phys ( aq_len, aq_len );
	if ( ! ena->aq.req ) {
		rc = -ENOMEM;
		goto err_alloc_aq;
	}
	memset ( ena->aq.req, 0, aq_len );

	/* Program queue addresses and capabilities */
	ena_set_base ( ena, ENA_ACQ_BASE, ena->acq.rsp );
	ena_set_caps ( ena, ENA_ACQ_CAPS, ENA_ACQ_COUNT,
		       sizeof ( ena->acq.rsp[0] ) );
	ena_set_base ( ena, ENA_AQ_BASE, ena->aq.req );
	ena_set_caps ( ena, ENA_AQ_CAPS, ENA_AQ_COUNT,
		       sizeof ( ena->aq.req[0] ) );

	DBGC ( ena, "ENA %p AQ [%08lx,%08lx) ACQ [%08lx,%08lx)\n",
	       ena, virt_to_phys ( ena->aq.req ),
	       ( virt_to_phys ( ena->aq.req ) + aq_len ),
	       virt_to_phys ( ena->acq.rsp ),
	       ( virt_to_phys ( ena->acq.rsp ) + acq_len ) );
	return 0;

	ena_clear_caps ( ena, ENA_AQ_CAPS );
	ena_clear_caps ( ena, ENA_ACQ_CAPS );
	free_phys ( ena->aq.req, aq_len );
 err_alloc_aq:
	free_phys ( ena->acq.rsp, acq_len );
 err_alloc_acq:
	return rc;
}

/**
 * Destroy admin queues
 *
 * @v ena		ENA device
 */
static void ena_destroy_admin ( struct ena_nic *ena ) {
	size_t aq_len = ( ENA_AQ_COUNT * sizeof ( ena->aq.req[0] ) );
	size_t acq_len = ( ENA_ACQ_COUNT * sizeof ( ena->acq.rsp[0] ) );

	/* Clear queue capabilities */
	ena_clear_caps ( ena, ENA_AQ_CAPS );
	ena_clear_caps ( ena, ENA_ACQ_CAPS );
	wmb();

	/* Free queues */
	free_phys ( ena->aq.req, aq_len );
	free_phys ( ena->acq.rsp, acq_len );
	DBGC ( ena, "ENA %p AQ and ACQ destroyed\n", ena );
}

/**
 * Get next available admin queue request
 *
 * @v ena		ENA device
 * @ret req		Admin queue request
 */
static union ena_aq_req * ena_admin_req ( struct ena_nic *ena ) {
	union ena_aq_req *req;
	unsigned int index;

	/* Get next request */
	index = ( ena->aq.prod % ENA_AQ_COUNT );
	req = &ena->aq.req[index];

	/* Initialise request */
	memset ( ( ( ( void * ) req ) + sizeof ( req->header ) ), 0,
		 ( sizeof ( *req ) - sizeof ( req->header ) ) );
	req->header.id = ena->aq.prod;

	/* Increment producer counter */
	ena->aq.prod++;

	return req;
}

/**
 * Issue admin queue request
 *
 * @v ena		ENA device
 * @v req		Admin queue request
 * @v rsp		Admin queue response to fill in
 * @ret rc		Return status code
 */
static int ena_admin ( struct ena_nic *ena, union ena_aq_req *req,
		       union ena_acq_rsp **rsp ) {
	unsigned int index;
	unsigned int i;
	int rc;

	/* Locate response */
	index = ( ena->acq.cons % ENA_ACQ_COUNT );
	*rsp = &ena->acq.rsp[index];

	/* Mark request as ready */
	req->header.flags ^= ENA_AQ_PHASE;
	wmb();
	DBGC2 ( ena, "ENA %p admin request %#x:\n",
		ena, le16_to_cpu ( req->header.id ) );
	DBGC2_HDA ( ena, virt_to_phys ( req ), req, sizeof ( *req ) );

	/* Ring doorbell */
	writel ( ena->aq.prod, ( ena->regs + ENA_AQ_DB ) );

	/* Wait for response */
	for ( i = 0 ; i < ENA_ADMIN_MAX_WAIT_MS ; i++ ) {

		/* Check for response */
		if ( ( (*rsp)->header.flags ^ ena->acq.phase ) & ENA_ACQ_PHASE){
			mdelay ( 1 );
			continue;
		}
		DBGC2 ( ena, "ENA %p admin response %#x:\n",
			ena, le16_to_cpu ( (*rsp)->header.id ) );
		DBGC2_HDA ( ena, virt_to_phys ( *rsp ), *rsp, sizeof ( **rsp ));

		/* Increment consumer counter */
		ena->acq.cons++;
		if ( ( ena->acq.cons % ENA_ACQ_COUNT ) == 0 )
			ena->acq.phase ^= ENA_ACQ_PHASE;

		/* Check command identifier */
		if ( (*rsp)->header.id != req->header.id ) {
			DBGC ( ena, "ENA %p admin response %#x mismatch:\n",
			       ena, le16_to_cpu ( (*rsp)->header.id ) );
			rc = -EILSEQ;
			goto err;
		}

		/* Check status */
		if ( (*rsp)->header.status != 0 ) {
			DBGC ( ena, "ENA %p admin response %#x status %d:\n",
			       ena, le16_to_cpu ( (*rsp)->header.id ),
			       (*rsp)->header.status );
			rc = -EIO;
			goto err;
		}

		/* Success */
		return 0;
	}

	rc = -ETIMEDOUT;
	DBGC ( ena, "ENA %p timed out waiting for admin request %#x:\n",
	       ena, le16_to_cpu ( req->header.id ) );
 err:
	DBGC_HDA ( ena, virt_to_phys ( req ), req, sizeof ( *req ) );
	DBGC_HDA ( ena, virt_to_phys ( *rsp ), *rsp, sizeof ( **rsp ) );
	return rc;
}

/**
 * Set async event notification queue config
 *
 * @v ena		ENA device
 * @v enabled		Bitmask of the groups to enable
 * @ret rc		Return status code
 */
static int ena_set_aenq_config ( struct ena_nic *ena, uint32_t enabled ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	union ena_feature *feature;
	int rc;

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_SET_FEATURE;
	req->set_feature.id = ENA_AENQ_CONFIG;
	feature = &req->set_feature.feature;
	feature->aenq.enabled = cpu_to_le32 ( enabled );

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Create async event notification queue
 *
 * @v ena		ENA device
 * @ret rc		Return status code
 */
static int ena_create_async ( struct ena_nic *ena ) {
	size_t aenq_len = ( ENA_AENQ_COUNT * sizeof ( ena->aenq.evt[0] ) );
	int rc;

	/* Allocate async event notification queue */
	ena->aenq.evt = malloc_phys ( aenq_len, aenq_len );
	if ( ! ena->aenq.evt ) {
		rc = -ENOMEM;
		goto err_alloc_aenq;
	}
	memset ( ena->aenq.evt, 0, aenq_len );

	/* Program queue address and capabilities */
	ena_set_base ( ena, ENA_AENQ_BASE, ena->aenq.evt );
	ena_set_caps ( ena, ENA_AENQ_CAPS, ENA_AENQ_COUNT,
		       sizeof ( ena->aenq.evt[0] ) );

	DBGC ( ena, "ENA %p AENQ [%08lx,%08lx)\n",
	       ena, virt_to_phys ( ena->aenq.evt ),
	       ( virt_to_phys ( ena->aenq.evt ) + aenq_len ) );

	/* Disable all events */
	if ( ( rc = ena_set_aenq_config ( ena, 0 ) ) != 0 )
		goto err_set_aenq_config;

	return 0;

 err_set_aenq_config:
	ena_clear_caps ( ena, ENA_AENQ_CAPS );
	free_phys ( ena->aenq.evt, aenq_len );
 err_alloc_aenq:
	return rc;
}

/**
 * Destroy async event notification queue
 *
 * @v ena		ENA device
 */
static void ena_destroy_async ( struct ena_nic *ena ) {
	size_t aenq_len = ( ENA_AENQ_COUNT * sizeof ( ena->aenq.evt[0] ) );

	/* Clear queue capabilities */
	ena_clear_caps ( ena, ENA_AENQ_CAPS );
	wmb();

	/* Free queue */
	free_phys ( ena->aenq.evt, aenq_len );
	DBGC ( ena, "ENA %p AENQ destroyed\n", ena );
}

/**
 * Create submission queue
 *
 * @v ena		ENA device
 * @v sq		Submission queue
 * @v cq		Corresponding completion queue
 * @ret rc		Return status code
 */
static int ena_create_sq ( struct ena_nic *ena, struct ena_sq *sq,
			   struct ena_cq *cq ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	unsigned int i;
	int rc;

	/* Allocate submission queue entries */
	sq->sqe.raw = malloc_phys ( sq->len, ENA_ALIGN );
	if ( ! sq->sqe.raw ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( sq->sqe.raw, 0, sq->len );

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_CREATE_SQ;
	req->create_sq.direction = sq->direction;
	req->create_sq.policy = cpu_to_le16 ( ENA_SQ_HOST_MEMORY |
					      ENA_SQ_CONTIGUOUS );
	req->create_sq.cq_id = cpu_to_le16 ( cq->id );
	req->create_sq.count = cpu_to_le16 ( sq->count );
	req->create_sq.address = cpu_to_le64 ( virt_to_bus ( sq->sqe.raw ) );

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		goto err_admin;

	/* Parse response */
	sq->id = le16_to_cpu ( rsp->create_sq.id );
	sq->doorbell = le32_to_cpu ( rsp->create_sq.doorbell );

	/* Reset producer counter and phase */
	sq->prod = 0;
	sq->phase = ENA_SQE_PHASE;

	/* Calculate fill level */
	sq->fill = sq->max;
	if ( sq->fill > cq->actual )
		sq->fill = cq->actual;

	/* Initialise buffer ID ring */
	for ( i = 0 ; i < sq->count ; i++ )
		sq->ids[i] = i;

	DBGC ( ena, "ENA %p %s SQ%d at [%08lx,%08lx) fill %d db +%04x CQ%d\n",
	       ena, ena_direction ( sq->direction ), sq->id,
	       virt_to_phys ( sq->sqe.raw ),
	       ( virt_to_phys ( sq->sqe.raw ) + sq->len ),
	       sq->fill, sq->doorbell, cq->id );
	return 0;

 err_admin:
	free_phys ( sq->sqe.raw, sq->len );
 err_alloc:
	return rc;
}

/**
 * Destroy submission queue
 *
 * @v ena		ENA device
 * @v sq		Submission queue
 * @ret rc		Return status code
 */
static int ena_destroy_sq ( struct ena_nic *ena, struct ena_sq *sq ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	int rc;

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_DESTROY_SQ;
	req->destroy_sq.id = cpu_to_le16 ( sq->id );
	req->destroy_sq.direction = sq->direction;

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		return rc;

	/* Free submission queue entries */
	free_phys ( sq->sqe.raw, sq->len );

	DBGC ( ena, "ENA %p %s SQ%d destroyed\n",
	       ena, ena_direction ( sq->direction ), sq->id );
	return 0;
}

/**
 * Create completion queue
 *
 * @v ena		ENA device
 * @v cq		Completion queue
 * @ret rc		Return status code
 */
static int ena_create_cq ( struct ena_nic *ena, struct ena_cq *cq ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	int rc;

	/* Allocate completion queue entries */
	cq->cqe.raw = malloc_phys ( cq->len, ENA_ALIGN );
	if ( ! cq->cqe.raw ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( cq->cqe.raw, 0, cq->len );

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_CREATE_CQ;
	req->create_cq.size = cq->size;
	req->create_cq.count = cpu_to_le16 ( cq->requested );
	req->create_cq.vector = cpu_to_le32 ( ENA_MSIX_NONE );
	req->create_cq.address = cpu_to_le64 ( virt_to_bus ( cq->cqe.raw ) );

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 ) {
		DBGC ( ena, "ENA %p CQ%d creation failed (broken firmware?)\n",
		       ena, cq->id );
		goto err_admin;
	}

	/* Parse response */
	cq->id = le16_to_cpu ( rsp->create_cq.id );
	cq->actual = le16_to_cpu ( rsp->create_cq.count );
	cq->doorbell = le32_to_cpu ( rsp->create_cq.doorbell );
	cq->mask = ( cq->actual - 1 );
	if ( cq->actual != cq->requested ) {
		DBGC ( ena, "ENA %p CQ%d requested %d actual %d\n",
		       ena, cq->id, cq->requested, cq->actual );
	}

	/* Reset consumer counter and phase */
	cq->cons = 0;
	cq->phase = ENA_CQE_PHASE;

	DBGC ( ena, "ENA %p CQ%d at [%08lx,%08lx) db +%04x\n",
	       ena, cq->id, virt_to_phys ( cq->cqe.raw ),
	       ( virt_to_phys ( cq->cqe.raw ) + cq->len ), cq->doorbell );
	return 0;

 err_admin:
	free_phys ( cq->cqe.raw, cq->len );
 err_alloc:
	return rc;
}

/**
 * Destroy completion queue
 *
 * @v ena		ENA device
 * @v cq		Completion queue
 * @ret rc		Return status code
 */
static int ena_destroy_cq ( struct ena_nic *ena, struct ena_cq *cq ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	int rc;

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_DESTROY_CQ;
	req->destroy_cq.id = cpu_to_le16 ( cq->id );

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		return rc;

	/* Free completion queue entries */
	free_phys ( cq->cqe.raw, cq->len );

	DBGC ( ena, "ENA %p CQ%d destroyed\n", ena, cq->id );
	return 0;
}

/**
 * Create queue pair
 *
 * @v ena		ENA device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int ena_create_qp ( struct ena_nic *ena, struct ena_qp *qp ) {
	int rc;

	/* Create completion queue */
	if ( ( rc = ena_create_cq ( ena, &qp->cq ) ) != 0 )
		goto err_create_cq;

	/* Create submission queue */
	if ( ( rc = ena_create_sq ( ena, &qp->sq, &qp->cq ) ) != 0 )
		goto err_create_sq;

	return 0;

	ena_destroy_sq ( ena, &qp->sq );
 err_create_sq:
	ena_destroy_cq ( ena, &qp->cq );
 err_create_cq:
	return rc;
}

/**
 * Destroy queue pair
 *
 * @v ena		ENA device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int ena_destroy_qp ( struct ena_nic *ena, struct ena_qp *qp ) {

	/* Destroy submission queue */
	ena_destroy_sq ( ena,  &qp->sq );

	/* Destroy completion queue */
	ena_destroy_cq ( ena, &qp->cq );

	return 0;
}

/**
 * Get device attributes
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ena_get_device_attributes ( struct net_device *netdev ) {
	struct ena_nic *ena = netdev->priv;
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	union ena_feature *feature;
	int rc;

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_GET_FEATURE;
	req->get_feature.id = ENA_DEVICE_ATTRIBUTES;

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		return rc;

	/* Parse response */
	feature = &rsp->get_feature.feature;
	memcpy ( netdev->hw_addr, feature->device.mac, ETH_ALEN );
	netdev->max_pkt_len = le32_to_cpu ( feature->device.mtu );
	netdev->mtu = ( netdev->max_pkt_len - ETH_HLEN );

	DBGC ( ena, "ENA %p MAC %s MTU %zd\n",
	       ena, eth_ntoa ( netdev->hw_addr ), netdev->max_pkt_len );
	return 0;
}

/**
 * Set host attributes
 *
 * @v ena		ENA device
 * @ret rc		Return status code
 */
static int ena_set_host_attributes ( struct ena_nic *ena ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	union ena_feature *feature;
	int rc;

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_SET_FEATURE;
	req->set_feature.id = ENA_HOST_ATTRIBUTES;
	feature = &req->set_feature.feature;
	feature->host.info = cpu_to_le64 ( virt_to_bus ( ena->info ) );

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get statistics (for debugging)
 *
 * @v ena		ENA device
 * @ret rc		Return status code
 */
static int ena_get_stats ( struct ena_nic *ena ) {
	union ena_aq_req *req;
	union ena_acq_rsp *rsp;
	struct ena_get_stats_rsp *stats;
	int rc;

	/* Do nothing unless debug messages are enabled */
	if ( ! DBG_LOG )
		return 0;

	/* Construct request */
	req = ena_admin_req ( ena );
	req->header.opcode = ENA_GET_STATS;
	req->get_stats.type = ENA_STATS_TYPE_BASIC;
	req->get_stats.scope = ENA_STATS_SCOPE_ETH;
	req->get_stats.device = ENA_DEVICE_MINE;

	/* Issue request */
	if ( ( rc = ena_admin ( ena, req, &rsp ) ) != 0 )
		return rc;

	/* Parse response */
	stats = &rsp->get_stats;
	DBGC ( ena, "ENA %p TX bytes %#llx packets %#llx\n", ena,
	       ( ( unsigned long long ) le64_to_cpu ( stats->tx_bytes ) ),
	       ( ( unsigned long long ) le64_to_cpu ( stats->tx_packets ) ) );
	DBGC ( ena, "ENA %p RX bytes %#llx packets %#llx drops %#llx\n", ena,
	       ( ( unsigned long long ) le64_to_cpu ( stats->rx_bytes ) ),
	       ( ( unsigned long long ) le64_to_cpu ( stats->rx_packets ) ),
	       ( ( unsigned long long ) le64_to_cpu ( stats->rx_drops ) ) );

	return 0;
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Refill receive queue
 *
 * @v netdev		Network device
 */
static void ena_refill_rx ( struct net_device *netdev ) {
	struct ena_nic *ena = netdev->priv;
	struct io_buffer *iobuf;
	struct ena_rx_sqe *sqe;
	physaddr_t address;
	size_t len = netdev->max_pkt_len;
	unsigned int refilled = 0;
	unsigned int index;
	unsigned int id;

	/* Refill queue */
	while ( ( ena->rx.sq.prod - ena->rx.cq.cons ) < ena->rx.sq.fill ) {

		/* Allocate I/O buffer */
		iobuf = alloc_iob ( len );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}

		/* Get next submission queue entry and buffer ID */
		index = ( ena->rx.sq.prod % ENA_RX_COUNT );
		sqe = &ena->rx.sq.sqe.rx[index];
		id = ena->rx_ids[index];

		/* Construct submission queue entry */
		address = virt_to_bus ( iobuf->data );
		sqe->len = cpu_to_le16 ( len );
		sqe->id = cpu_to_le16 ( id );
		sqe->address = cpu_to_le64 ( address );
		wmb();
		sqe->flags = ( ENA_SQE_FIRST | ENA_SQE_LAST | ENA_SQE_CPL |
			       ena->rx.sq.phase );

		/* Increment producer counter */
		ena->rx.sq.prod++;
		if ( ( ena->rx.sq.prod % ENA_RX_COUNT ) == 0 )
			ena->rx.sq.phase ^= ENA_SQE_PHASE;

		/* Record I/O buffer */
		assert ( ena->rx_iobuf[id] == NULL );
		ena->rx_iobuf[id] = iobuf;

		DBGC2 ( ena, "ENA %p RX %d at [%08llx,%08llx)\n", ena, id,
			( ( unsigned long long ) address ),
			( ( unsigned long long ) address + len ) );
		refilled++;
	}

	/* Ring doorbell, if applicable */
	if ( refilled ) {
		wmb();
		writel ( ena->rx.sq.prod, ( ena->regs + ena->rx.sq.doorbell ) );
	}
}

/**
 * Discard unused receive I/O buffers
 *
 * @v ena		ENA device
 */
static void ena_empty_rx ( struct ena_nic *ena ) {
	unsigned int i;

	for ( i = 0 ; i < ENA_RX_COUNT ; i++ ) {
		if ( ena->rx_iobuf[i] )
			free_iob ( ena->rx_iobuf[i] );
		ena->rx_iobuf[i] = NULL;
	}
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ena_open ( struct net_device *netdev ) {
	struct ena_nic *ena = netdev->priv;
	int rc;

	/* Create transmit queue pair */
	if ( ( rc = ena_create_qp ( ena, &ena->tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive queue pair */
	if ( ( rc = ena_create_qp ( ena, &ena->rx ) ) != 0 )
		goto err_create_rx;

	/* Refill receive queue */
	ena_refill_rx ( netdev );

	return 0;

	ena_destroy_qp ( ena, &ena->rx );
 err_create_rx:
	ena_destroy_qp ( ena, &ena->tx );
 err_create_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void ena_close ( struct net_device *netdev ) {
	struct ena_nic *ena = netdev->priv;

	/* Dump statistics (for debugging) */
	ena_get_stats ( ena );

	/* Destroy receive queue pair */
	ena_destroy_qp ( ena, &ena->rx );

	/* Discard any unused receive buffers */
	ena_empty_rx ( ena );

	/* Destroy transmit queue pair */
	ena_destroy_qp ( ena, &ena->tx );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ena_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
	struct ena_nic *ena = netdev->priv;
	struct ena_tx_sqe *sqe;
	physaddr_t address;
	unsigned int index;
	unsigned int id;
	size_t len;

	/* Get next submission queue entry */
	if ( ( ena->tx.sq.prod - ena->tx.cq.cons ) >= ena->tx.sq.fill ) {
		DBGC ( ena, "ENA %p out of transmit descriptors\n", ena );
		return -ENOBUFS;
	}
	index = ( ena->tx.sq.prod % ENA_TX_COUNT );
	sqe = &ena->tx.sq.sqe.tx[index];
	id = ena->tx_ids[index];

	/* Construct submission queue entry */
	address = virt_to_bus ( iobuf->data );
	len = iob_len ( iobuf );
	sqe->len = cpu_to_le16 ( len );
	sqe->id = cpu_to_le16 ( id );
	sqe->address = cpu_to_le64 ( address );
	wmb();
	sqe->flags = ( ENA_SQE_FIRST | ENA_SQE_LAST | ENA_SQE_CPL |
		       ena->tx.sq.phase );
	wmb();

	/* Increment producer counter */
	ena->tx.sq.prod++;
	if ( ( ena->tx.sq.prod % ENA_TX_COUNT ) == 0 )
		ena->tx.sq.phase ^= ENA_SQE_PHASE;

	/* Record I/O buffer */
	assert ( ena->tx_iobuf[id] == NULL );
	ena->tx_iobuf[id] = iobuf;

	/* Ring doorbell */
	writel ( ena->tx.sq.prod, ( ena->regs + ena->tx.sq.doorbell ) );

	DBGC2 ( ena, "ENA %p TX %d at [%08llx,%08llx)\n", ena, id,
		( ( unsigned long long ) address ),
		( ( unsigned long long ) address + len ) );
	return 0;
}

/**
 * Poll for completed transmissions
 *
 * @v netdev		Network device
 */
static void ena_poll_tx ( struct net_device *netdev ) {
	struct ena_nic *ena = netdev->priv;
	struct ena_tx_cqe *cqe;
	struct io_buffer *iobuf;
	unsigned int index;
	unsigned int id;

	/* Check for completed packets */
	while ( ena->tx.cq.cons != ena->tx.sq.prod ) {

		/* Get next completion queue entry */
		index = ( ena->tx.cq.cons & ena->tx.cq.mask );
		cqe = &ena->tx.cq.cqe.tx[index];

		/* Stop if completion queue entry is empty */
		if ( ( cqe->flags ^ ena->tx.cq.phase ) & ENA_CQE_PHASE )
			return;

		/* Increment consumer counter */
		ena->tx.cq.cons++;
		if ( ! ( ena->tx.cq.cons & ena->tx.cq.mask ) )
			ena->tx.cq.phase ^= ENA_CQE_PHASE;

		/* Identify and free buffer ID */
		id = ENA_TX_CQE_ID ( le16_to_cpu ( cqe->id ) );
		ena->tx_ids[index] = id;

		/* Identify I/O buffer */
		iobuf = ena->tx_iobuf[id];
		assert ( iobuf != NULL );
		ena->tx_iobuf[id] = NULL;

		/* Complete transmit */
		DBGC2 ( ena, "ENA %p TX %d complete\n", ena, id );
		netdev_tx_complete ( netdev, iobuf );
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void ena_poll_rx ( struct net_device *netdev ) {
	struct ena_nic *ena = netdev->priv;
	struct ena_rx_cqe *cqe;
	struct io_buffer *iobuf;
	unsigned int index;
	unsigned int id;
	size_t len;

	/* Check for received packets */
	while ( ena->rx.cq.cons != ena->rx.sq.prod ) {

		/* Get next completion queue entry */
		index = ( ena->rx.cq.cons & ena->rx.cq.mask );
		cqe = &ena->rx.cq.cqe.rx[index];

		/* Stop if completion queue entry is empty */
		if ( ( cqe->flags ^ ena->rx.cq.phase ) & ENA_CQE_PHASE )
			return;

		/* Increment consumer counter */
		ena->rx.cq.cons++;
		if ( ! ( ena->rx.cq.cons & ena->rx.cq.mask ) )
			ena->rx.cq.phase ^= ENA_CQE_PHASE;

		/* Identify and free buffer ID */
		id = le16_to_cpu ( cqe->id );
		ena->rx_ids[index] = id;

		/* Populate I/O buffer */
		iobuf = ena->rx_iobuf[id];
		assert ( iobuf != NULL );
		ena->rx_iobuf[id] = NULL;
		len = le16_to_cpu ( cqe->len );
		iob_put ( iobuf, len );

		/* Hand off to network stack */
		DBGC2 ( ena, "ENA %p RX %d complete (length %zd)\n",
			ena, id, len );
		netdev_rx ( netdev, iobuf );
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void ena_poll ( struct net_device *netdev ) {

	/* Poll for transmit completions */
	ena_poll_tx ( netdev );

	/* Poll for receive completions */
	ena_poll_rx ( netdev );

	/* Refill receive ring */
	ena_refill_rx ( netdev );
}

/** ENA network device operations */
static struct net_device_operations ena_operations = {
	.open		= ena_open,
	.close		= ena_close,
	.transmit	= ena_transmit,
	.poll		= ena_poll,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/**
 * Assign memory BAR
 *
 * @v ena		ENA device
 * @v pci		PCI device
 * @ret rc		Return status code
 *
 * Some BIOSes in AWS EC2 are observed to fail to assign a base
 * address to the ENA device.  The device is the only device behind
 * its bridge, and the BIOS does assign a memory window to the bridge.
 * We therefore place the device at the start of the memory window.
 */
static int ena_membase ( struct ena_nic *ena, struct pci_device *pci ) {
	struct pci_bridge *bridge;

	/* Locate PCI bridge */
	bridge = pcibridge_find ( pci );
	if ( ! bridge ) {
		DBGC ( ena, "ENA %p found no PCI bridge\n", ena );
		return -ENOTCONN;
	}

	/* Sanity check */
	if ( PCI_SLOT ( pci->busdevfn ) || PCI_FUNC ( pci->busdevfn ) ) {
		DBGC ( ena, "ENA %p at " PCI_FMT " may not be only device "
		       "on bus\n", ena, PCI_ARGS ( pci ) );
		return -ENOTSUP;
	}

	/* Place device at start of memory window */
	pci_write_config_dword ( pci, PCI_BASE_ADDRESS_0, bridge->membase );
	pci->membase = bridge->membase;
	DBGC ( ena, "ENA %p at " PCI_FMT " claiming bridge " PCI_FMT " mem "
	       "%08x\n", ena, PCI_ARGS ( pci ), PCI_ARGS ( bridge->pci ),
	       bridge->membase );

	return 0;
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int ena_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct ena_nic *ena;
	struct ena_host_info *info;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *ena ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &ena_operations );
	ena = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( ena, 0, sizeof ( *ena ) );
	ena->acq.phase = ENA_ACQ_PHASE;
	ena_cq_init ( &ena->tx.cq, ENA_TX_COUNT,
		      sizeof ( ena->tx.cq.cqe.tx[0] ) );
	ena_sq_init ( &ena->tx.sq, ENA_SQ_TX, ENA_TX_COUNT, ENA_TX_COUNT,
		      sizeof ( ena->tx.sq.sqe.tx[0] ), ena->tx_ids );
	ena_cq_init ( &ena->rx.cq, ENA_RX_COUNT,
		      sizeof ( ena->rx.cq.cqe.rx[0] ) );
	ena_sq_init ( &ena->rx.sq, ENA_SQ_RX, ENA_RX_COUNT, ENA_RX_FILL,
		      sizeof ( ena->rx.sq.sqe.rx[0] ), ena->rx_ids );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Fix up PCI BAR if left unassigned by BIOS */
	if ( ( ! pci->membase ) && ( ( rc = ena_membase ( ena, pci ) ) != 0 ) )
		goto err_membase;

	/* Map registers */
	ena->regs = pci_ioremap ( pci, pci->membase, ENA_BAR_SIZE );
	if ( ! ena->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Allocate and initialise host info */
	info = malloc_phys ( PAGE_SIZE, PAGE_SIZE );
	if ( ! info ) {
		rc = -ENOMEM;
		goto err_info;
	}
	ena->info = info;
	memset ( info, 0, PAGE_SIZE );
	info->type = cpu_to_le32 ( ENA_HOST_INFO_TYPE_IPXE );
	snprintf ( info->dist_str, sizeof ( info->dist_str ), "%s",
		   ( product_name[0] ? product_name : product_short_name ) );
	snprintf ( info->kernel_str, sizeof ( info->kernel_str ), "%s",
		   product_version );
	info->version = cpu_to_le32 ( ENA_HOST_INFO_VERSION_WTF );
	info->spec = cpu_to_le16 ( ENA_HOST_INFO_SPEC_2_0 );
	info->busdevfn = cpu_to_le16 ( pci->busdevfn );
	DBGC2 ( ena, "ENA %p host info:\n", ena );
	DBGC2_HDA ( ena, virt_to_phys ( info ), info, sizeof ( *info ) );

	/* Reset the NIC */
	if ( ( rc = ena_reset ( ena ) ) != 0 )
		goto err_reset;

	/* Create admin queues */
	if ( ( rc = ena_create_admin ( ena ) ) != 0 )
		goto err_create_admin;

	/* Create async event notification queue */
	if ( ( rc = ena_create_async ( ena ) ) != 0 )
		goto err_create_async;

	/* Set host attributes */
	if ( ( rc = ena_set_host_attributes ( ena ) ) != 0 )
		goto err_set_host_attributes;

	/* Fetch MAC address */
	if ( ( rc = ena_get_device_attributes ( netdev ) ) != 0 )
		goto err_get_device_attributes;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Mark as link up, since we have no way to test link state on
	 * this hardware.
	 */
	netdev_link_up ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_get_device_attributes:
 err_set_host_attributes:
	ena_destroy_async ( ena );
 err_create_async:
	ena_destroy_admin ( ena );
 err_create_admin:
	ena_reset ( ena );
 err_reset:
	free_phys ( ena->info, PAGE_SIZE );
 err_info:
	iounmap ( ena->regs );
 err_ioremap:
 err_membase:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void ena_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct ena_nic *ena = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Destroy async event notification queue */
	ena_destroy_async ( ena );

	/* Destroy admin queues */
	ena_destroy_admin ( ena );

	/* Reset card */
	ena_reset ( ena );

	/* Free host info */
	free_phys ( ena->info, PAGE_SIZE );

	/* Free network device */
	iounmap ( ena->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** ENA PCI device IDs */
static struct pci_device_id ena_nics[] = {
	PCI_ROM ( 0x1d0f, 0xec20, "ena-vf", "ENA VF", 0 ),
	PCI_ROM ( 0x1d0f, 0xec21, "ena-vf-llq", "ENA VF (LLQ)", 0 ),
};

/** ENA PCI driver */
struct pci_driver ena_driver __pci_driver = {
	.ids = ena_nics,
	.id_count = ( sizeof ( ena_nics ) / sizeof ( ena_nics[0] ) ),
	.probe = ena_probe,
	.remove = ena_remove,
};
