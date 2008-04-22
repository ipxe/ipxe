/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <gpxe/malloc.h>
#include <gpxe/umalloc.h>
#include <byteswap.h>
#include <unistd.h>
#include <gpxe/pci.h>
#include <gpxe/ethernet.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include "mtnic.h"


/*


	mtnic.c - gPXE driver for Mellanox 10Gig ConnectX EN


*/


/* (mcb30) - The Mellanox driver used "1" as a universal error code;
 * this at least makes it a valid error number.
 */
#define MTNIC_ERROR -EIO


/** Set port number to use
 *
 * 0 - port 1
 * 1 - port 2
 */
#define MTNIC_PORT_NUM 0
/* Note: for verbose printing do Make ... DEBUG=mtnic */




/********************************************************************
*
*	MTNIC allocation functions
*
*********************************************************************/
/**
* mtnic_alloc_aligned
*
* @v 	unsigned int size       size
* @v 	void **va		virtual address
* @v 	u32 *pa			physical address
* @v	u32 aligment		aligment
*
* Function allocate aligned buffer and put it's virtual address in 'va'
* and it's physical aligned address in 'pa'
*/
static int
mtnic_alloc_aligned(unsigned int size, void **va, u32 *pa, unsigned int alignment)
{
	*va = alloc_memblock(size, alignment);
	if (!*va) {
		return MTNIC_ERROR;
	}
	*pa = (u32)virt_to_bus(*va);
	return 0;
}



/**
 *
 * mtnic alloc command interface
 *
 */
static int
mtnic_alloc_cmdif(struct mtnic_priv *priv)
{
        u32 bar = mtnic_pci_dev.dev.bar[0];

	priv->hcr = ioremap(bar + MTNIC_HCR_BASE, MTNIC_HCR_SIZE);
	if (!priv->hcr) {
		DBG("Couldn't map command register.");
		return MTNIC_ERROR;
	}
	mtnic_alloc_aligned(PAGE_SIZE, (void *)&priv->cmd.buf, &priv->cmd.mapping, PAGE_SIZE);
	if (!priv->cmd.buf) {
		DBG("Error in allocating buffer for command interface\n");
		return MTNIC_ERROR;
	}
        return 0;
}

/**
 * Free RX io buffers
 */
static void
mtnic_free_io_buffers(struct mtnic_ring *ring)
{
        int index;

	for (; ring->cons <= ring->prod; ++ring->cons) {
		index = ring->cons & ring->size_mask;
		if (ring->iobuf[index])
			free_iob(ring->iobuf[index]);
	}
}



/**
 *
 * mtnic alloc and attach io buffers
 *
 */
static int
mtnic_alloc_iobuf(struct mtnic_priv *priv, struct mtnic_ring *ring,
		  unsigned int size)
{
	struct mtnic_rx_desc *rx_desc_ptr = ring->buf;
	u32 index;

	while ((u32)(ring->prod - ring->cons) < UNITS_BUFFER_SIZE) {
		index = ring->prod & ring->size_mask;
		ring->iobuf[index] = alloc_iob(size);
		if (!&ring->iobuf[index]) {
			if (ring->prod <= (ring->cons + 1)) {
				DBG("Error allocating Rx io "
				    "buffer number %lx", index);
				/* In case of error freeing io buffer */
				mtnic_free_io_buffers(ring);
				return MTNIC_ERROR;
			}

			break;
		}

		/* Attach io_buffer to descriptor */
		rx_desc_ptr = ring->buf +
			(sizeof(struct mtnic_rx_desc) * index);
		rx_desc_ptr->data.count = cpu_to_be32(size);
		rx_desc_ptr->data.mem_type = priv->fw.mem_type_snoop_be;
		rx_desc_ptr->data.addr_l = cpu_to_be32(
			virt_to_bus(ring->iobuf[index]->data));

		++ ring->prod;
	}

	/* Update RX producer index (PI) */
	ring->db->count = cpu_to_be32(ring->prod & 0xffff);
	return 0;
}


/**
 * mtnic alloc ring
 *
 * 	Alloc and configure TX or RX ring
 *
 */
static int
mtnic_alloc_ring(struct mtnic_priv *priv, struct mtnic_ring *ring,
			    u32 size, u16 stride, u16 cq, u8 is_rx)
{
	unsigned int i;
	int err;
	struct mtnic_rx_desc *rx_desc;
	struct mtnic_tx_desc *tx_desc;

	ring->size = size; /* Number of descriptors */
	ring->size_mask = size - 1;
	ring->stride = stride; /* Size of each entry */
	ring->cq = cq; /* CQ number associated with this ring */
	ring->cons = 0;
	ring->prod = 0;

	/* Alloc descriptors buffer */
	ring->buf_size = ring->size * ((is_rx) ? sizeof(struct mtnic_rx_desc) :
			 sizeof(struct mtnic_tx_desc));
	err = mtnic_alloc_aligned(ring->buf_size, (void *)&ring->buf,
			    &ring->dma, PAGE_SIZE);
        if (err) {
		DBG("Failed allocating descriptor ring sizeof %lx\n",
		    ring->buf_size);
		return MTNIC_ERROR;
	}
        memset(ring->buf, 0, ring->buf_size);

	DBG("Allocated %s ring (addr:%p) - buf:%p size:%lx"
	    "buf_size:%lx dma:%lx\n",
	    is_rx ? "Rx" : "Tx", ring, ring->buf, ring->size,
	    ring->buf_size, ring->dma);


	if (is_rx) { /* RX ring */
		/* Alloc doorbell */
		err = mtnic_alloc_aligned(sizeof(struct mtnic_cq_db_record),
				    (void *)&ring->db, &ring->db_dma, 32);
		if (err) {
			DBG("Failed allocating Rx ring doorbell record\n");
			free(ring->buf);
			return MTNIC_ERROR;
		}

		/* ==- Configure Descriptor -== */
		/* Init ctrl seg of rx desc */
		for (i = 0; i < UNITS_BUFFER_SIZE; ++i) {
			rx_desc = ring->buf +
				  (sizeof(struct mtnic_rx_desc) * i);
			/* Pre-link descriptor */
			rx_desc->next = cpu_to_be16(i + 1);
		}
		/*The last ctrl descriptor is '0' and points to the first one*/

		/* Alloc IO_BUFFERS */
		err = mtnic_alloc_iobuf(priv, ring, DEF_IOBUF_SIZE);
		if (err) {
			DBG("ERROR Allocating io buffer");
			free(ring->buf);
			return MTNIC_ERROR;
		}

        } else { /* TX ring */
		/* Set initial ownership of all Tx Desc' to SW (1) */
		for (i = 0; i < ring->size; i++) {
			tx_desc = ring->buf + ring->stride * i;
			tx_desc->ctrl.op_own = cpu_to_be32(MTNIC_BIT_DESC_OWN);
		}
		/* DB */
		ring->db_offset = cpu_to_be32(
			((u32) priv->fw.tx_offset[priv->port]) << 8);

		/* Map Tx+CQ doorbells */
		DBG("Mapping TxCQ doorbell at offset:0x%lx\n",
		    priv->fw.txcq_db_offset);
		ring->txcq_db = ioremap(mtnic_pci_dev.dev.bar[2] +
				priv->fw.txcq_db_offset, PAGE_SIZE);
		if (!ring->txcq_db) {
			DBG("Couldn't map txcq doorbell, aborting...\n");
			free(ring->buf);
			return MTNIC_ERROR;
		}
	}

	return 0;
}



