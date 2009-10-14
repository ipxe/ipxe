/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 * Copyright (C) 2008 Mellanox Technologies Ltd.
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
#include <gpxe/pcibackup.h>
#include <gpxe/malloc.h>
#include <gpxe/umalloc.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_smc.h>
#include "hermon.h"

/**
 * @file
 *
 * Mellanox Hermon Infiniband HCA
 *
 */

/***************************************************************************
 *
 * Queue number allocation
 *
 ***************************************************************************
 */

/**
 * Allocate offsets within usage bitmask
 *
 * @v bits		Usage bitmask
 * @v bits_len		Length of usage bitmask
 * @v num_bits		Number of contiguous bits to allocate within bitmask
 * @ret bit		First free bit within bitmask, or negative error
 */
static int hermon_bitmask_alloc ( hermon_bitmask_t *bits,
				  unsigned int bits_len,
				  unsigned int num_bits ) {
	unsigned int bit = 0;
	hermon_bitmask_t mask = 1;
	unsigned int found = 0;

	/* Search bits for num_bits contiguous free bits */
	while ( bit < bits_len ) {
		if ( ( mask & *bits ) == 0 ) {
			if ( ++found == num_bits )
				goto found;
		} else {
			found = 0;
		}
		bit++;
		mask = ( mask << 1 ) | ( mask >> ( 8 * sizeof ( mask ) - 1 ) );
		if ( mask == 1 )
			bits++;
	}
	return -ENFILE;

 found:
	/* Mark bits as in-use */
	do {
		*bits |= mask;
		if ( mask == 1 )
			bits--;
		mask = ( mask >> 1 ) | ( mask << ( 8 * sizeof ( mask ) - 1 ) );
	} while ( --found );

	return ( bit - num_bits + 1 );
}

/**
 * Free offsets within usage bitmask
 *
 * @v bits		Usage bitmask
 * @v bit		Starting bit within bitmask
 * @v num_bits		Number of contiguous bits to free within bitmask
 */
static void hermon_bitmask_free ( hermon_bitmask_t *bits,
				  int bit, unsigned int num_bits ) {
	hermon_bitmask_t mask;

	for ( ; num_bits ; bit++, num_bits-- ) {
		mask = ( 1 << ( bit % ( 8 * sizeof ( mask ) ) ) );
		bits[ ( bit / ( 8 * sizeof ( mask ) ) ) ] &= ~mask;
	}
}

/***************************************************************************
 *
 * HCA commands
 *
 ***************************************************************************
 */

/**
 * Wait for Hermon command completion
 *
 * @v hermon		Hermon device
 * @v hcr		HCA command registers
 * @ret rc		Return status code
 */
static int hermon_cmd_wait ( struct hermon *hermon,
			     struct hermonprm_hca_command_register *hcr ) {
	unsigned int wait;

	for ( wait = HERMON_HCR_MAX_WAIT_MS ; wait ; wait-- ) {
		hcr->u.dwords[6] =
			readl ( hermon->config + HERMON_HCR_REG ( 6 ) );
		if ( ( MLX_GET ( hcr, go ) == 0 ) &&
		     ( MLX_GET ( hcr, t ) == hermon->toggle ) )
			return 0;
		mdelay ( 1 );
	}
	return -EBUSY;
}

/**
 * Issue HCA command
 *
 * @v hermon		Hermon device
 * @v command		Command opcode, flags and input/output lengths
 * @v op_mod		Opcode modifier (0 if no modifier applicable)
 * @v in		Input parameters
 * @v in_mod		Input modifier (0 if no modifier applicable)
 * @v out		Output parameters
 * @ret rc		Return status code
 */
static int hermon_cmd ( struct hermon *hermon, unsigned long command,
			unsigned int op_mod, const void *in,
			unsigned int in_mod, void *out ) {
	struct hermonprm_hca_command_register hcr;
	unsigned int opcode = HERMON_HCR_OPCODE ( command );
	size_t in_len = HERMON_HCR_IN_LEN ( command );
	size_t out_len = HERMON_HCR_OUT_LEN ( command );
	void *in_buffer;
	void *out_buffer;
	unsigned int status;
	unsigned int i;
	int rc;

	assert ( in_len <= HERMON_MBOX_SIZE );
	assert ( out_len <= HERMON_MBOX_SIZE );

	DBGC2 ( hermon, "Hermon %p command %02x in %zx%s out %zx%s\n",
		hermon, opcode, in_len,
		( ( command & HERMON_HCR_IN_MBOX ) ? "(mbox)" : "" ), out_len,
		( ( command & HERMON_HCR_OUT_MBOX ) ? "(mbox)" : "" ) );

	/* Check that HCR is free */
	if ( ( rc = hermon_cmd_wait ( hermon, &hcr ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p command interface locked\n",
		       hermon );
		return rc;
	}

	/* Flip HCR toggle */
	hermon->toggle = ( 1 - hermon->toggle );

	/* Prepare HCR */
	memset ( &hcr, 0, sizeof ( hcr ) );
	in_buffer = &hcr.u.dwords[0];
	if ( in_len && ( command & HERMON_HCR_IN_MBOX ) ) {
		in_buffer = hermon->mailbox_in;
		MLX_FILL_1 ( &hcr, 1, in_param_l, virt_to_bus ( in_buffer ) );
	}
	memcpy ( in_buffer, in, in_len );
	MLX_FILL_1 ( &hcr, 2, input_modifier, in_mod );
	out_buffer = &hcr.u.dwords[3];
	if ( out_len && ( command & HERMON_HCR_OUT_MBOX ) ) {
		out_buffer = hermon->mailbox_out;
		MLX_FILL_1 ( &hcr, 4, out_param_l,
			     virt_to_bus ( out_buffer ) );
	}
	MLX_FILL_4 ( &hcr, 6,
		     opcode, opcode,
		     opcode_modifier, op_mod,
		     go, 1,
		     t, hermon->toggle );
	DBGC ( hermon, "Hermon %p issuing command %04x\n",
	       hermon, opcode );
	DBGC2_HDA ( hermon, virt_to_phys ( hermon->config + HERMON_HCR_BASE ),
		    &hcr, sizeof ( hcr ) );
	if ( in_len && ( command & HERMON_HCR_IN_MBOX ) ) {
		DBGC2 ( hermon, "Input mailbox:\n" );
		DBGC2_HDA ( hermon, virt_to_phys ( in_buffer ), in_buffer,
			    ( ( in_len < 512 ) ? in_len : 512 ) );
	}

	/* Issue command */
	for ( i = 0 ; i < ( sizeof ( hcr ) / sizeof ( hcr.u.dwords[0] ) ) ;
	      i++ ) {
		writel ( hcr.u.dwords[i],
			 hermon->config + HERMON_HCR_REG ( i ) );
		barrier();
	}

	/* Wait for command completion */
	if ( ( rc = hermon_cmd_wait ( hermon, &hcr ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p timed out waiting for command:\n",
		       hermon );
		DBGC_HDA ( hermon,
			   virt_to_phys ( hermon->config + HERMON_HCR_BASE ),
			   &hcr, sizeof ( hcr ) );
		return rc;
	}

	/* Check command status */
	status = MLX_GET ( &hcr, status );
	if ( status != 0 ) {
		DBGC ( hermon, "Hermon %p command failed with status %02x:\n",
		       hermon, status );
		DBGC_HDA ( hermon,
			   virt_to_phys ( hermon->config + HERMON_HCR_BASE ),
			   &hcr, sizeof ( hcr ) );
		return -EIO;
	}

	/* Read output parameters, if any */
	hcr.u.dwords[3] = readl ( hermon->config + HERMON_HCR_REG ( 3 ) );
	hcr.u.dwords[4] = readl ( hermon->config + HERMON_HCR_REG ( 4 ) );
	memcpy ( out, out_buffer, out_len );
	if ( out_len ) {
		DBGC2 ( hermon, "Output%s:\n",
			( command & HERMON_HCR_OUT_MBOX ) ? " mailbox" : "" );
		DBGC2_HDA ( hermon, virt_to_phys ( out_buffer ), out_buffer,
			    ( ( out_len < 512 ) ? out_len : 512 ) );
	}

	return 0;
}

static inline int
hermon_cmd_query_dev_cap ( struct hermon *hermon,
			   struct hermonprm_query_dev_cap *dev_cap ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_QUERY_DEV_CAP,
						 1, sizeof ( *dev_cap ) ),
			    0, NULL, 0, dev_cap );
}

static inline int
hermon_cmd_query_fw ( struct hermon *hermon, struct hermonprm_query_fw *fw ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_QUERY_FW,
						 1, sizeof ( *fw ) ),
			    0, NULL, 0, fw );
}

static inline int
hermon_cmd_init_hca ( struct hermon *hermon,
		      const struct hermonprm_init_hca *init_hca ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_INIT_HCA,
						1, sizeof ( *init_hca ) ),
			    0, init_hca, 0, NULL );
}

static inline int
hermon_cmd_close_hca ( struct hermon *hermon ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_CLOSE_HCA ),
			    0, NULL, 0, NULL );
}

static inline int
hermon_cmd_init_port ( struct hermon *hermon, unsigned int port,
		       const struct hermonprm_init_port *init_port ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_INIT_PORT,
						1, sizeof ( *init_port ) ),
			    0, init_port, port, NULL );
}

static inline int
hermon_cmd_close_port ( struct hermon *hermon, unsigned int port ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_CLOSE_PORT ),
			    0, NULL, port, NULL );
}

static inline int
hermon_cmd_sw2hw_mpt ( struct hermon *hermon, unsigned int index,
		       const struct hermonprm_mpt *mpt ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_SW2HW_MPT,
						1, sizeof ( *mpt ) ),
			    0, mpt, index, NULL );
}

static inline int
hermon_cmd_write_mtt ( struct hermon *hermon,
		       const struct hermonprm_write_mtt *write_mtt ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_WRITE_MTT,
						1, sizeof ( *write_mtt ) ),
			    0, write_mtt, 1, NULL );
}

static inline int
hermon_cmd_map_eq ( struct hermon *hermon, unsigned long index_map,
		    const struct hermonprm_event_mask *mask ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_MAP_EQ,
						0, sizeof ( *mask ) ),
			    0, mask, index_map, NULL );
}

static inline int
hermon_cmd_sw2hw_eq ( struct hermon *hermon, unsigned int index,
		      const struct hermonprm_eqc *eqctx ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_SW2HW_EQ,
						1, sizeof ( *eqctx ) ),
			    0, eqctx, index, NULL );
}

static inline int
hermon_cmd_hw2sw_eq ( struct hermon *hermon, unsigned int index,
		      struct hermonprm_eqc *eqctx ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_HW2SW_EQ,
						 1, sizeof ( *eqctx ) ),
			    1, NULL, index, eqctx );
}

static inline int
hermon_cmd_query_eq ( struct hermon *hermon, unsigned int index,
		      struct hermonprm_eqc *eqctx ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_QUERY_EQ,
						 1, sizeof ( *eqctx ) ),
			    0, NULL, index, eqctx );
}

static inline int
hermon_cmd_sw2hw_cq ( struct hermon *hermon, unsigned long cqn,
		      const struct hermonprm_completion_queue_context *cqctx ){
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_SW2HW_CQ,
						1, sizeof ( *cqctx ) ),
			    0, cqctx, cqn, NULL );
}

static inline int
hermon_cmd_hw2sw_cq ( struct hermon *hermon, unsigned long cqn,
		      struct hermonprm_completion_queue_context *cqctx) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_HW2SW_CQ,
						 1, sizeof ( *cqctx ) ),
			    0, NULL, cqn, cqctx );
}

static inline int
hermon_cmd_rst2init_qp ( struct hermon *hermon, unsigned long qpn,
			 const struct hermonprm_qp_ee_state_transitions *ctx ){
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_RST2INIT_QP,
						1, sizeof ( *ctx ) ),
			    0, ctx, qpn, NULL );
}

static inline int
hermon_cmd_init2rtr_qp ( struct hermon *hermon, unsigned long qpn,
			 const struct hermonprm_qp_ee_state_transitions *ctx ){
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_INIT2RTR_QP,
						1, sizeof ( *ctx ) ),
			    0, ctx, qpn, NULL );
}

static inline int
hermon_cmd_rtr2rts_qp ( struct hermon *hermon, unsigned long qpn,
			const struct hermonprm_qp_ee_state_transitions *ctx ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_RTR2RTS_QP,
						1, sizeof ( *ctx ) ),
			    0, ctx, qpn, NULL );
}

