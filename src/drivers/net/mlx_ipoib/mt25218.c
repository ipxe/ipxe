/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Skeleton NIC driver for Etherboot
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include <errno.h>
#include <gpxe/pci.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/infiniband.h>

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"

#include "mt25218_imp.c"

#include "arbel.h"


static const struct ib_gid arbel_no_gid = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 }
};


#define MLX_RX_MAX_FILL NUM_IPOIB_RCV_WQES

struct mlx_nic {
	/** Queue pair handle */
	udqp_t ipoib_qph;
	/** Broadcast Address Vector */
	ud_av_t bcast_av;
	/** Send completion queue */
	cq_t snd_cqh;
	/** Receive completion queue */
	cq_t rcv_cqh;

	/** RX fill level */
	unsigned int rx_fill;
};


static struct io_buffer *static_ipoib_tx_ring[NUM_IPOIB_SND_WQES];
static struct io_buffer *static_ipoib_rx_ring[NUM_IPOIB_RCV_WQES];

static struct arbel static_arbel;
static struct arbel_completion_queue static_ipoib_send_cq;
static struct arbel_completion_queue static_ipoib_recv_cq;

static struct arbel_queue_pair static_ipoib_qp = {
	.qp = {
		.send = {
			.qp = &static_ipoib_qp.qp,
			.is_send = 1,
			.cq = &static_ipoib_send_cq.cq,
			.num_wqes = NUM_IPOIB_SND_WQES,
			.iobufs = static_ipoib_tx_ring,
			.list = LIST_HEAD_INIT (static_ipoib_qp.qp.send.list),
		},
		.recv = {
			.qp = &static_ipoib_qp.qp,
			.is_send = 0,
			.cq = &static_ipoib_recv_cq.cq,
			.num_wqes = NUM_IPOIB_RCV_WQES,
			.iobufs = static_ipoib_rx_ring,
			.list = LIST_HEAD_INIT (static_ipoib_qp.qp.recv.list),
		},
	},
	.send = {
		.doorbell_idx = IPOIB_SND_QP_DB_IDX,
	},
	.recv = {
		.doorbell_idx = IPOIB_RCV_QP_DB_IDX,
	},
};
static struct arbel_completion_queue static_ipoib_send_cq = {
	.cq = {
		.cqn = 1234, /* Only used for debug messages */
		.num_cqes = NUM_IPOIB_SND_CQES,
		.work_queues = LIST_HEAD_INIT (static_ipoib_send_cq.cq.work_queues),
	},
	.doorbell_idx = IPOIB_SND_CQ_CI_DB_IDX,
};
static struct arbel_completion_queue static_ipoib_recv_cq = {
	.cq = {
		.cqn = 2345, /* Only used for debug messages */
		.num_cqes = NUM_IPOIB_RCV_CQES,
		.work_queues = LIST_HEAD_INIT (static_ipoib_recv_cq.cq.work_queues),
	},
	.doorbell_idx = IPOIB_RCV_CQ_CI_DB_IDX,
};
static struct ib_device static_ibdev = {
	.priv = &static_arbel,
};


/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int mlx_open ( struct net_device *netdev ) {

	( void ) netdev;

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void mlx_close ( struct net_device *netdev ) {

	( void ) netdev;

}

static int arbel_post_send ( struct ib_device *ibdev,
			     struct ib_queue_pair *qp,
			     struct ib_address_vector *av,
			     struct io_buffer *iobuf );

static int mlx_transmit_direct ( struct net_device *netdev,
				 struct io_buffer *iobuf ) {
	struct mlx_nic *mlx = netdev->priv;
	int rc;

	struct ud_av_st *bcast_av = mlx->bcast_av;
	struct arbelprm_ud_address_vector *bav =
		( struct arbelprm_ud_address_vector * ) &bcast_av->av;
	struct ib_address_vector av = {
		.dest_qp = bcast_av->dest_qp,
		.qkey = bcast_av->qkey,
		.dlid = MLX_GET ( bav, rlid ),
		.rate = ( MLX_GET ( bav, max_stat_rate ) ? 1 : 4 ),
		.sl = MLX_GET ( bav, sl ),
		.gid_present = 1,
	};
	memcpy ( &av.gid, ( ( void * ) bav ) + 16, 16 );

	rc = arbel_post_send ( &static_ibdev, &static_ipoib_qp.qp, &av, iobuf );

	return rc;
}

static void arbel_poll_cq ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq,
			    ib_completer_t complete_send,
			    ib_completer_t complete_recv );