/**
 * mtnic alloc CQ
 *
 *	Alloc and configure CQ.
 *
 */
static int
mtnic_alloc_cq(struct net_device *dev, int num, struct mtnic_cq *cq,
			  u8 is_rx, u32 size, u32 offset_ind)
{
	int err ;
	unsigned int i;

	cq->num = num;
	cq->dev = dev;
	cq->size = size;
	cq->last = 0;
	cq->is_rx = is_rx;
	cq->offset_ind = offset_ind;

	/* Alloc doorbell */
	err = mtnic_alloc_aligned(sizeof(struct mtnic_cq_db_record),
			    (void *)&cq->db, &cq->db_dma, 32);
	if (err) {
		DBG("Failed allocating CQ doorbell record\n");
		return MTNIC_ERROR;
	}
	memset(cq->db, 0, sizeof(struct mtnic_cq_db_record));

	/* Alloc CQEs buffer */
	cq->buf_size = size * sizeof(struct mtnic_cqe);
	err = mtnic_alloc_aligned(cq->buf_size,
			    (void *)&cq->buf, &cq->dma, PAGE_SIZE);
	if (err) {
		DBG("Failed allocating CQ buffer\n");
		free(cq->db);
		return MTNIC_ERROR;
	}
        memset(cq->buf, 0, cq->buf_size);
        DBG("Allocated CQ (addr:%p) - size:%lx buf:%p buf_size:%lx "
	    "dma:%lx db:%p db_dma:%lx\n"
	    "cqn offset:%lx \n", cq, cq->size, cq->buf,
	    cq->buf_size, cq->dma, cq->db,
	    cq->db_dma, offset_ind);


	/* Set ownership of all CQEs to HW */
	DBG("Setting HW ownership for CQ:%d\n", num);
	for (i = 0; i < cq->size; i++) {
		/* Initial HW ownership is 1 */
		cq->buf[i].op_tr_own = MTNIC_BIT_CQ_OWN;
	}
	return 0;
}



/**
 * mtnic_alloc_resources
 *
 * 	Alloc and configure CQs, Tx, Rx
 */
unsigned int
mtnic_alloc_resources(struct net_device *dev)
{
	struct mtnic_priv *priv = netdev_priv(dev);
        int err;
	int cq_ind = 0;
	int cq_offset = priv->fw.cq_offset;

	/* Alloc 1st CQ */
        err = mtnic_alloc_cq(dev, cq_ind, &priv->cq[cq_ind], 1 /* RX */,
			     UNITS_BUFFER_SIZE, cq_offset + cq_ind);
	if (err) {
		DBG("Failed allocating Rx CQ\n");
		return MTNIC_ERROR;
	}

	/* Alloc RX */
	err = mtnic_alloc_ring(priv, &priv->rx_ring, UNITS_BUFFER_SIZE,
			       sizeof(struct mtnic_rx_desc), cq_ind, /* RX */1);
	if (err) {
		DBG("Failed allocating Rx Ring\n");
		goto cq0_error;
	}

        ++cq_ind;

	/* alloc 2nd CQ */
	err = mtnic_alloc_cq(dev, cq_ind, &priv->cq[cq_ind], 0 /* TX */,
			     UNITS_BUFFER_SIZE, cq_offset + cq_ind);
	if (err) {
		DBG("Failed allocating Tx CQ\n");
		goto rx_error;
	}

	/* Alloc TX */
	err = mtnic_alloc_ring(priv, &priv->tx_ring, UNITS_BUFFER_SIZE,
			       sizeof(struct mtnic_tx_desc), cq_ind, /* TX */ 0);
	if (err) {
		DBG("Failed allocating Tx ring\n");
		goto cq1_error;
	}

	return 0;

cq1_error:
	free(priv->cq[1].buf);
	free(priv->cq[1].db);
rx_error:
	free(priv->rx_ring.buf);
	free(priv->rx_ring.db);
	mtnic_free_io_buffers(&priv->rx_ring);
cq0_error:
	free(priv->cq[0].buf);
	free(priv->cq[0].db);

	return MTNIC_ERROR;
}


/**
 *  mtnic alloc_eq
 *
 * Note: EQ is not used by the driver but must be allocated
 */
static int
mtnic_alloc_eq(struct mtnic_priv *priv)
{
	int err;
	unsigned int i;
	struct mtnic_eqe *eqe_desc = NULL;

	/* Allocating doorbell */
	priv->eq_db = ioremap(mtnic_pci_dev.dev.bar[2] +
			      priv->fw.eq_db_offset, sizeof(u32));
	if (!priv->eq_db) {
		DBG("Couldn't map EQ doorbell, aborting...\n");
		return MTNIC_ERROR;
	}

	/* Allocating buffer */
	priv->eq.size = NUM_EQES;
	priv->eq.buf_size = priv->eq.size * sizeof(struct mtnic_eqe);
        err = mtnic_alloc_aligned(priv->eq.buf_size, (void *)&priv->eq.buf,
			    &priv->eq.dma, PAGE_SIZE);
	if (err) {
		DBG("Failed allocating EQ buffer\n");
		iounmap(priv->eq_db);
		return MTNIC_ERROR;
	}
	memset(priv->eq.buf, 0, priv->eq.buf_size);

	for (i = 0; i < priv->eq.size; i++)
		eqe_desc = priv->eq.buf + (sizeof(struct mtnic_eqe) * i);
		eqe_desc->own |= MTNIC_BIT_EQE_OWN;

	mdelay(20);
	return 0;
}











/********************************************************************
*
* Mtnic commands functions
* -=-=-=-=-=-=-=-=-=-=-=-=
*
*
*
*********************************************************************/
static inline int
cmdif_go_bit(struct mtnic_priv *priv)
{
	struct mtnic_if_cmd_reg *hcr = priv->hcr;
	u32 status;
	int i;

	for (i = 0; i < TBIT_RETRIES; i++) {
		status = be32_to_cpu(readl(&hcr->status_go_opcode));
		if ((status & MTNIC_BC_MASK(MTNIC_MASK_CMD_REG_T_BIT)) ==
		    (priv->cmd.tbit << MTNIC_BC_OFF(MTNIC_MASK_CMD_REG_T_BIT))) {
			/* Read expected t-bit - now return go-bit value */
			return status & MTNIC_BC_MASK(MTNIC_MASK_CMD_REG_GO_BIT);
		}
	}

	DBG("Invalid tbit after %d retries!\n", TBIT_RETRIES);
	return 1; /* Return busy... */
}

