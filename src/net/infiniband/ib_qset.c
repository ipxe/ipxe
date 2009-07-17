/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <errno.h>
#include <string.h>
#include <gpxe/iobuf.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_qset.h>

/**
 * @file
 *
 * Infiniband queue sets
 *
 */

/**
 * Create queue set
 *
 * @v ibdev		Infiniband device
 * @v qset		Queue set
 * @v num_cqes		Number of completion queue entries
 * @v cq_op		Completion queue operations
 * @v num_send_wqes	Number of send work queue entries
 * @v num_recv_wqes	Number of receive work queue entries
 * @v qkey		Queue key
 * @ret rc		Return status code
 */
int ib_create_qset ( struct ib_device *ibdev, struct ib_queue_set *qset,
		     unsigned int num_cqes,
		     struct ib_completion_queue_operations *cq_op,
		     unsigned int num_send_wqes, unsigned int num_recv_wqes,
		     unsigned long qkey ) {
	int rc;

	/* Sanity check */
	assert ( qset->cq == NULL );
	assert ( qset->qp == NULL );

	/* Store queue parameters */
	qset->recv_max_fill = num_recv_wqes;

	/* Allocate completion queue */
	qset->cq = ib_create_cq ( ibdev, num_cqes, cq_op );
	if ( ! qset->cq ) {
		DBGC ( ibdev, "IBDEV %p could not allocate completion queue\n",
		       ibdev );
		rc = -ENOMEM;
		goto err;
	}

	/* Allocate queue pair */
	qset->qp = ib_create_qp ( ibdev, num_send_wqes, qset->cq,
				  num_recv_wqes, qset->cq, qkey );
	if ( ! qset->qp ) {
		DBGC ( ibdev, "IBDEV %p could not allocate queue pair\n",
		       ibdev );
		rc = -ENOMEM;
		goto err;
	}

	return 0;

 err:
	ib_destroy_qset ( ibdev, qset );
	return rc;
}

/**
 * Refill IPoIB receive ring
 *
 * @v ibdev		Infiniband device
 * @v qset		Queue set
 */
void ib_qset_refill_recv ( struct ib_device *ibdev,
			   struct ib_queue_set *qset ) {
	struct io_buffer *iobuf;
	int rc;

	while ( qset->qp->recv.fill < qset->recv_max_fill ) {

		/* Allocate I/O buffer */
		iobuf = alloc_iob ( IB_MAX_PAYLOAD_SIZE );
		if ( ! iobuf ) {
			/* Non-fatal; we will refill on next attempt */
			return;
		}

		/* Post I/O buffer */
		if ( ( rc = ib_post_recv ( ibdev, qset->qp, iobuf ) ) != 0 ) {
			DBGC ( ibdev, "IBDEV %p could not refill: %s\n",
			       ibdev, strerror ( rc ) );
			free_iob ( iobuf );
			/* Give up */
			return;
		}
	}
}

/**
 * Destroy queue set
 *
 * @v ibdev		Infiniband device
 * @v qset		Queue set
 */
void ib_destroy_qset ( struct ib_device *ibdev,
		       struct ib_queue_set *qset ) {

	if ( qset->qp )
		ib_destroy_qp ( ibdev, qset->qp );
	if ( qset->cq )
		ib_destroy_cq ( ibdev, qset->cq );
	memset ( qset, 0, sizeof ( *qset ) );
}
