/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/dma.h>
#include <ipxe/pci.h>
#include <ipxe/fault.h>
#include "gve.h"

/** @file
 *
 * Google Virtual Ethernet network driver
 *
 */

/* Disambiguate the various error causes */
#define EINFO_EIO_ADMIN_UNSET						\
	__einfo_uniqify ( EINFO_EIO, 0x00, "Uncompleted" )
#define EIO_ADMIN_UNSET							\
	__einfo_error ( EINFO_EIO_ADMIN_UNSET )
#define EINFO_EIO_ADMIN_ABORTED						\
	__einfo_uniqify ( EINFO_EIO, 0x10, "Aborted" )
#define EIO_ADMIN_ABORTED						\
	__einfo_error ( EINFO_EIO_ADMIN_ABORTED )
#define EINFO_EIO_ADMIN_EXISTS						\
	__einfo_uniqify ( EINFO_EIO, 0x11, "Already exists" )
#define EIO_ADMIN_EXISTS						\
	__einfo_error ( EINFO_EIO_ADMIN_EXISTS )
#define EINFO_EIO_ADMIN_CANCELLED					\
	__einfo_uniqify ( EINFO_EIO, 0x12, "Cancelled" )
#define EIO_ADMIN_CANCELLED						\
	__einfo_error ( EINFO_EIO_ADMIN_CANCELLED )
#define EINFO_EIO_ADMIN_DATALOSS					\
	__einfo_uniqify ( EINFO_EIO, 0x13, "Data loss" )
#define EIO_ADMIN_DATALOSS						\
	__einfo_error ( EINFO_EIO_ADMIN_DATALOSS )
#define EINFO_EIO_ADMIN_DEADLINE					\
	__einfo_uniqify ( EINFO_EIO, 0x14, "Deadline exceeded" )
#define EIO_ADMIN_DEADLINE						\
	__einfo_error ( EINFO_EIO_ADMIN_DEADLINE )
#define EINFO_EIO_ADMIN_PRECONDITION					\
	__einfo_uniqify ( EINFO_EIO, 0x15, "Failed precondition" )
#define EIO_ADMIN_PRECONDITION						\
	__einfo_error ( EINFO_EIO_ADMIN_PRECONDITION )
#define EINFO_EIO_ADMIN_INTERNAL					\
	__einfo_uniqify ( EINFO_EIO, 0x16, "Internal error" )
#define EIO_ADMIN_INTERNAL						\
	__einfo_error ( EINFO_EIO_ADMIN_INTERNAL )
#define EINFO_EIO_ADMIN_INVAL						\
	__einfo_uniqify ( EINFO_EIO, 0x17, "Invalid argument" )
#define EIO_ADMIN_INVAL							\
	__einfo_error ( EINFO_EIO_ADMIN_INVAL )
#define EINFO_EIO_ADMIN_NOT_FOUND					\
	__einfo_uniqify ( EINFO_EIO, 0x18, "Not found" )
#define EIO_ADMIN_NOT_FOUND						\
	__einfo_error ( EINFO_EIO_ADMIN_NOT_FOUND )
#define EINFO_EIO_ADMIN_RANGE						\
	__einfo_uniqify ( EINFO_EIO, 0x19, "Out of range" )
#define EIO_ADMIN_RANGE							\
	__einfo_error ( EINFO_EIO_ADMIN_RANGE )
#define EINFO_EIO_ADMIN_PERM						\
	__einfo_uniqify ( EINFO_EIO, 0x1a, "Permission denied" )
#define EIO_ADMIN_PERM							\
	__einfo_error ( EINFO_EIO_ADMIN_PERM )
#define EINFO_EIO_ADMIN_UNAUTH						\
	__einfo_uniqify ( EINFO_EIO, 0x1b, "Unauthenticated" )
#define EIO_ADMIN_UNAUTH						\
	__einfo_error ( EINFO_EIO_ADMIN_UNAUTH )
#define EINFO_EIO_ADMIN_RESOURCE					\
	__einfo_uniqify ( EINFO_EIO, 0x1c, "Resource exhausted" )
#define EIO_ADMIN_RESOURCE						\
	__einfo_error ( EINFO_EIO_ADMIN_RESOURCE )
#define EINFO_EIO_ADMIN_UNAVAIL						\
	__einfo_uniqify ( EINFO_EIO, 0x1d, "Unavailable" )
#define EIO_ADMIN_UNAVAIL						\
	__einfo_error ( EINFO_EIO_ADMIN_UNAVAIL )
#define EINFO_EIO_ADMIN_NOTSUP						\
	__einfo_uniqify ( EINFO_EIO, 0x1e, "Unimplemented" )
#define EIO_ADMIN_NOTSUP	       					\
	__einfo_error ( EINFO_EIO_ADMIN_NOTSUP )
#define EINFO_EIO_ADMIN_UNKNOWN						\
	__einfo_uniqify ( EINFO_EIO, 0x1f, "Unknown error" )
#define EIO_ADMIN_UNKNOWN						\
	__einfo_error ( EINFO_EIO_ADMIN_UNKNOWN )