static void temp_complete_send ( struct ib_device *ibdev __unused,
				 struct ib_queue_pair *qp,
				 struct ib_completion *completion,
				 struct io_buffer *iobuf ) {
	struct net_device *netdev = qp->priv;

	DBG ( "Wahey! TX completion\n" );
	netdev_tx_complete_err ( netdev, iobuf,
				 ( completion->syndrome ? -EIO : 0 ) );
}

static void temp_complete_recv ( struct ib_device *ibdev __unused,
				 struct ib_queue_pair *qp,
				 struct ib_completion *completion,
				 struct io_buffer *iobuf ) {
	struct net_device *netdev = qp->priv;
	struct mlx_nic *mlx = netdev->priv;

	DBG ( "Yay! RX completion on %p len %zx:\n", iobuf, completion->len );
	if ( completion->syndrome ) {
		netdev_rx_err ( netdev, iobuf, -EIO );
	} else {
		iob_put ( iobuf, completion->len );
		iob_pull ( iobuf, sizeof ( struct ib_global_route_header ) );
		netdev_rx ( netdev, iobuf );
	}

	mlx->rx_fill--;
}

static int arbel_post_recv ( struct ib_device *ibdev,
			     struct ib_queue_pair *qp,
			     struct io_buffer *iobuf );