/* Base Command interface */
static int
mtnic_cmd(struct mtnic_priv *priv, void *in_imm,
	  void *out_imm, u32 in_modifier, u16 op)
{

	struct mtnic_if_cmd_reg *hcr = priv->hcr;
	int err = 0;
	u32 out_param_h = 0;
	u32 out_param_l = 0;
	u32 in_param_h = 0;
	u32 in_param_l = 0;


	static u16 token = 0x8000;
	u32 status;
	unsigned int timeout = 0;

	token++;

	if (cmdif_go_bit(priv)) {
		DBG("GO BIT BUSY:%p.\n", hcr + 6);
		err = MTNIC_ERROR;
		goto out;
	}
	if (in_imm) {
		in_param_h = *((u32*)in_imm);
		in_param_l = *((u32*)in_imm + 1);
	} else {
		in_param_l = cpu_to_be32(priv->cmd.mapping);
	}
	out_param_l = cpu_to_be32(priv->cmd.mapping);

	/* writing to MCR */
	writel(in_param_h,			&hcr->in_param_h);
	writel(in_param_l,			&hcr->in_param_l);
	writel((u32) cpu_to_be32(in_modifier),	&hcr->input_modifier);
	writel(out_param_h,			&hcr->out_param_h);
	writel(out_param_l,			&hcr->out_param_l);
	writel((u32)cpu_to_be32(token << 16),	&hcr->token);
	wmb();

	/* flip toggle bit before each write to the HCR */
	priv->cmd.tbit = !priv->cmd.tbit;
	writel((u32)
		cpu_to_be32(MTNIC_BC_MASK(MTNIC_MASK_CMD_REG_GO_BIT) |
		(priv->cmd.tbit << MTNIC_BC_OFF(MTNIC_MASK_CMD_REG_T_BIT)) | op),
		&hcr->status_go_opcode);

	while (cmdif_go_bit(priv) && (timeout <= GO_BIT_TIMEOUT)) {
		mdelay(1);
		++timeout;
	}

	if (cmdif_go_bit(priv)) {
		DBG("Command opcode:0x%x token:0x%x TIMEOUT.\n", op, token);
		err = MTNIC_ERROR;
		goto out;
	}

	if (out_imm) {
		*((u32 *)out_imm) = readl(&hcr->out_param_h);
		*((u32 *)out_imm + 1) = readl(&hcr->out_param_l);
	}

	status = be32_to_cpu((u32)readl(&hcr->status_go_opcode)) >> 24;
	/*DBG("Command opcode:0x%x token:0x%x returned:0x%lx\n",
		  op, token, status);*/

	if (status) {
		return status;
	}

out:
	return err;
}

/* MAP PAGES wrapper */
static int
mtnic_map_cmd(struct mtnic_priv *priv, u16 op, struct mtnic_pages pages)
{
        unsigned int j;
	u32 addr;
	unsigned int len;
	u32 *page_arr = priv->cmd.buf;
	int nent = 0;
	int err = 0;

	memset(page_arr, 0, PAGE_SIZE);

	len = PAGE_SIZE * pages.num;
	pages.buf = (u32 *)umalloc(PAGE_SIZE * (pages.num + 1));
	addr = PAGE_SIZE + ((virt_to_bus(pages.buf) & 0xfffff000) + PAGE_SIZE);
	DBG("Mapping pages: size: %lx address: %p\n", pages.num, pages.buf);

	if (addr & (PAGE_MASK)) {
		DBG("Got FW area not aligned to %d (%llx/%x)\n",
		    PAGE_SIZE, (u64) addr, len);
		return MTNIC_ERROR;
	}

	/* Function maps each PAGE seperately */
	for (j = 0; j < len; j+= PAGE_SIZE) {
		page_arr[nent * 4 + 3] = cpu_to_be32(addr + j);
		if (++nent == MTNIC_MAILBOX_SIZE / 16) {
			err = mtnic_cmd(priv, NULL, NULL, nent, op);
			if (err)
				return MTNIC_ERROR;
                        nent = 0;
		}
	}

	if (nent)
		err = mtnic_cmd(priv, NULL, NULL, nent, op);

	return err;
}



/*
 * Query FW
 */
static int
mtnic_QUERY_FW(struct mtnic_priv *priv)
{
	int err;
	struct mtnic_if_query_fw_out_mbox *cmd = priv->cmd.buf;

	err = mtnic_cmd(priv, NULL, NULL, 0, MTNIC_IF_CMD_QUERY_FW);
	if (err)
		return MTNIC_ERROR;

	/* Get FW and interface versions */
	priv->fw_ver = ((u64) be16_to_cpu(cmd->rev_maj) << 32) |
		((u64) be16_to_cpu(cmd->rev_min) << 16) |
		(u64) be16_to_cpu(cmd->rev_smin);
	priv->fw.ifc_rev = be16_to_cpu(cmd->ifc_rev);

	/* Get offset for internal error reports (debug) */
	priv->fw.err_buf.offset = be64_to_cpu(cmd->err_buf_start);
	priv->fw.err_buf.size = be32_to_cpu(cmd->err_buf_size);

	DBG("Error buf offset is %llx\n", priv->fw.err_buf.offset);

	/* Get number of required FW (4k) pages */
	priv->fw.fw_pages.num = be16_to_cpu(cmd->fw_pages);

	return 0;
}


static int
mtnic_OPEN_NIC(struct mtnic_priv *priv)
{

	struct mtnic_if_open_nic_in_mbox *open_nic = priv->cmd.buf;
	u32 extra_pages[2] = {0};
	int err;

	memset(open_nic, 0, sizeof *open_nic);

        /* port 1 */
	open_nic->log_rx_p1 = 0;
	open_nic->log_cq_p1 = 1;

	open_nic->log_tx_p1 = 0;
	open_nic->steer_p1 = MTNIC_IF_STEER_RSS;
	/* MAC + VLAN - leave reserved */

	/* port 2 */
	open_nic->log_rx_p2 = 0;
	open_nic->log_cq_p2 = 1;

	open_nic->log_tx_p2 = 0;
	open_nic->steer_p2 = MTNIC_IF_STEER_RSS;
	/* MAC + VLAN - leave reserved */

	err = mtnic_cmd(priv, NULL, extra_pages, 0, MTNIC_IF_CMD_OPEN_NIC);
	priv->fw.extra_pages.num = be32_to_cpu(*(extra_pages+1));
	DBG("Extra pages num is %lx\n", priv->fw.extra_pages.num);
	return err;
}

static int
mtnic_CONFIG_RX(struct mtnic_priv *priv)
{
	struct mtnic_if_config_rx_in_imm config_rx;

	memset(&config_rx, 0, sizeof config_rx);
	return mtnic_cmd(priv, &config_rx, NULL, 0, MTNIC_IF_CMD_CONFIG_RX);
}