#define EIO_ADMIN( status )						\
	EUNIQ ( EINFO_EIO, ( (status) & 0x1f ),				\
		EIO_ADMIN_UNSET, EIO_ADMIN_ABORTED, EIO_ADMIN_EXISTS,	\
		EIO_ADMIN_CANCELLED, EIO_ADMIN_DATALOSS,		\
		EIO_ADMIN_DEADLINE, EIO_ADMIN_PRECONDITION,		\
		EIO_ADMIN_INTERNAL, EIO_ADMIN_INVAL,			\
		EIO_ADMIN_NOT_FOUND, EIO_ADMIN_RANGE, EIO_ADMIN_PERM,	\
		EIO_ADMIN_UNAUTH, EIO_ADMIN_RESOURCE,			\
		EIO_ADMIN_UNAVAIL, EIO_ADMIN_NOTSUP, EIO_ADMIN_UNKNOWN )

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_reset ( struct gve_nic *gve ) {
	uint32_t pfn;
	unsigned int i;

	/* Skip reset if admin queue page frame number is already
	 * clear.  Triggering a reset on an already-reset device seems
	 * to cause a delayed reset to be scheduled.  This can cause
	 * the device to end up in a reset loop, where each attempt to
	 * recover from reset triggers another reset a few seconds
	 * later.
	 */
	pfn = readl ( gve->cfg + GVE_CFG_ADMIN_PFN );
	if ( ! pfn ) {
		DBGC ( gve, "GVE %p skipping reset\n", gve );
		return 0;
	}

	/* Clear admin queue page frame number */
	writel ( 0, gve->cfg + GVE_CFG_ADMIN_PFN );
	wmb();

	/* Wait for device to reset */
	for ( i = 0 ; i < GVE_RESET_MAX_WAIT_MS ; i++ ) {

		/* Delay */
		mdelay ( 1 );

		/* Check for reset completion */
		pfn = readl ( gve->cfg + GVE_CFG_ADMIN_PFN );
		if ( ! pfn )
			return 0;
	}

	DBGC ( gve, "GVE %p reset timed out (PFN %#08x devstat %#08x)\n",
	       gve, bswap_32 ( pfn ),
	       bswap_32 ( readl ( gve->cfg + GVE_CFG_DEVSTAT ) ) );
	return -ETIMEDOUT;
}

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/**
 * Allocate admin queue
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_admin_alloc ( struct gve_nic *gve ) {
	struct dma_device *dma = gve->dma;
	struct gve_admin *admin = &gve->admin;
	struct gve_scratch *scratch = &gve->scratch;
	size_t admin_len = ( GVE_ADMIN_COUNT * sizeof ( admin->cmd[0] ) );
	size_t scratch_len = sizeof ( *scratch->buf );
	int rc;

	/* Allocate admin queue */
	admin->cmd = dma_alloc ( dma, &admin->map, admin_len, GVE_ALIGN );
	if ( ! admin->cmd ) {
		rc = -ENOMEM;
		goto err_admin;
	}

	/* Allocate scratch buffer */
	scratch->buf = dma_alloc ( dma, &scratch->map, scratch_len, GVE_ALIGN );
	if ( ! scratch->buf ) {
		rc = -ENOMEM;
		goto err_scratch;
	}

	DBGC ( gve, "GVE %p AQ at [%08lx,%08lx) scratch [%08lx,%08lx)\n",
	       gve, virt_to_phys ( admin->cmd ),
	       ( virt_to_phys ( admin->cmd ) + admin_len ),
	       virt_to_phys ( scratch->buf ),
	       ( virt_to_phys ( scratch->buf ) + scratch_len ) );
	return 0;

	dma_free ( &scratch->map, scratch->buf, scratch_len );
 err_scratch:
	dma_free ( &admin->map, admin->cmd, admin_len );
 err_admin:
	return rc;
}

/**
 * Free admin queue
 *
 * @v gve		GVE device
 */
static void gve_admin_free ( struct gve_nic *gve ) {
	struct gve_admin *admin = &gve->admin;
	struct gve_scratch *scratch = &gve->scratch;
	size_t admin_len = ( GVE_ADMIN_COUNT * sizeof ( admin->cmd[0] ) );
	size_t scratch_len = sizeof ( *scratch->buf );

	/* Free scratch buffer */
	dma_free ( &scratch->map, scratch->buf, scratch_len );

	/* Free admin queue */
	dma_free ( &admin->map, admin->cmd, admin_len );
}

/**
 * Enable admin queue
 *
 * @v gve		GVE device
 */
static void gve_admin_enable ( struct gve_nic *gve ) {
	struct gve_admin *admin = &gve->admin;
	size_t admin_len = ( GVE_ADMIN_COUNT * sizeof ( admin->cmd[0] ) );
	physaddr_t base;

	/* Reset queue */
	admin->prod = 0;

	/* Program queue addresses and capabilities */
	base = dma ( &admin->map, admin->cmd );
	writel ( bswap_32 ( base / GVE_PAGE_SIZE ),
		 gve->cfg + GVE_CFG_ADMIN_PFN );
	writel ( bswap_32 ( base & 0xffffffffUL ),
		 gve->cfg + GVE_CFG_ADMIN_BASE_LO );
	if ( sizeof ( base ) > sizeof ( uint32_t ) ) {
		writel ( bswap_32 ( ( ( uint64_t ) base ) >> 32 ),
			 gve->cfg + GVE_CFG_ADMIN_BASE_HI );
	} else {
		writel ( 0, gve->cfg + GVE_CFG_ADMIN_BASE_HI );
	}
	writel ( bswap_16 ( admin_len ), gve->cfg + GVE_CFG_ADMIN_LEN );
	writel ( bswap_32 ( GVE_CFG_DRVSTAT_RUN ), gve->cfg + GVE_CFG_DRVSTAT );
}

/**
 * Get next available admin queue command slot
 *
 * @v gve		GVE device
 * @ret cmd		Admin queue command
 */
static union gve_admin_command * gve_admin_command ( struct gve_nic *gve ) {
	struct gve_admin *admin = &gve->admin;
	union gve_admin_command *cmd;
	unsigned int index;

	/* Get next command slot */
	index = admin->prod;
	cmd = &admin->cmd[ index % GVE_ADMIN_COUNT ];

	/* Initialise request */
	memset ( cmd, 0, sizeof ( *cmd ) );

	return cmd;
}

/**
 * Wait for admin queue command to complete
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_admin_wait ( struct gve_nic *gve ) {
	struct gve_admin *admin = &gve->admin;
	uint32_t evt;
	uint32_t pfn;
	unsigned int i;

	/* Wait for any outstanding commands to complete */
	for ( i = 0 ; i < GVE_ADMIN_MAX_WAIT_MS ; i++ ) {

		/* Check event counter */
		rmb();
		evt = bswap_32 ( readl ( gve->cfg + GVE_CFG_ADMIN_EVT ) );
		if ( evt == admin->prod )
			return 0;

		/* Check for device reset */
		pfn = readl ( gve->cfg + GVE_CFG_ADMIN_PFN );
		if ( ! pfn )
			break;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( gve, "GVE %p AQ %#02x %s (completed %#02x, status %#08x)\n",
	       gve, admin->prod, ( pfn ? "timed out" : "saw reset" ), evt,
	       bswap_32 ( readl ( gve->cfg + GVE_CFG_DEVSTAT ) ) );
	return ( pfn ? -ETIMEDOUT : -ECONNRESET );
}

