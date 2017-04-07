/**************************************************************************
 *
 * Driver datapath for Solarflare network cards
 *
 * Written by Shradha Shah <sshah@solarflare.com>
 *
 * Copyright 2012-2017 Solarflare Communications Inc.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/pci.h>
#include <ipxe/malloc.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include "efx_hunt.h"
#include "efx_bitfield.h"
#include "ef10_regs.h"

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

void efx_hunt_free_special_buffer(void *buf, int bytes)
{
	free_dma(buf, bytes);
}

static void *efx_hunt_alloc_special_buffer(int bytes,
					   struct efx_special_buffer *entry)
{
	void *buffer;
	dma_addr_t dma_addr;

	/* Allocate the buffer, aligned on a buffer address boundary.  This
	 * buffer will be passed into an MC_CMD_INIT_*Q command to setup the
	 * appropriate type of queue via MCDI.
	 */
	buffer = malloc_dma(bytes, EFX_BUF_ALIGN);
	if (!buffer)
		return NULL;

	entry->dma_addr = dma_addr = virt_to_bus(buffer);
	assert((dma_addr & (EFX_BUF_ALIGN - 1)) == 0);

	/* Buffer table entries aren't allocated, so set id to zero */
	entry->id = 0;
	DBGP("Allocated 0x%x bytes at %p\n", bytes, buffer);

	return buffer;
}

/*******************************************************************************
 *
 *
 * TX
 *
 *
 ******************************************************************************/
static void
efx_hunt_build_tx_desc(efx_tx_desc_t *txd, struct io_buffer *iob)
{
	dma_addr_t dma_addr;

	dma_addr = virt_to_bus(iob->data);

	EFX_POPULATE_QWORD_4(*txd,
			     ESF_DZ_TX_KER_TYPE, 0,
			     ESF_DZ_TX_KER_CONT, 0,
			     ESF_DZ_TX_KER_BYTE_CNT, iob_len(iob),
			     ESF_DZ_TX_KER_BUF_ADDR, dma_addr);
}

static void
efx_hunt_notify_tx_desc(struct efx_nic *efx)
{
	struct efx_tx_queue *txq = &efx->txq;
	int ptr = txq->write_ptr & EFX_TXD_MASK;
	efx_dword_t reg;

	EFX_POPULATE_DWORD_1(reg, ERF_DZ_TX_DESC_WPTR_DWORD, ptr);
	efx_writel_page(efx, &reg, 0, ER_DZ_TX_DESC_UPD_DWORD);
}

int
efx_hunt_transmit(struct net_device *netdev, struct io_buffer *iob)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct efx_tx_queue *txq = &efx->txq;
	int fill_level, space;
	efx_tx_desc_t *txd;
	int buf_id;

	fill_level = txq->write_ptr - txq->read_ptr;
	space = EFX_TXD_SIZE - fill_level - 1;
	if (space < 1)
		return -ENOBUFS;

	/* Save the iobuffer for later completion */
	buf_id = txq->write_ptr & EFX_TXD_MASK;
	assert(txq->buf[buf_id] == NULL);
	txq->buf[buf_id] = iob;

	DBGCIO(efx, "tx_buf[%d] for iob %p data %p len %zd\n",
	       buf_id, iob, iob->data, iob_len(iob));

	/* Form the descriptor, and push it to hardware */
	txd = txq->ring + buf_id;
	efx_hunt_build_tx_desc(txd, iob);
	++txq->write_ptr;
	efx_hunt_notify_tx_desc(efx);

	return 0;
}

static void
efx_hunt_transmit_done(struct efx_nic *efx, int id)
{
	struct efx_tx_queue *txq = &efx->txq;
	unsigned int read_ptr, stop;

	/* Complete all buffers from read_ptr up to and including id */
	read_ptr = txq->read_ptr & EFX_TXD_MASK;
	stop = (id + 1) & EFX_TXD_MASK;

	while (read_ptr != stop) {
		struct io_buffer *iob = txq->buf[read_ptr];

		assert(iob);
		/* Complete the tx buffer */
		if (iob)
			netdev_tx_complete(efx->netdev, iob);
		DBGCIO(efx, "tx_buf[%d] for iob %p done\n", read_ptr, iob);
		txq->buf[read_ptr] = NULL;

		++txq->read_ptr;
		read_ptr = txq->read_ptr & EFX_TXD_MASK;
	}
}