static void mlx_refill_rx ( struct net_device *netdev ) {
	struct mlx_nic *mlx = netdev->priv;
	struct io_buffer *iobuf;
	int rc;

	while ( mlx->rx_fill < MLX_RX_MAX_FILL ) {
		iobuf = alloc_iob ( 2048 );
		if ( ! iobuf )
			break;
		DBG ( "Posting RX buffer %p:\n", iobuf );
		if ( ( rc = arbel_post_recv ( &static_ibdev,
					      &static_ipoib_qp.qp,
					      iobuf ) ) != 0 ) {
			free_iob ( iobuf );
			break;
		}
		mlx->rx_fill++;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void mlx_poll ( struct net_device *netdev ) {
	struct mlx_nic *mlx = netdev->priv;
	int rc;

	if ( ( rc = poll_error_buf() ) != 0 ) {
		DBG ( "poll_error_buf() failed: %s\n", strerror ( rc ) );
		return;
	}

	/* Drain event queue.  We can ignore events, since we're going
	 * to just poll all completion queues anyway.
	 */
	if ( ( rc = drain_eq() ) != 0 ) {
		DBG ( "drain_eq() failed: %s\n", strerror ( rc ) );
		return;
	}

	/* Poll completion queues */
	arbel_poll_cq ( &static_ibdev, &static_ipoib_send_cq.cq,
			temp_complete_send, temp_complete_recv );
	arbel_poll_cq ( &static_ibdev, &static_ipoib_recv_cq.cq,
			temp_complete_send, temp_complete_recv );
	//	mlx_poll_cq ( netdev, mlx->rcv_cqh, mlx_rx_complete );

	mlx_refill_rx ( netdev );
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void mlx_irq ( struct net_device *netdev, int enable ) {

	( void ) netdev;
	( void ) enable;

}

static struct net_device_operations mlx_operations = {
	.open		= mlx_open,
	.close		= mlx_close,
	.transmit	= mlx_transmit_direct,
	.poll		= mlx_poll,
	.irq		= mlx_irq,
};

/***************************************************************************
 *
 * HCA commands
 *
 ***************************************************************************
 */

/**
 * Wait for Arbel command completion
 *
 * @v arbel		Arbel device
 * @ret rc		Return status code
 */
static int arbel_cmd_wait ( struct arbel *arbel,
			    struct arbelprm_hca_command_register *hcr ) {
	unsigned int wait;

	for ( wait = ARBEL_HCR_MAX_WAIT_MS ; wait ; wait-- ) {
		hcr->u.dwords[6] =
			readl ( arbel->config + ARBEL_HCR_REG ( 6 ) );
		if ( MLX_GET ( hcr, go ) == 0 )
			return 0;
		mdelay ( 1 );
	}
	return -EBUSY;
}

/**
 * Issue HCA command
 *
 * @v arbel		Arbel device
 * @v command		Command opcode, flags and input/output lengths
 * @v op_mod		Opcode modifier (0 if no modifier applicable)
 * @v in		Input parameters
 * @v in_mod		Input modifier (0 if no modifier applicable)
 * @v out		Output parameters
 * @ret rc		Return status code
 */
static int arbel_cmd ( struct arbel *arbel, unsigned long command,
		       unsigned int op_mod, const void *in,
		       unsigned int in_mod, void *out ) {
	struct arbelprm_hca_command_register hcr;
	unsigned int opcode = ARBEL_HCR_OPCODE ( command );
	size_t in_len = ARBEL_HCR_IN_LEN ( command );
	size_t out_len = ARBEL_HCR_OUT_LEN ( command );
	void *in_buffer;
	void *out_buffer;
	unsigned int status;
	unsigned int i;
	int rc;

	DBGC ( arbel, "Arbel %p command %02x in %zx%s out %zx%s\n",
	       arbel, opcode, in_len,
	       ( ( command & ARBEL_HCR_IN_MBOX ) ? "(mbox)" : "" ), out_len,
	       ( ( command & ARBEL_HCR_OUT_MBOX ) ? "(mbox)" : "" ) );

	/* Check that HCR is free */
	if ( ( rc = arbel_cmd_wait ( arbel, &hcr ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p command interface locked\n", arbel );
		return rc;
	}

	/* Prepare HCR */
	memset ( &hcr, 0, sizeof ( hcr ) );
	in_buffer = &hcr.u.dwords[0];
	if ( in_len && ( command & ARBEL_HCR_IN_MBOX ) ) {
		in_buffer = arbel->mailbox_in;
		MLX_FILL_1 ( &hcr, 1, in_param_l, virt_to_bus ( in_buffer ) );
	}
	memcpy ( in_buffer, in, in_len );
	MLX_FILL_1 ( &hcr, 2, input_modifier, in_mod );
	out_buffer = &hcr.u.dwords[3];
	if ( out_len && ( command & ARBEL_HCR_OUT_MBOX ) ) {
		out_buffer = arbel->mailbox_out;
		MLX_FILL_1 ( &hcr, 4, out_param_l,
			     virt_to_bus ( out_buffer ) );
	}
	MLX_FILL_3 ( &hcr, 6,
		     opcode, opcode,
		     opcode_modifier, op_mod,
		     go, 1 );

	/* Issue command */
	for ( i = 0 ; i < ( sizeof ( hcr ) / sizeof ( hcr.u.dwords[0] ) ) ;
	      i++ ) {
		writel ( hcr.u.dwords[i],
			 arbel->config + ARBEL_HCR_REG ( i ) );
		barrier();
	}

	/* Wait for command completion */
	if ( ( rc = arbel_cmd_wait ( arbel, &hcr ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p timed out waiting for command:\n",
		       arbel );
		DBGC_HD ( arbel, &hcr, sizeof ( hcr ) );
		return rc;
	}

	/* Check command status */
	status = MLX_GET ( &hcr, status );
	if ( status != 0 ) {
		DBGC ( arbel, "Arbel %p command failed with status %02x:\n",
		       arbel, status );
		DBGC_HD ( arbel, &hcr, sizeof ( hcr ) );
		return -EIO;
	}

	/* Read output parameters, if any */
	hcr.u.dwords[3] = readl ( arbel->config + ARBEL_HCR_REG ( 3 ) );
	hcr.u.dwords[4] = readl ( arbel->config + ARBEL_HCR_REG ( 4 ) );
	memcpy ( out, out_buffer, out_len );

	return 0;
}

static inline int
arbel_cmd_query_dev_lim ( struct arbel *arbel,
			  struct arbelprm_query_dev_lim *dev_lim ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_OUT_CMD ( ARBEL_HCR_QUERY_DEV_LIM, 
					       1, sizeof ( *dev_lim ) ),
			   0, NULL, 0, dev_lim );
}

static inline int
arbel_cmd_sw2hw_cq ( struct arbel *arbel, unsigned long cqn,
		     const struct arbelprm_completion_queue_context *cqctx ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_SW2HW_CQ,
					      1, sizeof ( *cqctx ) ),
			   0, cqctx, cqn, NULL );
}

/***************************************************************************
 *
 * Completion queue operations
 *
 ***************************************************************************
 */

/**
 * Create completion queue
 *
 * @v ibdev		Infiniband device
 * @v 
 */
static int arbel_create_cq ( struct ib_device *ibdev,
			     struct ib_completion_queue **new_cq ) {
	struct arbel *arbel = ibdev->priv;
	struct arbelprm_completion_queue_context cqctx;
	struct ib_completion_queue *cq;

	cq = zalloc ( sizeof ( *cq ) );
	if ( ! cq )
		return -ENOMEM;

	

	memset ( &cqctx, 0, sizeof ( cqctx ) );
	

	return arbel_cmd_sw2hw_cq ( arbel, 0, &cqctx );
}


/***************************************************************************
 *
 * Work request operations
 *
 ***************************************************************************
 */

/**
 * Ring doorbell register in UAR
 *
 * @v arbel		Arbel device
 * @v db_reg		Doorbell register structure
 * @v offset		Address of doorbell
 */
static void arbel_ring_doorbell ( struct arbel *arbel,
				  union arbelprm_doorbell_register *db_reg,
				  unsigned int offset ) {

	DBG ( "arbel_ring_doorbell %08lx:%08lx to %lx\n",
	      db_reg->dword[0], db_reg->dword[1],
	      virt_to_phys ( arbel->uar + offset ) );

	barrier();
	writel ( db_reg->dword[0], ( arbel->uar + offset + 0 ) );
	barrier();
	writel ( db_reg->dword[1], ( arbel->uar + offset + 4 ) );
}

/**
 * Post send work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int arbel_post_send ( struct ib_device *ibdev,
			     struct ib_queue_pair *qp,
			     struct ib_address_vector *av,
			     struct io_buffer *iobuf ) {
	struct arbel *arbel = ibdev->priv;
	struct arbel_queue_pair *arbel_qp
		= container_of ( qp, struct arbel_queue_pair, qp );
	struct ib_work_queue *wq = &qp->send;
	struct arbel_send_work_queue *arbel_send_wq = &arbel_qp->send;
	struct arbelprm_ud_send_wqe *prev_wqe;
	struct arbelprm_ud_send_wqe *wqe;
	union arbelprm_doorbell_record *db_rec;
	union arbelprm_doorbell_register db_reg;
	const struct ib_gid *gid;
	unsigned int wqe_idx_mask;
	size_t nds;

	/* Allocate work queue entry */
	wqe_idx_mask = ( wq->num_wqes - 1 );
	if ( wq->iobufs[wq->next_idx & wqe_idx_mask] ) {
		DBGC ( arbel, "Arbel %p send queue full", arbel );
		return -ENOBUFS;
	}
	wq->iobufs[wq->next_idx & wqe_idx_mask] = iobuf;
	prev_wqe = &arbel_send_wq->wqe[(wq->next_idx - 1) & wqe_idx_mask].ud;
	wqe = &arbel_send_wq->wqe[wq->next_idx & wqe_idx_mask].ud;

	/* Construct work queue entry */
	MLX_FILL_1 ( &wqe->next, 1, always1, 1 );
	memset ( &wqe->ctrl, 0, sizeof ( wqe->ctrl ) );
	MLX_FILL_1 ( &wqe->ctrl, 0, always1, 1 );
	memset ( &wqe->ud, 0, sizeof ( wqe->ud ) );
	MLX_FILL_2 ( &wqe->ud, 0,
		     ud_address_vector.pd, GLOBAL_PD,
		     ud_address_vector.port_number, PXE_IB_PORT );
	MLX_FILL_2 ( &wqe->ud, 1,
		     ud_address_vector.rlid, av->dlid,
		     ud_address_vector.g, av->gid_present );
	MLX_FILL_2 ( &wqe->ud, 2,
		     ud_address_vector.max_stat_rate,
			 ( ( av->rate >= 3 ) ? 0 : 1 ),
		     ud_address_vector.msg, 3 );
	MLX_FILL_1 ( &wqe->ud, 3, ud_address_vector.sl, av->sl );
	gid = ( av->gid_present ? &av->gid : &arbel_no_gid );
	memcpy ( &wqe->ud.u.dwords[4], gid, sizeof ( *gid ) );
	MLX_FILL_1 ( &wqe->ud, 8, destination_qp, av->dest_qp );
	MLX_FILL_1 ( &wqe->ud, 9, q_key, av->qkey );
	MLX_FILL_1 ( &wqe->data[0], 0, byte_count, iob_len ( iobuf ) );
	MLX_FILL_1 ( &wqe->data[0], 3,
		     local_address_l, virt_to_bus ( iobuf->data ) );

	/* Update previous work queue entry's "next" field */
	nds = ( ( offsetof ( typeof ( *wqe ), data ) +
		  sizeof ( wqe->data[0] ) ) >> 4 );
	MLX_SET ( &prev_wqe->next, nopcode, ARBEL_OPCODE_SEND );
	MLX_FILL_3 ( &prev_wqe->next, 1,
		     nds, nds,
		     f, 1,
		     always1, 1 );

	/* Update doorbell record */
	barrier();
	db_rec = &arbel->db_rec[arbel_send_wq->doorbell_idx];
	MLX_FILL_1 ( &db_rec->qp, 0,
		     counter, ( ( wq->next_idx + 1 ) & 0xffff ) );

	/* Ring doorbell register */
	MLX_FILL_4 ( &db_reg.send, 0,
		     nopcode, ARBEL_OPCODE_SEND,
		     f, 1,
		     wqe_counter, ( wq->next_idx & 0xffff ),
		     wqe_cnt, 1 );
	MLX_FILL_2 ( &db_reg.send, 1,
		     nds, nds,
		     qpn, qp->qpn );
	arbel_ring_doorbell ( arbel, &db_reg, POST_SND_OFFSET );

	/* Update work queue's index */
	wq->next_idx++;

	return 0;
}

/**
 * Post receive work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int arbel_post_recv ( struct ib_device *ibdev,
			     struct ib_queue_pair *qp,
			     struct io_buffer *iobuf ) {
	struct arbel *arbel = ibdev->priv;
	struct arbel_queue_pair *arbel_qp
		= container_of ( qp, struct arbel_queue_pair, qp );
	struct ib_work_queue *wq = &qp->recv;
	struct arbel_recv_work_queue *arbel_recv_wq = &arbel_qp->recv;
	struct arbelprm_recv_wqe *wqe;
	union arbelprm_doorbell_record *db_rec;
	unsigned int wqe_idx_mask;

	/* Allocate work queue entry */
	wqe_idx_mask = ( wq->num_wqes - 1 );
	if ( wq->iobufs[wq->next_idx & wqe_idx_mask] ) {
		DBGC ( arbel, "Arbel %p receive queue full", arbel );
		return -ENOBUFS;
	}
	wq->iobufs[wq->next_idx & wqe_idx_mask] = iobuf;
	wqe = &arbel_recv_wq->wqe[wq->next_idx & wqe_idx_mask].recv;

	/* Construct work queue entry */
	MLX_FILL_1 ( &wqe->data[0], 0, byte_count, iob_tailroom ( iobuf ) );
	MLX_FILL_1 ( &wqe->data[0], 1, l_key, arbel->reserved_lkey );
	MLX_FILL_1 ( &wqe->data[0], 3,
		     local_address_l, virt_to_bus ( iobuf->data ) );

	/* Update doorbell record */
	barrier();
	db_rec = &arbel->db_rec[arbel_recv_wq->doorbell_idx];
	MLX_FILL_1 ( &db_rec->qp, 0,
		     counter, ( ( wq->next_idx + 1 ) & 0xffff ) );	

	/* Update work queue's index */
	wq->next_idx++;

	return 0;	
}

/**
 * Handle completion
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 * @v cqe		Hardware completion queue entry
 * @v complete_send	Send completion handler
 * @v complete_recv	Receive completion handler
 * @ret rc		Return status code
 */
static int arbel_complete ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq,
			    union arbelprm_completion_entry *cqe,
			    ib_completer_t complete_send,
			    ib_completer_t complete_recv ) {
	struct arbel *arbel = ibdev->priv;
	struct ib_completion completion;
	struct ib_work_queue *wq;
	struct ib_queue_pair *qp;
	struct arbel_queue_pair *arbel_qp;
	struct arbel_send_work_queue *arbel_send_wq;
	struct arbel_recv_work_queue *arbel_recv_wq;
	struct io_buffer *iobuf;
	ib_completer_t complete;
	unsigned int opcode;
	unsigned long qpn;
	int is_send;
	unsigned long wqe_adr;
	unsigned int wqe_idx;
	int rc = 0;

	/* Parse completion */
	memset ( &completion, 0, sizeof ( completion ) );
	completion.len = MLX_GET ( &cqe->normal, byte_cnt );
	qpn = MLX_GET ( &cqe->normal, my_qpn );
	is_send = MLX_GET ( &cqe->normal, s );
	wqe_adr = ( MLX_GET ( &cqe->normal, wqe_adr ) << 6 );
	opcode = MLX_GET ( &cqe->normal, opcode );
	if ( opcode >= ARBEL_OPCODE_RECV_ERROR ) {
		/* "s" field is not valid for error opcodes */
		is_send = ( opcode == ARBEL_OPCODE_SEND_ERROR );
		completion.syndrome = MLX_GET ( &cqe->error, syndrome );
		DBGC ( arbel, "Arbel %p CPN %lx syndrome %x vendor %lx\n",
		       arbel, cq->cqn, completion.syndrome,
		       MLX_GET ( &cqe->error, vendor_code ) );
		rc = -EIO;
		/* Don't return immediately; propagate error to completer */
	}

	/* Identify work queue */
	wq = ib_find_wq ( cq, qpn, is_send );
	if ( ! wq ) {
		DBGC ( arbel, "Arbel %p CQN %lx unknown %s QPN %lx\n",
		       arbel, cq->cqn, ( is_send ? "send" : "recv" ), qpn );
		return -EIO;
	}
	qp = wq->qp;
	arbel_qp = container_of ( qp, struct arbel_queue_pair, qp );

	/* Identify work queue entry index */
	if ( is_send ) {
		arbel_send_wq = &arbel_qp->send;
		wqe_idx = ( ( wqe_adr - virt_to_bus ( arbel_send_wq->wqe ) ) /
			    sizeof ( arbel_send_wq->wqe[0] ) );
	} else {
		arbel_recv_wq = &arbel_qp->recv;
		wqe_idx = ( ( wqe_adr - virt_to_bus ( arbel_recv_wq->wqe ) ) /
			    sizeof ( arbel_recv_wq->wqe[0] ) );
	}

	/* Identify I/O buffer */
	iobuf = wq->iobufs[wqe_idx];
	if ( ! iobuf ) {
		DBGC ( arbel, "Arbel %p CQN %lx QPN %lx empty WQE %x\n",
		       arbel, cq->cqn, qpn, wqe_idx );
		return -EIO;
	}
	wq->iobufs[wqe_idx] = NULL;

	/* Pass off to caller's completion handler */
	complete = ( is_send ? complete_send : complete_recv );
	complete ( ibdev, qp, &completion, iobuf );

	return rc;
}			     

