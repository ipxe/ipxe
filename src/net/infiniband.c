/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/list.h>
#include <gpxe/if_arp.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include <gpxe/ipoib.h>
#include <gpxe/process.h>
#include <gpxe/infiniband.h>

/** @file
 *
 * Infiniband protocol
 *
 */

/** List of Infiniband devices */
struct list_head ib_devices = LIST_HEAD_INIT ( ib_devices );

/**
 * Create completion queue
 *
 * @v ibdev		Infiniband device
 * @v num_cqes		Number of completion queue entries
 * @v op		Completion queue operations
 * @ret cq		New completion queue
 */
struct ib_completion_queue *
ib_create_cq ( struct ib_device *ibdev, unsigned int num_cqes,
	       struct ib_completion_queue_operations *op ) {
	struct ib_completion_queue *cq;
	int rc;

	DBGC ( ibdev, "IBDEV %p creating completion queue\n", ibdev );

	/* Allocate and initialise data structure */
	cq = zalloc ( sizeof ( *cq ) );
	if ( ! cq )
		return NULL;
	cq->num_cqes = num_cqes;
	INIT_LIST_HEAD ( &cq->work_queues );
	cq->op = op;

	/* Perform device-specific initialisation and get CQN */
	if ( ( rc = ibdev->op->create_cq ( ibdev, cq ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not initialise completion "
		       "queue: %s\n", ibdev, strerror ( rc ) );
		free ( cq );
		return NULL;
	}

	DBGC ( ibdev, "IBDEV %p created %d-entry completion queue %p (%p) "
	       "with CQN %#lx\n", ibdev, num_cqes, cq,
	       ib_cq_get_drvdata ( cq ), cq->cqn );
	return cq;
}

/**
 * Destroy completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 */
void ib_destroy_cq ( struct ib_device *ibdev,
		     struct ib_completion_queue *cq ) {
	DBGC ( ibdev, "IBDEV %p destroying completion queue %#lx\n",
	       ibdev, cq->cqn );
	assert ( list_empty ( &cq->work_queues ) );
	ibdev->op->destroy_cq ( ibdev, cq );
	free ( cq );
}

/**
 * Create queue pair
 *
 * @v ibdev		Infiniband device
 * @v num_send_wqes	Number of send work queue entries
 * @v send_cq		Send completion queue
 * @v num_recv_wqes	Number of receive work queue entries
 * @v recv_cq		Receive completion queue
 * @v qkey		Queue key
 * @ret qp		Queue pair
 */
struct ib_queue_pair * ib_create_qp ( struct ib_device *ibdev,
				      unsigned int num_send_wqes,
				      struct ib_completion_queue *send_cq,
				      unsigned int num_recv_wqes,
				      struct ib_completion_queue *recv_cq,
				      unsigned long qkey ) {
	struct ib_queue_pair *qp;
	size_t total_size;
	int rc;

	DBGC ( ibdev, "IBDEV %p creating queue pair\n", ibdev );

	/* Allocate and initialise data structure */
	total_size = ( sizeof ( *qp ) +
		       ( num_send_wqes * sizeof ( qp->send.iobufs[0] ) ) +
		       ( num_recv_wqes * sizeof ( qp->recv.iobufs[0] ) ) );
	qp = zalloc ( total_size );
	if ( ! qp )
		return NULL;
	qp->qkey = qkey;
	qp->send.qp = qp;
	qp->send.is_send = 1;
	qp->send.cq = send_cq;
	list_add ( &qp->send.list, &send_cq->work_queues );
	qp->send.num_wqes = num_send_wqes;
	qp->send.iobufs = ( ( ( void * ) qp ) + sizeof ( *qp ) );
	qp->recv.qp = qp;
	qp->recv.cq = recv_cq;
	list_add ( &qp->recv.list, &recv_cq->work_queues );
	qp->recv.num_wqes = num_recv_wqes;
	qp->recv.iobufs = ( ( ( void * ) qp ) + sizeof ( *qp ) +
			    ( num_send_wqes * sizeof ( qp->send.iobufs[0] ) ));

	/* Perform device-specific initialisation and get QPN */
	if ( ( rc = ibdev->op->create_qp ( ibdev, qp ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not initialise queue pair: "
		       "%s\n", ibdev, strerror ( rc ) );
		list_del ( &qp->send.list );
		list_del ( &qp->recv.list );
		free ( qp );
		return NULL;
	}

	DBGC ( ibdev, "IBDEV %p created queue pair %p (%p) with QPN %#lx\n",
	       ibdev, qp, ib_qp_get_drvdata ( qp ), qp->qpn );
	DBGC ( ibdev, "IBDEV %p QPN %#lx has %d send entries at [%p,%p)\n",
	       ibdev, qp->qpn, num_send_wqes, qp->send.iobufs,
	       qp->recv.iobufs );
	DBGC ( ibdev, "IBDEV %p QPN %#lx has %d receive entries at [%p,%p)\n",
	       ibdev, qp->qpn, num_recv_wqes, qp->recv.iobufs,
	       ( ( ( void * ) qp ) + total_size ) );
	return qp;
}

/**
 * Modify queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v mod_list		Modification list
 * @v qkey		New queue key, if applicable
 * @ret rc		Return status code
 */
int ib_modify_qp ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		   unsigned long mod_list, unsigned long qkey ) {
	int rc;

	DBGC ( ibdev, "IBDEV %p modifying QPN %#lx\n", ibdev, qp->qpn );

	if ( mod_list & IB_MODIFY_QKEY )
		qp->qkey = qkey;

	if ( ( rc = ibdev->op->modify_qp ( ibdev, qp, mod_list ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not modify QPN %#lx: %s\n",
		       ibdev, qp->qpn, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Destroy queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 */
void ib_destroy_qp ( struct ib_device *ibdev, struct ib_queue_pair *qp ) {
	struct io_buffer *iobuf;
	unsigned int i;

	DBGC ( ibdev, "IBDEV %p destroying QPN %#lx\n",
	       ibdev, qp->qpn );

	/* Perform device-specific destruction */
	ibdev->op->destroy_qp ( ibdev, qp );

	/* Complete any remaining I/O buffers with errors */
	for ( i = 0 ; i < qp->send.num_wqes ; i++ ) {
		if ( ( iobuf = qp->send.iobufs[i] ) != NULL )
			ib_complete_send ( ibdev, qp, iobuf, -ECANCELED );
	}
	for ( i = 0 ; i < qp->recv.num_wqes ; i++ ) {
		if ( ( iobuf = qp->recv.iobufs[i] ) != NULL ) {
			ib_complete_recv ( ibdev, qp, NULL, iobuf,
					   -ECANCELED );
		}
	}

	/* Remove work queues from completion queue */
	list_del ( &qp->send.list );
	list_del ( &qp->recv.list );

	/* Free QP */
	free ( qp );
}

/**
 * Find work queue belonging to completion queue
 *
 * @v cq		Completion queue
 * @v qpn		Queue pair number
 * @v is_send		Find send work queue (rather than receive)
 * @ret wq		Work queue, or NULL if not found
 */
struct ib_work_queue * ib_find_wq ( struct ib_completion_queue *cq,
				    unsigned long qpn, int is_send ) {
	struct ib_work_queue *wq;

	list_for_each_entry ( wq, &cq->work_queues, list ) {
		if ( ( wq->qp->qpn == qpn ) && ( wq->is_send == is_send ) )
			return wq;
	}
	return NULL;
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
int ib_post_send ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		   struct ib_address_vector *av,
		   struct io_buffer *iobuf ) {
	int rc;

	/* Check queue fill level */
	if ( qp->send.fill >= qp->send.num_wqes ) {
		DBGC ( ibdev, "IBDEV %p QPN %#lx send queue full\n",
		       ibdev, qp->qpn );
		return -ENOBUFS;
	}

	/* Post to hardware */
	if ( ( rc = ibdev->op->post_send ( ibdev, qp, av, iobuf ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p QPN %#lx could not post send WQE: "
		       "%s\n", ibdev, qp->qpn, strerror ( rc ) );
		return rc;
	}

	qp->send.fill++;
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
int ib_post_recv ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		   struct io_buffer *iobuf ) {
	int rc;

	/* Check queue fill level */
	if ( qp->recv.fill >= qp->recv.num_wqes ) {
		DBGC ( ibdev, "IBDEV %p QPN %#lx receive queue full\n",
		       ibdev, qp->qpn );
		return -ENOBUFS;
	}

	/* Post to hardware */
	if ( ( rc = ibdev->op->post_recv ( ibdev, qp, iobuf ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p QPN %#lx could not post receive WQE: "
		       "%s\n", ibdev, qp->qpn, strerror ( rc ) );
		return rc;
	}

	qp->recv.fill++;
	return 0;
}

/**
 * Complete send work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
void ib_complete_send ( struct ib_device *ibdev, struct ib_queue_pair *qp,
			struct io_buffer *iobuf, int rc ) {
	qp->send.cq->op->complete_send ( ibdev, qp, iobuf, rc );
	qp->send.fill--;
}

/**
 * Complete receive work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
void ib_complete_recv ( struct ib_device *ibdev, struct ib_queue_pair *qp,
			struct ib_address_vector *av,
			struct io_buffer *iobuf, int rc ) {
	qp->recv.cq->op->complete_recv ( ibdev, qp, av, iobuf, rc );
	qp->recv.fill--;
}

/***************************************************************************
 *
 * Event queues
 *
 ***************************************************************************
 */

/**
 * Handle Infiniband link state change
 *
 * @v ibdev		Infiniband device
 */
void ib_link_state_changed ( struct ib_device *ibdev ) {

	/* Notify IPoIB of link state change */
	ipoib_link_state_changed ( ibdev );
}

/**
 * Single-step the Infiniband event queue
 *
 * @v process		Infiniband event queue process
 */
static void ib_step ( struct process *process __unused ) {
	struct ib_device *ibdev;

	list_for_each_entry ( ibdev, &ib_devices, list ) {
		ibdev->op->poll_eq ( ibdev );
	}
}

/** Infiniband event queue process */
struct process ib_process __permanent_process = {
	.step = ib_step,
};

/***************************************************************************
 *
 * Infiniband device creation/destruction
 *
 ***************************************************************************
 */

/**
 * Allocate Infiniband device
 *
 * @v priv_size		Size of driver private data area
 * @ret ibdev		Infiniband device, or NULL
 */
struct ib_device * alloc_ibdev ( size_t priv_size ) {
	struct ib_device *ibdev;
	void *drv_priv;
	size_t total_len;

	total_len = ( sizeof ( *ibdev ) + priv_size );
	ibdev = zalloc ( total_len );
	if ( ibdev ) {
		drv_priv = ( ( ( void * ) ibdev ) + sizeof ( *ibdev ) );
		ib_set_drvdata ( ibdev, drv_priv );
		ibdev->lid = IB_LID_NONE;
		ibdev->pkey = IB_PKEY_NONE;
	}
	return ibdev;
}

/**
 * Register Infiniband device
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
int register_ibdev ( struct ib_device *ibdev ) {
	int rc;

	/* Add to device list */
	ibdev_get ( ibdev );
	list_add_tail ( &ibdev->list, &ib_devices );

	/* Open link */
	if ( ( rc = ib_open ( ibdev ) ) != 0 )
		goto err_open;

	/* Add IPoIB device */
	if ( ( rc = ipoib_probe ( ibdev ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not add IPoIB device: %s\n",
		       ibdev, strerror ( rc ) );
		goto err_ipoib_probe;
	}

	DBGC ( ibdev, "IBDEV %p registered (phys %s)\n", ibdev,
	       ibdev->dev->name );
	return 0;

 err_ipoib_probe:
	ib_close ( ibdev );
 err_open:
	list_del ( &ibdev->list );
	ibdev_put ( ibdev );
	return rc;
}

/**
 * Unregister Infiniband device
 *
 * @v ibdev		Infiniband device
 */
void unregister_ibdev ( struct ib_device *ibdev ) {

	/* Close device */
	ipoib_remove ( ibdev );
	ib_close ( ibdev );

	/* Remove from device list */
	list_del ( &ibdev->list );
	ibdev_put ( ibdev );
	DBGC ( ibdev, "IBDEV %p unregistered\n", ibdev );
}