static int
mtnic_CONFIG_TX(struct mtnic_priv *priv)
{
	struct mtnic_if_config_send_in_imm config_tx;

	config_tx.enph_gpf = 0;
	return mtnic_cmd(priv, &config_tx, NULL, 0, MTNIC_IF_CMD_CONFIG_TX);
}

static int
mtnic_HEART_BEAT(struct mtnic_priv *priv, u32 *link_state)
{
	struct mtnic_if_heart_beat_out_imm heart_beat;

	int err;
	u32 flags;
	err = mtnic_cmd(priv, NULL, &heart_beat, 0, MTNIC_IF_CMD_HEART_BEAT);
	if (!err) {
		flags = be32_to_cpu(heart_beat.flags);
		if (flags & MTNIC_BC_MASK(MTNIC_MASK_HEAR_BEAT_INT_ERROR)) {
			DBG("Internal error detected\n");
			return MTNIC_ERROR;
		}
		*link_state = flags &
			~((u32) MTNIC_BC_MASK(MTNIC_MASK_HEAR_BEAT_INT_ERROR));
	}
	return err;
}


/*
 * Port commands
 */

static int
mtnic_SET_PORT_DEFAULT_RING(struct mtnic_priv *priv, u8 port, u16 ring)
{
	struct mtnic_if_set_port_default_ring_in_imm def_ring;

	memset(&def_ring, 0, sizeof(def_ring));
	def_ring.ring = ring;
	return mtnic_cmd(priv, &def_ring, NULL, port + 1,
			 MTNIC_IF_CMD_SET_PORT_DEFAULT_RING);
}

static int
mtnic_CONFIG_PORT_RSS_STEER(struct mtnic_priv *priv, int port)
{
	memset(priv->cmd.buf, 0, PAGE_SIZE);
	return  mtnic_cmd(priv, NULL, NULL, port + 1,
			       MTNIC_IF_CMD_CONFIG_PORT_RSS_STEER);
}

static int
mtnic_SET_PORT_RSS_INDIRECTION(struct mtnic_priv *priv, int port)

{
	memset(priv->cmd.buf, 0, PAGE_SIZE);
	return mtnic_cmd(priv, NULL, NULL, port + 1,
			       MTNIC_IF_CMD_SET_PORT_RSS_INDIRECTION);
}


/*
 * Config commands
 */
static int
mtnic_CONFIG_CQ(struct mtnic_priv *priv, int port,
		    u16 cq_ind, struct mtnic_cq *cq)
{
	struct mtnic_if_config_cq_in_mbox *config_cq = priv->cmd.buf;

	memset(config_cq, 0, sizeof *config_cq);
	config_cq->cq = cq_ind;
	config_cq->size = fls(UNITS_BUFFER_SIZE - 1);
	config_cq->offset = ((cq->dma) & (PAGE_MASK)) >> 6;
	config_cq->db_record_addr_l = cpu_to_be32(cq->db_dma);
        config_cq->page_address[1] = cpu_to_be32(cq->dma);
	DBG("config cq address: %lx dma_address: %lx"
	    "offset: %d size %d index: %d "
	    , config_cq->page_address[1],cq->dma,
	    config_cq->offset, config_cq->size, config_cq->cq );

	return mtnic_cmd(priv, NULL, NULL, port + 1,
			       MTNIC_IF_CMD_CONFIG_CQ);
}


static int
mtnic_CONFIG_TX_RING(struct mtnic_priv *priv, u8 port,
			 u16 ring_ind, struct mtnic_ring *ring)
{
	struct mtnic_if_config_send_ring_in_mbox *config_tx_ring = priv->cmd.buf;
	memset(config_tx_ring, 0, sizeof *config_tx_ring);
	config_tx_ring->ring = cpu_to_be16(ring_ind);
	config_tx_ring->size = fls(UNITS_BUFFER_SIZE - 1);
	config_tx_ring->cq = cpu_to_be16(ring->cq);
	config_tx_ring->page_address[1] = cpu_to_be32(ring->dma);

	return mtnic_cmd(priv, NULL, NULL, port + 1,
			 MTNIC_IF_CMD_CONFIG_TX_RING);
}

static int
mtnic_CONFIG_RX_RING(struct mtnic_priv *priv, u8 port,
			 u16 ring_ind, struct mtnic_ring *ring)
{
	struct mtnic_if_config_rx_ring_in_mbox *config_rx_ring = priv->cmd.buf;
	memset(config_rx_ring, 0, sizeof *config_rx_ring);
	config_rx_ring->ring = ring_ind;
		MTNIC_BC_PUT(config_rx_ring->stride_size, fls(UNITS_BUFFER_SIZE - 1),
			     MTNIC_MASK_CONFIG_RX_RING_SIZE);
		MTNIC_BC_PUT(config_rx_ring->stride_size, 1,
			     MTNIC_MASK_CONFIG_RX_RING_STRIDE);
	config_rx_ring->cq = cpu_to_be16(ring->cq);
	config_rx_ring->db_record_addr_l = cpu_to_be32(ring->db_dma);

        DBG("Config RX ring starting at address:%lx\n", ring->dma);

	config_rx_ring->page_address[1] = cpu_to_be32(ring->dma);

	return mtnic_cmd(priv, NULL, NULL, port + 1,
			       MTNIC_IF_CMD_CONFIG_RX_RING);
}

static int
mtnic_CONFIG_EQ(struct mtnic_priv *priv)
{
	struct mtnic_if_config_eq_in_mbox *eq = priv->cmd.buf;

	if (priv->eq.dma & (PAGE_MASK)) {
		DBG("misalligned eq buffer:%lx\n",
		    priv->eq.dma);
		return MTNIC_ERROR;
        }

        memset(eq, 0, sizeof *eq);
	MTNIC_BC_PUT(eq->offset, priv->eq.dma >> 6, MTNIC_MASK_CONFIG_EQ_OFFSET);
	MTNIC_BC_PUT(eq->size, fls(priv->eq.size - 1) - 1, MTNIC_MASK_CONFIG_EQ_SIZE);
	MTNIC_BC_PUT(eq->int_vector, 0, MTNIC_MASK_CONFIG_EQ_INT_VEC);
	eq->page_address[1] = cpu_to_be32(priv->eq.dma);

	return mtnic_cmd(priv, NULL, NULL, 0, MTNIC_IF_CMD_CONFIG_EQ);
}




static int
mtnic_SET_RX_RING_ADDR(struct mtnic_priv *priv, u8 port, u64* mac)
{
	struct mtnic_if_set_rx_ring_addr_in_imm ring_addr;
	u32 modifier = ((u32) port + 1) << 16;

	memset(&ring_addr, 0, sizeof(ring_addr));

	ring_addr.mac_31_0 = cpu_to_be32(*mac & 0xffffffff);
	ring_addr.mac_47_32 = cpu_to_be16((*mac >> 32) & 0xffff);
	ring_addr.flags_vlan_id |= cpu_to_be16(
		MTNIC_BC_MASK(MTNIC_MASK_SET_RX_RING_ADDR_BY_MAC));

	return mtnic_cmd(priv, &ring_addr, NULL, modifier, MTNIC_IF_CMD_SET_RX_RING_ADDR);
}