/**
 * Poll completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 * @v complete_send	Send completion handler
 * @v complete_recv	Receive completion handler
 */
static void arbel_poll_cq ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq,
			    ib_completer_t complete_send,
			    ib_completer_t complete_recv ) {
	struct arbel *arbel = ibdev->priv;
	struct arbel_completion_queue *arbel_cq
		= container_of ( cq, struct arbel_completion_queue, cq );
	union arbelprm_doorbell_record *db_rec;
	union arbelprm_completion_entry *cqe;
	unsigned int cqe_idx_mask;
	int rc;

	while ( 1 ) {
		/* Look for completion entry */
		cqe_idx_mask = ( cq->num_cqes - 1 );
		cqe = &arbel_cq->cqe[cq->next_idx & cqe_idx_mask];
		if ( MLX_GET ( &cqe->normal, owner ) != 0 ) {
			/* Entry still owned by hardware; end of poll */
			break;
		}

		/* Handle completion */
		if ( ( rc = arbel_complete ( ibdev, cq, cqe, complete_send,
					     complete_recv ) ) != 0 ) {
			DBGC ( arbel, "Arbel %p failed to complete: %s\n",
			       arbel, strerror ( rc ) );
			DBGC_HD ( arbel, cqe, sizeof ( *cqe ) );
		}

		/* Return ownership to hardware */
		MLX_FILL_1 ( &cqe->normal, 7, owner, 1 );
		barrier();
		/* Update completion queue's index */
		cq->next_idx++;
		/* Update doorbell record */
		db_rec = &arbel->db_rec[arbel_cq->doorbell_idx];
		MLX_FILL_1 ( &db_rec->cq_ci, 0,
			     counter, ( cq->next_idx & 0xffffffffUL ) );
	}
}

