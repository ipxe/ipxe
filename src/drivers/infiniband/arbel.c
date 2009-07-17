/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * Based in part upon the original driver by Mellanox Technologies
 * Ltd.  Portions may be Copyright (c) Mellanox Technologies Ltd.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/io.h>
#include <gpxe/pci.h>
#include <gpxe/malloc.h>
#include <gpxe/umalloc.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_smc.h>
#include "arbel.h"

/**
 * @file
 *
 * Mellanox Arbel Infiniband HCA
 *
 */

/***************************************************************************
 *
 * Queue number allocation
 *
 ***************************************************************************
 */

/**
 * Allocate queue number
 *
 * @v q_inuse		Queue usage bitmask
 * @v max_inuse		Maximum number of in-use queues
 * @ret qn_offset	Free queue number offset, or negative error
 */
static int arbel_alloc_qn_offset ( arbel_bitmask_t *q_inuse,
				   unsigned int max_inuse ) {
	unsigned int qn_offset = 0;
	arbel_bitmask_t mask = 1;

	while ( qn_offset < max_inuse ) {
		if ( ( mask & *q_inuse ) == 0 ) {
			*q_inuse |= mask;
			return qn_offset;
		}
		qn_offset++;
		mask <<= 1;
		if ( ! mask ) {
			mask = 1;
			q_inuse++;
		}
	}
	return -ENFILE;
}

/**
 * Free queue number
 *
 * @v q_inuse		Queue usage bitmask
 * @v qn_offset		Queue number offset
 */
static void arbel_free_qn_offset ( arbel_bitmask_t *q_inuse, int qn_offset ) {
	arbel_bitmask_t mask;

	mask = ( 1 << ( qn_offset % ( 8 * sizeof ( mask ) ) ) );
	q_inuse += ( qn_offset / ( 8 * sizeof ( mask ) ) );
	*q_inuse &= ~mask;
}

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

	assert ( in_len <= ARBEL_MBOX_SIZE );
	assert ( out_len <= ARBEL_MBOX_SIZE );

	DBGC2 ( arbel, "Arbel %p command %02x in %zx%s out %zx%s\n",
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
	DBGC2_HD ( arbel, &hcr, sizeof ( hcr ) );
	if ( in_len ) {
		DBGC2 ( arbel, "Input:\n" );
		DBGC2_HD ( arbel, in, ( ( in_len < 512 ) ? in_len : 512 ) );
	}

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
	if ( out_len ) {
		DBGC2 ( arbel, "Output:\n" );
		DBGC2_HD ( arbel, out, ( ( out_len < 512 ) ? out_len : 512 ) );
	}

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
arbel_cmd_query_fw ( struct arbel *arbel, struct arbelprm_query_fw *fw ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_OUT_CMD ( ARBEL_HCR_QUERY_FW, 
					       1, sizeof ( *fw ) ),
			   0, NULL, 0, fw );
}

static inline int
arbel_cmd_init_hca ( struct arbel *arbel,
		     const struct arbelprm_init_hca *init_hca ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_INIT_HCA,
					      1, sizeof ( *init_hca ) ),
			   0, init_hca, 0, NULL );
}

static inline int
arbel_cmd_close_hca ( struct arbel *arbel ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_CLOSE_HCA ),
			   0, NULL, 0, NULL );
}

static inline int
arbel_cmd_init_ib ( struct arbel *arbel, unsigned int port,
		    const struct arbelprm_init_ib *init_ib ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_INIT_IB,
					      1, sizeof ( *init_ib ) ),
			   0, init_ib, port, NULL );
}

static inline int
arbel_cmd_close_ib ( struct arbel *arbel, unsigned int port ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_CLOSE_IB ),
			   0, NULL, port, NULL );
}

static inline int
arbel_cmd_sw2hw_mpt ( struct arbel *arbel, unsigned int index,
		      const struct arbelprm_mpt *mpt ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_SW2HW_MPT,
					      1, sizeof ( *mpt ) ),
			   0, mpt, index, NULL );
}

static inline int
arbel_cmd_map_eq ( struct arbel *arbel, unsigned long index_map,
		   const struct arbelprm_event_mask *mask ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_MAP_EQ,
					      0, sizeof ( *mask ) ),
			   0, mask, index_map, NULL );
}

static inline int
arbel_cmd_sw2hw_eq ( struct arbel *arbel, unsigned int index,
		     const struct arbelprm_eqc *eqctx ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_SW2HW_EQ,
					      1, sizeof ( *eqctx ) ),
			   0, eqctx, index, NULL );
}

static inline int
arbel_cmd_hw2sw_eq ( struct arbel *arbel, unsigned int index,
		     struct arbelprm_eqc *eqctx ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_OUT_CMD ( ARBEL_HCR_HW2SW_EQ,
					       1, sizeof ( *eqctx ) ),
			   1, NULL, index, eqctx );
}

static inline int
arbel_cmd_sw2hw_cq ( struct arbel *arbel, unsigned long cqn,
		     const struct arbelprm_completion_queue_context *cqctx ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_SW2HW_CQ,
					      1, sizeof ( *cqctx ) ),
			   0, cqctx, cqn, NULL );
}

static inline int
arbel_cmd_hw2sw_cq ( struct arbel *arbel, unsigned long cqn,
		     struct arbelprm_completion_queue_context *cqctx) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_OUT_CMD ( ARBEL_HCR_HW2SW_CQ,
					       1, sizeof ( *cqctx ) ),
			   0, NULL, cqn, cqctx );
}

static inline int
arbel_cmd_rst2init_qpee ( struct arbel *arbel, unsigned long qpn,
			  const struct arbelprm_qp_ee_state_transitions *ctx ){
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_RST2INIT_QPEE,
					      1, sizeof ( *ctx ) ),
			   0, ctx, qpn, NULL );
}

static inline int
arbel_cmd_init2rtr_qpee ( struct arbel *arbel, unsigned long qpn,
			  const struct arbelprm_qp_ee_state_transitions *ctx ){
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_INIT2RTR_QPEE,
					      1, sizeof ( *ctx ) ),
			   0, ctx, qpn, NULL );
}

static inline int
arbel_cmd_rtr2rts_qpee ( struct arbel *arbel, unsigned long qpn,
			 const struct arbelprm_qp_ee_state_transitions *ctx ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_RTR2RTS_QPEE,
					      1, sizeof ( *ctx ) ),
			   0, ctx, qpn, NULL );
}

static inline int
arbel_cmd_rts2rts_qp ( struct arbel *arbel, unsigned long qpn,
		       const struct arbelprm_qp_ee_state_transitions *ctx ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_RTS2RTS_QPEE,
					      1, sizeof ( *ctx ) ),
			   0, ctx, qpn, NULL );
}

static inline int
arbel_cmd_2rst_qpee ( struct arbel *arbel, unsigned long qpn ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_2RST_QPEE ),
			   0x03, NULL, qpn, NULL );
}

static inline int
arbel_cmd_mad_ifc ( struct arbel *arbel, unsigned int port,
		    union arbelprm_mad *mad ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_INOUT_CMD ( ARBEL_HCR_MAD_IFC,
						 1, sizeof ( *mad ),
						 1, sizeof ( *mad ) ),
			   0x03, mad, port, mad );
}

static inline int
arbel_cmd_read_mgm ( struct arbel *arbel, unsigned int index,
		     struct arbelprm_mgm_entry *mgm ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_OUT_CMD ( ARBEL_HCR_READ_MGM,
					       1, sizeof ( *mgm ) ),
			   0, NULL, index, mgm );
}

static inline int
arbel_cmd_write_mgm ( struct arbel *arbel, unsigned int index,
		      const struct arbelprm_mgm_entry *mgm ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_WRITE_MGM,
					      1, sizeof ( *mgm ) ),
			   0, mgm, index, NULL );
}

static inline int
arbel_cmd_mgid_hash ( struct arbel *arbel, const struct ib_gid *gid,
		      struct arbelprm_mgm_hash *hash ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_INOUT_CMD ( ARBEL_HCR_MGID_HASH,
						 1, sizeof ( *gid ),
						 0, sizeof ( *hash ) ),
			   0, gid, 0, hash );
}

static inline int
arbel_cmd_run_fw ( struct arbel *arbel ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_RUN_FW ),
			   0, NULL, 0, NULL );
}

static inline int
arbel_cmd_disable_lam ( struct arbel *arbel ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_DISABLE_LAM ),
			   0, NULL, 0, NULL );
}

static inline int
arbel_cmd_enable_lam ( struct arbel *arbel, struct arbelprm_access_lam *lam ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_OUT_CMD ( ARBEL_HCR_ENABLE_LAM,
					       1, sizeof ( *lam ) ),
			   1, NULL, 0, lam );
}

static inline int
arbel_cmd_unmap_icm ( struct arbel *arbel, unsigned int page_count ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_UNMAP_ICM ),
			   0, NULL, page_count, NULL );
}

static inline int
arbel_cmd_map_icm ( struct arbel *arbel,
		    const struct arbelprm_virtual_physical_mapping *map ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_MAP_ICM,
					      1, sizeof ( *map ) ),
			   0, map, 1, NULL );
}

static inline int
arbel_cmd_unmap_icm_aux ( struct arbel *arbel ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_UNMAP_ICM_AUX ),
			   0, NULL, 0, NULL );
}

static inline int
arbel_cmd_map_icm_aux ( struct arbel *arbel,
			const struct arbelprm_virtual_physical_mapping *map ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_MAP_ICM_AUX,
					      1, sizeof ( *map ) ),
			   0, map, 1, NULL );
}