static int
mtnic_SET_PORT_STATE(struct mtnic_priv *priv, u8 port, u8 state)
{
	struct mtnic_if_set_port_state_in_imm port_state;

	port_state.state = state ? cpu_to_be32(
		MTNIC_BC_MASK(MTNIC_MASK_CONFIG_PORT_STATE)) : 0;
	port_state.reserved = 0;
	return mtnic_cmd(priv, &port_state, NULL, port + 1,
			 MTNIC_IF_CMD_SET_PORT_STATE);
}

static int
mtnic_SET_PORT_MTU(struct mtnic_priv *priv, u8 port, u16 mtu)
{
	struct mtnic_if_set_port_mtu_in_imm set_mtu;

	memset(&set_mtu, 0, sizeof(set_mtu));
	set_mtu.mtu = cpu_to_be16(mtu);
	return mtnic_cmd(priv, &set_mtu, NULL, port + 1,
				MTNIC_IF_CMD_SET_PORT_MTU);
}


static int
mtnic_CONFIG_PORT_VLAN_FILTER(struct mtnic_priv *priv, int port)
{
	struct mtnic_if_config_port_vlan_filter_in_mbox *vlan_filter = priv->cmd.buf;

	/* When no vlans are configured we disable the filter
	 * (i.e., pass all vlans) because we ignore them anyhow */
	memset(vlan_filter, 0xff, sizeof(*vlan_filter));
	return mtnic_cmd(priv, NULL, NULL, port + 1,
				MTNIC_IF_CMD_CONFIG_PORT_VLAN_FILTER);
}


static int
mtnic_RELEASE_RESOURCE(struct mtnic_priv *priv, u8 port, u8 type, u8 index)
{
	struct mtnic_if_release_resource_in_imm rel;
	memset(&rel, 0, sizeof rel);
	rel.index = index;
	rel.type = type;
	return mtnic_cmd(priv,
				&rel, NULL, (type == MTNIC_IF_RESOURCE_TYPE_EQ) ?
				0 : port + 1, MTNIC_IF_CMD_RELEASE_RESOURCE);
}


static int
mtnic_QUERY_CAP(struct mtnic_priv *priv, u8 index, u8 mod, u64 *result)
{
	struct mtnic_if_query_cap_in_imm cap;
	u32 out_imm[2];
	int err;

	memset(&cap, 0, sizeof cap);
	cap.cap_index = index;
	cap.cap_modifier = mod;
	err = mtnic_cmd(priv, &cap, &out_imm, 0, MTNIC_IF_CMD_QUERY_CAP);

	*((u32*)result) = be32_to_cpu(*(out_imm+1));
	*((u32*)result + 1) = be32_to_cpu(*out_imm);

	DBG("Called Query cap with index:0x%x mod:%d result:0x%llx"
	    " error:%d\n", index, mod, *result, err);
	return err;
}


#define DO_QUERY_CAP(cap, mod, var)				\
		err = mtnic_QUERY_CAP(priv, cap, mod, &result);	\
		if (err)					\
			return err;				\
		(var) = result

static int
mtnic_query_cap(struct mtnic_priv *priv)
{
	int err = 0;
	int i;
        u64 result;

	DO_QUERY_CAP(MTNIC_IF_CAP_NUM_PORTS, 0, priv->fw.num_ports);
        for (i = 0; i < priv->fw.num_ports; i++) {
		DO_QUERY_CAP(MTNIC_IF_CAP_DEFAULT_MAC, i + 1, priv->fw.mac[i]);
	}

	return 0;
}

static int
mtnic_query_offsets(struct mtnic_priv *priv)
{
	int err;
	int i;
	u64 result;

	DO_QUERY_CAP(MTNIC_IF_CAP_MEM_KEY,
		     MTNIC_IF_MEM_TYPE_SNOOP,
		     priv->fw.mem_type_snoop_be);
	priv->fw.mem_type_snoop_be = cpu_to_be32(priv->fw.mem_type_snoop_be);
	DO_QUERY_CAP(MTNIC_IF_CAP_TX_CQ_DB_OFFSET, 0, priv->fw.txcq_db_offset);
	DO_QUERY_CAP(MTNIC_IF_CAP_EQ_DB_OFFSET, 0, priv->fw.eq_db_offset);

	for (i = 0; i < priv->fw.num_ports; i++) {
		DO_QUERY_CAP(MTNIC_IF_CAP_CQ_OFFSET, i + 1, priv->fw.cq_offset);
		DO_QUERY_CAP(MTNIC_IF_CAP_TX_OFFSET, i + 1, priv->fw.tx_offset[i]);
		DO_QUERY_CAP(MTNIC_IF_CAP_RX_OFFSET, i + 1, priv->fw.rx_offset[i]);
		DBG("--> Port %d CQ offset:0x%x\n", i, priv->fw.cq_offset);
		DBG("--> Port %d Tx offset:0x%x\n", i, priv->fw.tx_offset[i]);
		DBG("--> Port %d Rx offset:0x%x\n", i, priv->fw.rx_offset[i]);
	}

	mdelay(20);
	return 0;
}











/********************************************************************
*
*	MTNIC initalization functions
*
*
*
*
*********************************************************************/

/**
 * Reset device
 */
void
mtnic_reset(void)
{
	void *reset = ioremap(mtnic_pci_dev.dev.bar[0] + MTNIC_RESET_OFFSET, 4);
	writel(cpu_to_be32(1), reset);
	iounmap(reset);
}


/**
 * Restore PCI config
 */
static int
restore_config(void)
{
	int i;
	int rc;

	for (i = 0; i < 64; ++i) {
		if (i != 22 && i != 23) {
			rc = pci_write_config_dword(mtnic_pci_dev.dev.dev,
						    i << 2,
						    mtnic_pci_dev.dev.
						    dev_config_space[i]);
			if (rc)
				return rc;
		}
	}
	return 0;
}



/**
 * Init PCI configuration
 */
static int
mtnic_init_pci(struct pci_device *dev)
{
	int i;
	int err;

	/* save bars */
	DBG("bus=%d devfn=0x%x", dev->bus, dev->devfn);
	for (i = 0; i < 6; ++i) {
		mtnic_pci_dev.dev.bar[i] =
		    pci_bar_start(dev, PCI_BASE_ADDRESS_0 + (i << 2));
		DBG("bar[%d]= 0x%08lx \n", i, mtnic_pci_dev.dev.bar[i]);
	}

	/* save config space */
	for (i = 0; i < 64; ++i) {
		err = pci_read_config_dword(dev, i << 2,
					   &mtnic_pci_dev.dev.
					   dev_config_space[i]);
		if (err) {
			DBG("Can not save configuration space");
			return err;
		}
	}

	mtnic_pci_dev.dev.dev = dev;

        return 0;
}