int efx_hunt_tx_init(struct net_device *netdev, dma_addr_t *dma_addr)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct efx_tx_queue *txq = &efx->txq;
	size_t bytes;

	/* Allocate hardware transmit queue */
	bytes = sizeof(efx_tx_desc_t) * EFX_TXD_SIZE;
	txq->ring = efx_hunt_alloc_special_buffer(bytes, &txq->entry);
	if (!txq->ring)
		return -ENOMEM;

	txq->read_ptr = txq->write_ptr = 0;
	*dma_addr = txq->entry.dma_addr;
	return 0;
}

/*******************************************************************************
 *
 *
 * RX
 *
 *
 ******************************************************************************/
static void
efx_hunt_build_rx_desc(efx_rx_desc_t *rxd, struct io_buffer *iob)
{
	dma_addr_t dma_addr = virt_to_bus(iob->data);

	EFX_POPULATE_QWORD_2(*rxd,
			     ESF_DZ_RX_KER_BYTE_CNT, EFX_RX_BUF_SIZE,
			     ESF_DZ_RX_KER_BUF_ADDR, dma_addr);
}

static void
efx_hunt_notify_rx_desc(struct efx_nic *efx)
{
	struct efx_rx_queue *rxq = &efx->rxq;
	int ptr = rxq->write_ptr & EFX_RXD_MASK;
	efx_dword_t reg;

	EFX_POPULATE_DWORD_1(reg, ERF_DZ_RX_DESC_WPTR, ptr);
	efx_writel_page(efx, &reg, 0, ER_DZ_RX_DESC_UPD);
}

static void
efx_hunt_rxq_fill(struct efx_nic *efx)
{
	struct efx_rx_queue *rxq = &efx->rxq;
	int fill_level = rxq->write_ptr - rxq->read_ptr;
	int space = EFX_NUM_RX_DESC - fill_level - 1;
	int pushed = 0;

	while (space) {
		int buf_id = rxq->write_ptr & (EFX_NUM_RX_DESC - 1);
		int desc_id = rxq->write_ptr & EFX_RXD_MASK;
		struct io_buffer *iob;
		efx_rx_desc_t *rxd;

		assert(rxq->buf[buf_id] == NULL);
		iob = alloc_iob(EFX_RX_BUF_SIZE);
		if (!iob)
			break;

		DBGCP(efx, "pushing rx_buf[%d] iob %p data %p\n",
		      buf_id, iob, iob->data);

		rxq->buf[buf_id] = iob;
		rxd = rxq->ring + desc_id;
		efx_hunt_build_rx_desc(rxd, iob);
		++rxq->write_ptr;
		++pushed;
		--space;
	}

	/* Push the ptr to hardware */
	if (pushed > 0) {
		efx_hunt_notify_rx_desc(efx);

		DBGCP(efx, "pushed %d rx buffers to fill level %d\n",
		      pushed, rxq->write_ptr - rxq->read_ptr);
	}
}

static void
efx_hunt_receive(struct efx_nic *efx, unsigned int id, int len, int drop)
{
	struct efx_rx_queue *rxq = &efx->rxq;
	unsigned int read_ptr = rxq->read_ptr & EFX_RXD_MASK;
	unsigned int buf_ptr = rxq->read_ptr & EFX_NUM_RX_DESC_MASK;
	struct io_buffer *iob;

	/* id is the lower 4 bits of the desc index + 1 in huntington*/
	/* hence anding with 15 */
	assert((id & 15) == ((read_ptr + (len != 0)) & 15));

	/* Pop this rx buffer out of the software ring */
	iob = rxq->buf[buf_ptr];
	rxq->buf[buf_ptr] = NULL;

	DBGCIO(efx, "popping rx_buf[%d] iob %p data %p with %d bytes %s %x\n",
	       read_ptr, iob, iob->data, len, drop ? "bad" : "ok", drop);

	/* Pass the packet up if required */
	if (drop)
		netdev_rx_err(efx->netdev, iob, EBADMSG);
	else {
		iob_put(iob, len);
		iob_pull(iob, efx->rx_prefix_size);
		netdev_rx(efx->netdev, iob);
	}

	++rxq->read_ptr;
}