static inline int
hermon_cmd_rts2rts_qp ( struct hermon *hermon, unsigned long qpn,
			const struct hermonprm_qp_ee_state_transitions *ctx ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_RTS2RTS_QP,
						1, sizeof ( *ctx ) ),
			    0, ctx, qpn, NULL );
}

static inline int
hermon_cmd_2rst_qp ( struct hermon *hermon, unsigned long qpn ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_2RST_QP ),
			    0x03, NULL, qpn, NULL );
}

static inline int
hermon_cmd_query_qp ( struct hermon *hermon, unsigned long qpn,
		      struct hermonprm_qp_ee_state_transitions *ctx ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_QUERY_QP,
						 1, sizeof ( *ctx ) ),
			    0, NULL, qpn, ctx );
}

static inline int
hermon_cmd_conf_special_qp ( struct hermon *hermon, unsigned int internal_qps,
			     unsigned long base_qpn ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_CONF_SPECIAL_QP ),
			    internal_qps, NULL, base_qpn, NULL );
}

static inline int
hermon_cmd_mad_ifc ( struct hermon *hermon, unsigned int port,
		     union hermonprm_mad *mad ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_INOUT_CMD ( HERMON_HCR_MAD_IFC,
						   1, sizeof ( *mad ),
						   1, sizeof ( *mad ) ),
			    0x03, mad, port, mad );
}

static inline int
hermon_cmd_read_mcg ( struct hermon *hermon, unsigned int index,
		      struct hermonprm_mcg_entry *mcg ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_OUT_CMD ( HERMON_HCR_READ_MCG,
						 1, sizeof ( *mcg ) ),
			    0, NULL, index, mcg );
}

static inline int
hermon_cmd_write_mcg ( struct hermon *hermon, unsigned int index,
		       const struct hermonprm_mcg_entry *mcg ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_WRITE_MCG,
						1, sizeof ( *mcg ) ),
			    0, mcg, index, NULL );
}

static inline int
hermon_cmd_mgid_hash ( struct hermon *hermon, const struct ib_gid *gid,
		       struct hermonprm_mgm_hash *hash ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_INOUT_CMD ( HERMON_HCR_MGID_HASH,
						   1, sizeof ( *gid ),
						   0, sizeof ( *hash ) ),
			    0, gid, 0, hash );
}

static inline int
hermon_cmd_run_fw ( struct hermon *hermon ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_RUN_FW ),
			    0, NULL, 0, NULL );
}

static inline int
hermon_cmd_unmap_icm ( struct hermon *hermon, unsigned int page_count,
		       const struct hermonprm_scalar_parameter *offset ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_UNMAP_ICM,
						0, sizeof ( *offset ) ),
			    0, offset, page_count, NULL );
}

static inline int
hermon_cmd_map_icm ( struct hermon *hermon,
		     const struct hermonprm_virtual_physical_mapping *map ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_MAP_ICM,
						1, sizeof ( *map ) ),
			    0, map, 1, NULL );
}

static inline int
hermon_cmd_unmap_icm_aux ( struct hermon *hermon ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_UNMAP_ICM_AUX ),
			    0, NULL, 0, NULL );
}

static inline int
hermon_cmd_map_icm_aux ( struct hermon *hermon,
		       const struct hermonprm_virtual_physical_mapping *map ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_MAP_ICM_AUX,
						1, sizeof ( *map ) ),
			    0, map, 1, NULL );
}

static inline int
hermon_cmd_set_icm_size ( struct hermon *hermon,
			  const struct hermonprm_scalar_parameter *icm_size,
			  struct hermonprm_scalar_parameter *icm_aux_size ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_INOUT_CMD ( HERMON_HCR_SET_ICM_SIZE,
						   0, sizeof ( *icm_size ),
						   0, sizeof (*icm_aux_size) ),
			    0, icm_size, 0, icm_aux_size );
}

static inline int
hermon_cmd_unmap_fa ( struct hermon *hermon ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_VOID_CMD ( HERMON_HCR_UNMAP_FA ),
			    0, NULL, 0, NULL );
}

static inline int
hermon_cmd_map_fa ( struct hermon *hermon,
		    const struct hermonprm_virtual_physical_mapping *map ) {
	return hermon_cmd ( hermon,
			    HERMON_HCR_IN_CMD ( HERMON_HCR_MAP_FA,
						1, sizeof ( *map ) ),
			    0, map, 1, NULL );
}

static inline int
hermon_cmd_sense_port ( struct hermon *hermon, unsigned int port,
			struct hermonprm_sense_port *port_type ) {
	return hermon_cmd ( hermon,
                            HERMON_HCR_OUT_CMD ( HERMON_HCR_SENSE_PORT,
                                                 1, sizeof ( *port_type ) ),
                            0, NULL, port, port_type );
}


/***************************************************************************
 *
 * Memory translation table operations
 *
 ***************************************************************************
 */

/**
 * Allocate MTT entries
 *
 * @v hermon		Hermon device
 * @v memory		Memory to map into MTT
 * @v len		Length of memory to map
 * @v mtt		MTT descriptor to fill in
 * @ret rc		Return status code
 */
static int hermon_alloc_mtt ( struct hermon *hermon,
			      const void *memory, size_t len,
			      struct hermon_mtt *mtt ) {
	struct hermonprm_write_mtt write_mtt;
	physaddr_t start;
	unsigned int page_offset;
	unsigned int num_pages;
	int mtt_offset;
	unsigned int mtt_base_addr;
	unsigned int i;
	int rc;

	/* Find available MTT entries */
	start = virt_to_phys ( memory );
	page_offset = ( start & ( HERMON_PAGE_SIZE - 1 ) );
	start -= page_offset;
	len += page_offset;
	num_pages = ( ( len + HERMON_PAGE_SIZE - 1 ) / HERMON_PAGE_SIZE );
	mtt_offset = hermon_bitmask_alloc ( hermon->mtt_inuse, HERMON_MAX_MTTS,
					    num_pages );
	if ( mtt_offset < 0 ) {
		DBGC ( hermon, "Hermon %p could not allocate %d MTT entries\n",
		       hermon, num_pages );
		rc = mtt_offset;
		goto err_mtt_offset;
	}
	mtt_base_addr = ( ( hermon->cap.reserved_mtts + mtt_offset ) *
			  hermon->cap.mtt_entry_size );

	/* Fill in MTT structure */
	mtt->mtt_offset = mtt_offset;
	mtt->num_pages = num_pages;
	mtt->mtt_base_addr = mtt_base_addr;
	mtt->page_offset = page_offset;

	/* Construct and issue WRITE_MTT commands */
	for ( i = 0 ; i < num_pages ; i++ ) {
		memset ( &write_mtt, 0, sizeof ( write_mtt ) );
		MLX_FILL_1 ( &write_mtt.mtt_base_addr, 1,
			     value, mtt_base_addr );
		MLX_FILL_2 ( &write_mtt.mtt, 1,
			     p, 1,
			     ptag_l, ( start >> 3 ) );
		if ( ( rc = hermon_cmd_write_mtt ( hermon,
						   &write_mtt ) ) != 0 ) {
			DBGC ( hermon, "Hermon %p could not write MTT at %x\n",
			       hermon, mtt_base_addr );
			goto err_write_mtt;
		}
		start += HERMON_PAGE_SIZE;
		mtt_base_addr += hermon->cap.mtt_entry_size;
	}

	return 0;

 err_write_mtt:
	hermon_bitmask_free ( hermon->mtt_inuse, mtt_offset, num_pages );
 err_mtt_offset:
	return rc;
}

/**
 * Free MTT entries
 *
 * @v hermon		Hermon device
 * @v mtt		MTT descriptor
 */