/**
 *  Initial hardware
 */
static inline
int mtnic_init_card(struct net_device *dev)
{
	struct mtnic_priv *priv = netdev_priv(dev);
	int err = 0;


	/* Set state */
	priv->state = CARD_DOWN;
	/* Set port */
        priv->port = MTNIC_PORT_NUM;

        /* Alloc command interface */
	err = mtnic_alloc_cmdif(priv);
	if (err) {
		DBG("Failed to init command interface, aborting.\n");
		return MTNIC_ERROR;
	}



        /**
	 *  Bring up HW
	 */
	err = mtnic_QUERY_FW(priv);
	if (err) {
		DBG("QUERY_FW command failed, aborting.\n");
		goto cmd_error;
	}

	DBG("Command interface revision:%d\n", priv->fw.ifc_rev);

	/* Allocate memory for FW and start it */
	err = mtnic_map_cmd(priv, MTNIC_IF_CMD_MAP_FW, priv->fw.fw_pages);
	if (err) {
		DBG("Eror In MAP_FW\n");
		if (priv->fw.fw_pages.buf)
			free(priv->fw.fw_pages.buf);
		goto cmd_error;
	}

	/* Run firmware */
	err = mtnic_cmd(priv, NULL, NULL, 0, MTNIC_IF_CMD_RUN_FW);
	if (err) {
		DBG("Eror In RUN FW\n");
		goto map_fw_error;
	}

        DBG("FW version:%d.%d.%d\n",
	    (u16) (priv->fw_ver >> 32),
		(u16) ((priv->fw_ver >> 16) & 0xffff),
		(u16) (priv->fw_ver & 0xffff));


	/* Get device information */
	err = mtnic_query_cap(priv);
	if (err) {
		DBG("Insufficient resources, aborting.\n");
		goto map_fw_error;
	}

	/* Open NIC */
	err = mtnic_OPEN_NIC(priv);
	if (err) {
		DBG("Failed opening NIC, aborting.\n");
		goto map_fw_error;
	}

	/* Allocate and map pages worksace */
	err = mtnic_map_cmd(priv, MTNIC_IF_CMD_MAP_PAGES, priv->fw.extra_pages);
	if (err) {
		DBG("Couldn't allocate %lx FW extra pages, aborting.\n",
		    priv->fw.extra_pages.num);
		if (priv->fw.extra_pages.buf)
			free(priv->fw.extra_pages.buf);
		goto map_fw_error;
	}

	/* Get device offsets */
	err = mtnic_query_offsets(priv);
	if (err) {
		DBG("Failed retrieving resource offests, aborting.\n");
		free(priv->fw.extra_pages.buf);
		goto map_extra_error;
	}


        /* Alloc EQ */
	err = mtnic_alloc_eq(priv);
	if (err) {
		DBG("Failed init shared resources. error: %d\n", err);
		goto map_extra_error;
        }

	/* Configure HW */
	err = mtnic_CONFIG_EQ(priv);
	if (err) {
		DBG("Failed configuring EQ\n");
		goto eq_error;
	}
	err = mtnic_CONFIG_RX(priv);
	if (err) {
		DBG("Failed Rx configuration\n");
		goto eq_error;
	}
	err = mtnic_CONFIG_TX(priv);
	if (err) {
		DBG("Failed Tx configuration\n");
		goto eq_error;
	}

        DBG("Activating port:%d\n", MTNIC_PORT_NUM + 1);

        priv->state = CARD_INITIALIZED;

	return 0;


eq_error:
	iounmap(priv->eq_db);
	free(priv->eq.buf);
map_extra_error:
	free(priv->fw.extra_pages.buf);
map_fw_error:
	free(priv->fw.fw_pages.buf);

cmd_error:
	iounmap(priv->hcr);
	free(priv->cmd.buf);
	free(priv);

	return MTNIC_ERROR;
}










/*******************************************************************
*
* Process functions
*
*	process compliations of TX and RX
*
*
********************************************************************/
void mtnic_process_tx_cq(struct mtnic_priv *priv, struct net_device *dev,
			 struct mtnic_cq *cq)
{
	struct mtnic_cqe *cqe = cq->buf;
	struct mtnic_ring *ring = &priv->tx_ring;
	u16 index;


	index = cq->last & (cq->size-1);
	cqe = &cq->buf[index];

	/* Owner bit changes every round */
	while (XNOR(cqe->op_tr_own & MTNIC_BIT_CQ_OWN, cq->last & cq->size)) {
		netdev_tx_complete (dev, ring->iobuf[index]);
                ++cq->last;
                index = cq->last & (cq->size-1);
		cqe = &cq->buf[index];
        }

	/* Update consumer index */
	cq->db->update_ci = cpu_to_be32(cq->last & 0xffffff);
	wmb(); /* ensure HW sees CQ consumer before we post new buffers */
	ring->cons = cq->last;
}


int mtnic_process_rx_cq(struct mtnic_priv *priv, struct net_device *dev, struct mtnic_cq *cq)
{
	struct mtnic_cqe *cqe;
	struct mtnic_ring *ring = &priv->rx_ring;
	int index;
	int err;
	struct io_buffer *rx_iob;


	/* We assume a 1:1 mapping between CQEs and Rx descriptors, so Rx
	 * descriptor offset can be deduced from the CQE index instead of
	 * reading 'cqe->index' */
	index = cq->last & (cq->size-1);
	cqe = &cq->buf[index];

	/* Process all completed CQEs */
	while (XNOR(cqe->op_tr_own & MTNIC_BIT_CQ_OWN, cq->last & cq->size)) {
		/* Drop packet on bad receive or bad checksum */
		if ((cqe->op_tr_own & 0x1f) == MTNIC_OPCODE_ERROR) {
			DBG("CQE completed with error - vendor \n");
			free_iob(ring->iobuf[index]);
			goto next;
		}
		if (cqe->enc_bf & MTNIC_BIT_BAD_FCS) {
			DBG("Accepted packet with bad FCS\n");
			free_iob(ring->iobuf[index]);
			goto next;
		}

		/*
		 * Packet is OK - process it.
		 */
                rx_iob = ring->iobuf[index];
		iob_put(rx_iob, DEF_IOBUF_SIZE);
		/* Add this packet to the receive queue. */
		netdev_rx(dev, rx_iob);
                ring->iobuf[index] = NULL;

next:
		++cq->last;
		index = cq->last & (cq->size-1);
		cqe = &cq->buf[index];
	}

	/* Update consumer index */
	cq->db->update_ci = cpu_to_be32(cq->last & 0xffffff);
	wmb(); /* ensure HW sees CQ consumer before we post new buffers */
	ring->cons = cq->last;

	if (ring->prod - ring->cons < (MAX_GAP_PROD_CONS)) {
		err = mtnic_alloc_iobuf(priv, &priv->rx_ring, DEF_IOBUF_SIZE);
		if (err) {
			DBG("ERROR Allocating io buffer");
			return MTNIC_ERROR;
		}
	}

	return 0;
}
