int efx_hunt_rx_init(struct net_device *netdev, dma_addr_t *dma_addr)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct efx_rx_queue *rxq = &efx->rxq;
	size_t bytes;

	/* Allocate hardware receive queue */
	bytes = sizeof(efx_rx_desc_t) * EFX_RXD_SIZE;
	rxq->ring = efx_hunt_alloc_special_buffer(bytes, &rxq->entry);
	if (rxq->ring == NULL)
		return -ENOMEM;

	rxq->read_ptr = rxq->write_ptr = 0;
	*dma_addr = rxq->entry.dma_addr;
	return 0;
}

/*******************************************************************************
 *
 *
 * Event queues and interrupts
 *
 *
 ******************************************************************************/
int efx_hunt_ev_init(struct net_device *netdev, dma_addr_t *dma_addr)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct efx_ev_queue *evq = &efx->evq;
	size_t bytes;

	/* Allocate the hardware event queue */
	bytes = sizeof(efx_event_t) * EFX_EVQ_SIZE;
	evq->ring = efx_hunt_alloc_special_buffer(bytes, &evq->entry);
	if (evq->ring == NULL)
		return -ENOMEM;

	memset(evq->ring, 0xff, bytes);
	evq->read_ptr = 0;
	*dma_addr = evq->entry.dma_addr;
	return 0;
}

static void
efx_hunt_clear_interrupts(struct efx_nic *efx)
{
	efx_dword_t reg;
	/* read the ISR */
	efx_readl(efx, &reg, ER_DZ_BIU_INT_ISR);
}

/**
 * See if an event is present
 *
 * @v event            EFX event structure
 * @ret True           An event is pending
 * @ret False          No event is pending
 *
 * We check both the high and low dword of the event for all ones.  We
 * wrote all ones when we cleared the event, and no valid event can
 * have all ones in either its high or low dwords.  This approach is
 * robust against reordering.
 *
 * Note that using a single 64-bit comparison is incorrect; even
 * though the CPU read will be atomic, the DMA write may not be.
 */
static inline int
efx_hunt_event_present(efx_event_t *event)
{
	return (!(EFX_DWORD_IS_ALL_ONES(event->dword[0]) |
		  EFX_DWORD_IS_ALL_ONES(event->dword[1])));
}

static void
efx_hunt_evq_read_ack(struct efx_nic *efx)
{
	struct efx_ev_queue *evq = &efx->evq;
	efx_dword_t reg;

	if (efx->workaround_35388) {
		EFX_POPULATE_DWORD_2(reg, ERF_DD_EVQ_IND_RPTR_FLAGS,
				     EFE_DD_EVQ_IND_RPTR_FLAGS_HIGH,
				     ERF_DD_EVQ_IND_RPTR,
				    evq->read_ptr >> ERF_DD_EVQ_IND_RPTR_WIDTH);
		efx_writel_page(efx, &reg, 0, ER_DD_EVQ_INDIRECT);
		EFX_POPULATE_DWORD_2(reg, ERF_DD_EVQ_IND_RPTR_FLAGS,
				     EFE_DD_EVQ_IND_RPTR_FLAGS_LOW,
				     ERF_DD_EVQ_IND_RPTR, evq->read_ptr &
				     ((1 << ERF_DD_EVQ_IND_RPTR_WIDTH) - 1));
		efx_writel_page(efx, &reg, 0, ER_DD_EVQ_INDIRECT);
	} else {
		EFX_POPULATE_DWORD_1(reg, ERF_DZ_EVQ_RPTR, evq->read_ptr);
		efx_writel_table(efx, &reg, 0, ER_DZ_EVQ_RPTR);
	}
}