static void hermon_free_mtt ( struct hermon *hermon,
			      struct hermon_mtt *mtt ) {
	hermon_bitmask_free ( hermon->mtt_inuse, mtt->mtt_offset,
			      mtt->num_pages );
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
static int hermon_mad ( struct ib_device *ibdev, union ib_mad *mad ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	union hermonprm_mad mad_ifc;
	int rc;

	linker_assert ( sizeof ( *mad ) == sizeof ( mad_ifc.mad ),
			mad_size_mismatch );

	/* Copy in request packet */
	memcpy ( &mad_ifc.mad, mad, sizeof ( mad_ifc.mad ) );

	/* Issue MAD */
	if ( ( rc = hermon_cmd_mad_ifc ( hermon, ibdev->port,
					 &mad_ifc ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not issue MAD IFC: %s\n",
		       hermon, strerror ( rc ) );
		return rc;
	}

	/* Copy out reply packet */
	memcpy ( mad, &mad_ifc.mad, sizeof ( *mad ) );

	if ( mad->hdr.status != 0 ) {
		DBGC ( hermon, "Hermon %p MAD IFC status %04x\n",
		       hermon, ntohs ( mad->hdr.status ) );
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
static int hermon_create_cq ( struct ib_device *ibdev,
			      struct ib_completion_queue *cq ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_completion_queue *hermon_cq;
	struct hermonprm_completion_queue_context cqctx;
	int cqn_offset;
	unsigned int i;
	int rc;

	/* Find a free completion queue number */
	cqn_offset = hermon_bitmask_alloc ( hermon->cq_inuse,
					    HERMON_MAX_CQS, 1 );
	if ( cqn_offset < 0 ) {
		DBGC ( hermon, "Hermon %p out of completion queues\n",
		       hermon );
		rc = cqn_offset;
		goto err_cqn_offset;
	}
	cq->cqn = ( hermon->cap.reserved_cqs + cqn_offset );

	/* Allocate control structures */
	hermon_cq = zalloc ( sizeof ( *hermon_cq ) );
	if ( ! hermon_cq ) {
		rc = -ENOMEM;
		goto err_hermon_cq;
	}

	/* Allocate completion queue itself */
	hermon_cq->cqe_size = ( cq->num_cqes * sizeof ( hermon_cq->cqe[0] ) );
	hermon_cq->cqe = malloc_dma ( hermon_cq->cqe_size,
				      sizeof ( hermon_cq->cqe[0] ) );
	if ( ! hermon_cq->cqe ) {
		rc = -ENOMEM;
		goto err_cqe;
	}
	memset ( hermon_cq->cqe, 0, hermon_cq->cqe_size );
	for ( i = 0 ; i < cq->num_cqes ; i++ ) {
		MLX_FILL_1 ( &hermon_cq->cqe[i].normal, 7, owner, 1 );
	}
	barrier();

	/* Allocate MTT entries */
	if ( ( rc = hermon_alloc_mtt ( hermon, hermon_cq->cqe,
				       hermon_cq->cqe_size,
				       &hermon_cq->mtt ) ) != 0 )
		goto err_alloc_mtt;

	/* Hand queue over to hardware */
	memset ( &cqctx, 0, sizeof ( cqctx ) );
	MLX_FILL_1 ( &cqctx, 0, st, 0xa /* "Event fired" */ );
	MLX_FILL_1 ( &cqctx, 2,
		     page_offset, ( hermon_cq->mtt.page_offset >> 5 ) );
	MLX_FILL_2 ( &cqctx, 3,
		     usr_page, HERMON_UAR_NON_EQ_PAGE,
		     log_cq_size, fls ( cq->num_cqes - 1 ) );
	MLX_FILL_1 ( &cqctx, 7, mtt_base_addr_l,
		     ( hermon_cq->mtt.mtt_base_addr >> 3 ) );
	MLX_FILL_1 ( &cqctx, 15, db_record_addr_l,
		     ( virt_to_phys ( &hermon_cq->doorbell ) >> 3 ) );
	if ( ( rc = hermon_cmd_sw2hw_cq ( hermon, cq->cqn, &cqctx ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p SW2HW_CQ failed: %s\n",
		       hermon, strerror ( rc ) );
		goto err_sw2hw_cq;
	}

	DBGC ( hermon, "Hermon %p CQN %#lx ring at [%p,%p)\n",
	       hermon, cq->cqn, hermon_cq->cqe,
	       ( ( ( void * ) hermon_cq->cqe ) + hermon_cq->cqe_size ) );
	ib_cq_set_drvdata ( cq, hermon_cq );
	return 0;

 err_sw2hw_cq:
	hermon_free_mtt ( hermon, &hermon_cq->mtt );
 err_alloc_mtt:
	free_dma ( hermon_cq->cqe, hermon_cq->cqe_size );
 err_cqe:
	free ( hermon_cq );
 err_hermon_cq:
	hermon_bitmask_free ( hermon->cq_inuse, cqn_offset, 1 );
 err_cqn_offset:
	return rc;
}

/**
 * Destroy completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 */
static void hermon_destroy_cq ( struct ib_device *ibdev,
				struct ib_completion_queue *cq ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_completion_queue *hermon_cq = ib_cq_get_drvdata ( cq );
	struct hermonprm_completion_queue_context cqctx;
	int cqn_offset;
	int rc;

	/* Take ownership back from hardware */
	if ( ( rc = hermon_cmd_hw2sw_cq ( hermon, cq->cqn, &cqctx ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p FATAL HW2SW_CQ failed on CQN %#lx: "
		       "%s\n", hermon, cq->cqn, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}

	/* Free MTT entries */
	hermon_free_mtt ( hermon, &hermon_cq->mtt );

	/* Free memory */
	free_dma ( hermon_cq->cqe, hermon_cq->cqe_size );
	free ( hermon_cq );

	/* Mark queue number as free */
	cqn_offset = ( cq->cqn - hermon->cap.reserved_cqs );
	hermon_bitmask_free ( hermon->cq_inuse, cqn_offset, 1 );

	ib_cq_set_drvdata ( cq, NULL );
}

/***************************************************************************
 *
 * Queue pair operations
 *
 ***************************************************************************
 */

/**
 * Assign queue pair number
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int hermon_alloc_qpn ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	unsigned int port_offset;
	int qpn_offset;

	/* Calculate queue pair number */
	port_offset = ( ibdev->port - HERMON_PORT_BASE );

	switch ( qp->type ) {
	case IB_QPT_SMI:
		qp->qpn = ( hermon->special_qpn_base + port_offset );
		return 0;
	case IB_QPT_GSI:
		qp->qpn = ( hermon->special_qpn_base + 2 + port_offset );
		return 0;
	case IB_QPT_UD:
	case IB_QPT_RC:
		/* Find a free queue pair number */
		qpn_offset = hermon_bitmask_alloc ( hermon->qp_inuse,
						    HERMON_MAX_QPS, 1 );
		if ( qpn_offset < 0 ) {
			DBGC ( hermon, "Hermon %p out of queue pairs\n",
			       hermon );
			return qpn_offset;
		}
		qp->qpn = ( ( random() & HERMON_QPN_RANDOM_MASK ) |
			    ( hermon->qpn_base + qpn_offset ) );
		return 0;
	default:
		DBGC ( hermon, "Hermon %p unsupported QP type %d\n",
		       hermon, qp->type );
		return -ENOTSUP;
	}
}

/**
 * Free queue pair number
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 */
static void hermon_free_qpn ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	int qpn_offset;

	qpn_offset = ( ( qp->qpn & ~HERMON_QPN_RANDOM_MASK )
		       - hermon->qpn_base );
	if ( qpn_offset >= 0 )
		hermon_bitmask_free ( hermon->qp_inuse, qpn_offset, 1 );
}

/**
 * Calculate transmission rate
 *
 * @v av		Address vector
 * @ret hermon_rate	Hermon rate
 */
static unsigned int hermon_rate ( struct ib_address_vector *av ) {
	return ( ( ( av->rate >= IB_RATE_2_5 ) && ( av->rate <= IB_RATE_120 ) )
		 ? ( av->rate + 5 ) : 0 );
}

/**
 * Calculate schedule queue
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @ret sched_queue	Schedule queue
 */
static unsigned int hermon_sched_queue ( struct ib_device *ibdev,
					 struct ib_queue_pair *qp ) {
	return ( ( ( qp->type == IB_QPT_SMI ) ?
		   HERMON_SCHED_QP0 : HERMON_SCHED_DEFAULT ) |
		 ( ( ibdev->port - 1 ) << 6 ) );
}

/** Queue pair transport service type map */
static uint8_t hermon_qp_st[] = {
	[IB_QPT_SMI] = HERMON_ST_MLX,
	[IB_QPT_GSI] = HERMON_ST_MLX,
	[IB_QPT_UD] = HERMON_ST_UD,
	[IB_QPT_RC] = HERMON_ST_RC,
};

/**
 * Dump queue pair context (for debugging only)
 *
 * @v hermon		Hermon device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static inline int hermon_dump_qpctx ( struct hermon *hermon,
				      struct ib_queue_pair *qp ) {
	struct hermonprm_qp_ee_state_transitions qpctx;
	int rc;

	memset ( &qpctx, 0, sizeof ( qpctx ) );
	if ( ( rc = hermon_cmd_query_qp ( hermon, qp->qpn, &qpctx ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p QUERY_QP failed: %s\n",
		       hermon, strerror ( rc ) );
		return rc;
	}
	DBGC ( hermon, "Hermon %p QPN %lx context:\n", hermon, qp->qpn );
	DBGC_HDA ( hermon, 0, &qpctx.u.dwords[2],
		   ( sizeof ( qpctx ) - 8 ) );

	return 0;
}

/**
 * Create queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int hermon_create_qp ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_queue_pair *hermon_qp;
	struct hermonprm_qp_ee_state_transitions qpctx;
	int rc;

	/* Calculate queue pair number */
	if ( ( rc = hermon_alloc_qpn ( ibdev, qp ) ) != 0 )
		goto err_alloc_qpn;

	/* Allocate control structures */
	hermon_qp = zalloc ( sizeof ( *hermon_qp ) );
	if ( ! hermon_qp ) {
		rc = -ENOMEM;
		goto err_hermon_qp;
	}

	/* Calculate doorbell address */
	hermon_qp->send.doorbell =
		( hermon->uar + HERMON_UAR_NON_EQ_PAGE * HERMON_PAGE_SIZE +
		  HERMON_DB_POST_SND_OFFSET );

	/* Allocate work queue buffer */
	hermon_qp->send.num_wqes = ( qp->send.num_wqes /* headroom */ + 1 +
				( 2048 / sizeof ( hermon_qp->send.wqe[0] ) ) );
	hermon_qp->send.num_wqes =
		( 1 << fls ( hermon_qp->send.num_wqes - 1 ) ); /* round up */
	hermon_qp->send.wqe_size = ( hermon_qp->send.num_wqes *
				     sizeof ( hermon_qp->send.wqe[0] ) );
	hermon_qp->recv.wqe_size = ( qp->recv.num_wqes *
				     sizeof ( hermon_qp->recv.wqe[0] ) );
	hermon_qp->wqe_size = ( hermon_qp->send.wqe_size +
				hermon_qp->recv.wqe_size );
	hermon_qp->wqe = malloc_dma ( hermon_qp->wqe_size,
				      sizeof ( hermon_qp->send.wqe[0] ) );
	if ( ! hermon_qp->wqe ) {
		rc = -ENOMEM;
		goto err_alloc_wqe;
	}
	hermon_qp->send.wqe = hermon_qp->wqe;
	memset ( hermon_qp->send.wqe, 0xff, hermon_qp->send.wqe_size );
	hermon_qp->recv.wqe = ( hermon_qp->wqe + hermon_qp->send.wqe_size );
	memset ( hermon_qp->recv.wqe, 0, hermon_qp->recv.wqe_size );

	/* Allocate MTT entries */
	if ( ( rc = hermon_alloc_mtt ( hermon, hermon_qp->wqe,
				       hermon_qp->wqe_size,
				       &hermon_qp->mtt ) ) != 0 ) {
		goto err_alloc_mtt;
	}

	/* Transition queue to INIT state */
	memset ( &qpctx, 0, sizeof ( qpctx ) );
	MLX_FILL_2 ( &qpctx, 2,
		     qpc_eec_data.pm_state, HERMON_PM_STATE_MIGRATED,
		     qpc_eec_data.st, hermon_qp_st[qp->type] );
	MLX_FILL_1 ( &qpctx, 3, qpc_eec_data.pd, HERMON_GLOBAL_PD );
	MLX_FILL_4 ( &qpctx, 4,
		     qpc_eec_data.log_rq_size, fls ( qp->recv.num_wqes - 1 ),
		     qpc_eec_data.log_rq_stride,
		     ( fls ( sizeof ( hermon_qp->recv.wqe[0] ) - 1 ) - 4 ),
		     qpc_eec_data.log_sq_size,
		     fls ( hermon_qp->send.num_wqes - 1 ),
		     qpc_eec_data.log_sq_stride,
		     ( fls ( sizeof ( hermon_qp->send.wqe[0] ) - 1 ) - 4 ) );
	MLX_FILL_1 ( &qpctx, 5,
		     qpc_eec_data.usr_page, HERMON_UAR_NON_EQ_PAGE );
	MLX_FILL_1 ( &qpctx, 33, qpc_eec_data.cqn_snd, qp->send.cq->cqn );
	MLX_FILL_4 ( &qpctx, 38,
		     qpc_eec_data.rre, 1,
		     qpc_eec_data.rwe, 1,
		     qpc_eec_data.rae, 1,
		     qpc_eec_data.page_offset,
		     ( hermon_qp->mtt.page_offset >> 6 ) );
	MLX_FILL_1 ( &qpctx, 41, qpc_eec_data.cqn_rcv, qp->recv.cq->cqn );
	MLX_FILL_1 ( &qpctx, 43, qpc_eec_data.db_record_addr_l,
		     ( virt_to_phys ( &hermon_qp->recv.doorbell ) >> 2 ) );
	MLX_FILL_1 ( &qpctx, 53, qpc_eec_data.mtt_base_addr_l,
		     ( hermon_qp->mtt.mtt_base_addr >> 3 ) );
	if ( ( rc = hermon_cmd_rst2init_qp ( hermon, qp->qpn,
					     &qpctx ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p RST2INIT_QP failed: %s\n",
		       hermon, strerror ( rc ) );
		goto err_rst2init_qp;
	}
	hermon_qp->state = HERMON_QP_ST_INIT;

	DBGC ( hermon, "Hermon %p QPN %#lx send ring at [%p,%p)\n",
	       hermon, qp->qpn, hermon_qp->send.wqe,
	       ( ((void *)hermon_qp->send.wqe ) + hermon_qp->send.wqe_size ) );
	DBGC ( hermon, "Hermon %p QPN %#lx receive ring at [%p,%p)\n",
	       hermon, qp->qpn, hermon_qp->recv.wqe,
	       ( ((void *)hermon_qp->recv.wqe ) + hermon_qp->recv.wqe_size ) );
	ib_qp_set_drvdata ( qp, hermon_qp );
	return 0;

	hermon_cmd_2rst_qp ( hermon, qp->qpn );
 err_rst2init_qp:
	hermon_free_mtt ( hermon, &hermon_qp->mtt );
 err_alloc_mtt:
	free_dma ( hermon_qp->wqe, hermon_qp->wqe_size );
 err_alloc_wqe:
	free ( hermon_qp );
 err_hermon_qp:
	hermon_free_qpn ( ibdev, qp );
 err_alloc_qpn:
	return rc;
}

/**
 * Modify queue pair
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @ret rc		Return status code
 */
static int hermon_modify_qp ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_queue_pair *hermon_qp = ib_qp_get_drvdata ( qp );
	struct hermonprm_qp_ee_state_transitions qpctx;
	int rc;

	/* Transition queue to RTR state, if applicable */
	if ( hermon_qp->state < HERMON_QP_ST_RTR ) {
		memset ( &qpctx, 0, sizeof ( qpctx ) );
		MLX_FILL_2 ( &qpctx, 4,
			     qpc_eec_data.mtu, HERMON_MTU_2048,
			     qpc_eec_data.msg_max, 31 );
		MLX_FILL_1 ( &qpctx, 7,
			     qpc_eec_data.remote_qpn_een, qp->av.qpn );
		MLX_FILL_1 ( &qpctx, 9,
			     qpc_eec_data.primary_address_path.rlid,
			     qp->av.lid );
		MLX_FILL_1 ( &qpctx, 10,
			     qpc_eec_data.primary_address_path.max_stat_rate,
			     hermon_rate ( &qp->av ) );
		memcpy ( &qpctx.u.dwords[12], &qp->av.gid,
			 sizeof ( qp->av.gid ) );
		MLX_FILL_1 ( &qpctx, 16,
			     qpc_eec_data.primary_address_path.sched_queue,
			     hermon_sched_queue ( ibdev, qp ) );
		MLX_FILL_1 ( &qpctx, 39,
			     qpc_eec_data.next_rcv_psn, qp->recv.psn );
		if ( ( rc = hermon_cmd_init2rtr_qp ( hermon, qp->qpn,
						     &qpctx ) ) != 0 ) {
			DBGC ( hermon, "Hermon %p INIT2RTR_QP failed: %s\n",
			       hermon, strerror ( rc ) );
			return rc;
		}
		hermon_qp->state = HERMON_QP_ST_RTR;
	}

	/* Transition queue to RTS state */
	if ( hermon_qp->state < HERMON_QP_ST_RTS ) {
		memset ( &qpctx, 0, sizeof ( qpctx ) );
		MLX_FILL_1 ( &qpctx, 10,
			     qpc_eec_data.primary_address_path.ack_timeout,
			     14 /* 4.096us * 2^(14) = 67ms */ );
		MLX_FILL_2 ( &qpctx, 30,
			     qpc_eec_data.retry_count, HERMON_RETRY_MAX,
			     qpc_eec_data.rnr_retry, HERMON_RETRY_MAX );
		MLX_FILL_1 ( &qpctx, 32,
			     qpc_eec_data.next_send_psn, qp->send.psn );
		if ( ( rc = hermon_cmd_rtr2rts_qp ( hermon, qp->qpn,
						    &qpctx ) ) != 0 ) {
			DBGC ( hermon, "Hermon %p RTR2RTS_QP failed: %s\n",
			       hermon, strerror ( rc ) );
			return rc;
		}
		hermon_qp->state = HERMON_QP_ST_RTS;
	}

	/* Update parameters in RTS state */
	memset ( &qpctx, 0, sizeof ( qpctx ) );
	MLX_FILL_1 ( &qpctx, 0, opt_param_mask, HERMON_QP_OPT_PARAM_QKEY );
	MLX_FILL_1 ( &qpctx, 44, qpc_eec_data.q_key, qp->qkey );
	if ( ( rc = hermon_cmd_rts2rts_qp ( hermon, qp->qpn, &qpctx ) ) != 0 ){
		DBGC ( hermon, "Hermon %p RTS2RTS_QP failed: %s\n",
		       hermon, strerror ( rc ) );
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
static void hermon_destroy_qp ( struct ib_device *ibdev,
				struct ib_queue_pair *qp ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_queue_pair *hermon_qp = ib_qp_get_drvdata ( qp );
	int rc;

	/* Take ownership back from hardware */
	if ( ( rc = hermon_cmd_2rst_qp ( hermon, qp->qpn ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p FATAL 2RST_QP failed on QPN %#lx: "
		       "%s\n", hermon, qp->qpn, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}

	/* Free MTT entries */
	hermon_free_mtt ( hermon, &hermon_qp->mtt );

	/* Free memory */
	free_dma ( hermon_qp->wqe, hermon_qp->wqe_size );
	free ( hermon_qp );

	/* Mark queue number as free */
	hermon_free_qpn ( ibdev, qp );

	ib_qp_set_drvdata ( qp, NULL );
}

/***************************************************************************
 *
 * Work request operations
 *
 ***************************************************************************
 */

/**
 * Construct UD send work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @v wqe		Send work queue entry
 * @ret opcode		Control opcode
 */
static unsigned int
hermon_fill_ud_send_wqe ( struct ib_device *ibdev,
			  struct ib_queue_pair *qp __unused,
			  struct ib_address_vector *av,
			  struct io_buffer *iobuf,
			  union hermon_send_wqe *wqe ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );

	MLX_FILL_1 ( &wqe->ud.ctrl, 1, ds,
		     ( ( offsetof ( typeof ( wqe->ud ), data[1] ) / 16 ) ) );
	MLX_FILL_1 ( &wqe->ud.ctrl, 2, c, 0x03 /* generate completion */ );
	MLX_FILL_2 ( &wqe->ud.ud, 0,
		     ud_address_vector.pd, HERMON_GLOBAL_PD,
		     ud_address_vector.port_number, ibdev->port );
	MLX_FILL_2 ( &wqe->ud.ud, 1,
		     ud_address_vector.rlid, av->lid,
		     ud_address_vector.g, av->gid_present );
	MLX_FILL_1 ( &wqe->ud.ud, 2,
		     ud_address_vector.max_stat_rate, hermon_rate ( av ) );
	MLX_FILL_1 ( &wqe->ud.ud, 3, ud_address_vector.sl, av->sl );
	memcpy ( &wqe->ud.ud.u.dwords[4], &av->gid, sizeof ( av->gid ) );
	MLX_FILL_1 ( &wqe->ud.ud, 8, destination_qp, av->qpn );
	MLX_FILL_1 ( &wqe->ud.ud, 9, q_key, av->qkey );
	MLX_FILL_1 ( &wqe->ud.data[0], 0, byte_count, iob_len ( iobuf ) );
	MLX_FILL_1 ( &wqe->ud.data[0], 1, l_key, hermon->lkey );
	MLX_FILL_1 ( &wqe->ud.data[0], 3,
		     local_address_l, virt_to_bus ( iobuf->data ) );
	return HERMON_OPCODE_SEND;
}

/**
 * Construct MLX send work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @v wqe		Send work queue entry
 * @ret opcode		Control opcode
 */
static unsigned int
hermon_fill_mlx_send_wqe ( struct ib_device *ibdev,
			   struct ib_queue_pair *qp,
			   struct ib_address_vector *av,
			   struct io_buffer *iobuf,
			   union hermon_send_wqe *wqe ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct io_buffer headers;

	/* Construct IB headers */
	iob_populate ( &headers, &wqe->mlx.headers, 0,
		       sizeof ( wqe->mlx.headers ) );
	iob_reserve ( &headers, sizeof ( wqe->mlx.headers ) );
	ib_push ( ibdev, &headers, qp, iob_len ( iobuf ), av );

	/* Fill work queue entry */
	MLX_FILL_1 ( &wqe->mlx.ctrl, 1, ds,
		     ( ( offsetof ( typeof ( wqe->mlx ), data[2] ) / 16 ) ) );
	MLX_FILL_5 ( &wqe->mlx.ctrl, 2,
		     c, 0x03 /* generate completion */,
		     icrc, 0 /* generate ICRC */,
		     max_statrate, hermon_rate ( av ),
		     slr, 0,
		     v15, ( ( qp->ext_qpn == IB_QPN_SMI ) ? 1 : 0 ) );
	MLX_FILL_1 ( &wqe->mlx.ctrl, 3, rlid, av->lid );
	MLX_FILL_1 ( &wqe->mlx.data[0], 0,
		     byte_count, iob_len ( &headers ) );
	MLX_FILL_1 ( &wqe->mlx.data[0], 1, l_key, hermon->lkey );
	MLX_FILL_1 ( &wqe->mlx.data[0], 3,
		     local_address_l, virt_to_bus ( headers.data ) );
	MLX_FILL_1 ( &wqe->mlx.data[1], 0,
		     byte_count, ( iob_len ( iobuf ) + 4 /* ICRC */ ) );
	MLX_FILL_1 ( &wqe->mlx.data[1], 1, l_key, hermon->lkey );
	MLX_FILL_1 ( &wqe->mlx.data[1], 3,
		     local_address_l, virt_to_bus ( iobuf->data ) );
	return HERMON_OPCODE_SEND;
}

/**
 * Construct RC send work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @v wqe		Send work queue entry
 * @ret opcode		Control opcode
 */
static unsigned int
hermon_fill_rc_send_wqe ( struct ib_device *ibdev,
			  struct ib_queue_pair *qp __unused,
			  struct ib_address_vector *av __unused,
			  struct io_buffer *iobuf,
			  union hermon_send_wqe *wqe ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );

	MLX_FILL_1 ( &wqe->rc.ctrl, 1, ds,
		     ( ( offsetof ( typeof ( wqe->rc ), data[1] ) / 16 ) ) );
	MLX_FILL_1 ( &wqe->rc.ctrl, 2, c, 0x03 /* generate completion */ );
	MLX_FILL_1 ( &wqe->rc.data[0], 0, byte_count, iob_len ( iobuf ) );
	MLX_FILL_1 ( &wqe->rc.data[0], 1, l_key, hermon->lkey );
	MLX_FILL_1 ( &wqe->rc.data[0], 3,
		     local_address_l, virt_to_bus ( iobuf->data ) );
	return HERMON_OPCODE_SEND;
}

/** Work queue entry constructors */
static unsigned int
( * hermon_fill_send_wqe[] ) ( struct ib_device *ibdev,
			       struct ib_queue_pair *qp,
			       struct ib_address_vector *av,
			       struct io_buffer *iobuf,
			       union hermon_send_wqe *wqe ) = {
	[IB_QPT_SMI] = hermon_fill_mlx_send_wqe,
	[IB_QPT_GSI] = hermon_fill_mlx_send_wqe,
	[IB_QPT_UD] = hermon_fill_ud_send_wqe,
	[IB_QPT_RC] = hermon_fill_rc_send_wqe,
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
static int hermon_post_send ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp,
			      struct ib_address_vector *av,
			      struct io_buffer *iobuf ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_queue_pair *hermon_qp = ib_qp_get_drvdata ( qp );
	struct ib_work_queue *wq = &qp->send;
	struct hermon_send_work_queue *hermon_send_wq = &hermon_qp->send;
	union hermon_send_wqe *wqe;
	union hermonprm_doorbell_register db_reg;
	unsigned int wqe_idx_mask;
	unsigned int opcode;

	/* Allocate work queue entry */
	wqe_idx_mask = ( wq->num_wqes - 1 );
	if ( wq->iobufs[wq->next_idx & wqe_idx_mask] ) {
		DBGC ( hermon, "Hermon %p send queue full", hermon );
		return -ENOBUFS;
	}
	wq->iobufs[wq->next_idx & wqe_idx_mask] = iobuf;
	wqe = &hermon_send_wq->wqe[ wq->next_idx &
				    ( hermon_send_wq->num_wqes - 1 ) ];

	/* Construct work queue entry */
	memset ( ( ( ( void * ) wqe ) + 4 /* avoid ctrl.owner */ ), 0,
		   ( sizeof ( *wqe ) - 4 ) );
	assert ( qp->type < ( sizeof ( hermon_fill_send_wqe ) /
			      sizeof ( hermon_fill_send_wqe[0] ) ) );
	assert ( hermon_fill_send_wqe[qp->type] != NULL );
	opcode = hermon_fill_send_wqe[qp->type] ( ibdev, qp, av, iobuf, wqe );
	barrier();
	MLX_FILL_2 ( &wqe->ctrl, 0,
		     opcode, opcode,
		     owner,
		     ( ( wq->next_idx & hermon_send_wq->num_wqes ) ? 1 : 0 ) );
	DBGCP ( hermon, "Hermon %p posting send WQE:\n", hermon );
	DBGCP_HD ( hermon, wqe, sizeof ( *wqe ) );
	barrier();

	/* Ring doorbell register */
	MLX_FILL_1 ( &db_reg.send, 0, qn, qp->qpn );
	DBGCP ( hermon, "Ringing doorbell %08lx with %08x\n",
		virt_to_phys ( hermon_send_wq->doorbell ), db_reg.dword[0] );
	writel ( db_reg.dword[0], ( hermon_send_wq->doorbell ) );

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
static int hermon_post_recv ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp,
			      struct io_buffer *iobuf ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_queue_pair *hermon_qp = ib_qp_get_drvdata ( qp );
	struct ib_work_queue *wq = &qp->recv;
	struct hermon_recv_work_queue *hermon_recv_wq = &hermon_qp->recv;
	struct hermonprm_recv_wqe *wqe;
	unsigned int wqe_idx_mask;

	/* Allocate work queue entry */
	wqe_idx_mask = ( wq->num_wqes - 1 );
	if ( wq->iobufs[wq->next_idx & wqe_idx_mask] ) {
		DBGC ( hermon, "Hermon %p receive queue full", hermon );
		return -ENOBUFS;
	}
	wq->iobufs[wq->next_idx & wqe_idx_mask] = iobuf;
	wqe = &hermon_recv_wq->wqe[wq->next_idx & wqe_idx_mask].recv;

	/* Construct work queue entry */
	MLX_FILL_1 ( &wqe->data[0], 0, byte_count, iob_tailroom ( iobuf ) );
	MLX_FILL_1 ( &wqe->data[0], 1, l_key, hermon->lkey );
	MLX_FILL_1 ( &wqe->data[0], 3,
		     local_address_l, virt_to_bus ( iobuf->data ) );

	/* Update work queue's index */
	wq->next_idx++;

	/* Update doorbell record */
	barrier();
	MLX_FILL_1 ( &hermon_recv_wq->doorbell, 0, receive_wqe_counter,
		     ( wq->next_idx & 0xffff ) );

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
static int hermon_complete ( struct ib_device *ibdev,
			     struct ib_completion_queue *cq,
			     union hermonprm_completion_entry *cqe ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct ib_work_queue *wq;
	struct ib_queue_pair *qp;
	struct hermon_queue_pair *hermon_qp;
	struct io_buffer *iobuf;
	struct ib_address_vector recv_av;
	struct ib_global_route_header *grh;
	struct ib_address_vector *av;
	unsigned int opcode;
	unsigned long qpn;
	int is_send;
	unsigned int wqe_idx;
	size_t len;
	int rc = 0;

	/* Parse completion */
	qpn = MLX_GET ( &cqe->normal, qpn );
	is_send = MLX_GET ( &cqe->normal, s_r );
	opcode = MLX_GET ( &cqe->normal, opcode );
	if ( opcode >= HERMON_OPCODE_RECV_ERROR ) {
		/* "s" field is not valid for error opcodes */
		is_send = ( opcode == HERMON_OPCODE_SEND_ERROR );
		DBGC ( hermon, "Hermon %p CQN %lx syndrome %x vendor %x\n",
		       hermon, cq->cqn, MLX_GET ( &cqe->error, syndrome ),
		       MLX_GET ( &cqe->error, vendor_error_syndrome ) );
		rc = -EIO;
		/* Don't return immediately; propagate error to completer */
	}

	/* Identify work queue */
	wq = ib_find_wq ( cq, qpn, is_send );
	if ( ! wq ) {
		DBGC ( hermon, "Hermon %p CQN %lx unknown %s QPN %lx\n",
		       hermon, cq->cqn, ( is_send ? "send" : "recv" ), qpn );
		return -EIO;
	}
	qp = wq->qp;
	hermon_qp = ib_qp_get_drvdata ( qp );

	/* Identify I/O buffer */
	wqe_idx = ( MLX_GET ( &cqe->normal, wqe_counter ) &
		    ( wq->num_wqes - 1 ) );
	iobuf = wq->iobufs[wqe_idx];
	if ( ! iobuf ) {
		DBGC ( hermon, "Hermon %p CQN %lx QPN %lx empty WQE %x\n",
		       hermon, cq->cqn, qp->qpn, wqe_idx );
		return -EIO;
	}
	wq->iobufs[wqe_idx] = NULL;

	if ( is_send ) {
		/* Hand off to completion handler */
		ib_complete_send ( ibdev, qp, iobuf, rc );
	} else {
		/* Set received length */
		len = MLX_GET ( &cqe->normal, byte_cnt );
		assert ( len <= iob_tailroom ( iobuf ) );
		iob_put ( iobuf, len );
		switch ( qp->type ) {
		case IB_QPT_SMI:
		case IB_QPT_GSI:
		case IB_QPT_UD:
			assert ( iob_len ( iobuf ) >= sizeof ( *grh ) );
			grh = iobuf->data;
			iob_pull ( iobuf, sizeof ( *grh ) );
			/* Construct address vector */
			av = &recv_av;
			memset ( av, 0, sizeof ( *av ) );
			av->qpn = MLX_GET ( &cqe->normal, srq_rqpn );
			av->lid = MLX_GET ( &cqe->normal, slid_smac47_32 );
			av->sl = MLX_GET ( &cqe->normal, sl );
			av->gid_present = MLX_GET ( &cqe->normal, g );
			memcpy ( &av->gid, &grh->sgid, sizeof ( av->gid ) );
			break;
		case IB_QPT_RC:
			av = &qp->av;
			break;
		default:
			assert ( 0 );
			return -EINVAL;
		}
		/* Hand off to completion handler */
		ib_complete_recv ( ibdev, qp, av, iobuf, rc );
	}

	return rc;
}

/**
 * Poll completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 */
static void hermon_poll_cq ( struct ib_device *ibdev,
			     struct ib_completion_queue *cq ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_completion_queue *hermon_cq = ib_cq_get_drvdata ( cq );
	union hermonprm_completion_entry *cqe;
	unsigned int cqe_idx_mask;
	int rc;

	while ( 1 ) {
		/* Look for completion entry */
		cqe_idx_mask = ( cq->num_cqes - 1 );
		cqe = &hermon_cq->cqe[cq->next_idx & cqe_idx_mask];
		if ( MLX_GET ( &cqe->normal, owner ) ^
		     ( ( cq->next_idx & cq->num_cqes ) ? 1 : 0 ) ) {
			/* Entry still owned by hardware; end of poll */
			break;
		}
		DBGCP ( hermon, "Hermon %p completion:\n", hermon );
		DBGCP_HD ( hermon, cqe, sizeof ( *cqe ) );

		/* Handle completion */
		if ( ( rc = hermon_complete ( ibdev, cq, cqe ) ) != 0 ) {
			DBGC ( hermon, "Hermon %p failed to complete: %s\n",
			       hermon, strerror ( rc ) );
			DBGC_HD ( hermon, cqe, sizeof ( *cqe ) );
		}

		/* Update completion queue's index */
		cq->next_idx++;

		/* Update doorbell record */
		MLX_FILL_1 ( &hermon_cq->doorbell, 0, update_ci,
			     ( cq->next_idx & 0x00ffffffUL ) );
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
 * @v hermon		Hermon device
 * @ret rc		Return status code
 */
static int hermon_create_eq ( struct hermon *hermon ) {
	struct hermon_event_queue *hermon_eq = &hermon->eq;
	struct hermonprm_eqc eqctx;
	struct hermonprm_event_mask mask;
	unsigned int i;
	int rc;

	/* Select event queue number */
	hermon_eq->eqn = ( 4 * hermon->cap.reserved_uars );
	if ( hermon_eq->eqn < hermon->cap.reserved_eqs )
		hermon_eq->eqn = hermon->cap.reserved_eqs;

	/* Calculate doorbell address */
	hermon_eq->doorbell =
		( hermon->uar + HERMON_DB_EQ_OFFSET ( hermon_eq->eqn ) );

	/* Allocate event queue itself */
	hermon_eq->eqe_size =
		( HERMON_NUM_EQES * sizeof ( hermon_eq->eqe[0] ) );
	hermon_eq->eqe = malloc_dma ( hermon_eq->eqe_size,
				      sizeof ( hermon_eq->eqe[0] ) );
	if ( ! hermon_eq->eqe ) {
		rc = -ENOMEM;
		goto err_eqe;
	}
	memset ( hermon_eq->eqe, 0, hermon_eq->eqe_size );
	for ( i = 0 ; i < HERMON_NUM_EQES ; i++ ) {
		MLX_FILL_1 ( &hermon_eq->eqe[i].generic, 7, owner, 1 );
	}
	barrier();

	/* Allocate MTT entries */
	if ( ( rc = hermon_alloc_mtt ( hermon, hermon_eq->eqe,
				       hermon_eq->eqe_size,
				       &hermon_eq->mtt ) ) != 0 )
		goto err_alloc_mtt;

	/* Hand queue over to hardware */
	memset ( &eqctx, 0, sizeof ( eqctx ) );
	MLX_FILL_1 ( &eqctx, 0, st, 0xa /* "Fired" */ );
	MLX_FILL_1 ( &eqctx, 2,
		     page_offset, ( hermon_eq->mtt.page_offset >> 5 ) );
	MLX_FILL_1 ( &eqctx, 3, log_eq_size, fls ( HERMON_NUM_EQES - 1 ) );
	MLX_FILL_1 ( &eqctx, 7, mtt_base_addr_l,
		     ( hermon_eq->mtt.mtt_base_addr >> 3 ) );
	if ( ( rc = hermon_cmd_sw2hw_eq ( hermon, hermon_eq->eqn,
					  &eqctx ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p SW2HW_EQ failed: %s\n",
		       hermon, strerror ( rc ) );
		goto err_sw2hw_eq;
	}

	/* Map events to this event queue */
	memset ( &mask, 0, sizeof ( mask ) );
	MLX_FILL_1 ( &mask, 1, port_state_change, 1 );
	if ( ( rc = hermon_cmd_map_eq ( hermon,
					( HERMON_MAP_EQ | hermon_eq->eqn ),
					&mask ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p MAP_EQ failed: %s\n",
		       hermon, strerror ( rc )  );
		goto err_map_eq;
	}

	DBGC ( hermon, "Hermon %p EQN %#lx ring at [%p,%p])\n",
	       hermon, hermon_eq->eqn, hermon_eq->eqe,
	       ( ( ( void * ) hermon_eq->eqe ) + hermon_eq->eqe_size ) );
	return 0;

 err_map_eq:
	hermon_cmd_hw2sw_eq ( hermon, hermon_eq->eqn, &eqctx );
 err_sw2hw_eq:
	hermon_free_mtt ( hermon, &hermon_eq->mtt );
 err_alloc_mtt:
	free_dma ( hermon_eq->eqe, hermon_eq->eqe_size );
 err_eqe:
	memset ( hermon_eq, 0, sizeof ( *hermon_eq ) );
	return rc;
}

/**
 * Destroy event queue
 *
 * @v hermon		Hermon device
 */
static void hermon_destroy_eq ( struct hermon *hermon ) {
	struct hermon_event_queue *hermon_eq = &hermon->eq;
	struct hermonprm_eqc eqctx;
	struct hermonprm_event_mask mask;
	int rc;

	/* Unmap events from event queue */
	memset ( &mask, 0, sizeof ( mask ) );
	MLX_FILL_1 ( &mask, 1, port_state_change, 1 );
	if ( ( rc = hermon_cmd_map_eq ( hermon,
					( HERMON_UNMAP_EQ | hermon_eq->eqn ),
					&mask ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p FATAL MAP_EQ failed to unmap: %s\n",
		       hermon, strerror ( rc ) );
		/* Continue; HCA may die but system should survive */
	}

	/* Take ownership back from hardware */
	if ( ( rc = hermon_cmd_hw2sw_eq ( hermon, hermon_eq->eqn,
					  &eqctx ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p FATAL HW2SW_EQ failed: %s\n",
		       hermon, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}

	/* Free MTT entries */
	hermon_free_mtt ( hermon, &hermon_eq->mtt );

	/* Free memory */
	free_dma ( hermon_eq->eqe, hermon_eq->eqe_size );
	memset ( hermon_eq, 0, sizeof ( *hermon_eq ) );
}

/**
 * Handle port state event
 *
 * @v hermon		Hermon device
 * @v eqe		Port state change event queue entry
 */
static void hermon_event_port_state_change ( struct hermon *hermon,
					     union hermonprm_event_entry *eqe){
	unsigned int port;
	int link_up;

	/* Get port and link status */
	port = ( MLX_GET ( &eqe->port_state_change, data.p ) - 1 );
	link_up = ( MLX_GET ( &eqe->generic, event_sub_type ) & 0x04 );
	DBGC ( hermon, "Hermon %p port %d link %s\n", hermon, ( port + 1 ),
	       ( link_up ? "up" : "down" ) );

	/* Sanity check */
	if ( port >= hermon->cap.num_ports ) {
		DBGC ( hermon, "Hermon %p port %d does not exist!\n",
		       hermon, ( port + 1 ) );
		return;
	}

	/* Update MAD parameters */
	ib_smc_update ( hermon->ibdev[port], hermon_mad );

	/* Notify Infiniband core of link state change */
	ib_link_state_changed ( hermon->ibdev[port] );
}

/**
 * Poll event queue
 *
 * @v ibdev		Infiniband device
 */
static void hermon_poll_eq ( struct ib_device *ibdev ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermon_event_queue *hermon_eq = &hermon->eq;
	union hermonprm_event_entry *eqe;
	union hermonprm_doorbell_register db_reg;
	unsigned int eqe_idx_mask;
	unsigned int event_type;

	while ( 1 ) {
		/* Look for event entry */
		eqe_idx_mask = ( HERMON_NUM_EQES - 1 );
		eqe = &hermon_eq->eqe[hermon_eq->next_idx & eqe_idx_mask];
		if ( MLX_GET ( &eqe->generic, owner ) ^
		     ( ( hermon_eq->next_idx & HERMON_NUM_EQES ) ? 1 : 0 ) ) {
			/* Entry still owned by hardware; end of poll */
			break;
		}
		DBGCP ( hermon, "Hermon %p event:\n", hermon );
		DBGCP_HD ( hermon, eqe, sizeof ( *eqe ) );

		/* Handle event */
		event_type = MLX_GET ( &eqe->generic, event_type );
		switch ( event_type ) {
		case HERMON_EV_PORT_STATE_CHANGE:
			hermon_event_port_state_change ( hermon, eqe );
			break;
		default:
			DBGC ( hermon, "Hermon %p unrecognised event type "
			       "%#x:\n", hermon, event_type );
			DBGC_HD ( hermon, eqe, sizeof ( *eqe ) );
			break;
		}

		/* Update event queue's index */
		hermon_eq->next_idx++;

		/* Ring doorbell */
		MLX_FILL_1 ( &db_reg.event, 0,
			     ci, ( hermon_eq->next_idx & 0x00ffffffUL ) );
		DBGCP ( hermon, "Ringing doorbell %08lx with %08x\n",
			virt_to_phys ( hermon_eq->doorbell ),
			db_reg.dword[0] );
		writel ( db_reg.dword[0], hermon_eq->doorbell );
	}
}

/***************************************************************************
 *
 * Infiniband link-layer operations
 *
 ***************************************************************************
 */

/**
 * Sense port type
 *
 * @v ibdev		Infiniband device
 * @ret port_type	Port type, or negative error
 */
static int hermon_sense_port_type ( struct ib_device *ibdev ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermonprm_sense_port sense_port;
	int port_type;
	int rc;

	/* If DPDP is not supported, always assume Infiniband */
	if ( ! hermon->cap.dpdp )
		return HERMON_PORT_TYPE_IB;

	/* Sense the port type */
	if ( ( rc = hermon_cmd_sense_port ( hermon, ibdev->port,
					    &sense_port ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p port %d sense failed: %s\n",
		       hermon, ibdev->port, strerror ( rc ) );
		return rc;
	}
	port_type = MLX_GET ( &sense_port, port_type );

	DBGC ( hermon, "Hermon %p port %d type %d\n",
	       hermon, ibdev->port, port_type );
	return port_type;
}

/**
 * Initialise Infiniband link
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
static int hermon_open ( struct ib_device *ibdev ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermonprm_init_port init_port;
	int port_type;
	int rc;

	/* Check we are connected to an Infiniband network */
	if ( ( rc = port_type = hermon_sense_port_type ( ibdev ) ) < 0 )
		return rc;
	if ( port_type != HERMON_PORT_TYPE_IB ) {
		DBGC ( hermon, "Hermon %p port %d not connected to an "
		       "Infiniband network", hermon, ibdev->port );
		return -ENOTCONN;
        }

	/* Init Port */
	memset ( &init_port, 0, sizeof ( init_port ) );
	MLX_FILL_2 ( &init_port, 0,
		     port_width_cap, 3,
		     vl_cap, 1 );
	MLX_FILL_2 ( &init_port, 1,
		     mtu, HERMON_MTU_2048,
		     max_gid, 1 );
	MLX_FILL_1 ( &init_port, 2, max_pkey, 64 );
	if ( ( rc = hermon_cmd_init_port ( hermon, ibdev->port,
					   &init_port ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not intialise port: %s\n",
		       hermon, strerror ( rc ) );
		return rc;
	}

	/* Update MAD parameters */
	ib_smc_update ( ibdev, hermon_mad );

	return 0;
}

/**
 * Close Infiniband link
 *
 * @v ibdev		Infiniband device
 */
static void hermon_close ( struct ib_device *ibdev ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	int rc;

	if ( ( rc = hermon_cmd_close_port ( hermon, ibdev->port ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not close port: %s\n",
		       hermon, strerror ( rc ) );
		/* Nothing we can do about this */
	}
}

/**
 * Inform embedded subnet management agent of a received MAD
 *
 * @v ibdev		Infiniband device
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int hermon_inform_sma ( struct ib_device *ibdev,
			       union ib_mad *mad ) {
	int rc;

	/* Send the MAD to the embedded SMA */
	if ( ( rc = hermon_mad ( ibdev, mad ) ) != 0 )
		return rc;

	/* Update parameters held in software */
	ib_smc_update ( ibdev, hermon_mad );

	return 0;
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
static int hermon_mcast_attach ( struct ib_device *ibdev,
				 struct ib_queue_pair *qp,
				 struct ib_gid *gid ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermonprm_mgm_hash hash;
	struct hermonprm_mcg_entry mcg;
	unsigned int index;
	int rc;

	/* Generate hash table index */
	if ( ( rc = hermon_cmd_mgid_hash ( hermon, gid, &hash ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not hash GID: %s\n",
		       hermon, strerror ( rc ) );
		return rc;
	}
	index = MLX_GET ( &hash, hash );

	/* Check for existing hash table entry */
	if ( ( rc = hermon_cmd_read_mcg ( hermon, index, &mcg ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not read MCG %#x: %s\n",
		       hermon, index, strerror ( rc ) );
		return rc;
	}
	if ( MLX_GET ( &mcg, hdr.members_count ) != 0 ) {
		/* FIXME: this implementation allows only a single QP
		 * per multicast group, and doesn't handle hash
		 * collisions.  Sufficient for IPoIB but may need to
		 * be extended in future.
		 */
		DBGC ( hermon, "Hermon %p MGID index %#x already in use\n",
		       hermon, index );
		return -EBUSY;
	}

	/* Update hash table entry */
	MLX_FILL_1 ( &mcg, 1, hdr.members_count, 1 );
	MLX_FILL_1 ( &mcg, 8, qp[0].qpn, qp->qpn );
	memcpy ( &mcg.u.dwords[4], gid, sizeof ( *gid ) );
	if ( ( rc = hermon_cmd_write_mcg ( hermon, index, &mcg ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not write MCG %#x: %s\n",
		       hermon, index, strerror ( rc ) );
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
static void hermon_mcast_detach ( struct ib_device *ibdev,
				  struct ib_queue_pair *qp __unused,
				  struct ib_gid *gid ) {
	struct hermon *hermon = ib_get_drvdata ( ibdev );
	struct hermonprm_mgm_hash hash;
	struct hermonprm_mcg_entry mcg;
	unsigned int index;
	int rc;

	/* Generate hash table index */
	if ( ( rc = hermon_cmd_mgid_hash ( hermon, gid, &hash ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not hash GID: %s\n",
		       hermon, strerror ( rc ) );
		return;
	}
	index = MLX_GET ( &hash, hash );

	/* Clear hash table entry */
	memset ( &mcg, 0, sizeof ( mcg ) );
	if ( ( rc = hermon_cmd_write_mcg ( hermon, index, &mcg ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not write MCG %#x: %s\n",
		       hermon, index, strerror ( rc ) );
		return;
	}
}

/** Hermon Infiniband operations */
static struct ib_device_operations hermon_ib_operations = {
	.create_cq	= hermon_create_cq,
	.destroy_cq	= hermon_destroy_cq,
	.create_qp	= hermon_create_qp,
	.modify_qp	= hermon_modify_qp,
	.destroy_qp	= hermon_destroy_qp,
	.post_send	= hermon_post_send,
	.post_recv	= hermon_post_recv,
	.poll_cq	= hermon_poll_cq,
	.poll_eq	= hermon_poll_eq,
	.open		= hermon_open,
	.close		= hermon_close,
	.mcast_attach	= hermon_mcast_attach,
	.mcast_detach	= hermon_mcast_detach,
	.set_port_info	= hermon_inform_sma,
	.set_pkey_table	= hermon_inform_sma,
};

/***************************************************************************
 *
 * Firmware control
 *
 ***************************************************************************
 */

/**
 * Map virtual to physical address for firmware usage
 *
 * @v hermon		Hermon device
 * @v map		Mapping function
 * @v va		Virtual address
 * @v pa		Physical address
 * @v len		Length of region
 * @ret rc		Return status code
 */
static int hermon_map_vpm ( struct hermon *hermon,
			    int ( *map ) ( struct hermon *hermon,
			    const struct hermonprm_virtual_physical_mapping* ),
			    uint64_t va, physaddr_t pa, size_t len ) {
	struct hermonprm_virtual_physical_mapping mapping;
	int rc;

	assert ( ( va & ( HERMON_PAGE_SIZE - 1 ) ) == 0 );
	assert ( ( pa & ( HERMON_PAGE_SIZE - 1 ) ) == 0 );
	assert ( ( len & ( HERMON_PAGE_SIZE - 1 ) ) == 0 );

	/* These mappings tend to generate huge volumes of
	 * uninteresting debug data, which basically makes it
	 * impossible to use debugging otherwise.
	 */
	DBG_DISABLE ( DBGLVL_LOG | DBGLVL_EXTRA );

	while ( len ) {
		memset ( &mapping, 0, sizeof ( mapping ) );
		MLX_FILL_1 ( &mapping, 0, va_h, ( va >> 32 ) );
		MLX_FILL_1 ( &mapping, 1, va_l, ( va >> 12 ) );
		MLX_FILL_2 ( &mapping, 3,
			     log2size, 0,
			     pa_l, ( pa >> 12 ) );
		if ( ( rc = map ( hermon, &mapping ) ) != 0 ) {
			DBG_ENABLE ( DBGLVL_LOG | DBGLVL_EXTRA );
			DBGC ( hermon, "Hermon %p could not map %llx => %lx: "
			       "%s\n", hermon, va, pa, strerror ( rc ) );
			return rc;
		}
		pa += HERMON_PAGE_SIZE;
		va += HERMON_PAGE_SIZE;
		len -= HERMON_PAGE_SIZE;
	}

	DBG_ENABLE ( DBGLVL_LOG | DBGLVL_EXTRA );
	return 0;
}

/**
 * Start firmware running
 *
 * @v hermon		Hermon device
 * @ret rc		Return status code
 */
static int hermon_start_firmware ( struct hermon *hermon ) {
	struct hermonprm_query_fw fw;
	unsigned int fw_pages;
	size_t fw_size;
	physaddr_t fw_base;
	int rc;

	/* Get firmware parameters */
	if ( ( rc = hermon_cmd_query_fw ( hermon, &fw ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not query firmware: %s\n",
		       hermon, strerror ( rc ) );
		goto err_query_fw;
	}
	DBGC ( hermon, "Hermon %p firmware version %d.%d.%d\n", hermon,
	       MLX_GET ( &fw, fw_rev_major ), MLX_GET ( &fw, fw_rev_minor ),
	       MLX_GET ( &fw, fw_rev_subminor ) );
	fw_pages = MLX_GET ( &fw, fw_pages );
	DBGC ( hermon, "Hermon %p requires %d pages (%d kB) for firmware\n",
	       hermon, fw_pages, ( fw_pages * ( HERMON_PAGE_SIZE / 1024 ) ) );

	/* Allocate firmware pages and map firmware area */
	fw_size = ( fw_pages * HERMON_PAGE_SIZE );
	hermon->firmware_area = umalloc ( fw_size );
	if ( ! hermon->firmware_area ) {
		rc = -ENOMEM;
		goto err_alloc_fa;
	}
	fw_base = user_to_phys ( hermon->firmware_area, 0 );
	DBGC ( hermon, "Hermon %p firmware area at physical [%lx,%lx)\n",
	       hermon, fw_base, ( fw_base + fw_size ) );
	if ( ( rc = hermon_map_vpm ( hermon, hermon_cmd_map_fa,
				     0, fw_base, fw_size ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not map firmware: %s\n",
		       hermon, strerror ( rc ) );
		goto err_map_fa;
	}

	/* Start firmware */
	if ( ( rc = hermon_cmd_run_fw ( hermon ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not run firmware: %s\n",
		       hermon, strerror ( rc ) );
		goto err_run_fw;
	}

	DBGC ( hermon, "Hermon %p firmware started\n", hermon );
	return 0;

 err_run_fw:
 err_map_fa:
	hermon_cmd_unmap_fa ( hermon );
	ufree ( hermon->firmware_area );
	hermon->firmware_area = UNULL;
 err_alloc_fa:
 err_query_fw:
	return rc;
}

/**
 * Stop firmware running
 *
 * @v hermon		Hermon device
 */
static void hermon_stop_firmware ( struct hermon *hermon ) {
	int rc;

	if ( ( rc = hermon_cmd_unmap_fa ( hermon ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p FATAL could not stop firmware: %s\n",
		       hermon, strerror ( rc ) );
		/* Leak memory and return; at least we avoid corruption */
		return;
	}
	ufree ( hermon->firmware_area );
	hermon->firmware_area = UNULL;
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
 * @v hermon		Hermon device
 * @ret rc		Return status code
 */
static int hermon_get_cap ( struct hermon *hermon ) {
	struct hermonprm_query_dev_cap dev_cap;
	int rc;

	if ( ( rc = hermon_cmd_query_dev_cap ( hermon, &dev_cap ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not get device limits: %s\n",
		       hermon, strerror ( rc ) );
		return rc;
	}

	hermon->cap.cmpt_entry_size = MLX_GET ( &dev_cap, c_mpt_entry_sz );
	hermon->cap.reserved_qps =
		( 1 << MLX_GET ( &dev_cap, log2_rsvd_qps ) );
	hermon->cap.qpc_entry_size = MLX_GET ( &dev_cap, qpc_entry_sz );
	hermon->cap.altc_entry_size = MLX_GET ( &dev_cap, altc_entry_sz );
	hermon->cap.auxc_entry_size = MLX_GET ( &dev_cap, aux_entry_sz );
	hermon->cap.reserved_srqs =
		( 1 << MLX_GET ( &dev_cap, log2_rsvd_srqs ) );
	hermon->cap.srqc_entry_size = MLX_GET ( &dev_cap, srq_entry_sz );
	hermon->cap.reserved_cqs =
		( 1 << MLX_GET ( &dev_cap, log2_rsvd_cqs ) );
	hermon->cap.cqc_entry_size = MLX_GET ( &dev_cap, cqc_entry_sz );
	hermon->cap.reserved_eqs = MLX_GET ( &dev_cap, num_rsvd_eqs );
	hermon->cap.eqc_entry_size = MLX_GET ( &dev_cap, eqc_entry_sz );
	hermon->cap.reserved_mtts =
		( 1 << MLX_GET ( &dev_cap, log2_rsvd_mtts ) );
	hermon->cap.mtt_entry_size = MLX_GET ( &dev_cap, mtt_entry_sz );
	hermon->cap.reserved_mrws =
		( 1 << MLX_GET ( &dev_cap, log2_rsvd_mrws ) );
	hermon->cap.dmpt_entry_size = MLX_GET ( &dev_cap, d_mpt_entry_sz );
	hermon->cap.reserved_uars = MLX_GET ( &dev_cap, num_rsvd_uars );
	hermon->cap.num_ports = MLX_GET ( &dev_cap, num_ports );
	hermon->cap.dpdp = MLX_GET ( &dev_cap, dpdp );

	/* Sanity check */
	if ( hermon->cap.num_ports > HERMON_MAX_PORTS ) {
		DBGC ( hermon, "Hermon %p has %d ports (only %d supported)\n",
		       hermon, hermon->cap.num_ports, HERMON_MAX_PORTS );
		hermon->cap.num_ports = HERMON_MAX_PORTS;
	}

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
	usage = ( ( usage + HERMON_PAGE_SIZE - 1 ) &
		  ~( HERMON_PAGE_SIZE - 1 ) );
	return usage;
}

/**
 * Allocate ICM
 *
 * @v hermon		Hermon device
 * @v init_hca		INIT_HCA structure to fill in
 * @ret rc		Return status code
 */
static int hermon_alloc_icm ( struct hermon *hermon,
			      struct hermonprm_init_hca *init_hca ) {
	struct hermonprm_scalar_parameter icm_size;
	struct hermonprm_scalar_parameter icm_aux_size;
	uint64_t icm_offset = 0;
	unsigned int log_num_qps, log_num_srqs, log_num_cqs, log_num_eqs;
	unsigned int log_num_mtts, log_num_mpts;
	size_t cmpt_max_len;
	size_t qp_cmpt_len, srq_cmpt_len, cq_cmpt_len, eq_cmpt_len;
	size_t icm_len, icm_aux_len;
	physaddr_t icm_phys;
	int i;
	int rc;

	/*
	 * Start by carving up the ICM virtual address space
	 *
	 */

	/* Calculate number of each object type within ICM */
	log_num_qps = fls ( hermon->cap.reserved_qps +
			    HERMON_RSVD_SPECIAL_QPS + HERMON_MAX_QPS - 1 );
	log_num_srqs = fls ( hermon->cap.reserved_srqs - 1 );
	log_num_cqs = fls ( hermon->cap.reserved_cqs + HERMON_MAX_CQS - 1 );
	log_num_eqs = fls ( hermon->cap.reserved_eqs + HERMON_MAX_EQS - 1 );
	log_num_mtts = fls ( hermon->cap.reserved_mtts + HERMON_MAX_MTTS - 1 );

	/* ICM starts with the cMPT tables, which are sparse */
	cmpt_max_len = ( HERMON_CMPT_MAX_ENTRIES *
			 ( ( uint64_t ) hermon->cap.cmpt_entry_size ) );
	qp_cmpt_len = icm_usage ( log_num_qps, hermon->cap.cmpt_entry_size );
	hermon->icm_map[HERMON_ICM_QP_CMPT].offset = icm_offset;
	hermon->icm_map[HERMON_ICM_QP_CMPT].len = qp_cmpt_len;
	icm_offset += cmpt_max_len;
	srq_cmpt_len = icm_usage ( log_num_srqs, hermon->cap.cmpt_entry_size );
	hermon->icm_map[HERMON_ICM_SRQ_CMPT].offset = icm_offset;
	hermon->icm_map[HERMON_ICM_SRQ_CMPT].len = srq_cmpt_len;
	icm_offset += cmpt_max_len;
	cq_cmpt_len = icm_usage ( log_num_cqs, hermon->cap.cmpt_entry_size );
	hermon->icm_map[HERMON_ICM_CQ_CMPT].offset = icm_offset;
	hermon->icm_map[HERMON_ICM_CQ_CMPT].len = cq_cmpt_len;
	icm_offset += cmpt_max_len;
	eq_cmpt_len = icm_usage ( log_num_eqs, hermon->cap.cmpt_entry_size );
	hermon->icm_map[HERMON_ICM_EQ_CMPT].offset = icm_offset;
	hermon->icm_map[HERMON_ICM_EQ_CMPT].len = eq_cmpt_len;
	icm_offset += cmpt_max_len;

	hermon->icm_map[HERMON_ICM_OTHER].offset = icm_offset;

	/* Queue pair contexts */
	MLX_FILL_1 ( init_hca, 12,
		     qpc_eec_cqc_eqc_rdb_parameters.qpc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_2 ( init_hca, 13,
		     qpc_eec_cqc_eqc_rdb_parameters.qpc_base_addr_l,
		     ( icm_offset >> 5 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_qp,
		     log_num_qps );
	DBGC ( hermon, "Hermon %p ICM QPC base = %llx\n", hermon, icm_offset );
	icm_offset += icm_usage ( log_num_qps, hermon->cap.qpc_entry_size );

	/* Extended alternate path contexts */
	MLX_FILL_1 ( init_hca, 24,
		     qpc_eec_cqc_eqc_rdb_parameters.altc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_1 ( init_hca, 25,
		     qpc_eec_cqc_eqc_rdb_parameters.altc_base_addr_l,
		     icm_offset );
	DBGC ( hermon, "Hermon %p ICM ALTC base = %llx\n", hermon, icm_offset);
	icm_offset += icm_usage ( log_num_qps,
				  hermon->cap.altc_entry_size );

	/* Extended auxiliary contexts */
	MLX_FILL_1 ( init_hca, 28,
		     qpc_eec_cqc_eqc_rdb_parameters.auxc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_1 ( init_hca, 29,
		     qpc_eec_cqc_eqc_rdb_parameters.auxc_base_addr_l,
		     icm_offset );
	DBGC ( hermon, "Hermon %p ICM AUXC base = %llx\n", hermon, icm_offset);
	icm_offset += icm_usage ( log_num_qps,
				  hermon->cap.auxc_entry_size );

	/* Shared receive queue contexts */
	MLX_FILL_1 ( init_hca, 18,
		     qpc_eec_cqc_eqc_rdb_parameters.srqc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_2 ( init_hca, 19,
		     qpc_eec_cqc_eqc_rdb_parameters.srqc_base_addr_l,
		     ( icm_offset >> 5 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_srq,
		     log_num_srqs );
	DBGC ( hermon, "Hermon %p ICM SRQC base = %llx\n", hermon, icm_offset);
	icm_offset += icm_usage ( log_num_srqs,
				  hermon->cap.srqc_entry_size );

	/* Completion queue contexts */
	MLX_FILL_1 ( init_hca, 20,
		     qpc_eec_cqc_eqc_rdb_parameters.cqc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_2 ( init_hca, 21,
		     qpc_eec_cqc_eqc_rdb_parameters.cqc_base_addr_l,
		     ( icm_offset >> 5 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_cq,
		     log_num_cqs );
	DBGC ( hermon, "Hermon %p ICM CQC base = %llx\n", hermon, icm_offset );
	icm_offset += icm_usage ( log_num_cqs, hermon->cap.cqc_entry_size );

	/* Event queue contexts */
	MLX_FILL_1 ( init_hca, 32,
		     qpc_eec_cqc_eqc_rdb_parameters.eqc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_2 ( init_hca, 33,
		     qpc_eec_cqc_eqc_rdb_parameters.eqc_base_addr_l,
		     ( icm_offset >> 5 ),
		     qpc_eec_cqc_eqc_rdb_parameters.log_num_of_eq,
		     log_num_eqs );
	DBGC ( hermon, "Hermon %p ICM EQC base = %llx\n", hermon, icm_offset );
	icm_offset += icm_usage ( log_num_eqs, hermon->cap.eqc_entry_size );

	/* Memory translation table */
	MLX_FILL_1 ( init_hca, 64,
		     tpt_parameters.mtt_base_addr_h, ( icm_offset >> 32 ) );
	MLX_FILL_1 ( init_hca, 65,
		     tpt_parameters.mtt_base_addr_l, icm_offset );
	DBGC ( hermon, "Hermon %p ICM MTT base = %llx\n", hermon, icm_offset );
	icm_offset += icm_usage ( log_num_mtts,
				  hermon->cap.mtt_entry_size );

	/* Memory protection table */
	log_num_mpts = fls ( hermon->cap.reserved_mrws + 1 - 1 );
	MLX_FILL_1 ( init_hca, 60,
		     tpt_parameters.dmpt_base_adr_h, ( icm_offset >> 32 ) );
	MLX_FILL_1 ( init_hca, 61,
		     tpt_parameters.dmpt_base_adr_l, icm_offset );
	MLX_FILL_1 ( init_hca, 62,
		     tpt_parameters.log_dmpt_sz, log_num_mpts );
	DBGC ( hermon, "Hermon %p ICM DMPT base = %llx\n", hermon, icm_offset);
	icm_offset += icm_usage ( log_num_mpts,
				  hermon->cap.dmpt_entry_size );

	/* Multicast table */
	MLX_FILL_1 ( init_hca, 48,
		     multicast_parameters.mc_base_addr_h,
		     ( icm_offset >> 32 ) );
	MLX_FILL_1 ( init_hca, 49,
		     multicast_parameters.mc_base_addr_l, icm_offset );
	MLX_FILL_1 ( init_hca, 52,
		     multicast_parameters.log_mc_table_entry_sz,
		     fls ( sizeof ( struct hermonprm_mcg_entry ) - 1 ) );
	MLX_FILL_1 ( init_hca, 53,
		     multicast_parameters.log_mc_table_hash_sz, 3 );
	MLX_FILL_1 ( init_hca, 54,
		     multicast_parameters.log_mc_table_sz, 3 );
	DBGC ( hermon, "Hermon %p ICM MC base = %llx\n", hermon, icm_offset );
	icm_offset += ( ( 8 * sizeof ( struct hermonprm_mcg_entry ) +
			  HERMON_PAGE_SIZE - 1 ) & ~( HERMON_PAGE_SIZE - 1 ) );

	hermon->icm_map[HERMON_ICM_OTHER].len =
		( icm_offset - hermon->icm_map[HERMON_ICM_OTHER].offset );

	/*
	 * Allocate and map physical memory for (portions of) ICM
	 *
	 * Map is:
	 *   ICM AUX area (aligned to its own size)
	 *   cMPT areas
	 *   Other areas
	 */

	/* Calculate physical memory required for ICM */
	icm_len = 0;
	for ( i = 0 ; i < HERMON_ICM_NUM_REGIONS ; i++ ) {
		icm_len += hermon->icm_map[i].len;
	}

	/* Get ICM auxiliary area size */
	memset ( &icm_size, 0, sizeof ( icm_size ) );
	MLX_FILL_1 ( &icm_size, 0, value_hi, ( icm_offset >> 32 ) );
	MLX_FILL_1 ( &icm_size, 1, value, icm_offset );
	if ( ( rc = hermon_cmd_set_icm_size ( hermon, &icm_size,
					      &icm_aux_size ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not set ICM size: %s\n",
		       hermon, strerror ( rc ) );
		goto err_set_icm_size;
	}
	icm_aux_len = ( MLX_GET ( &icm_aux_size, value ) * HERMON_PAGE_SIZE );

	/* Allocate ICM data and auxiliary area */
	DBGC ( hermon, "Hermon %p requires %zd kB ICM and %zd kB AUX ICM\n",
	       hermon, ( icm_len / 1024 ), ( icm_aux_len / 1024 ) );
	hermon->icm = umalloc ( icm_aux_len + icm_len );
	if ( ! hermon->icm ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	icm_phys = user_to_phys ( hermon->icm, 0 );

	/* Map ICM auxiliary area */
	DBGC ( hermon, "Hermon %p mapping ICM AUX => %08lx\n",
	       hermon, icm_phys );
	if ( ( rc = hermon_map_vpm ( hermon, hermon_cmd_map_icm_aux,
				     0, icm_phys, icm_aux_len ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not map AUX ICM: %s\n",
		       hermon, strerror ( rc ) );		
		goto err_map_icm_aux;
	}
	icm_phys += icm_aux_len;

	/* MAP ICM area */
	for ( i = 0 ; i < HERMON_ICM_NUM_REGIONS ; i++ ) {
		DBGC ( hermon, "Hermon %p mapping ICM %llx+%zx => %08lx\n",
		       hermon, hermon->icm_map[i].offset,
		       hermon->icm_map[i].len, icm_phys );
		if ( ( rc = hermon_map_vpm ( hermon, hermon_cmd_map_icm,
					     hermon->icm_map[i].offset,
					     icm_phys,
					     hermon->icm_map[i].len ) ) != 0 ){
			DBGC ( hermon, "Hermon %p could not map ICM: %s\n",
			       hermon, strerror ( rc ) );
			goto err_map_icm;
		}
		icm_phys += hermon->icm_map[i].len;
	}

	return 0;

 err_map_icm:
	assert ( i == 0 ); /* We don't handle partial failure at present */
 err_map_icm_aux:
	hermon_cmd_unmap_icm_aux ( hermon );
	ufree ( hermon->icm );
	hermon->icm = UNULL;
 err_alloc:
 err_set_icm_size:
	return rc;
}

/**
 * Free ICM
 *
 * @v hermon		Hermon device
 */
static void hermon_free_icm ( struct hermon *hermon ) {
	struct hermonprm_scalar_parameter unmap_icm;
	int i;

	for ( i = ( HERMON_ICM_NUM_REGIONS - 1 ) ; i >= 0 ; i-- ) {
		memset ( &unmap_icm, 0, sizeof ( unmap_icm ) );
		MLX_FILL_1 ( &unmap_icm, 0, value_hi,
			     ( hermon->icm_map[i].offset >> 32 ) );
		MLX_FILL_1 ( &unmap_icm, 1, value,
			     hermon->icm_map[i].offset );
		hermon_cmd_unmap_icm ( hermon,
				       ( 1 << fls ( ( hermon->icm_map[i].len /
						      HERMON_PAGE_SIZE ) - 1)),
				       &unmap_icm );
	}
	hermon_cmd_unmap_icm_aux ( hermon );
	ufree ( hermon->icm );
	hermon->icm = UNULL;
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
 * @v hermon		Hermon device
 * @ret rc		Return status code
 */
static int hermon_setup_mpt ( struct hermon *hermon ) {
	struct hermonprm_mpt mpt;
	uint32_t key;
	int rc;

	/* Derive key */
	key = ( hermon->cap.reserved_mrws | HERMON_MKEY_PREFIX );
	hermon->lkey = ( ( key << 8 ) | ( key >> 24 ) );

	/* Initialise memory protection table */
	memset ( &mpt, 0, sizeof ( mpt ) );
	MLX_FILL_7 ( &mpt, 0,
		     atomic, 1,
		     rw, 1,
		     rr, 1,
		     lw, 1,
		     lr, 1,
		     pa, 1,
		     r_w, 1 );
	MLX_FILL_1 ( &mpt, 2, mem_key, key );
	MLX_FILL_1 ( &mpt, 3,
		     pd, HERMON_GLOBAL_PD );
	MLX_FILL_1 ( &mpt, 10, len64, 1 );
	if ( ( rc = hermon_cmd_sw2hw_mpt ( hermon,
					   hermon->cap.reserved_mrws,
					   &mpt ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not set up MPT: %s\n",
		       hermon, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Configure special queue pairs
 *
 * @v hermon		Hermon device
 * @ret rc		Return status code
 */
static int hermon_configure_special_qps ( struct hermon *hermon ) {
	int rc;

	/* Special QP block must be aligned on its own size */
	hermon->special_qpn_base = ( ( hermon->cap.reserved_qps +
				       HERMON_NUM_SPECIAL_QPS - 1 )
				     & ~( HERMON_NUM_SPECIAL_QPS - 1 ) );
	hermon->qpn_base = ( hermon->special_qpn_base +
			     HERMON_NUM_SPECIAL_QPS );
	DBGC ( hermon, "Hermon %p special QPs at [%lx,%lx]\n", hermon,
	       hermon->special_qpn_base, ( hermon->qpn_base - 1 ) );

	/* Issue command to configure special QPs */
	if ( ( rc = hermon_cmd_conf_special_qp ( hermon, 0x00,
					  hermon->special_qpn_base ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not configure special QPs: "
		       "%s\n", hermon, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Reset device
 *
 * @v hermon		Hermon device
 * @v pci		PCI device
 */
static void hermon_reset ( struct hermon *hermon,
			   struct pci_device *pci ) {
	struct pci_config_backup backup;
	static const uint8_t backup_exclude[] =
		PCI_CONFIG_BACKUP_EXCLUDE ( 0x58, 0x5c );

	pci_backup ( pci, &backup, backup_exclude );
	writel ( HERMON_RESET_MAGIC,
		 ( hermon->config + HERMON_RESET_OFFSET ) );
	mdelay ( HERMON_RESET_WAIT_TIME_MS );
	pci_restore ( pci, &backup, backup_exclude );
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @v id		PCI ID
 * @ret rc		Return status code
 */
static int hermon_probe ( struct pci_device *pci,
			  const struct pci_device_id *id __unused ) {
	struct hermon *hermon;
	struct ib_device *ibdev;
	struct hermonprm_init_hca init_hca;
	unsigned int i;
	int rc;

	/* Allocate Hermon device */
	hermon = zalloc ( sizeof ( *hermon ) );
	if ( ! hermon ) {
		rc = -ENOMEM;
		goto err_alloc_hermon;
	}
	pci_set_drvdata ( pci, hermon );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Get PCI BARs */
	hermon->config = ioremap ( pci_bar_start ( pci, HERMON_PCI_CONFIG_BAR),
				   HERMON_PCI_CONFIG_BAR_SIZE );
	hermon->uar = ioremap ( pci_bar_start ( pci, HERMON_PCI_UAR_BAR ),
				HERMON_UAR_NON_EQ_PAGE * HERMON_PAGE_SIZE );

	/* Reset device */
	hermon_reset ( hermon, pci );

	/* Allocate space for mailboxes */
	hermon->mailbox_in = malloc_dma ( HERMON_MBOX_SIZE,
					  HERMON_MBOX_ALIGN );
	if ( ! hermon->mailbox_in ) {
		rc = -ENOMEM;
		goto err_mailbox_in;
	}
	hermon->mailbox_out = malloc_dma ( HERMON_MBOX_SIZE,
					   HERMON_MBOX_ALIGN );
	if ( ! hermon->mailbox_out ) {
		rc = -ENOMEM;
		goto err_mailbox_out;
	}

	/* Start firmware */
	if ( ( rc = hermon_start_firmware ( hermon ) ) != 0 )
		goto err_start_firmware;

	/* Get device limits */
	if ( ( rc = hermon_get_cap ( hermon ) ) != 0 )
		goto err_get_cap;

	/* Allocate Infiniband devices */
	for ( i = 0 ; i < hermon->cap.num_ports ; i++ ) {
	        ibdev = alloc_ibdev ( 0 );
		if ( ! ibdev ) {
			rc = -ENOMEM;
			goto err_alloc_ibdev;
		}
		hermon->ibdev[i] = ibdev;
		ibdev->op = &hermon_ib_operations;
		ibdev->dev = &pci->dev;
		ibdev->port = ( HERMON_PORT_BASE + i );
		ib_set_drvdata ( ibdev, hermon );
	}

	/* Allocate ICM */
	memset ( &init_hca, 0, sizeof ( init_hca ) );
	if ( ( rc = hermon_alloc_icm ( hermon, &init_hca ) ) != 0 )
		goto err_alloc_icm;

	/* Initialise HCA */
	MLX_FILL_1 ( &init_hca, 0, version, 0x02 /* "Must be 0x02" */ );
	MLX_FILL_1 ( &init_hca, 5, udp, 1 );
	MLX_FILL_1 ( &init_hca, 74, uar_parameters.log_max_uars, 8 );
	if ( ( rc = hermon_cmd_init_hca ( hermon, &init_hca ) ) != 0 ) {
		DBGC ( hermon, "Hermon %p could not initialise HCA: %s\n",
		       hermon, strerror ( rc ) );
		goto err_init_hca;
	}

	/* Set up memory protection */
	if ( ( rc = hermon_setup_mpt ( hermon ) ) != 0 )
		goto err_setup_mpt;
	for ( i = 0 ; i < hermon->cap.num_ports ; i++ )
		hermon->ibdev[i]->rdma_key = hermon->lkey;

	/* Set up event queue */
	if ( ( rc = hermon_create_eq ( hermon ) ) != 0 )
		goto err_create_eq;

	/* Configure special QPs */
	if ( ( rc = hermon_configure_special_qps ( hermon ) ) != 0 )
		goto err_conf_special_qps;

	/* Update IPoIB MAC address */
	for ( i = 0 ; i < hermon->cap.num_ports ; i++ ) {
		ib_smc_update ( hermon->ibdev[i], hermon_mad );
	}

	/* Register Infiniband devices */
	for ( i = 0 ; i < hermon->cap.num_ports ; i++ ) {
		if ( ( rc = register_ibdev ( hermon->ibdev[i] ) ) != 0 ) {
			DBGC ( hermon, "Hermon %p could not register IB "
			       "device: %s\n", hermon, strerror ( rc ) );
			goto err_register_ibdev;
		}
	}

	return 0;

	i = hermon->cap.num_ports;
 err_register_ibdev:
	for ( i-- ; ( signed int ) i >= 0 ; i-- )
		unregister_ibdev ( hermon->ibdev[i] );
 err_conf_special_qps:
	hermon_destroy_eq ( hermon );
 err_create_eq:
 err_setup_mpt:
	hermon_cmd_close_hca ( hermon );
 err_init_hca:
	hermon_free_icm ( hermon );
 err_alloc_icm:
	i = hermon->cap.num_ports;
 err_alloc_ibdev:
	for ( i-- ; ( signed int ) i >= 0 ; i-- )
		ibdev_put ( hermon->ibdev[i] );
 err_get_cap:
	hermon_stop_firmware ( hermon );
 err_start_firmware:
	free_dma ( hermon->mailbox_out, HERMON_MBOX_SIZE );
 err_mailbox_out:
	free_dma ( hermon->mailbox_in, HERMON_MBOX_SIZE );
 err_mailbox_in:
	free ( hermon );
 err_alloc_hermon:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void hermon_remove ( struct pci_device *pci ) {
	struct hermon *hermon = pci_get_drvdata ( pci );
	int i;

	for ( i = ( hermon->cap.num_ports - 1 ) ; i >= 0 ; i-- )
		unregister_ibdev ( hermon->ibdev[i] );
	hermon_destroy_eq ( hermon );
	hermon_cmd_close_hca ( hermon );
	hermon_free_icm ( hermon );
	hermon_stop_firmware ( hermon );
	hermon_stop_firmware ( hermon );
	free_dma ( hermon->mailbox_out, HERMON_MBOX_SIZE );
	free_dma ( hermon->mailbox_in, HERMON_MBOX_SIZE );
	for ( i = ( hermon->cap.num_ports - 1 ) ; i >= 0 ; i-- )
		ibdev_put ( hermon->ibdev[i] );
	free ( hermon );
}

static struct pci_device_id hermon_nics[] = {
	PCI_ROM ( 0x15b3, 0x6340, "mt25408", "MT25408 HCA driver", 0 ),
	PCI_ROM ( 0x15b3, 0x634a, "mt25418", "MT25418 HCA driver", 0 ),
	PCI_ROM ( 0x15b3, 0x6732, "mt26418", "MT26418 HCA driver", 0 ),
	PCI_ROM ( 0x15b3, 0x673c, "mt26428", "MT26428 HCA driver", 0 ),
};

struct pci_driver hermon_driver __pci_driver = {
	.ids = hermon_nics,
	.id_count = ( sizeof ( hermon_nics ) / sizeof ( hermon_nics[0] ) ),
	.probe = hermon_probe,
	.remove = hermon_remove,
};