/********************************************************************
*
* net_device functions
*
*
*	open, poll, close, probe, disable, irq
*
*********************************************************************/
static int
mtnic_open(struct net_device *dev)
{
	struct mtnic_priv *priv = netdev_priv(dev);
	int err = 0;
	struct mtnic_ring *ring;
	struct mtnic_cq *cq;
	int cq_ind = 0;
	u32 dev_link_state;

	DBG("starting port:%d", priv->port);

	/* Alloc and configure CQs, TX, RX */
	err = mtnic_alloc_resources(dev);
	if (err) {
		DBG("Error allocating resources\n");
		return MTNIC_ERROR;
	}

	/* Pass CQs configuration to HW */
        for (cq_ind = 0; cq_ind < NUM_CQS; ++cq_ind) {
		cq = &priv->cq[cq_ind];
		err = mtnic_CONFIG_CQ(priv, priv->port, cq_ind, cq);
		if (err) {
			DBG("Failed configuring CQ:%d error %d\n",
			    cq_ind, err);
			if (cq_ind)
				goto cq_error;
			else
				return MTNIC_ERROR;
                }
		/* Update consumer index */
		cq->db->update_ci = cpu_to_be32(cq->last & 0xffffff);
	}


	/* Pass Tx configuration to HW */
	ring = &priv->tx_ring;
        err = mtnic_CONFIG_TX_RING(priv, priv->port, 0, ring);
	if (err) {
		DBG("Failed configuring Tx ring:0\n");
                goto cq_error;
	}

	/* Pass RX configuration to HW */
        ring = &priv->rx_ring;
        err = mtnic_CONFIG_RX_RING(priv, priv->port, 0, ring);
	if (err) {
		DBG("Failed configuring Rx ring:0\n");
		goto tx_error;
	}

	/* Configure Rx steering */
	err = mtnic_CONFIG_PORT_RSS_STEER(priv, priv->port);
	if (!err)
		err = mtnic_SET_PORT_RSS_INDIRECTION(priv, priv->port);
	if (err) {
		DBG("Failed configuring RSS steering\n");
		goto rx_error;
	}

	/* Set the port default ring to ring 0 */
	err = mtnic_SET_PORT_DEFAULT_RING(priv, priv->port, 0);
	if (err) {
		DBG("Failed setting default ring\n");
		goto rx_error;
	}

	/* Set Mac address */
	err = mtnic_SET_RX_RING_ADDR(priv, priv->port, &priv->fw.mac[priv->port]);
	if (err) {
		DBG("Failed setting default MAC address\n");
		goto rx_error;
	}

	/* Set MTU  */
	err = mtnic_SET_PORT_MTU(priv, priv->port, DEF_MTU);
	if (err) {
		DBG("Failed setting MTU\n");
		goto rx_error;
	}

	/* Configure VLAN filter */
	err = mtnic_CONFIG_PORT_VLAN_FILTER(priv, priv->port);
        if (err) {
		DBG("Failed configuring VLAN filter\n");
		goto rx_error;
	}

	/* Bring up physical link */
	err = mtnic_SET_PORT_STATE(priv, priv->port, 1);
	if (err) {
		DBG("Failed bringing up port\n");
		goto rx_error;
	}
	mdelay(300); /* Let link state stabilize if cable was connected */

	priv->state = CARD_UP;

	err = mtnic_HEART_BEAT(priv, &dev_link_state);
	if (err) {
		DBG("Failed getting device link state\n");
		return MTNIC_ERROR;
	}
	if (!(dev_link_state & 0x3)) {
		DBG("Link down, check cables and restart\n");
		return MTNIC_ERROR;
	}

	return 0;


rx_error:
	err = mtnic_RELEASE_RESOURCE(priv, priv->port,
				      MTNIC_IF_RESOURCE_TYPE_RX_RING, 0);
tx_error:
	err |= mtnic_RELEASE_RESOURCE(priv, priv->port,
				      MTNIC_IF_RESOURCE_TYPE_TX_RING, 0);
cq_error:
	while (cq_ind) {
		err |= mtnic_RELEASE_RESOURCE(priv, priv->port,
					      MTNIC_IF_RESOURCE_TYPE_CQ, --cq_ind);
	}
	if (err)
		DBG("Eror Releasing resources\n");

	return MTNIC_ERROR;
}



/** Check if we got completion for receive and transmit and
 * check the line with heart_bit command */
static void
mtnic_poll(struct net_device *dev)
{
	struct mtnic_priv *priv = netdev_priv(dev);
	struct mtnic_cq *cq;
	u32 dev_link_state;
	int err;
	unsigned int i;

        /* In case of an old error then return */
	if (priv->state != CARD_UP)
		return;

	/* We do not check the device every call _poll call,
		since it will slow it down */
	if ((priv->poll_counter % ROUND_TO_CHECK) == 0) {
		/* Check device */
		err = mtnic_HEART_BEAT(priv, &dev_link_state);
		if (err) {
			DBG("Device has internal error\n");
			priv->state = CARD_DOWN;
			return;
		}
		if (!(dev_link_state & 0x3)) {
			DBG("Link down, check cables and restart\n");
			priv->state = CARD_DOWN;
			return;
		}
	}

	/* Polling CQ */
	for (i = 0; i < NUM_CQS; i++) {
		cq = &priv->cq[i]; //Passing on the 2 cqs.

		if (cq->is_rx) {
                        err = mtnic_process_rx_cq(priv, cq->dev, cq);
			if (err) {
				priv->state = CARD_DOWN;
				DBG(" Error allocating RX buffers\n");
				return;
			}
                } else {
                        mtnic_process_tx_cq(priv, cq->dev, cq);
		}
	}
	++ priv->poll_counter;
}

static int
mtnic_transmit( struct net_device *dev, struct io_buffer *iobuf )
{

	struct mtnic_priv *priv = netdev_priv(dev);
	struct mtnic_ring *ring;
	struct mtnic_tx_desc *tx_desc;
	struct mtnic_data_seg *data;
	u32 index;

	/* In case of an error then return */
	if (priv->state != CARD_UP)
		return MTNIC_ERROR;

	ring = &priv->tx_ring;

        index = ring->prod & ring->size_mask;
	if ((ring->prod - ring->cons) >= ring->size) {
		DBG("No space left for descriptors!!! cons: %lx prod: %lx\n",
		    ring->cons, ring->prod);
		mdelay(5);
		return MTNIC_ERROR;/* no space left */
	}

        /* get current descriptor */
	tx_desc = ring->buf + (index * sizeof(struct mtnic_tx_desc));

	/* Prepare ctrl segement */
	tx_desc->ctrl.size_vlan = cpu_to_be32(2);
        tx_desc->ctrl.flags = cpu_to_be32(MTNIC_BIT_TX_COMP |
					  MTNIC_BIT_NO_ICRC);
        tx_desc->ctrl.op_own = cpu_to_be32(MTNIC_OPCODE_SEND) |
			       ((ring->prod & ring->size) ?
			       cpu_to_be32(MTNIC_BIT_DESC_OWN) : 0);

	/* Prepare Data Seg */
	data = &tx_desc->data;
	data->addr_l = cpu_to_be32((u32)virt_to_bus(iobuf->data));
	data->count = cpu_to_be32(iob_len(iobuf));
	data->mem_type = priv->fw.mem_type_snoop_be;

	/* Attach io_buffer */
	ring->iobuf[index] = iobuf;

	/* Update producer index */
	++ring->prod;

	/* Ring doorbell! */
	wmb();
	writel((u32) ring->db_offset, &ring->txcq_db->send_db);

	return 0;
}