/**
 * Issue admin queue command
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_admin ( struct gve_nic *gve ) {
	struct gve_admin *admin = &gve->admin;
	union gve_admin_command *cmd;
	unsigned int index;
	uint32_t opcode;
	uint32_t status;
	int rc;

	/* Ensure admin queue is idle */
	if ( ( rc = gve_admin_wait ( gve ) ) != 0 )
		return rc;

	/* Get next command slot */
	index = admin->prod;
	cmd = &admin->cmd[ index % GVE_ADMIN_COUNT ];
	opcode = cmd->hdr.opcode;
	DBGC2 ( gve, "GVE %p AQ %#02x command %#04x request:\n",
		gve, index, opcode );
	DBGC2_HDA ( gve, 0, cmd, sizeof ( *cmd ) );

	/* Increment producer counter */
	admin->prod++;

	/* Ring doorbell */
	wmb();
	writel ( bswap_32 ( admin->prod ), gve->cfg + GVE_CFG_ADMIN_DB );

	/* Wait for command to complete */
	if ( ( rc = gve_admin_wait ( gve ) ) != 0 )
		return rc;

	/* Check command status */
	status = be32_to_cpu ( cmd->hdr.status );
	if ( status != GVE_ADMIN_STATUS_OK ) {
		rc = -EIO_ADMIN ( status );
		DBGC ( gve, "GVE %p AQ %#02x command %#04x failed: %#08x\n",
		       gve, index, opcode, status );
		DBGC_HDA ( gve, 0, cmd, sizeof ( *cmd ) );
		DBGC ( gve, "GVE %p AQ error: %s\n", gve, strerror ( rc ) );
		return rc;
	}

	DBGC2 ( gve, "GVE %p AQ %#02x command %#04x result:\n",
		gve, index, opcode );
	DBGC2_HDA ( gve, 0, cmd, sizeof ( *cmd ) );
	return 0;
}

/**
 * Issue simple admin queue command
 *
 * @v gve		GVE device
 * @v opcode		Operation code
 * @v id		ID parameter (or zero if not applicable)
 * @ret rc		Return status code
 *
 * Several admin queue commands take either an empty parameter list or
 * a single 32-bit ID parameter.
 */