static inline int
arbel_cmd_set_icm_size ( struct arbel *arbel,
			 const struct arbelprm_scalar_parameter *icm_size,
			 struct arbelprm_scalar_parameter *icm_aux_size ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_INOUT_CMD ( ARBEL_HCR_SET_ICM_SIZE,
						 0, sizeof ( *icm_size ),
						 0, sizeof ( *icm_aux_size ) ),
			   0, icm_size, 0, icm_aux_size );
}

static inline int
arbel_cmd_unmap_fa ( struct arbel *arbel ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_VOID_CMD ( ARBEL_HCR_UNMAP_FA ),
			   0, NULL, 0, NULL );
}

static inline int
arbel_cmd_map_fa ( struct arbel *arbel,
		   const struct arbelprm_virtual_physical_mapping *map ) {
	return arbel_cmd ( arbel,
			   ARBEL_HCR_IN_CMD ( ARBEL_HCR_MAP_FA,
					      1, sizeof ( *map ) ),
			   0, map, 1, NULL );
}

/***************************************************************************
 *
 * MAD operations
 *
 ***************************************************************************
 */

/**
 * Issue management datagram
 *
 * @v ibdev		Infiniband device
 * @v mad		Management datagram
 * @ret rc		Return status code
 */
static int arbel_mad ( struct ib_device *ibdev, union ib_mad *mad ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	union arbelprm_mad mad_ifc;
	int rc;

	linker_assert ( sizeof ( *mad ) == sizeof ( mad_ifc.mad ),
			mad_size_mismatch );

	/* Copy in request packet */
	memcpy ( &mad_ifc.mad, mad, sizeof ( mad_ifc.mad ) );

	/* Issue MAD */
	if ( ( rc = arbel_cmd_mad_ifc ( arbel, ibdev->port,
					&mad_ifc ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not issue MAD IFC: %s\n",
		       arbel, strerror ( rc ) );
		return rc;
	}

	/* Copy out reply packet */
	memcpy ( mad, &mad_ifc.mad, sizeof ( *mad ) );

	if ( mad->hdr.status != 0 ) {
		DBGC ( arbel, "Arbel %p MAD IFC status %04x\n",
		       arbel, ntohs ( mad->hdr.status ) );
		return -EIO;
	}
	return 0;
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
 * @v cq		Completion queue
 * @ret rc		Return status code
 */
static int arbel_create_cq ( struct ib_device *ibdev,
			     struct ib_completion_queue *cq ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_completion_queue *arbel_cq;
	struct arbelprm_completion_queue_context cqctx;
	struct arbelprm_cq_ci_db_record *ci_db_rec;
	struct arbelprm_cq_arm_db_record *arm_db_rec;
	int cqn_offset;
	unsigned int i;
	int rc;

	/* Find a free completion queue number */
	cqn_offset = arbel_alloc_qn_offset ( arbel->cq_inuse, ARBEL_MAX_CQS );
	if ( cqn_offset < 0 ) {
		DBGC ( arbel, "Arbel %p out of completion queues\n", arbel );
		rc = cqn_offset;
		goto err_cqn_offset;
	}
	cq->cqn = ( arbel->limits.reserved_cqs + cqn_offset );

	/* Allocate control structures */
	arbel_cq = zalloc ( sizeof ( *arbel_cq ) );
	if ( ! arbel_cq ) {
		rc = -ENOMEM;
		goto err_arbel_cq;
	}
	arbel_cq->ci_doorbell_idx = arbel_cq_ci_doorbell_idx ( cqn_offset );
	arbel_cq->arm_doorbell_idx = arbel_cq_arm_doorbell_idx ( cqn_offset );

	/* Allocate completion queue itself */
	arbel_cq->cqe_size = ( cq->num_cqes * sizeof ( arbel_cq->cqe[0] ) );
	arbel_cq->cqe = malloc_dma ( arbel_cq->cqe_size,
				     sizeof ( arbel_cq->cqe[0] ) );
	if ( ! arbel_cq->cqe ) {
		rc = -ENOMEM;
		goto err_cqe;
	}
	memset ( arbel_cq->cqe, 0, arbel_cq->cqe_size );
	for ( i = 0 ; i < cq->num_cqes ; i++ ) {
		MLX_FILL_1 ( &arbel_cq->cqe[i].normal, 7, owner, 1 );
	}
	barrier();

	/* Initialise doorbell records */
	ci_db_rec = &arbel->db_rec[arbel_cq->ci_doorbell_idx].cq_ci;
	MLX_FILL_1 ( ci_db_rec, 0, counter, 0 );
	MLX_FILL_2 ( ci_db_rec, 1,
		     res, ARBEL_UAR_RES_CQ_CI,
		     cq_number, cq->cqn );
	arm_db_rec = &arbel->db_rec[arbel_cq->arm_doorbell_idx].cq_arm;
	MLX_FILL_1 ( arm_db_rec, 0, counter, 0 );
	MLX_FILL_2 ( arm_db_rec, 1,
		     res, ARBEL_UAR_RES_CQ_ARM,
		     cq_number, cq->cqn );

	/* Hand queue over to hardware */
	memset ( &cqctx, 0, sizeof ( cqctx ) );
	MLX_FILL_1 ( &cqctx, 0, st, 0xa /* "Event fired" */ );
	MLX_FILL_1 ( &cqctx, 2, start_address_l,
		     virt_to_bus ( arbel_cq->cqe ) );
	MLX_FILL_2 ( &cqctx, 3,
		     usr_page, arbel->limits.reserved_uars,
		     log_cq_size, fls ( cq->num_cqes - 1 ) );
	MLX_FILL_1 ( &cqctx, 5, c_eqn, ARBEL_NO_EQ );
	MLX_FILL_1 ( &cqctx, 6, pd, ARBEL_GLOBAL_PD );
	MLX_FILL_1 ( &cqctx, 7, l_key, arbel->reserved_lkey );
	MLX_FILL_1 ( &cqctx, 12, cqn, cq->cqn );
	MLX_FILL_1 ( &cqctx, 13,
		     cq_ci_db_record, arbel_cq->ci_doorbell_idx );
	MLX_FILL_1 ( &cqctx, 14,
		     cq_state_db_record, arbel_cq->arm_doorbell_idx );
	if ( ( rc = arbel_cmd_sw2hw_cq ( arbel, cq->cqn, &cqctx ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p SW2HW_CQ failed: %s\n",
		       arbel, strerror ( rc ) );
		goto err_sw2hw_cq;
	}

	DBGC ( arbel, "Arbel %p CQN %#lx ring at [%p,%p)\n",
	       arbel, cq->cqn, arbel_cq->cqe,
	       ( ( ( void * ) arbel_cq->cqe ) + arbel_cq->cqe_size ) );
	ib_cq_set_drvdata ( cq, arbel_cq );
	return 0;

 err_sw2hw_cq:
	MLX_FILL_1 ( ci_db_rec, 1, res, ARBEL_UAR_RES_NONE );
	MLX_FILL_1 ( arm_db_rec, 1, res, ARBEL_UAR_RES_NONE );
	free_dma ( arbel_cq->cqe, arbel_cq->cqe_size );
 err_cqe:
	free ( arbel_cq );
 err_arbel_cq:
	arbel_free_qn_offset ( arbel->cq_inuse, cqn_offset );
 err_cqn_offset:
	return rc;
}

/**
 * Destroy completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 */
static void arbel_destroy_cq ( struct ib_device *ibdev,
			       struct ib_completion_queue *cq ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_completion_queue *arbel_cq = ib_cq_get_drvdata ( cq );
	struct arbelprm_completion_queue_context cqctx;
	struct arbelprm_cq_ci_db_record *ci_db_rec;
	struct arbelprm_cq_arm_db_record *arm_db_rec;
	int cqn_offset;
	int rc;

	/* Take ownership back from hardware */
	if ( ( rc = arbel_cmd_hw2sw_cq ( arbel, cq->cqn, &cqctx ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p FATAL HW2SW_CQ failed on CQN %#lx: "
		       "%s\n", arbel, cq->cqn, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}

	/* Clear doorbell records */
	ci_db_rec = &arbel->db_rec[arbel_cq->ci_doorbell_idx].cq_ci;
	arm_db_rec = &arbel->db_rec[arbel_cq->arm_doorbell_idx].cq_arm;
	MLX_FILL_1 ( ci_db_rec, 1, res, ARBEL_UAR_RES_NONE );
	MLX_FILL_1 ( arm_db_rec, 1, res, ARBEL_UAR_RES_NONE );

	/* Free memory */
	free_dma ( arbel_cq->cqe, arbel_cq->cqe_size );
	free ( arbel_cq );

	/* Mark queue number as free */
	cqn_offset = ( cq->cqn - arbel->limits.reserved_cqs );
	arbel_free_qn_offset ( arbel->cq_inuse, cqn_offset );

	ib_cq_set_drvdata ( cq, NULL );
}

/***************************************************************************
 *
 * Queue pair operations
 *
 ***************************************************************************
 */

/**
 * Create send work queue
 *
 * @v arbel_send_wq	Send work queue
 * @v num_wqes		Number of work queue entries
 * @ret rc		Return status code
 */
static int arbel_create_send_wq ( struct arbel_send_work_queue *arbel_send_wq,
				  unsigned int num_wqes ) {
	struct arbelprm_ud_send_wqe *wqe;
	struct arbelprm_ud_send_wqe *next_wqe;
	unsigned int wqe_idx_mask;
	unsigned int i;

	/* Allocate work queue */
	arbel_send_wq->wqe_size = ( num_wqes *
				    sizeof ( arbel_send_wq->wqe[0] ) );
	arbel_send_wq->wqe = malloc_dma ( arbel_send_wq->wqe_size,
					  sizeof ( arbel_send_wq->wqe[0] ) );
	if ( ! arbel_send_wq->wqe )
		return -ENOMEM;
	memset ( arbel_send_wq->wqe, 0, arbel_send_wq->wqe_size );

	/* Link work queue entries */
	wqe_idx_mask = ( num_wqes - 1 );
	for ( i = 0 ; i < num_wqes ; i++ ) {
		wqe = &arbel_send_wq->wqe[i].ud;
		next_wqe = &arbel_send_wq->wqe[ ( i + 1 ) & wqe_idx_mask ].ud;
		MLX_FILL_1 ( &wqe->next, 0, nda_31_6,
			     ( virt_to_bus ( next_wqe ) >> 6 ) );
	}
	
	return 0;
}

/**
 * Create receive work queue
 *
 * @v arbel_recv_wq	Receive work queue
 * @v num_wqes		Number of work queue entries
 * @ret rc		Return status code
 */
static int arbel_create_recv_wq ( struct arbel_recv_work_queue *arbel_recv_wq,
				  unsigned int num_wqes ) {
	struct arbelprm_recv_wqe *wqe;
	struct arbelprm_recv_wqe *next_wqe;
	unsigned int wqe_idx_mask;
	size_t nds;
	unsigned int i;
	unsigned int j;

	/* Allocate work queue */
	arbel_recv_wq->wqe_size = ( num_wqes *
				    sizeof ( arbel_recv_wq->wqe[0] ) );
	arbel_recv_wq->wqe = malloc_dma ( arbel_recv_wq->wqe_size,
					  sizeof ( arbel_recv_wq->wqe[0] ) );
	if ( ! arbel_recv_wq->wqe )
		return -ENOMEM;
	memset ( arbel_recv_wq->wqe, 0, arbel_recv_wq->wqe_size );

	/* Link work queue entries */
	wqe_idx_mask = ( num_wqes - 1 );
	nds = ( ( offsetof ( typeof ( *wqe ), data ) +
		  sizeof ( wqe->data[0] ) ) >> 4 );
	for ( i = 0 ; i < num_wqes ; i++ ) {
		wqe = &arbel_recv_wq->wqe[i].recv;
		next_wqe = &arbel_recv_wq->wqe[( i + 1 ) & wqe_idx_mask].recv;
		MLX_FILL_1 ( &wqe->next, 0, nda_31_6,
			     ( virt_to_bus ( next_wqe ) >> 6 ) );
		MLX_FILL_1 ( &wqe->next, 1, nds, ( sizeof ( *wqe ) / 16 ) );
		for ( j = 0 ; ( ( ( void * ) &wqe->data[j] ) <
				( ( void * ) ( wqe + 1 ) ) ) ; j++ ) {
			MLX_FILL_1 ( &wqe->data[j], 1,
				     l_key, ARBEL_INVALID_LKEY );
		}
	}
	
	return 0;
}

/**
 * Create queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int arbel_create_qp ( struct ib_device *ibdev,
			     struct ib_queue_pair *qp ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_queue_pair *arbel_qp;
	struct arbelprm_qp_ee_state_transitions qpctx;
	struct arbelprm_qp_db_record *send_db_rec;
	struct arbelprm_qp_db_record *recv_db_rec;
	int qpn_offset;
	int rc;

	/* Find a free queue pair number */
	qpn_offset = arbel_alloc_qn_offset ( arbel->qp_inuse, ARBEL_MAX_QPS );
	if ( qpn_offset < 0 ) {
		DBGC ( arbel, "Arbel %p out of queue pairs\n", arbel );
		rc = qpn_offset;
		goto err_qpn_offset;
	}
	qp->qpn = ( ARBEL_QPN_BASE + arbel->limits.reserved_qps + qpn_offset );

	/* Allocate control structures */
	arbel_qp = zalloc ( sizeof ( *arbel_qp ) );
	if ( ! arbel_qp ) {
		rc = -ENOMEM;
		goto err_arbel_qp;
	}
	arbel_qp->send.doorbell_idx = arbel_send_doorbell_idx ( qpn_offset );
	arbel_qp->recv.doorbell_idx = arbel_recv_doorbell_idx ( qpn_offset );

	/* Create send and receive work queues */
	if ( ( rc = arbel_create_send_wq ( &arbel_qp->send,
					   qp->send.num_wqes ) ) != 0 )
		goto err_create_send_wq;
	if ( ( rc = arbel_create_recv_wq ( &arbel_qp->recv,
					   qp->recv.num_wqes ) ) != 0 )
		goto err_create_recv_wq;

	/* Initialise doorbell records */
	send_db_rec = &arbel->db_rec[arbel_qp->send.doorbell_idx].qp;
	MLX_FILL_1 ( send_db_rec, 0, counter, 0 );
	MLX_FILL_2 ( send_db_rec, 1,
		     res, ARBEL_UAR_RES_SQ,
		     qp_number, qp->qpn );
	recv_db_rec = &arbel->db_rec[arbel_qp->recv.doorbell_idx].qp;
	MLX_FILL_1 ( recv_db_rec, 0, counter, 0 );
	MLX_FILL_2 ( recv_db_rec, 1,
		     res, ARBEL_UAR_RES_RQ,
		     qp_number, qp->qpn );

	/* Hand queue over to hardware */
	memset ( &qpctx, 0, sizeof ( qpctx ) );
	MLX_FILL_3 ( &qpctx, 2,
		     qpc_eec_data.de, 1,
		     qpc_eec_data.pm_state, 0x03 /* Always 0x03 for UD */,
		     qpc_eec_data.st, ARBEL_ST_UD );
	MLX_FILL_6 ( &qpctx, 4,
		     qpc_eec_data.mtu, ARBEL_MTU_2048,
		     qpc_eec_data.msg_max, 11 /* 2^11 = 2048 */,
		     qpc_eec_data.log_rq_size, fls ( qp->recv.num_wqes - 1 ),
		     qpc_eec_data.log_rq_stride,
		     ( fls ( sizeof ( arbel_qp->recv.wqe[0] ) - 1 ) - 4 ),
		     qpc_eec_data.log_sq_size, fls ( qp->send.num_wqes - 1 ),
		     qpc_eec_data.log_sq_stride,
		     ( fls ( sizeof ( arbel_qp->send.wqe[0] ) - 1 ) - 4 ) );
	MLX_FILL_1 ( &qpctx, 5,
		     qpc_eec_data.usr_page, arbel->limits.reserved_uars );
	MLX_FILL_1 ( &qpctx, 10, qpc_eec_data.primary_address_path.port_number,
		     ibdev->port );
	MLX_FILL_1 ( &qpctx, 27, qpc_eec_data.pd, ARBEL_GLOBAL_PD );
	MLX_FILL_1 ( &qpctx, 29, qpc_eec_data.wqe_lkey, arbel->reserved_lkey );
	MLX_FILL_1 ( &qpctx, 30, qpc_eec_data.ssc, 1 );
	MLX_FILL_1 ( &qpctx, 33, qpc_eec_data.cqn_snd, qp->send.cq->cqn );
	MLX_FILL_1 ( &qpctx, 34, qpc_eec_data.snd_wqe_base_adr_l,
		     ( virt_to_bus ( arbel_qp->send.wqe ) >> 6 ) );
	MLX_FILL_1 ( &qpctx, 35, qpc_eec_data.snd_db_record_index,
		     arbel_qp->send.doorbell_idx );
	MLX_FILL_1 ( &qpctx, 38, qpc_eec_data.rsc, 1 );
	MLX_FILL_1 ( &qpctx, 41, qpc_eec_data.cqn_rcv, qp->recv.cq->cqn );
	MLX_FILL_1 ( &qpctx, 42, qpc_eec_data.rcv_wqe_base_adr_l,
		     ( virt_to_bus ( arbel_qp->recv.wqe ) >> 6 ) );
	MLX_FILL_1 ( &qpctx, 43, qpc_eec_data.rcv_db_record_index,
		     arbel_qp->recv.doorbell_idx );
	if ( ( rc = arbel_cmd_rst2init_qpee ( arbel, qp->qpn, &qpctx )) != 0 ){
		DBGC ( arbel, "Arbel %p RST2INIT_QPEE failed: %s\n",
		       arbel, strerror ( rc ) );
		goto err_rst2init_qpee;
	}
	memset ( &qpctx, 0, sizeof ( qpctx ) );
	MLX_FILL_2 ( &qpctx, 4,
		     qpc_eec_data.mtu, ARBEL_MTU_2048,
		     qpc_eec_data.msg_max, 11 /* 2^11 = 2048 */ );
	if ( ( rc = arbel_cmd_init2rtr_qpee ( arbel, qp->qpn, &qpctx )) != 0 ){
		DBGC ( arbel, "Arbel %p INIT2RTR_QPEE failed: %s\n",
		       arbel, strerror ( rc ) );
		goto err_init2rtr_qpee;
	}
	memset ( &qpctx, 0, sizeof ( qpctx ) );
	if ( ( rc = arbel_cmd_rtr2rts_qpee ( arbel, qp->qpn, &qpctx ) ) != 0 ){
		DBGC ( arbel, "Arbel %p RTR2RTS_QPEE failed: %s\n",
		       arbel, strerror ( rc ) );
		goto err_rtr2rts_qpee;
	}

	DBGC ( arbel, "Arbel %p QPN %#lx send ring at [%p,%p)\n",
	       arbel, qp->qpn, arbel_qp->send.wqe,
	       ( ( (void *) arbel_qp->send.wqe ) + arbel_qp->send.wqe_size ) );
	DBGC ( arbel, "Arbel %p QPN %#lx receive ring at [%p,%p)\n",
	       arbel, qp->qpn, arbel_qp->recv.wqe,
	       ( ( (void *) arbel_qp->recv.wqe ) + arbel_qp->recv.wqe_size ) );
	ib_qp_set_drvdata ( qp, arbel_qp );
	return 0;

 err_rtr2rts_qpee:
 err_init2rtr_qpee:
	arbel_cmd_2rst_qpee ( arbel, qp->qpn );
 err_rst2init_qpee:
	MLX_FILL_1 ( send_db_rec, 1, res, ARBEL_UAR_RES_NONE );
	MLX_FILL_1 ( recv_db_rec, 1, res, ARBEL_UAR_RES_NONE );
	free_dma ( arbel_qp->recv.wqe, arbel_qp->recv.wqe_size );
 err_create_recv_wq:
	free_dma ( arbel_qp->send.wqe, arbel_qp->send.wqe_size );
 err_create_send_wq:
	free ( arbel_qp );
 err_arbel_qp:
	arbel_free_qn_offset ( arbel->qp_inuse, qpn_offset );
 err_qpn_offset:
	return rc;
}

/**
 * Modify queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int arbel_modify_qp ( struct ib_device *ibdev,
			     struct ib_queue_pair *qp ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbelprm_qp_ee_state_transitions qpctx;
	int rc;

	/* Issue RTS2RTS_QP */
	memset ( &qpctx, 0, sizeof ( qpctx ) );
	MLX_FILL_1 ( &qpctx, 0, opt_param_mask, ARBEL_QPEE_OPT_PARAM_QKEY );
	MLX_FILL_1 ( &qpctx, 44, qpc_eec_data.q_key, qp->qkey );
	if ( ( rc = arbel_cmd_rts2rts_qp ( arbel, qp->qpn, &qpctx ) ) != 0 ){
		DBGC ( arbel, "Arbel %p RTS2RTS_QP failed: %s\n",
		       arbel, strerror ( rc ) );
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
static void arbel_destroy_qp ( struct ib_device *ibdev,
			       struct ib_queue_pair *qp ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_queue_pair *arbel_qp = ib_qp_get_drvdata ( qp );
	struct arbelprm_qp_db_record *send_db_rec;
	struct arbelprm_qp_db_record *recv_db_rec;
	int qpn_offset;
	int rc;

	/* Take ownership back from hardware */
	if ( ( rc = arbel_cmd_2rst_qpee ( arbel, qp->qpn ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p FATAL 2RST_QPEE failed on QPN %#lx: "
		       "%s\n", arbel, qp->qpn, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}

	/* Clear doorbell records */
	send_db_rec = &arbel->db_rec[arbel_qp->send.doorbell_idx].qp;
	recv_db_rec = &arbel->db_rec[arbel_qp->recv.doorbell_idx].qp;
	MLX_FILL_1 ( send_db_rec, 1, res, ARBEL_UAR_RES_NONE );
	MLX_FILL_1 ( recv_db_rec, 1, res, ARBEL_UAR_RES_NONE );

	/* Free memory */
	free_dma ( arbel_qp->send.wqe, arbel_qp->send.wqe_size );
	free_dma ( arbel_qp->recv.wqe, arbel_qp->recv.wqe_size );
	free ( arbel_qp );

	/* Mark queue number as free */
	qpn_offset = ( qp->qpn - ARBEL_QPN_BASE - arbel->limits.reserved_qps );
	arbel_free_qn_offset ( arbel->qp_inuse, qpn_offset );

	ib_qp_set_drvdata ( qp, NULL );
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

	DBGC2 ( arbel, "Arbel %p ringing doorbell %08x:%08x at %lx\n",
		arbel, db_reg->dword[0], db_reg->dword[1],
		virt_to_phys ( arbel->uar + offset ) );

	barrier();
	writel ( db_reg->dword[0], ( arbel->uar + offset + 0 ) );
	barrier();
	writel ( db_reg->dword[1], ( arbel->uar + offset + 4 ) );
}

/** GID used for GID-less send work queue entries */
static const struct ib_gid arbel_no_gid = {
	{ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0 } }
};

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
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_queue_pair *arbel_qp = ib_qp_get_drvdata ( qp );
	struct ib_work_queue *wq = &qp->send;
	struct arbel_send_work_queue *arbel_send_wq = &arbel_qp->send;
	struct arbelprm_ud_send_wqe *prev_wqe;
	struct arbelprm_ud_send_wqe *wqe;
	struct arbelprm_qp_db_record *qp_db_rec;
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
		     ud_address_vector.pd, ARBEL_GLOBAL_PD,
		     ud_address_vector.port_number, ibdev->port );
	MLX_FILL_2 ( &wqe->ud, 1,
		     ud_address_vector.rlid, av->lid,
		     ud_address_vector.g, av->gid_present );
	MLX_FILL_2 ( &wqe->ud, 2,
		     ud_address_vector.max_stat_rate,
			 ( ( av->rate >= 3 ) ? 0 : 1 ),
		     ud_address_vector.msg, 3 );
	MLX_FILL_1 ( &wqe->ud, 3, ud_address_vector.sl, av->sl );
	gid = ( av->gid_present ? &av->gid : &arbel_no_gid );
	memcpy ( &wqe->ud.u.dwords[4], gid, sizeof ( *gid ) );
	MLX_FILL_1 ( &wqe->ud, 8, destination_qp, av->qpn );
	MLX_FILL_1 ( &wqe->ud, 9, q_key, av->qkey );
	MLX_FILL_1 ( &wqe->data[0], 0, byte_count, iob_len ( iobuf ) );
	MLX_FILL_1 ( &wqe->data[0], 1, l_key, arbel->reserved_lkey );
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
	qp_db_rec = &arbel->db_rec[arbel_send_wq->doorbell_idx].qp;
	MLX_FILL_1 ( qp_db_rec, 0,
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
	arbel_ring_doorbell ( arbel, &db_reg, ARBEL_DB_POST_SND_OFFSET );

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
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_queue_pair *arbel_qp = ib_qp_get_drvdata ( qp );
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
 * @ret rc		Return status code
 */
static int arbel_complete ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq,
			    union arbelprm_completion_entry *cqe ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct ib_work_queue *wq;
	struct ib_queue_pair *qp;
	struct arbel_queue_pair *arbel_qp;
	struct arbel_send_work_queue *arbel_send_wq;
	struct arbel_recv_work_queue *arbel_recv_wq;
	struct arbelprm_recv_wqe *recv_wqe;
	struct io_buffer *iobuf;
	struct ib_address_vector av;
	struct ib_global_route_header *grh;
	unsigned int opcode;
	unsigned long qpn;
	int is_send;
	unsigned long wqe_adr;
	unsigned int wqe_idx;
	size_t len;
	int rc = 0;

	/* Parse completion */
	qpn = MLX_GET ( &cqe->normal, my_qpn );
	is_send = MLX_GET ( &cqe->normal, s );
	wqe_adr = ( MLX_GET ( &cqe->normal, wqe_adr ) << 6 );
	opcode = MLX_GET ( &cqe->normal, opcode );
	if ( opcode >= ARBEL_OPCODE_RECV_ERROR ) {
		/* "s" field is not valid for error opcodes */
		is_send = ( opcode == ARBEL_OPCODE_SEND_ERROR );
		DBGC ( arbel, "Arbel %p CPN %lx syndrome %x vendor %x\n",
		       arbel, cq->cqn, MLX_GET ( &cqe->error, syndrome ),
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
	arbel_qp = ib_qp_get_drvdata ( qp );
	arbel_send_wq = &arbel_qp->send;
	arbel_recv_wq = &arbel_qp->recv;

	/* Identify work queue entry index */
	if ( is_send ) {
		wqe_idx = ( ( wqe_adr - virt_to_bus ( arbel_send_wq->wqe ) ) /
			    sizeof ( arbel_send_wq->wqe[0] ) );
		assert ( wqe_idx < qp->send.num_wqes );
	} else {
		wqe_idx = ( ( wqe_adr - virt_to_bus ( arbel_recv_wq->wqe ) ) /
			    sizeof ( arbel_recv_wq->wqe[0] ) );
		assert ( wqe_idx < qp->recv.num_wqes );
	}

	/* Identify I/O buffer */
	iobuf = wq->iobufs[wqe_idx];
	if ( ! iobuf ) {
		DBGC ( arbel, "Arbel %p CQN %lx QPN %lx empty WQE %x\n",
		       arbel, cq->cqn, qpn, wqe_idx );
		return -EIO;
	}
	wq->iobufs[wqe_idx] = NULL;

	if ( is_send ) {
		/* Hand off to completion handler */
		ib_complete_send ( ibdev, qp, iobuf, rc );
	} else {
		/* Set received length */
		len = MLX_GET ( &cqe->normal, byte_cnt );
		recv_wqe = &arbel_recv_wq->wqe[wqe_idx].recv;
		assert ( MLX_GET ( &recv_wqe->data[0], local_address_l ) ==
			 virt_to_bus ( iobuf->data ) );
		assert ( MLX_GET ( &recv_wqe->data[0], byte_count ) ==
			 iob_tailroom ( iobuf ) );
		MLX_FILL_1 ( &recv_wqe->data[0], 0, byte_count, 0 );
		MLX_FILL_1 ( &recv_wqe->data[0], 1,
			     l_key, ARBEL_INVALID_LKEY );
		assert ( len <= iob_tailroom ( iobuf ) );
		iob_put ( iobuf, len );
		assert ( iob_len ( iobuf ) >= sizeof ( *grh ) );
		grh = iobuf->data;
		iob_pull ( iobuf, sizeof ( *grh ) );
		/* Construct address vector */
		memset ( &av, 0, sizeof ( av ) );
		av.qpn = MLX_GET ( &cqe->normal, rqpn );
		av.lid = MLX_GET ( &cqe->normal, rlid );
		av.sl = MLX_GET ( &cqe->normal, sl );
		av.gid_present = MLX_GET ( &cqe->normal, g );
		memcpy ( &av.gid, &grh->sgid, sizeof ( av.gid ) );
		/* Hand off to completion handler */
		ib_complete_recv ( ibdev, qp, &av, iobuf, rc );
	}

	return rc;
}			     

/**
 * Poll completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 */
static void arbel_poll_cq ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_completion_queue *arbel_cq = ib_cq_get_drvdata ( cq );
	struct arbelprm_cq_ci_db_record *ci_db_rec;
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
		if ( ( rc = arbel_complete ( ibdev, cq, cqe ) ) != 0 ) {
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
		ci_db_rec = &arbel->db_rec[arbel_cq->ci_doorbell_idx].cq_ci;
		MLX_FILL_1 ( ci_db_rec, 0,
			     counter, ( cq->next_idx & 0xffffffffUL ) );
	}
}

/***************************************************************************
 *
 * Event queues
 *
 ***************************************************************************
 */

/**
 * Create event queue
 *
 * @v arbel		Arbel device
 * @ret rc		Return status code
 */
static int arbel_create_eq ( struct arbel *arbel ) {
	struct arbel_event_queue *arbel_eq = &arbel->eq;
	struct arbelprm_eqc eqctx;
	struct arbelprm_event_mask mask;
	unsigned int i;
	int rc;

	/* Select event queue number */
	arbel_eq->eqn = arbel->limits.reserved_eqs;

	/* Calculate doorbell address */
	arbel_eq->doorbell = ( arbel->eq_ci_doorbells +
			       ARBEL_DB_EQ_OFFSET ( arbel_eq->eqn ) );

	/* Allocate event queue itself */
	arbel_eq->eqe_size =
		( ARBEL_NUM_EQES * sizeof ( arbel_eq->eqe[0] ) );
	arbel_eq->eqe = malloc_dma ( arbel_eq->eqe_size,
				     sizeof ( arbel_eq->eqe[0] ) );
	if ( ! arbel_eq->eqe ) {
		rc = -ENOMEM;
		goto err_eqe;
	}
	memset ( arbel_eq->eqe, 0, arbel_eq->eqe_size );
	for ( i = 0 ; i < ARBEL_NUM_EQES ; i++ ) {
		MLX_FILL_1 ( &arbel_eq->eqe[i].generic, 7, owner, 1 );
	}
	barrier();

	/* Hand queue over to hardware */
	memset ( &eqctx, 0, sizeof ( eqctx ) );
	MLX_FILL_1 ( &eqctx, 0, st, 0xa /* "Fired" */ );
	MLX_FILL_1 ( &eqctx, 2,
		     start_address_l, virt_to_phys ( arbel_eq->eqe ) );
	MLX_FILL_1 ( &eqctx, 3, log_eq_size, fls ( ARBEL_NUM_EQES - 1 ) );
	MLX_FILL_1 ( &eqctx, 6, pd, ARBEL_GLOBAL_PD );
	MLX_FILL_1 ( &eqctx, 7, lkey, arbel->reserved_lkey );
	if ( ( rc = arbel_cmd_sw2hw_eq ( arbel, arbel_eq->eqn,
					 &eqctx ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p SW2HW_EQ failed: %s\n",
		       arbel, strerror ( rc ) );
		goto err_sw2hw_eq;
	}

	/* Map events to this event queue */
	memset ( &mask, 0, sizeof ( mask ) );
	MLX_FILL_1 ( &mask, 1, port_state_change, 1 );
	if ( ( rc = arbel_cmd_map_eq ( arbel,
				       ( ARBEL_MAP_EQ | arbel_eq->eqn ),
				       &mask ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p MAP_EQ failed: %s\n",
		       arbel, strerror ( rc )  );
		goto err_map_eq;
	}

	DBGC ( arbel, "Arbel %p EQN %#lx ring at [%p,%p])\n",
	       arbel, arbel_eq->eqn, arbel_eq->eqe,
	       ( ( ( void * ) arbel_eq->eqe ) + arbel_eq->eqe_size ) );
	return 0;

 err_map_eq:
	arbel_cmd_hw2sw_eq ( arbel, arbel_eq->eqn, &eqctx );
 err_sw2hw_eq:
	free_dma ( arbel_eq->eqe, arbel_eq->eqe_size );
 err_eqe:
	memset ( arbel_eq, 0, sizeof ( *arbel_eq ) );
	return rc;
}

/**
 * Destroy event queue
 *
 * @v arbel		Arbel device
 */
static void arbel_destroy_eq ( struct arbel *arbel ) {
	struct arbel_event_queue *arbel_eq = &arbel->eq;
	struct arbelprm_eqc eqctx;
	struct arbelprm_event_mask mask;
	int rc;

	/* Unmap events from event queue */
	memset ( &mask, 0, sizeof ( mask ) );
	MLX_FILL_1 ( &mask, 1, port_state_change, 1 );
	if ( ( rc = arbel_cmd_map_eq ( arbel,
				       ( ARBEL_UNMAP_EQ | arbel_eq->eqn ),
				       &mask ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p FATAL MAP_EQ failed to unmap: %s\n",
		       arbel, strerror ( rc ) );
		/* Continue; HCA may die but system should survive */
	}

	/* Take ownership back from hardware */
	if ( ( rc = arbel_cmd_hw2sw_eq ( arbel, arbel_eq->eqn,
					 &eqctx ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p FATAL HW2SW_EQ failed: %s\n",
		       arbel, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}

	/* Free memory */
	free_dma ( arbel_eq->eqe, arbel_eq->eqe_size );
	memset ( arbel_eq, 0, sizeof ( *arbel_eq ) );
}

/**
 * Handle port state event
 *
 * @v arbel		Arbel device
 * @v eqe		Port state change event queue entry
 */
static void arbel_event_port_state_change ( struct arbel *arbel,
					    union arbelprm_event_entry *eqe){
	unsigned int port;
	int link_up;

	/* Get port and link status */
	port = ( MLX_GET ( &eqe->port_state_change, data.p ) - 1 );
	link_up = ( MLX_GET ( &eqe->generic, event_sub_type ) & 0x04 );
	DBGC ( arbel, "Arbel %p port %d link %s\n", arbel, ( port + 1 ),
	       ( link_up ? "up" : "down" ) );

	/* Sanity check */
	if ( port >= ARBEL_NUM_PORTS ) {
		DBGC ( arbel, "Arbel %p port %d does not exist!\n",
		       arbel, ( port + 1 ) );
		return;
	}

	/* Update MAD parameters */
	ib_smc_update ( arbel->ibdev[port], arbel_mad );

	/* Notify Infiniband core of link state change */
	ib_link_state_changed ( arbel->ibdev[port] );
}

/**
 * Poll event queue
 *
 * @v ibdev		Infiniband device
 */
static void arbel_poll_eq ( struct ib_device *ibdev ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbel_event_queue *arbel_eq = &arbel->eq;
	union arbelprm_event_entry *eqe;
	union arbelprm_eq_doorbell_register db_reg;
	unsigned int eqe_idx_mask;
	unsigned int event_type;

	while ( 1 ) {
		/* Look for event entry */
		eqe_idx_mask = ( ARBEL_NUM_EQES - 1 );
		eqe = &arbel_eq->eqe[arbel_eq->next_idx & eqe_idx_mask];
		if ( MLX_GET ( &eqe->generic, owner ) != 0 ) {
			/* Entry still owned by hardware; end of poll */
			break;
		}
		DBGCP ( arbel, "Arbel %p event:\n", arbel );
		DBGCP_HD ( arbel, eqe, sizeof ( *eqe ) );

		/* Handle event */
		event_type = MLX_GET ( &eqe->generic, event_type );
		switch ( event_type ) {
		case ARBEL_EV_PORT_STATE_CHANGE:
			arbel_event_port_state_change ( arbel, eqe );
			break;
		default:
			DBGC ( arbel, "Arbel %p unrecognised event type "
			       "%#x:\n", arbel, event_type );
			DBGC_HD ( arbel, eqe, sizeof ( *eqe ) );
			break;
		}

		/* Return ownership to hardware */
		MLX_FILL_1 ( &eqe->generic, 7, owner, 1 );
		barrier();

		/* Update event queue's index */
		arbel_eq->next_idx++;

		/* Ring doorbell */
		MLX_FILL_1 ( &db_reg.ci, 0, ci, arbel_eq->next_idx );
		DBGCP ( arbel, "Ringing doorbell %08lx with %08x\n",
			virt_to_phys ( arbel_eq->doorbell ),
			db_reg.dword[0] );
		writel ( db_reg.dword[0], arbel_eq->doorbell );
	}
}

/***************************************************************************
 *
 * Infiniband link-layer operations
 *
 ***************************************************************************
 */

/**
 * Initialise Infiniband link
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
static int arbel_open ( struct ib_device *ibdev ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbelprm_init_ib init_ib;
	int rc;

	memset ( &init_ib, 0, sizeof ( init_ib ) );
	MLX_FILL_3 ( &init_ib, 0,
		     mtu_cap, ARBEL_MTU_2048,
		     port_width_cap, 3,
		     vl_cap, 1 );
	MLX_FILL_1 ( &init_ib, 1, max_gid, 1 );
	MLX_FILL_1 ( &init_ib, 2, max_pkey, 64 );
	if ( ( rc = arbel_cmd_init_ib ( arbel, ibdev->port,
					&init_ib ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not intialise IB: %s\n",
		       arbel, strerror ( rc ) );
		return rc;
	}

	/* Update MAD parameters */
	ib_smc_update ( ibdev, arbel_mad );

	return 0;
}

/**
 * Close Infiniband link
 *
 * @v ibdev		Infiniband device
 */
static void arbel_close ( struct ib_device *ibdev ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	int rc;

	if ( ( rc = arbel_cmd_close_ib ( arbel, ibdev->port ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not close IB: %s\n",
		       arbel, strerror ( rc ) );
		/* Nothing we can do about this */
	}
}

/***************************************************************************
 *
 * Multicast group operations
 *
 ***************************************************************************
 */

/**
 * Attach to multicast group
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v gid		Multicast GID
 * @ret rc		Return status code
 */
static int arbel_mcast_attach ( struct ib_device *ibdev,
				struct ib_queue_pair *qp,
				struct ib_gid *gid ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbelprm_mgm_hash hash;
	struct arbelprm_mgm_entry mgm;
	unsigned int index;
	int rc;

	/* Generate hash table index */
	if ( ( rc = arbel_cmd_mgid_hash ( arbel, gid, &hash ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not hash GID: %s\n",
		       arbel, strerror ( rc ) );
		return rc;
	}
	index = MLX_GET ( &hash, hash );

	/* Check for existing hash table entry */
	if ( ( rc = arbel_cmd_read_mgm ( arbel, index, &mgm ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not read MGM %#x: %s\n",
		       arbel, index, strerror ( rc ) );
		return rc;
	}
	if ( MLX_GET ( &mgm, mgmqp_0.qi ) != 0 ) {
		/* FIXME: this implementation allows only a single QP
		 * per multicast group, and doesn't handle hash
		 * collisions.  Sufficient for IPoIB but may need to
		 * be extended in future.
		 */
		DBGC ( arbel, "Arbel %p MGID index %#x already in use\n",
		       arbel, index );
		return -EBUSY;
	}

	/* Update hash table entry */
	MLX_FILL_2 ( &mgm, 8,
		     mgmqp_0.qpn_i, qp->qpn,
		     mgmqp_0.qi, 1 );
	memcpy ( &mgm.u.dwords[4], gid, sizeof ( *gid ) );
	if ( ( rc = arbel_cmd_write_mgm ( arbel, index, &mgm ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not write MGM %#x: %s\n",
		       arbel, index, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Detach from multicast group
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v gid		Multicast GID
 */
static void arbel_mcast_detach ( struct ib_device *ibdev,
				 struct ib_queue_pair *qp __unused,
				 struct ib_gid *gid ) {
	struct arbel *arbel = ib_get_drvdata ( ibdev );
	struct arbelprm_mgm_hash hash;
	struct arbelprm_mgm_entry mgm;
	unsigned int index;
	int rc;

	/* Generate hash table index */
	if ( ( rc = arbel_cmd_mgid_hash ( arbel, gid, &hash ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not hash GID: %s\n",
		       arbel, strerror ( rc ) );
		return;
	}
	index = MLX_GET ( &hash, hash );

	/* Clear hash table entry */
	memset ( &mgm, 0, sizeof ( mgm ) );
	if ( ( rc = arbel_cmd_write_mgm ( arbel, index, &mgm ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not write MGM %#x: %s\n",
		       arbel, index, strerror ( rc ) );
		return;
	}
}

/** Arbel Infiniband operations */
static struct ib_device_operations arbel_ib_operations = {
	.create_cq	= arbel_create_cq,
	.destroy_cq	= arbel_destroy_cq,
	.create_qp	= arbel_create_qp,
	.modify_qp	= arbel_modify_qp,
	.destroy_qp	= arbel_destroy_qp,
	.post_send	= arbel_post_send,
	.post_recv	= arbel_post_recv,
	.poll_cq	= arbel_poll_cq,
	.poll_eq	= arbel_poll_eq,
	.open		= arbel_open,
	.close		= arbel_close,
	.mcast_attach	= arbel_mcast_attach,
	.mcast_detach	= arbel_mcast_detach,
};

/***************************************************************************
 *
 * Firmware control
 *
 ***************************************************************************
 */

/**
 * Start firmware running
 *
 * @v arbel		Arbel device
 * @ret rc		Return status code
 */
static int arbel_start_firmware ( struct arbel *arbel ) {
	struct arbelprm_query_fw fw;
	struct arbelprm_access_lam lam;
	struct arbelprm_virtual_physical_mapping map_fa;
	unsigned int fw_pages;
	unsigned int log2_fw_pages;
	size_t fw_size;
	physaddr_t fw_base;
	uint64_t eq_set_ci_base_addr;
	int rc;

	/* Get firmware parameters */
	if ( ( rc = arbel_cmd_query_fw ( arbel, &fw ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not query firmware: %s\n",
		       arbel, strerror ( rc ) );
		goto err_query_fw;
	}
	DBGC ( arbel, "Arbel %p firmware version %d.%d.%d\n", arbel,
	       MLX_GET ( &fw, fw_rev_major ), MLX_GET ( &fw, fw_rev_minor ),
	       MLX_GET ( &fw, fw_rev_subminor ) );
	fw_pages = MLX_GET ( &fw, fw_pages );
	log2_fw_pages = fls ( fw_pages - 1 );
	fw_pages = ( 1 << log2_fw_pages );
	DBGC ( arbel, "Arbel %p requires %d kB for firmware\n",
	       arbel, ( fw_pages * 4 ) );
	eq_set_ci_base_addr =
		( ( (uint64_t) MLX_GET ( &fw, eq_set_ci_base_addr_h ) << 32 ) |
		  ( (uint64_t) MLX_GET ( &fw, eq_set_ci_base_addr_l ) ) );
	arbel->eq_ci_doorbells = ioremap ( eq_set_ci_base_addr, 0x200 );

	/* Enable locally-attached memory.  Ignore failure; there may
	 * be no attached memory.
	 */
	arbel_cmd_enable_lam ( arbel, &lam );

	/* Allocate firmware pages and map firmware area */
	fw_size = ( fw_pages * 4096 );
	arbel->firmware_area = umalloc ( fw_size * 2 );
	if ( ! arbel->firmware_area ) {
		rc = -ENOMEM;
		goto err_alloc_fa;
	}
	fw_base = ( user_to_phys ( arbel->firmware_area, fw_size ) &
		    ~( fw_size - 1 ) );
	DBGC ( arbel, "Arbel %p firmware area at physical [%lx,%lx)\n",
	       arbel, fw_base, ( fw_base + fw_size ) );
	memset ( &map_fa, 0, sizeof ( map_fa ) );
	MLX_FILL_2 ( &map_fa, 3,
		     log2size, log2_fw_pages,
		     pa_l, ( fw_base >> 12 ) );
	if ( ( rc = arbel_cmd_map_fa ( arbel, &map_fa ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not map firmware: %s\n",
		       arbel, strerror ( rc ) );
		goto err_map_fa;
	}

	/* Start firmware */
	if ( ( rc = arbel_cmd_run_fw ( arbel ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not run firmware: %s\n",
		       arbel, strerror ( rc ) );
		goto err_run_fw;
	}

	DBGC ( arbel, "Arbel %p firmware started\n", arbel );
	return 0;

 err_run_fw:
	arbel_cmd_unmap_fa ( arbel );
 err_map_fa:
	ufree ( arbel->firmware_area );
	arbel->firmware_area = UNULL;
 err_alloc_fa:
 err_query_fw:
	return rc;
}

/**
 * Stop firmware running
 *
 * @v arbel		Arbel device
 */
static void arbel_stop_firmware ( struct arbel *arbel ) {
	int rc;

	if ( ( rc = arbel_cmd_unmap_fa ( arbel ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p FATAL could not stop firmware: %s\n",
		       arbel, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}
	ufree ( arbel->firmware_area );
	arbel->firmware_area = UNULL;
}

/***************************************************************************
 *
 * Infinihost Context Memory management
 *
 ***************************************************************************
 */

/**
 * Get device limits
 *
 * @v arbel		Arbel device
 * @ret rc		Return status code
 */
static int arbel_get_limits ( struct arbel *arbel ) {
	struct arbelprm_query_dev_lim dev_lim;
	int rc;

	if ( ( rc = arbel_cmd_query_dev_lim ( arbel, &dev_lim ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not get device limits: %s\n",
		       arbel, strerror ( rc ) );
		return rc;
	}

	arbel->limits.reserved_qps =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_qps ) );
	arbel->limits.qpc_entry_size = MLX_GET ( &dev_lim, qpc_entry_sz );
	arbel->limits.eqpc_entry_size = MLX_GET ( &dev_lim, eqpc_entry_sz );
	arbel->limits.reserved_srqs =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_srqs ) );
	arbel->limits.srqc_entry_size = MLX_GET ( &dev_lim, srq_entry_sz );
	arbel->limits.reserved_ees =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_ees ) );
	arbel->limits.eec_entry_size = MLX_GET ( &dev_lim, eec_entry_sz );
	arbel->limits.eeec_entry_size = MLX_GET ( &dev_lim, eeec_entry_sz );
	arbel->limits.reserved_cqs =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_cqs ) );
	arbel->limits.cqc_entry_size = MLX_GET ( &dev_lim, cqc_entry_sz );
	arbel->limits.reserved_eqs = MLX_GET ( &dev_lim, num_rsvd_eqs );
	arbel->limits.reserved_mtts =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_mtts ) );
	arbel->limits.mtt_entry_size = MLX_GET ( &dev_lim, mtt_entry_sz );
	arbel->limits.reserved_mrws =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_mrws ) );
	arbel->limits.mpt_entry_size = MLX_GET ( &dev_lim, mpt_entry_sz );
	arbel->limits.reserved_rdbs =
		( 1 << MLX_GET ( &dev_lim, log2_rsvd_rdbs ) );
	arbel->limits.eqc_entry_size = MLX_GET ( &dev_lim, eqc_entry_sz );
	arbel->limits.reserved_uars = MLX_GET ( &dev_lim, num_rsvd_uars );

	return 0;
}

/**
 * Get ICM usage
 *
 * @v log_num_entries	Log2 of the number of entries
 * @v entry_size	Entry size
 * @ret usage		Usage size in ICM
 */
static size_t icm_usage ( unsigned int log_num_entries, size_t entry_size ) {
	size_t usage;

	usage = ( ( 1 << log_num_entries ) * entry_size );
	usage = ( ( usage + 4095 ) & ~4095 );
	return usage;
}

/**
 * Allocate ICM
 *
 * @v arbel		Arbel device
 * @v init_hca		INIT_HCA structure to fill in
 * @ret rc		Return status code
 */
static int arbel_alloc_icm ( struct arbel *arbel,
			     struct arbelprm_init_hca *init_hca ) {
	struct arbelprm_scalar_parameter icm_size;
	struct arbelprm_scalar_parameter icm_aux_size;
	struct arbelprm_virtual_physical_mapping map_icm_aux;
	struct arbelprm_virtual_physical_mapping map_icm;
	union arbelprm_doorbell_record *db_rec;
	size_t icm_offset = 0;
	unsigned int log_num_qps, log_num_srqs, log_num_ees, log_num_cqs;
	unsigned int log_num_mtts, log_num_mpts, log_num_rdbs, log_num_eqs;
	int rc;

	icm_offset = ( ( arbel->limits.reserved_uars + 1 ) << 12 );

	/* Queue pair contexts */
	log_num_qps = fls ( arbel->limits.reserved_qps + ARBEL_MAX_QPS - 1 );
	MLX_FILL_2 ( init_hca, 13,
		     qpc_eec_cqc_eqc_rdb_parameters.qpc_base_addr_l,
		     ( icm_offset >> 7 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_qp,
		     log_num_qps );
	DBGC ( arbel, "Arbel %p ICM QPC base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_qps, arbel->limits.qpc_entry_size );

	/* Extended queue pair contexts */
	MLX_FILL_1 ( init_hca, 25,
		     qpc_eec_cqc_eqc_rdb_parameters.eqpc_base_addr_l,
		     icm_offset );
	DBGC ( arbel, "Arbel %p ICM EQPC base = %zx\n", arbel, icm_offset );
	//	icm_offset += icm_usage ( log_num_qps, arbel->limits.eqpc_entry_size );
	icm_offset += icm_usage ( log_num_qps, arbel->limits.qpc_entry_size );	

	/* Shared receive queue contexts */
	log_num_srqs = fls ( arbel->limits.reserved_srqs - 1 );
	MLX_FILL_2 ( init_hca, 19,
		     qpc_eec_cqc_eqc_rdb_parameters.srqc_base_addr_l,
		     ( icm_offset >> 5 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_srq,
		     log_num_srqs );
	DBGC ( arbel, "Arbel %p ICM SRQC base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_srqs, arbel->limits.srqc_entry_size );

	/* End-to-end contexts */
	log_num_ees = fls ( arbel->limits.reserved_ees - 1 );
	MLX_FILL_2 ( init_hca, 17,
		     qpc_eec_cqc_eqc_rdb_parameters.eec_base_addr_l,
		     ( icm_offset >> 7 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_ee,
		     log_num_ees );
	DBGC ( arbel, "Arbel %p ICM EEC base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_ees, arbel->limits.eec_entry_size );

	/* Extended end-to-end contexts */
	MLX_FILL_1 ( init_hca, 29,
		     qpc_eec_cqc_eqc_rdb_parameters.eeec_base_addr_l,
		     icm_offset );
	DBGC ( arbel, "Arbel %p ICM EEEC base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_ees, arbel->limits.eeec_entry_size );

	/* Completion queue contexts */
	log_num_cqs = fls ( arbel->limits.reserved_cqs + ARBEL_MAX_CQS - 1 );
	MLX_FILL_2 ( init_hca, 21,
		     qpc_eec_cqc_eqc_rdb_parameters.cqc_base_addr_l,
		     ( icm_offset >> 6 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_cq,
		     log_num_cqs );
	DBGC ( arbel, "Arbel %p ICM CQC base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_cqs, arbel->limits.cqc_entry_size );

	/* Memory translation table */
	log_num_mtts = fls ( arbel->limits.reserved_mtts - 1 );
	MLX_FILL_1 ( init_hca, 65,
		     tpt_parameters.mtt_base_addr_l, icm_offset );
	DBGC ( arbel, "Arbel %p ICM MTT base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_mtts, arbel->limits.mtt_entry_size );

	/* Memory protection table */
	log_num_mpts = fls ( arbel->limits.reserved_mrws + 1 - 1 );
	MLX_FILL_1 ( init_hca, 61,
		     tpt_parameters.mpt_base_adr_l, icm_offset );
	MLX_FILL_1 ( init_hca, 62,
		     tpt_parameters.log_mpt_sz, log_num_mpts );
	DBGC ( arbel, "Arbel %p ICM MTT base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_mpts, arbel->limits.mpt_entry_size );

	/* RDMA something or other */
	log_num_rdbs = fls ( arbel->limits.reserved_rdbs - 1 );
	MLX_FILL_1 ( init_hca, 37,
		     qpc_eec_cqc_eqc_rdb_parameters.rdb_base_addr_l,
		     icm_offset );
	DBGC ( arbel, "Arbel %p ICM RDB base = %zx\n", arbel, icm_offset );
	icm_offset += icm_usage ( log_num_rdbs, 32 );

	/* Event queue contexts */
	log_num_eqs =  fls ( arbel->limits.reserved_eqs + ARBEL_MAX_EQS - 1 );
	MLX_FILL_2 ( init_hca, 33,
		     qpc_eec_cqc_eqc_rdb_parameters.eqc_base_addr_l,
		     ( icm_offset >> 6 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_eq,
		     log_num_eqs );
	DBGC ( arbel, "Arbel %p ICM EQ base = %zx\n", arbel, icm_offset );
	icm_offset += ( ( 1 << log_num_eqs ) * arbel->limits.eqc_entry_size );

	/* Multicast table */
	MLX_FILL_1 ( init_hca, 49,
		     multicast_parameters.mc_base_addr_l, icm_offset );
	MLX_FILL_1 ( init_hca, 52,
		     multicast_parameters.log_mc_table_entry_sz,
		     fls ( sizeof ( struct arbelprm_mgm_entry ) - 1 ) );
	MLX_FILL_1 ( init_hca, 53,
		     multicast_parameters.mc_table_hash_sz, 8 );
	MLX_FILL_1 ( init_hca, 54,
		     multicast_parameters.log_mc_table_sz, 3 );
	DBGC ( arbel, "Arbel %p ICM MC base = %zx\n", arbel, icm_offset );
	icm_offset += ( 8 * sizeof ( struct arbelprm_mgm_entry ) );

	arbel->icm_len = icm_offset;
	arbel->icm_len = ( ( arbel->icm_len + 4095 ) & ~4095 );

	/* Get ICM auxiliary area size */
	memset ( &icm_size, 0, sizeof ( icm_size ) );
	MLX_FILL_1 ( &icm_size, 1, value, arbel->icm_len );
	if ( ( rc = arbel_cmd_set_icm_size ( arbel, &icm_size,
					     &icm_aux_size ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not set ICM size: %s\n",
		       arbel, strerror ( rc ) );
		goto err_set_icm_size;
	}
	arbel->icm_aux_len = ( MLX_GET ( &icm_aux_size, value ) * 4096 );

	/* Allocate ICM data and auxiliary area */
	DBGC ( arbel, "Arbel %p requires %zd kB ICM and %zd kB AUX ICM\n",
	       arbel, ( arbel->icm_len / 1024 ),
	       ( arbel->icm_aux_len / 1024 ) );
	arbel->icm = umalloc ( arbel->icm_len + arbel->icm_aux_len );
	if ( ! arbel->icm ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Map ICM auxiliary area */
	memset ( &map_icm_aux, 0, sizeof ( map_icm_aux ) );
	MLX_FILL_2 ( &map_icm_aux, 3,
		     log2size, fls ( ( arbel->icm_aux_len / 4096 ) - 1 ),
		     pa_l,
		     ( user_to_phys ( arbel->icm, arbel->icm_len ) >> 12 ) );
	if ( ( rc = arbel_cmd_map_icm_aux ( arbel, &map_icm_aux ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not map AUX ICM: %s\n",
		       arbel, strerror ( rc ) );
		goto err_map_icm_aux;
	}

	/* MAP ICM area */
	memset ( &map_icm, 0, sizeof ( map_icm ) );
	MLX_FILL_2 ( &map_icm, 3,
		     log2size, fls ( ( arbel->icm_len / 4096 ) - 1 ),
		     pa_l, ( user_to_phys ( arbel->icm, 0 ) >> 12 ) );
	if ( ( rc = arbel_cmd_map_icm ( arbel, &map_icm ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not map ICM: %s\n",
		       arbel, strerror ( rc ) );
		goto err_map_icm;
	}

	/* Initialise UAR context */
	arbel->db_rec = phys_to_virt ( user_to_phys ( arbel->icm, 0 ) +
				       ( arbel->limits.reserved_uars *
					 ARBEL_PAGE_SIZE ) );
	memset ( arbel->db_rec, 0, ARBEL_PAGE_SIZE );
	db_rec = &arbel->db_rec[ARBEL_GROUP_SEPARATOR_DOORBELL];
	MLX_FILL_1 ( &db_rec->qp, 1, res, ARBEL_UAR_RES_GROUP_SEP );

	return 0;

	arbel_cmd_unmap_icm ( arbel, ( arbel->icm_len / 4096 ) );
 err_map_icm:
	arbel_cmd_unmap_icm_aux ( arbel );
 err_map_icm_aux:
	ufree ( arbel->icm );
	arbel->icm = UNULL;
 err_alloc:
 err_set_icm_size:
	return rc;
}

/**
 * Free ICM
 *
 * @v arbel		Arbel device
 */
static void arbel_free_icm ( struct arbel *arbel ) {
	arbel_cmd_unmap_icm ( arbel, ( arbel->icm_len / 4096 ) );
	arbel_cmd_unmap_icm_aux ( arbel );
	ufree ( arbel->icm );
	arbel->icm = UNULL;
}

/***************************************************************************
 *
 * PCI interface
 *
 ***************************************************************************
 */

/**
 * Set up memory protection table
 *
 * @v arbel		Arbel device
 * @ret rc		Return status code
 */
static int arbel_setup_mpt ( struct arbel *arbel ) {
	struct arbelprm_mpt mpt;
	uint32_t key;
	int rc;

	/* Derive key */
	key = ( arbel->limits.reserved_mrws | ARBEL_MKEY_PREFIX );
	arbel->reserved_lkey = ( ( key << 8 ) | ( key >> 24 ) );

	/* Initialise memory protection table */
	memset ( &mpt, 0, sizeof ( mpt ) );
	MLX_FILL_4 ( &mpt, 0,
		     r_w, 1,
		     pa, 1,
		     lr, 1,
		     lw, 1 );
	MLX_FILL_1 ( &mpt, 2, mem_key, key );
	MLX_FILL_1 ( &mpt, 3, pd, ARBEL_GLOBAL_PD );
	MLX_FILL_1 ( &mpt, 6, reg_wnd_len_h, 0xffffffffUL );
	MLX_FILL_1 ( &mpt, 7, reg_wnd_len_l, 0xffffffffUL );
	if ( ( rc = arbel_cmd_sw2hw_mpt ( arbel, arbel->limits.reserved_mrws,
					  &mpt ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not set up MPT: %s\n",
		       arbel, strerror ( rc ) );
		return rc;
	}

	return 0;
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
	struct arbel *arbel;
	struct ib_device *ibdev;
	struct arbelprm_init_hca init_hca;
	int i;
	int rc;

	/* Allocate Arbel device */
	arbel = zalloc ( sizeof ( *arbel ) );
	if ( ! arbel ) {
		rc = -ENOMEM;
		goto err_alloc_arbel;
	}
	pci_set_drvdata ( pci, arbel );

	/* Allocate Infiniband devices */
	for ( i = 0 ; i < ARBEL_NUM_PORTS ; i++ ) {
		ibdev = alloc_ibdev ( 0 );
		if ( ! ibdev ) {
			rc = -ENOMEM;
			goto err_alloc_ibdev;
		}
		arbel->ibdev[i] = ibdev;
		ibdev->op = &arbel_ib_operations;
		ibdev->dev = &pci->dev;
		ibdev->port = ( ARBEL_PORT_BASE + i );
		ib_set_drvdata ( ibdev, arbel );
	}

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Get PCI BARs */
	arbel->config = ioremap ( pci_bar_start ( pci, ARBEL_PCI_CONFIG_BAR ),
				  ARBEL_PCI_CONFIG_BAR_SIZE );
	arbel->uar = ioremap ( ( pci_bar_start ( pci, ARBEL_PCI_UAR_BAR ) +
				 ARBEL_PCI_UAR_IDX * ARBEL_PCI_UAR_SIZE ),
			       ARBEL_PCI_UAR_SIZE );

	/* Allocate space for mailboxes */
	arbel->mailbox_in = malloc_dma ( ARBEL_MBOX_SIZE, ARBEL_MBOX_ALIGN );
	if ( ! arbel->mailbox_in ) {
		rc = -ENOMEM;
		goto err_mailbox_in;
	}
	arbel->mailbox_out = malloc_dma ( ARBEL_MBOX_SIZE, ARBEL_MBOX_ALIGN );
	if ( ! arbel->mailbox_out ) {
		rc = -ENOMEM;
		goto err_mailbox_out;
	}

	/* Start firmware */
	if ( ( rc = arbel_start_firmware ( arbel ) ) != 0 )
		goto err_start_firmware;

	/* Get device limits */
	if ( ( rc = arbel_get_limits ( arbel ) ) != 0 )
		goto err_get_limits;

	/* Allocate ICM */
	memset ( &init_hca, 0, sizeof ( init_hca ) );
	if ( ( rc = arbel_alloc_icm ( arbel, &init_hca ) ) != 0 )
		goto err_alloc_icm;

	/* Initialise HCA */
	MLX_FILL_1 ( &init_hca, 74, uar_parameters.log_max_uars, 1 );
	if ( ( rc = arbel_cmd_init_hca ( arbel, &init_hca ) ) != 0 ) {
		DBGC ( arbel, "Arbel %p could not initialise HCA: %s\n",
		       arbel, strerror ( rc ) );
		goto err_init_hca;
	}

	/* Set up memory protection */
	if ( ( rc = arbel_setup_mpt ( arbel ) ) != 0 )
		goto err_setup_mpt;

	/* Set up event queue */
	if ( ( rc = arbel_create_eq ( arbel ) ) != 0 )
		goto err_create_eq;

	/* Update MAD parameters */
	for ( i = 0 ; i < ARBEL_NUM_PORTS ; i++ )
		ib_smc_update ( arbel->ibdev[i], arbel_mad );

	/* Register Infiniband devices */
	for ( i = 0 ; i < ARBEL_NUM_PORTS ; i++ ) {
		if ( ( rc = register_ibdev ( arbel->ibdev[i] ) ) != 0 ) {
			DBGC ( arbel, "Arbel %p could not register IB "
			       "device: %s\n", arbel, strerror ( rc ) );
			goto err_register_ibdev;
		}
	}

	return 0;

	i = ARBEL_NUM_PORTS;
 err_register_ibdev:
	for ( i-- ; i >= 0 ; i-- )
		unregister_ibdev ( arbel->ibdev[i] );
	arbel_destroy_eq ( arbel );
 err_create_eq:
 err_setup_mpt:
	arbel_cmd_close_hca ( arbel );
 err_init_hca:
	arbel_free_icm ( arbel );
 err_alloc_icm:
 err_get_limits:
	arbel_stop_firmware ( arbel );
 err_start_firmware:
	free_dma ( arbel->mailbox_out, ARBEL_MBOX_SIZE );
 err_mailbox_out:
	free_dma ( arbel->mailbox_in, ARBEL_MBOX_SIZE );
 err_mailbox_in:
	i = ARBEL_NUM_PORTS;
 err_alloc_ibdev:
	for ( i-- ; i >= 0 ; i-- )
		ibdev_put ( arbel->ibdev[i] );
	free ( arbel );
 err_alloc_arbel:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void arbel_remove ( struct pci_device *pci ) {
	struct arbel *arbel = pci_get_drvdata ( pci );
	int i;

	for ( i = ( ARBEL_NUM_PORTS - 1 ) ; i >= 0 ; i-- )
		unregister_ibdev ( arbel->ibdev[i] );
	arbel_destroy_eq ( arbel );
	arbel_cmd_close_hca ( arbel );
	arbel_free_icm ( arbel );
	arbel_stop_firmware ( arbel );
	arbel_stop_firmware ( arbel );
	free_dma ( arbel->mailbox_out, ARBEL_MBOX_SIZE );
	free_dma ( arbel->mailbox_in, ARBEL_MBOX_SIZE );
	for ( i = ( ARBEL_NUM_PORTS - 1 ) ; i >= 0 ; i-- )
		ibdev_put ( arbel->ibdev[i] );
	free ( arbel );
}

static struct pci_device_id arbel_nics[] = {
	PCI_ROM ( 0x15b3, 0x6282, "mt25218", "MT25218 HCA driver", 0 ),
	PCI_ROM ( 0x15b3, 0x6274, "mt25204", "MT25204 HCA driver", 0 ),
};

struct pci_driver arbel_driver __pci_driver = {
	.ids = arbel_nics,
	.id_count = ( sizeof ( arbel_nics ) / sizeof ( arbel_nics[0] ) ),
	.probe = arbel_probe,
	.remove = arbel_remove,
};