static unsigned int
efx_hunt_handle_event(struct efx_nic *efx, efx_event_t *evt)
{
	struct efx_rx_queue *rxq = &efx->rxq;
	int ev_code, desc_ptr, len;
	int next_ptr_lbits, packet_drop;
	int rx_cont;

	/* Decode event */
	ev_code = EFX_QWORD_FIELD(*evt, ESF_DZ_EV_CODE);

	switch (ev_code) {
	case ESE_DZ_EV_CODE_TX_EV:
		desc_ptr = EFX_QWORD_FIELD(*evt, ESF_DZ_TX_DESCR_INDX);
		efx_hunt_transmit_done(efx, desc_ptr);
		break;

	case ESE_DZ_EV_CODE_RX_EV:
		len = EFX_QWORD_FIELD(*evt, ESF_DZ_RX_BYTES);
		next_ptr_lbits = EFX_QWORD_FIELD(*evt, ESF_DZ_RX_DSC_PTR_LBITS);
		rx_cont = EFX_QWORD_FIELD(*evt, ESF_DZ_RX_CONT);

		/* We don't expect to receive scattered packets, so drop the
		 * packet if RX_CONT is set on the current or previous event, or
		 * if len is zero.
		 */
		packet_drop = (len == 0) | (rx_cont << 1) |
			      (rxq->rx_cont_prev << 2);
		efx_hunt_receive(efx, next_ptr_lbits, len, packet_drop);
		rxq->rx_cont_prev = rx_cont;
		return 1;

	default:
		DBGCP(efx, "Unknown event type %d\n", ev_code);
		break;
	}
	return 0;
}

void efx_hunt_poll(struct net_device *netdev)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct efx_ev_queue *evq = &efx->evq;
	efx_event_t *evt;
	int budget = 10;

	/* Read the event queue by directly looking for events
	 * (we don't even bother to read the eventq write ptr)
	 */
	evt = evq->ring + evq->read_ptr;
	while (efx_hunt_event_present(evt) && (budget > 0)) {
		DBGCP(efx, "Event at index 0x%x address %p is "
		      EFX_QWORD_FMT "\n", evq->read_ptr,
		      evt, EFX_QWORD_VAL(*evt));

		budget -= efx_hunt_handle_event(efx, evt);

		/* Clear the event */
		EFX_SET_QWORD(*evt);

		/* Move to the next event. We don't ack the event
		 * queue until the end
		 */
		evq->read_ptr = ((evq->read_ptr + 1) & EFX_EVQ_MASK);
		evt = evq->ring + evq->read_ptr;
	}

	/* Push more rx buffers if needed */
	efx_hunt_rxq_fill(efx);

	/* Clear any pending interrupts */
	efx_hunt_clear_interrupts(efx);

	/* Ack the event queue if interrupts are enabled */
	if (efx->int_en)
		efx_hunt_evq_read_ack(efx);
}

void efx_hunt_irq(struct net_device *netdev, int enable)
{
	struct efx_nic *efx = netdev_priv(netdev);

	efx->int_en = enable;

	/* If interrupts are enabled, prime the event queue.  Otherwise ack any
	 * pending interrupts
	 */
	if (enable)
		efx_hunt_evq_read_ack(efx);
	else if (efx->netdev->state & NETDEV_OPEN)
		efx_hunt_clear_interrupts(efx);
}

/*******************************************************************************
 *
 *
 * Initialization and Close
 *
 *
 ******************************************************************************/
int efx_hunt_open(struct net_device *netdev)
{
	struct efx_nic *efx = netdev_priv(netdev);
	efx_dword_t cmd;

	/* Set interrupt moderation to 0*/
	EFX_POPULATE_DWORD_2(cmd,
			     ERF_DZ_TC_TIMER_MODE, 0,
			     ERF_DZ_TC_TIMER_VAL, 0);
	efx_writel_page(efx, &cmd, 0, ER_DZ_EVQ_TMR);

	/* Ack the eventq */
	if (efx->int_en)
		efx_hunt_evq_read_ack(efx);

	/* Push receive buffers */
	efx_hunt_rxq_fill(efx);

	return 0;
}

void efx_hunt_close(struct net_device *netdev)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct efx_rx_queue *rxq = &efx->rxq;
	struct efx_tx_queue *txq = &efx->txq;
	int i;

	/* Complete outstanding descriptors */
	for (i = 0; i < EFX_NUM_RX_DESC; i++) {
		if (rxq->buf[i]) {
			free_iob(rxq->buf[i]);
			rxq->buf[i] = NULL;
		}
	}

	for (i = 0; i < EFX_TXD_SIZE; i++) {
		if (txq->buf[i]) {
			netdev_tx_complete(efx->netdev, txq->buf[i]);
			txq->buf[i] = NULL;
		}
	}

	/* Clear interrupts */
	efx_hunt_clear_interrupts(efx);
}