static int gve_admin_simple ( struct gve_nic *gve, unsigned int opcode,
			      unsigned int id ) {
	union gve_admin_command *cmd;
	int rc;

	/* Construct request */
	cmd = gve_admin_command ( gve );
	cmd->hdr.opcode = opcode;
	cmd->simple.id = cpu_to_be32 ( id );

	/* Issue command */
	if ( ( rc = gve_admin ( gve ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get device descriptor
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_describe ( struct gve_nic *gve ) {
	struct net_device *netdev = gve->netdev;
	struct gve_device_descriptor *desc = &gve->scratch.buf->desc;
	union gve_admin_command *cmd;
	int rc;

	/* Construct request */
	cmd = gve_admin_command ( gve );
	cmd->hdr.opcode = GVE_ADMIN_DESCRIBE;
	cmd->desc.addr = cpu_to_be64 ( dma ( &gve->scratch.map, desc ) );
	cmd->desc.ver = cpu_to_be32 ( GVE_ADMIN_DESCRIBE_VER );
	cmd->desc.len = cpu_to_be32 ( sizeof ( *desc ) );

	/* Issue command */
	if ( ( rc = gve_admin ( gve ) ) != 0 )
		return rc;
	DBGC2 ( gve, "GVE %p device descriptor:\n", gve );
	DBGC2_HDA ( gve, 0, desc, sizeof ( *desc ) );

	/* Extract queue parameters */
	gve->events.count = be16_to_cpu ( desc->counters );
	gve->tx.count = be16_to_cpu ( desc->tx_count );
	gve->rx.count = be16_to_cpu ( desc->rx_count );
	DBGC ( gve, "GVE %p using %d TX, %d RX, %d events\n",
	       gve, gve->tx.count, gve->rx.count, gve->events.count );

	/* Extract network parameters */
	build_assert ( sizeof ( desc->mac ) == ETH_ALEN );
	memcpy ( netdev->hw_addr, &desc->mac, sizeof ( desc->mac ) );
	netdev->mtu = be16_to_cpu ( desc->mtu );
	netdev->max_pkt_len = ( netdev->mtu + ETH_HLEN );
	DBGC ( gve, "GVE %p MAC %s (\"%s\") MTU %zd\n",
	       gve, eth_ntoa ( netdev->hw_addr ),
	       inet_ntoa ( desc->mac.in ), netdev->mtu );

	return 0;
}

/**
 * Configure device resources
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_configure ( struct gve_nic *gve ) {
	struct gve_events *events = &gve->events;
	struct gve_irqs *irqs = &gve->irqs;
	union gve_admin_command *cmd;
	unsigned int db_off;
	unsigned int i;
	int rc;

	/* Construct request */
	cmd = gve_admin_command ( gve );
	cmd->hdr.opcode = GVE_ADMIN_CONFIGURE;
	cmd->conf.events =
		cpu_to_be64 ( dma ( &events->map, events->event ) );
	cmd->conf.irqs =
		cpu_to_be64 ( dma ( &irqs->map, irqs->irq ) );
	cmd->conf.num_events = cpu_to_be32 ( events->count );
	cmd->conf.num_irqs = cpu_to_be32 ( GVE_IRQ_COUNT );
	cmd->conf.irq_stride = cpu_to_be32 ( sizeof ( irqs->irq[0] ) );

	/* Issue command */
	if ( ( rc = gve_admin ( gve ) ) != 0 )
		return rc;

	/* Disable all interrupts */
	for ( i = 0 ; i < GVE_IRQ_COUNT ; i++ ) {
		db_off = ( be32_to_cpu ( irqs->irq[i].db_idx ) *
			   sizeof ( uint32_t ) );
		DBGC ( gve, "GVE %p IRQ %d doorbell +%#04x\n", gve, i, db_off );
		irqs->db[i] = ( gve->db + db_off );
		writel ( bswap_32 ( GVE_IRQ_DISABLE ), irqs->db[i] );
	}

	return 0;
}

/**
 * Deconfigure device resources
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_deconfigure ( struct gve_nic *gve ) {
	int rc;

	/* Issue command (with meaningless ID) */
	if ( ( rc = gve_admin_simple ( gve, GVE_ADMIN_DECONFIGURE, 0 ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Register queue page list
 *
 * @v gve		GVE device
 * @v qpl		Queue page list
 * @ret rc		Return status code
 */
static int gve_register ( struct gve_nic *gve, struct gve_qpl *qpl ) {
	struct gve_pages *pages = &gve->scratch.buf->pages;
	union gve_admin_command *cmd;
	void *addr;
	unsigned int i;
	int rc;

	/* Build page address list */
	for ( i = 0 ; i < qpl->count ; i++ ) {
		addr = ( qpl->data + ( i * GVE_PAGE_SIZE ) );
		pages->addr[i] = cpu_to_be64 ( dma ( &qpl->map, addr ) );
	}

	/* Construct request */
	cmd = gve_admin_command ( gve );
	cmd->hdr.opcode = GVE_ADMIN_REGISTER;
	cmd->reg.id = cpu_to_be32 ( qpl->id );
	cmd->reg.count = cpu_to_be32 ( qpl->count );
	cmd->reg.addr = cpu_to_be64 ( dma ( &gve->scratch.map, pages ) );
	cmd->reg.size = cpu_to_be64 ( GVE_PAGE_SIZE );

	/* Issue command */
	if ( ( rc = gve_admin ( gve ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Unregister page list
 *
 * @v gve		GVE device
 * @v qpl		Queue page list
 * @ret rc		Return status code
 */
static int gve_unregister ( struct gve_nic *gve, struct gve_qpl *qpl ) {
	int rc;

	/* Issue command */
	if ( ( rc = gve_admin_simple ( gve, GVE_ADMIN_UNREGISTER,
				       qpl->id ) ) != 0 ) {
		return rc;
	}

	return 0;
}

/**
 * Construct command to create transmit queue
 *
 * @v queue		Transmit queue
 * @v cmd		Admin queue command
 */
static void gve_create_tx_param ( struct gve_queue *queue,
				  union gve_admin_command *cmd ) {
	struct gve_admin_create_tx *create = &cmd->create_tx;
	const struct gve_queue_type *type = queue->type;

	/* Construct request parameters */
	create->res = cpu_to_be64 ( dma ( &queue->res_map, queue->res ) );
	create->desc =
		cpu_to_be64 ( dma ( &queue->desc_map, queue->desc.tx ) );
	create->qpl_id = cpu_to_be32 ( type->qpl );
	create->notify_id = cpu_to_be32 ( type->irq );
}

/**
 * Construct command to create receive queue
 *
 * @v queue		Receive queue
 * @v cmd		Admin queue command
 */
static void gve_create_rx_param ( struct gve_queue *queue,
				  union gve_admin_command *cmd ) {
	struct gve_admin_create_rx *create = &cmd->create_rx;
	const struct gve_queue_type *type = queue->type;

	/* Construct request parameters */
	create->notify_id = cpu_to_be32 ( type->irq );
	create->res = cpu_to_be64 ( dma ( &queue->res_map, queue->res ) );
	create->desc =
		cpu_to_be64 ( dma ( &queue->desc_map, queue->desc.rx ) );
	create->cmplt =
		cpu_to_be64 ( dma ( &queue->cmplt_map, queue->cmplt.rx ) );
	create->qpl_id = cpu_to_be32 ( type->qpl );
	create->bufsz = cpu_to_be16 ( GVE_BUF_SIZE );
}

/**
 * Create transmit or receive queue
 *
 * @v gve		GVE device
 * @v queue		Descriptor queue
 * @ret rc		Return status code
 */
static int gve_create_queue ( struct gve_nic *gve, struct gve_queue *queue ) {
	const struct gve_queue_type *type = queue->type;
	union gve_admin_command *cmd;
	unsigned int db_off;
	unsigned int evt_idx;
	int rc;

	/* Reset queue */
	queue->prod = 0;
	queue->cons = 0;

	/* Construct request */
	cmd = gve_admin_command ( gve );
	cmd->hdr.opcode = type->create;
	type->param ( queue, cmd );

	/* Issue command */
	if ( ( rc = gve_admin ( gve ) ) != 0 )
		return rc;

	/* Record indices */
	db_off = ( be32_to_cpu ( queue->res->db_idx ) * sizeof ( uint32_t ) );
	evt_idx = be32_to_cpu ( queue->res->evt_idx );
	DBGC ( gve, "GVE %p %s doorbell +%#04x event counter %d\n",
	       gve, type->name, db_off, evt_idx );
	queue->db = ( gve->db + db_off );
	assert ( evt_idx < gve->events.count );
	queue->event = &gve->events.event[evt_idx];
	assert ( queue->event->count == 0 );

	return 0;
}

/**
 * Destroy transmit or receive queue
 *
 * @v gve		GVE device
 * @v queue		Descriptor queue
 * @ret rc		Return status code
 */
static int gve_destroy_queue ( struct gve_nic *gve, struct gve_queue *queue ) {
	const struct gve_queue_type *type = queue->type;
	int rc;

	/* Issue command */
	if ( ( rc = gve_admin_simple ( gve, type->destroy, 0 ) ) != 0 )
		return rc;

	return 0;
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Allocate shared queue resources
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_alloc_shared ( struct gve_nic *gve ) {
	struct dma_device *dma = gve->dma;
	struct gve_irqs *irqs = &gve->irqs;
	struct gve_events *events = &gve->events;
	size_t irqs_len = ( GVE_IRQ_COUNT * sizeof ( irqs->irq[0] ) );
	size_t events_len = ( gve->events.count * sizeof ( events->event[0] ) );
	int rc;

	/* Allocate interrupt channels */
	irqs->irq = dma_alloc ( dma, &irqs->map, irqs_len, GVE_ALIGN );
	if ( ! irqs->irq ) {
		rc = -ENOMEM;
		goto err_irqs;
	}
	DBGC ( gve, "GVE %p IRQs at [%08lx,%08lx)\n",
	       gve, virt_to_phys ( irqs->irq ),
	       ( virt_to_phys ( irqs->irq ) + irqs_len ) );

	/* Allocate event counters */
	events->event = dma_alloc ( dma, &events->map, events_len, GVE_ALIGN );
	if ( ! events->event ) {
		rc = -ENOMEM;
		goto err_events;
	}
	DBGC ( gve, "GVE %p events at [%08lx,%08lx)\n",
	       gve, virt_to_phys ( events->event ),
	       ( virt_to_phys ( events->event ) + events_len ) );

	return 0;

	dma_free ( &events->map, events->event, events_len );
 err_events:
	dma_free ( &irqs->map, irqs->irq, irqs_len );
 err_irqs:
	return rc;
}

/**
 * Free shared queue resources
 *
 * @v gve		GVE device
 */
static void gve_free_shared ( struct gve_nic *gve ) {
	struct gve_irqs *irqs = &gve->irqs;
	struct gve_events *events = &gve->events;
	size_t irqs_len = ( GVE_IRQ_COUNT * sizeof ( irqs->irq[0] ) );
	size_t events_len = ( gve->events.count * sizeof ( events->event[0] ) );

	/* Free event counters */
	dma_free ( &events->map, events->event, events_len );

	/* Free interrupt channels */
	dma_free ( &irqs->map, irqs->irq, irqs_len );
}

/**
 * Allocate queue page list
 *
 * @v gve		GVE device
 * @v qpl		Queue page list
 * @v id		Queue page list ID
 * @v buffers		Number of data buffers
 * @ret rc		Return status code
 */
static int gve_alloc_qpl ( struct gve_nic *gve, struct gve_qpl *qpl,
			   uint32_t id, unsigned int buffers ) {
	size_t len;

	/* Record ID */
	qpl->id = id;

	/* Calculate number of pages required */
	build_assert ( GVE_BUF_SIZE <= GVE_PAGE_SIZE );
	qpl->count = ( ( buffers + GVE_BUF_PER_PAGE - 1 ) / GVE_BUF_PER_PAGE );
	assert ( qpl->count <= GVE_QPL_MAX );

	/* Allocate pages (as a single block) */
	len = ( qpl->count * GVE_PAGE_SIZE );
	qpl->data = dma_umalloc ( gve->dma, &qpl->map, len, GVE_ALIGN );
	if ( ! qpl->data )
		return -ENOMEM;

	DBGC ( gve, "GVE %p QPL %#08x at [%08lx,%08lx)\n",
	       gve, qpl->id, virt_to_phys ( qpl->data ),
	       ( virt_to_phys ( qpl->data ) + len ) );
	return 0;
}

/**
 * Free queue page list
 *
 * @v gve		GVE device
 * @v qpl		Queue page list
 */
static void gve_free_qpl ( struct gve_nic *nic __unused,
			   struct gve_qpl *qpl ) {
	size_t len = ( qpl->count * GVE_PAGE_SIZE );

	/* Free pages */
	dma_ufree ( &qpl->map, qpl->data, len );
}

/**
 * Get buffer address (within queue page list address space)
 *
 * @v queue		Descriptor queue
 * @v index		Buffer index
 * @ret addr		Buffer address within queue page list address space
 */
static inline __attribute__ (( always_inline)) size_t
gve_address ( struct gve_queue *queue, unsigned int index ) {

	/* We allocate sufficient pages for the maximum fill level of
	 * buffers, and reuse the pages in strict rotation as we
	 * progress through the queue.
	 */
	return ( ( index & ( queue->fill - 1 ) ) * GVE_BUF_SIZE );
}

/**
 * Get buffer address
 *
 * @v queue		Descriptor queue
 * @v index		Buffer index
 * @ret addr		Buffer address
 */
static inline __attribute__ (( always_inline )) void *
gve_buffer ( struct gve_queue *queue, unsigned int index ) {

	/* Pages are currently allocated as a single contiguous block */
	return ( queue->qpl.data + gve_address ( queue, index ) );
}

/**
 * Calculate next receive sequence number
 *
 * @v seq		Current sequence number, or zero to start sequence
 * @ret next		Next sequence number
 */
static inline __attribute__ (( always_inline )) unsigned int
gve_next ( unsigned int seq ) {

	/* The receive completion sequence number is a modulo 7
	 * counter that cycles through the non-zero three-bit values 1
	 * to 7 inclusive.
	 *
	 * Since 7 is coprime to 2^n, this ensures that the sequence
	 * number changes each time that a new completion is written
	 * to memory.
	 *
	 * Since the counter takes only non-zero values, this ensures
	 * that the sequence number changes whenever a new completion
	 * is first written to a zero-initialised completion ring.
	 */
	seq = ( ( seq + 1 ) & GVE_RX_SEQ_MASK );
	return ( seq ? seq : 1 );
}

/**
 * Allocate descriptor queue
 *
 * @v gve		GVE device
 * @v queue		Descriptor queue
 * @ret rc		Return status code
 */
static int gve_alloc_queue ( struct gve_nic *gve, struct gve_queue *queue ) {
	const struct gve_queue_type *type = queue->type;
	struct dma_device *dma = gve->dma;
	size_t desc_len = ( queue->count * type->desc_len );
	size_t cmplt_len = ( queue->count * type->cmplt_len );
	size_t res_len = sizeof ( *queue->res );
	struct gve_buffer *buf;
	unsigned int i;
	int rc;

	/* Sanity checks */
	if ( ( queue->count == 0 ) ||
	     ( queue->count & ( queue->count - 1 ) ) ) {
		DBGC ( gve, "GVE %p %s invalid queue size %d\n",
		       gve, type->name, queue->count );
		rc = -EINVAL;
		goto err_sanity;
	}

	/* Calculate maximum fill level */
	assert ( ( type->fill & ( type->fill - 1 ) ) == 0 );
	queue->fill = type->fill;
	if ( queue->fill > queue->count )
		queue->fill = queue->count;
	DBGC ( gve, "GVE %p %s using QPL %#08x with %d/%d descriptors\n",
	       gve, type->name, type->qpl, queue->fill, queue->count );

	/* Allocate queue page list */
	if ( ( rc = gve_alloc_qpl ( gve, &queue->qpl, type->qpl,
				    queue->fill ) ) != 0 )
		goto err_qpl;

	/* Allocate descriptors */
	queue->desc.raw = dma_umalloc ( dma, &queue->desc_map, desc_len,
					GVE_ALIGN );
	if ( ! queue->desc.raw ) {
		rc = -ENOMEM;
		goto err_desc;
	}
	DBGC ( gve, "GVE %p %s descriptors at [%08lx,%08lx)\n",
	       gve, type->name, virt_to_phys ( queue->desc.raw ),
	       ( virt_to_phys ( queue->desc.raw ) + desc_len ) );

	/* Allocate completions */
	if ( cmplt_len ) {
		queue->cmplt.raw = dma_umalloc ( dma, &queue->cmplt_map,
						 cmplt_len, GVE_ALIGN );
		if ( ! queue->cmplt.raw ) {
			rc = -ENOMEM;
			goto err_cmplt;
		}
		DBGC ( gve, "GVE %p %s completions at [%08lx,%08lx)\n",
		       gve, type->name, virt_to_phys ( queue->cmplt.raw ),
		       ( virt_to_phys ( queue->cmplt.raw ) + cmplt_len ) );
	}

	/* Allocate queue resources */
	queue->res = dma_alloc ( dma, &queue->res_map, res_len, GVE_ALIGN );
	if ( ! queue->res ) {
		rc = -ENOMEM;
		goto err_res;
	}
	memset ( queue->res, 0, res_len );

	/* Populate descriptor offsets */
	buf = ( queue->desc.raw + type->desc_len - sizeof ( *buf ) );
	for ( i = 0 ; i < queue->count ; i++ ) {
		buf->addr = cpu_to_be64 ( gve_address ( queue, i ) );
		buf = ( ( ( void * ) buf ) + type->desc_len );
	}

	return 0;

	dma_free ( &queue->res_map, queue->res, res_len );
 err_res:
	if ( cmplt_len )
		dma_ufree ( &queue->cmplt_map, queue->cmplt.raw, cmplt_len );
 err_cmplt:
	dma_ufree ( &queue->desc_map, queue->desc.raw, desc_len );
 err_desc:
	gve_free_qpl ( gve, &queue->qpl );
 err_qpl:
 err_sanity:
	return rc;
}

/**
 * Free descriptor queue
 *
 * @v gve		GVE device
 * @v queue		Descriptor queue
 */
static void gve_free_queue ( struct gve_nic *gve, struct gve_queue *queue ) {
	const struct gve_queue_type *type = queue->type;
	size_t desc_len = ( queue->count * type->desc_len );
	size_t cmplt_len = ( queue->count * type->cmplt_len );
	size_t res_len = sizeof ( *queue->res );

	/* Free queue resources */
	dma_free ( &queue->res_map, queue->res, res_len );

	/* Free completions, if applicable */
	if ( cmplt_len )
		dma_ufree ( &queue->cmplt_map, queue->cmplt.raw, cmplt_len );

	/* Free descriptors */
	dma_ufree ( &queue->desc_map, queue->desc.raw, desc_len );

	/* Free queue page list */
	gve_free_qpl ( gve, &queue->qpl );
}

/**
 * Start up device
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_start ( struct gve_nic *gve ) {
	struct net_device *netdev = gve->netdev;
	struct gve_queue *tx = &gve->tx;
	struct gve_queue *rx = &gve->rx;
	struct io_buffer *iobuf;
	unsigned int i;
	int rc;

	/* Cancel any pending transmissions */
	for ( i = 0 ; i < ( sizeof ( gve->tx_iobuf ) /
			    sizeof ( gve->tx_iobuf[0] ) ) ; i++ ) {
		iobuf = gve->tx_iobuf[i];
		gve->tx_iobuf[i] = NULL;
		if ( iobuf )
			netdev_tx_complete_err ( netdev, iobuf, -ECANCELED );
	}

	/* Invalidate receive completions */
	memset ( rx->cmplt.raw, 0, ( rx->count * rx->type->cmplt_len ) );

	/* Reset receive sequence */
	gve->seq = gve_next ( 0 );

	/* Configure device resources */
	if ( ( rc = gve_configure ( gve ) ) != 0 )
		goto err_configure;

	/* Register transmit queue page list */
	if ( ( rc = gve_register ( gve, &tx->qpl ) ) != 0 )
		goto err_register_tx;

	/* Register receive queue page list */
	if ( ( rc = gve_register ( gve, &rx->qpl ) ) != 0 )
		goto err_register_rx;

	/* Create transmit queue */
	if ( ( rc = gve_create_queue ( gve, tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive queue */
	if ( ( rc = gve_create_queue ( gve, rx ) ) != 0 )
		goto err_create_rx;

	return 0;

	gve_destroy_queue ( gve, rx );
 err_create_rx:
	gve_destroy_queue ( gve, tx );
 err_create_tx:
	gve_unregister ( gve, &rx->qpl );
 err_register_rx:
	gve_unregister ( gve, &tx->qpl );
 err_register_tx:
	gve_deconfigure ( gve );
 err_configure:
	return rc;
}

/**
 * Stop device
 *
 * @v gve		GVE device
 */
static void gve_stop ( struct gve_nic *gve ) {
	struct gve_queue *tx = &gve->tx;
	struct gve_queue *rx = &gve->rx;

	/* Destroy queues */
	gve_destroy_queue ( gve, rx );
	gve_destroy_queue ( gve, tx );

	/* Unregister page lists */
	gve_unregister ( gve, &rx->qpl );
	gve_unregister ( gve, &tx->qpl );

	/* Deconfigure device */
	gve_deconfigure ( gve );
}

/**
 * Device startup process
 *
 * @v gve		GVE device
 */
static void gve_startup ( struct gve_nic *gve ) {
	struct net_device *netdev = gve->netdev;
	int rc;

	/* Reset device */
	if ( ( rc = gve_reset ( gve ) ) != 0 )
		goto err_reset;

	/* Enable admin queue */
	gve_admin_enable ( gve );

	/* Start device */
	if ( ( rc = gve_start ( gve ) ) != 0 )
		goto err_start;

	/* Reset retry count */
	gve->retries = 0;

	/* (Ab)use link status to report startup status */
	netdev_link_up ( netdev );

	return;

	gve_stop ( gve );
 err_start:
 err_reset:
	DBGC ( gve, "GVE %p startup failed: %s\n", gve, strerror ( rc ) );
	netdev_link_err ( netdev, rc );
	if ( gve->retries++ < GVE_RESET_MAX_RETRY )
		process_add ( &gve->startup );
}

/**
 * Trigger startup process
 *
 * @v gve		GVE device
 */
static void gve_restart ( struct gve_nic *gve ) {
	struct net_device *netdev = gve->netdev;

	/* Mark link down to inhibit polling and transmit activity */
	netdev_link_down ( netdev );

	/* Schedule startup process */
	process_add ( &gve->startup );
}

/**
 * Reset recovery watchdog
 *
 * @v timer		Reset recovery watchdog timer
 * @v over		Failure indicator
 */
static void gve_watchdog ( struct retry_timer *timer, int over __unused ) {
	struct gve_nic *gve = container_of ( timer, struct gve_nic, watchdog );
	uint32_t activity;
	uint32_t pfn;
	int rc;

	/* Reschedule watchdog */
	start_timer_fixed ( &gve->watchdog, GVE_WATCHDOG_TIMEOUT );

	/* Reset device (for test purposes) if applicable */
	if ( ( rc = inject_fault ( VM_MIGRATED_RATE ) ) != 0 ) {
		DBGC ( gve, "GVE %p synthesising host reset\n", gve );
		writel ( 0, gve->cfg + GVE_CFG_ADMIN_PFN );
	}

	/* Check for activity since last timer invocation */
	activity = ( gve->tx.cons + gve->rx.cons );
	if ( activity != gve->activity ) {
		gve->activity = activity;
		return;
	}

	/* Check for reset */
	pfn = readl ( gve->cfg + GVE_CFG_ADMIN_PFN );
	if ( pfn ) {
		DBGC2 ( gve, "GVE %p idle but not in reset\n", gve );
		return;
	}

	/* Schedule restart */
	DBGC ( gve, "GVE %p watchdog detected reset by host\n", gve );
	gve_restart ( gve );
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int gve_open ( struct net_device *netdev ) {
	struct gve_nic *gve = netdev->priv;
	struct gve_queue *tx = &gve->tx;
	struct gve_queue *rx = &gve->rx;
	int rc;

	/* Allocate shared queue resources */
	if ( ( rc = gve_alloc_shared ( gve ) ) != 0 )
		goto err_alloc_shared;

	/* Allocate and prepopulate transmit queue */
	if ( ( rc = gve_alloc_queue ( gve, tx ) ) != 0 )
		goto err_alloc_tx;

	/* Allocate and prepopulate receive queue */
	if ( ( rc = gve_alloc_queue ( gve, rx ) ) != 0 )
		goto err_alloc_rx;

	/* Trigger startup */
	gve_restart ( gve );

	/* Start reset recovery watchdog timer */
	start_timer_fixed ( &gve->watchdog, GVE_WATCHDOG_TIMEOUT );

	return 0;

	gve_free_queue ( gve, rx );
 err_alloc_rx:
	gve_free_queue ( gve, tx );
 err_alloc_tx:
	gve_free_shared ( gve );
 err_alloc_shared:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void gve_close ( struct net_device *netdev ) {
	struct gve_nic *gve = netdev->priv;
	struct gve_queue *tx = &gve->tx;
	struct gve_queue *rx = &gve->rx;

	/* Stop reset recovery timer */
	stop_timer ( &gve->watchdog );

	/* Terminate startup process */
	process_del ( &gve->startup );

	/* Stop and reset device */
	gve_stop ( gve );
	gve_reset ( gve );

	/* Free queues */
	gve_free_queue ( gve, rx );
	gve_free_queue ( gve, tx );

	/* Free shared queue resources */
	gve_free_shared ( gve );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int gve_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
	struct gve_nic *gve = netdev->priv;
	struct gve_queue *tx = &gve->tx;
	struct gve_tx_descriptor *desc;
	unsigned int count;
	unsigned int index;
	size_t frag_len;
	size_t offset;
	size_t len;

	/* Do nothing if queues are not yet set up */
	if ( ! netdev_link_ok ( netdev ) )
		return -ENETDOWN;

	/* Defer packet if there is no space in the transmit ring */
	len = iob_len ( iobuf );
	count = ( ( len + GVE_BUF_SIZE - 1 ) / GVE_BUF_SIZE );
	if ( ( ( tx->prod - tx->cons ) + count ) > tx->fill ) {
		netdev_tx_defer ( netdev, iobuf );
		return 0;
	}

	/* Copy packet to queue pages and populate descriptors */
	for ( offset = 0 ; offset < len ; offset += frag_len ) {

		/* Sanity check */
		assert ( gve->tx_iobuf[ tx->prod % GVE_TX_FILL ] == NULL );

		/* Copy packet fragment */
		frag_len = ( len - offset );
		if ( frag_len > GVE_BUF_SIZE )
			frag_len = GVE_BUF_SIZE;
		memcpy ( gve_buffer ( tx, tx->prod ),
			 ( iobuf->data + offset ), frag_len );

		/* Populate descriptor */
		index = ( tx->prod++ & ( tx->count - 1 ) );
		desc = &tx->desc.tx[index];
		memset ( &desc->pkt, 0, sizeof ( desc->pkt ) );
		if ( offset ) {
			desc->pkt.type = GVE_TX_TYPE_CONT;
		} else {
			desc->pkt.type = GVE_TX_TYPE_START;
			desc->pkt.count = count;
			desc->pkt.total = cpu_to_be16 ( len );
		}
		desc->pkt.len = cpu_to_be16 ( frag_len );
		DBGC2 ( gve, "GVE %p TX %#04x %#02x:%#02x len %#04x/%#04x at "
			"%#08zx\n", gve, index, desc->pkt.type,
			desc->pkt.count, be16_to_cpu ( desc->pkt.len ),
			be16_to_cpu ( desc->pkt.total ),
			gve_address ( tx, index ) );
	}
	assert ( ( tx->prod - tx->cons ) <= tx->fill );

	/* Record I/O buffer against final descriptor */
	gve->tx_iobuf[ ( tx->prod - 1U ) % GVE_TX_FILL ] = iobuf;

	/* Ring doorbell */
	wmb();
	writel ( bswap_32 ( tx->prod ), tx->db );

	return 0;
}

/**
 * Poll for completed transmissions
 *
 * @v netdev		Network device
 */
static void gve_poll_tx ( struct net_device *netdev ) {
	struct gve_nic *gve = netdev->priv;
	struct gve_queue *tx = &gve->tx;
	struct io_buffer *iobuf;
	uint32_t count;

	/* Read event counter */
	count = be32_to_cpu ( tx->event->count );

	/* Process transmit completions */
	while ( count != tx->cons ) {
		DBGC2 ( gve, "GVE %p TX %#04x complete\n", gve, tx->cons );
		iobuf = gve->tx_iobuf[ tx->cons % GVE_TX_FILL ];
		gve->tx_iobuf[ tx->cons % GVE_TX_FILL ] = NULL;
		tx->cons++;
		if ( iobuf )
			netdev_tx_complete ( netdev, iobuf );
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void gve_poll_rx ( struct net_device *netdev ) {
	struct gve_nic *gve = netdev->priv;
	struct gve_queue *rx = &gve->rx;
	struct gve_rx_completion *cmplt;
	struct io_buffer *iobuf;
	unsigned int index;
	unsigned int seq;
	uint32_t cons;
	size_t total;
	size_t len;
	int rc;

	/* Process receive completions */
	cons = rx->cons;
	seq = gve->seq;
	total = 0;
	while ( 1 ) {

		/* Read next possible completion */
		index = ( cons++ & ( rx->count - 1 ) );
		cmplt = &rx->cmplt.rx[index];

		/* Check sequence number */
		if ( ( cmplt->pkt.seq & GVE_RX_SEQ_MASK ) != seq )
			break;
		seq = gve_next ( seq );

		/* Parse completion */
		len = be16_to_cpu ( cmplt->pkt.len );
		DBGC2 ( gve, "GVE %p RX %#04x %#02x:%#02x len %#04zx at "
			"%#08zx\n", gve, index, cmplt->pkt.seq,
			cmplt->pkt.flags, len, gve_address ( rx, index ) );

		/* Accumulate a complete packet */
		if ( cmplt->pkt.flags & GVE_RXF_ERROR ) {
			total = 0;
		} else {
			total += len;
			if ( cmplt->pkt.flags & GVE_RXF_MORE )
				continue;
		}
		gve->seq = seq;

		/* Allocate and populate I/O buffer */
		iobuf = ( total ? alloc_iob ( total ) : NULL );
		for ( ; rx->cons != cons ; rx->cons++ ) {

			/* Re-read completion length */
			index = ( rx->cons & ( rx->count - 1 ) );
			cmplt = &rx->cmplt.rx[index];

			/* Copy data */
			if ( iobuf ) {
				len = be16_to_cpu ( cmplt->pkt.len );
				memcpy ( iob_put ( iobuf, len ),
					 gve_buffer ( rx, rx->cons ), len );
			}
		}
		assert ( ( iobuf == NULL ) || ( iob_len ( iobuf ) == total ) );
		total = 0;

		/* Hand off packet to network stack */
		if ( iobuf ) {
			iob_pull ( iobuf, GVE_RX_PAD );
			netdev_rx ( netdev, iobuf );
		} else {
			rc = ( ( cmplt->pkt.flags & GVE_RXF_ERROR ) ?
			       -EIO : -ENOMEM );
			netdev_rx_err ( netdev, NULL, rc );
		}

		/* Sanity check */
		assert ( rx->cons == cons );
		assert ( gve->seq == seq );
		assert ( total == 0 );
	}
}

/**
 * Refill receive queue
 *
 * @v netdev		Network device
 */
static void gve_refill_rx ( struct net_device *netdev ) {
	struct gve_nic *gve = netdev->priv;
	struct gve_queue *rx = &gve->rx;
	unsigned int prod;

	/* The receive descriptors are prepopulated at the time of
	 * creating the receive queue (pointing to the preallocated
	 * queue pages).  Refilling is therefore just a case of
	 * ringing the doorbell if the device is not yet aware of any
	 * available descriptors.
	 */
	prod = ( rx->cons + rx->fill );
	if ( prod != rx->prod ) {
		rx->prod = prod;
		writel ( bswap_32 ( prod ), rx->db );
		DBGC2 ( gve, "GVE %p RX %#04x ready\n", gve, rx->prod );
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void gve_poll ( struct net_device *netdev ) {

	/* Do nothing if queues are not yet set up */
	if ( ! netdev_link_ok ( netdev ) )
		return;

	/* Poll for transmit completions */
	gve_poll_tx ( netdev );

	/* Poll for receive completions */
	gve_poll_rx ( netdev );

	/* Refill receive queue */
	gve_refill_rx ( netdev );
}

/** GVE network device operations */
static struct net_device_operations gve_operations = {
	.open		= gve_open,
	.close		= gve_close,
	.transmit	= gve_transmit,
	.poll		= gve_poll,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/** Transmit descriptor queue type */
static const struct gve_queue_type gve_tx_type = {
	.name = "TX",
	.param = gve_create_tx_param,
	.qpl = GVE_TX_QPL,
	.irq = GVE_TX_IRQ,
	.fill = GVE_TX_FILL,
	.desc_len = sizeof ( struct gve_tx_descriptor ),
	.create = GVE_ADMIN_CREATE_TX,
	.destroy = GVE_ADMIN_DESTROY_TX,
};

/** Receive descriptor queue type */
static const struct gve_queue_type gve_rx_type = {
	.name = "RX",
	.param = gve_create_rx_param,
	.qpl = GVE_RX_QPL,
	.irq = GVE_RX_IRQ,
	.fill = GVE_RX_FILL,
	.desc_len = sizeof ( struct gve_rx_descriptor ),
	.cmplt_len = sizeof ( struct gve_rx_completion ),
	.create = GVE_ADMIN_CREATE_RX,
	.destroy = GVE_ADMIN_DESTROY_RX,
};

/**
 * Set up admin queue and get device description
 *
 * @v gve		GVE device
 * @ret rc		Return status code
 */
static int gve_setup ( struct gve_nic *gve ) {
	unsigned int i;
	int rc;

	/* Attempt several times, since the device may decide to add
	 * in a few spurious resets.
	 */
	for ( i = 0 ; i < GVE_RESET_MAX_RETRY ; i++ ) {

		/* Reset device */
		if ( ( rc = gve_reset ( gve ) ) != 0 )
			continue;

		/* Enable admin queue */
		gve_admin_enable ( gve );

		/* Fetch MAC address */
		if ( ( rc = gve_describe ( gve ) ) != 0 )
			continue;

		/* Success */
		return 0;
	}

	DBGC ( gve, "GVE %p failed to get device description: %s\n",
	       gve, strerror ( rc ) );
	return rc;
}

/** Device startup process descriptor */
static struct process_descriptor gve_startup_desc =
	PROC_DESC_ONCE ( struct gve_nic, startup, gve_startup );

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int gve_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct gve_nic *gve;
	unsigned long cfg_start;
	unsigned long db_start;
	unsigned long db_size;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *gve ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &gve_operations );
	gve = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( gve, 0, sizeof ( *gve ) );
	gve->netdev = netdev;
	gve->tx.type = &gve_tx_type;
	gve->rx.type = &gve_rx_type;
	process_init_stopped ( &gve->startup, &gve_startup_desc,
			       &netdev->refcnt );
	timer_init ( &gve->watchdog, gve_watchdog, &netdev->refcnt );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Check PCI revision */
	pci_read_config_byte ( pci, PCI_REVISION, &gve->revision );
	DBGC ( gve, "GVE %p is revision %#02x\n", gve, gve->revision );

	/* Map configuration registers */
	cfg_start = pci_bar_start ( pci, GVE_CFG_BAR );
	gve->cfg = pci_ioremap ( pci, cfg_start, GVE_CFG_SIZE );
	if ( ! gve->cfg ) {
		rc = -ENODEV;
		goto err_cfg;
	}

	/* Map doorbell registers */
	db_start = pci_bar_start ( pci, GVE_DB_BAR );
	db_size = pci_bar_size ( pci, GVE_DB_BAR );
	gve->db = pci_ioremap ( pci, db_start, db_size );
	if ( ! gve->db ) {
		rc = -ENODEV;
		goto err_db;
	}

	/* Configure DMA */
	gve->dma = &pci->dma;
	dma_set_mask_64bit ( gve->dma );
	assert ( netdev->dma == NULL );

	/* Allocate admin queue */
	if ( ( rc = gve_admin_alloc ( gve ) ) != 0 )
		goto err_admin;

	/* Set up the device */
	if ( ( rc = gve_setup ( gve ) ) != 0 )
		goto err_setup;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_setup:
	gve_reset ( gve );
	gve_admin_free ( gve );
 err_admin:
	iounmap ( gve->db );
 err_db:
	iounmap ( gve->cfg );
 err_cfg:
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
static void gve_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct gve_nic *gve = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset device */
	gve_reset ( gve );

	/* Free admin queue */
	gve_admin_free ( gve );

	/* Unmap registers */
	iounmap ( gve->db );
	iounmap ( gve->cfg );

	/* Free network device */
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** GVE PCI device IDs */
static struct pci_device_id gve_nics[] = {
	PCI_ROM ( 0x1ae0, 0x0042, "gve", "gVNIC", 0 ),
};

/** GVE PCI driver */
struct pci_driver gve_driver __pci_driver = {
	.ids = gve_nics,
	.id_count = ( sizeof ( gve_nics ) / sizeof ( gve_nics[0] ) ),
	.probe = gve_probe,
	.remove = gve_remove,
};