static void
mtnic_close(struct net_device *dev)
{
	struct mtnic_priv *priv = netdev_priv(dev);
	int err = 0;
	DBG("Close called for port:%d\n", priv->port);

	if (priv->state == CARD_UP) {
		/* Disable port */
		err |= mtnic_SET_PORT_STATE(priv, priv->port, 0);
		/*
		 * Stop HW associated with this port
		 */
		mdelay(5);

		/* Stop RX */
		err |= mtnic_RELEASE_RESOURCE(priv, priv->port,
					      MTNIC_IF_RESOURCE_TYPE_RX_RING, 0);

		/* Stop TX */
		err |= mtnic_RELEASE_RESOURCE(priv, priv->port,
					      MTNIC_IF_RESOURCE_TYPE_TX_RING, 0);

		/* Stop CQs */
		err |= mtnic_RELEASE_RESOURCE(priv, priv->port,
						      MTNIC_IF_RESOURCE_TYPE_CQ, 0);
		err |= mtnic_RELEASE_RESOURCE(priv, priv->port,
						      MTNIC_IF_RESOURCE_TYPE_CQ, 1);
		if (err) {
			DBG("Close reported error %d", err);
		}

                /* Free memory */
                free(priv->tx_ring.buf);
		iounmap(priv->tx_ring.txcq_db);
                free(priv->cq[1].buf);
                free(priv->cq[1].db);

		/* Free RX buffers */
		mtnic_free_io_buffers(&priv->rx_ring);

		free(priv->rx_ring.buf);
		free(priv->rx_ring.db);
                free(priv->cq[0].buf);
		free(priv->cq[0].db);

		priv->state = CARD_INITIALIZED;

	}


}


static void
mtnic_disable(struct pci_device *pci)
{

	int err;
        struct net_device *dev = pci_get_drvdata(pci);
	struct mtnic_priv *priv = netdev_priv(dev);

	/* Should NOT happen! but just in case */
	if (priv->state == CARD_UP)
		mtnic_close(dev);

	if (priv->state == CARD_INITIALIZED) {
                err = mtnic_RELEASE_RESOURCE(priv, 0,
					     MTNIC_IF_RESOURCE_TYPE_EQ, 0);
                DBG("Calling MTNIC_CLOSE command\n");
		err |= mtnic_cmd(priv, NULL, NULL, 0,
					MTNIC_IF_CMD_CLOSE_NIC);
		if (err) {
			DBG("Error Releasing resources %d\n", err);
		}

                free(priv->cmd.buf);
		iounmap(priv->hcr);
                ufree((u32)priv->fw.fw_pages.buf);
		ufree((u32)priv->fw.extra_pages.buf);
                free(priv->eq.buf);
		iounmap(priv->eq_db);
                priv->state = CARD_DOWN;
	}

	unregister_netdev(dev);
	netdev_nullify(dev);
	netdev_put(dev);
}



static void
mtnic_irq(struct net_device *netdev __unused, int enable __unused)
{
	/* Not implemented */
}



/** mtnic net device operations */
static struct net_device_operations mtnic_operations = {
	.open		= mtnic_open,
	.close		= mtnic_close,
	.transmit	= mtnic_transmit,
	.poll		= mtnic_poll,
	.irq		= mtnic_irq,
};







static int
mtnic_probe(struct pci_device *pci,
	      const struct pci_device_id *id __unused)
{
	struct net_device *dev;
	struct mtnic_priv *priv;
	int err;
	u64 mac;
	u32 result = 0;
	void *dev_id;
	int i;

        adjust_pci_device(pci);

        err = mtnic_init_pci(pci);
	if (err) {
		DBG("Error in pci_init\n");
		return MTNIC_ERROR;
	}

	mtnic_reset();
        mdelay(1000);

        err = restore_config();
	if (err) {
		DBG("Error restoring config\n");
		return err;
	}

	/* Checking MTNIC device ID */
	dev_id = ioremap(mtnic_pci_dev.dev.bar[0] +
			       MTNIC_DEVICE_ID_OFFSET, 4);
	result = ntohl(readl(dev_id));
	iounmap(dev_id);
        if (result != MTNIC_DEVICE_ID) {
		DBG("Wrong Devie ID (0x%lx) !!!", result);
		return MTNIC_ERROR;
	}

	/* Initializing net device */
        dev = alloc_etherdev(sizeof(struct mtnic_priv));
	if (dev == NULL) {
		DBG("Net device allocation failed\n");
		return MTNIC_ERROR;
	}
	/*
	 * Initialize driver private data
	 */
	priv = netdev_priv(dev);
        memset(priv, 0, sizeof(struct mtnic_priv));
	priv->dev = dev;
	priv->pdev = pci;
	priv->dev->dev = &pci->dev;
	/* Attach pci device */
	pci_set_drvdata(pci, priv->dev);
        netdev_init(dev, &mtnic_operations);


	/* Initialize hardware */
	err = mtnic_init_card(dev);
	if (err) {
		DBG("Error in init_card\n");
		return MTNIC_ERROR;
	}

	/* Program the MAC address */
	mac = priv->fw.mac[priv->port];
	printf("Port %d  Mac address: 0x%12llx\n", MTNIC_PORT_NUM + 1, mac);
	for (i = 0;i < MAC_ADDRESS_SIZE; ++i) {
		dev->ll_addr[MAC_ADDRESS_SIZE - i - 1] = mac & 0xFF;
                mac = mac >> 8;
	}

	/* Mark as link up; we don't yet handle link state */
	netdev_link_up ( dev );

	if (register_netdev(dev)) {
		DBG("Netdev registration failed\n");
		return MTNIC_ERROR;
	}


	return 0;
}






static struct pci_device_id mtnic_nics[] = {
	PCI_ROM(0x15b3, 0x6368, "mtnic", "Mellanox MTNIC driver"),
};

struct pci_driver mtnic_driver __pci_driver = {
	.ids = mtnic_nics,
	.id_count = sizeof(mtnic_nics) / sizeof(mtnic_nics[0]),
	.probe = mtnic_probe,
	.remove = mtnic_disable,
};