/** Arbel Infiniband operations */
static struct ib_device_operations arbel_ib_operations = {
	.post_send	= arbel_post_send,
	.post_recv	= arbel_post_recv,
	.poll_cq	= arbel_poll_cq,
};

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void arbel_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );

	unregister_netdev ( netdev );
	ib_driver_close ( 0 );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @v id		PCI ID
 * @ret rc		Return status code
 */
static int arbel_probe ( struct pci_device *pci,
			 const struct pci_device_id *id __unused ) {
	struct net_device *netdev;
	struct mlx_nic *mlx;
	struct ib_mac *mac;
	udqp_t qph;
	int rc;

	/* Allocate net device */
	netdev = alloc_ibdev ( sizeof ( *mlx ) );
	if ( ! netdev )
		return -ENOMEM;
	netdev_init ( netdev, &mlx_operations );
	mlx = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( mlx, 0, sizeof ( *mlx ) );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Initialise hardware */
	if ( ( rc = ib_driver_init ( pci, &qph ) ) != 0 )
		goto err_ipoib_init;
	mlx->ipoib_qph = qph;
	mlx->bcast_av = ib_data.bcast_av;
	mlx->snd_cqh = ib_data.ipoib_snd_cq;
	mlx->rcv_cqh = ib_data.ipoib_rcv_cq;
	mac = ( ( struct ib_mac * ) netdev->ll_addr );
	mac->qpn = htonl ( ib_get_qpn ( mlx->ipoib_qph ) );
	memcpy ( &mac->gid, ib_data.port_gid.raw, sizeof ( mac->gid ) );

	/* Hack up IB structures */
	static_arbel.config = memfree_pci_dev.cr_space;
	static_arbel.mailbox_in = dev_buffers_p->inprm_buf;
	static_arbel.mailbox_out = dev_buffers_p->outprm_buf;
	static_arbel.uar = memfree_pci_dev.uar;
	static_arbel.db_rec = dev_ib_data.uar_context_base;
	static_arbel.reserved_lkey = dev_ib_data.mkey;
	static_ipoib_qp.send.wqe =
		( ( struct udqp_st * ) qph )->snd_wq;
	static_ipoib_qp.recv.wqe =
		( ( struct udqp_st * ) qph )->rcv_wq;
	static_ipoib_send_cq.cqe =
		( ( struct cq_st * ) ib_data.ipoib_snd_cq )->cq_buf;
	static_ipoib_recv_cq.cqe =
		( ( struct cq_st * ) ib_data.ipoib_rcv_cq )->cq_buf;
	static_ipoib_qp.qp.qpn = ib_get_qpn ( qph );
	static_ipoib_qp.qp.priv = netdev;
	list_add ( &static_ipoib_qp.qp.send.list,
		   &static_ipoib_send_cq.cq.work_queues );
	list_add ( &static_ipoib_qp.qp.recv.list,
		   &static_ipoib_recv_cq.cq.work_queues );

	struct arbelprm_query_dev_lim dev_lim;
	memset ( &dev_lim, 0xaa, sizeof ( dev_lim ) );
	if ( ( rc = arbel_cmd_query_dev_lim ( &static_arbel,
					      &dev_lim ) ) != 0 ) {
		DBG ( "QUERY_DEV_LIM failed: %s\n", strerror ( rc ) );
	}
	DBG ( "Device limits:\n ");
	DBG_HD ( &dev_lim, sizeof ( dev_lim ) );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	return 0;

 err_register_netdev:
 err_ipoib_init:
	ib_driver_close ( 0 );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
	return rc;
}

static struct pci_device_id arbel_nics[] = {
	PCI_ROM ( 0x15b3, 0x6282, "MT25218", "MT25218 HCA driver" ),
	PCI_ROM ( 0x15b3, 0x6274, "MT25204", "MT25204 HCA driver" ),
};

struct pci_driver arbel_driver __pci_driver = {
	.ids = arbel_nics,
	.id_count = ( sizeof ( arbel_nics ) / sizeof ( arbel_nics[0] ) ),
	.probe = arbel_probe,
	.remove = arbel_remove,
};
