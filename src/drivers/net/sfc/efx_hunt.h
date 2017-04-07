/**************************************************************************
 *
 * GPL net driver for Solarflare network cards
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

#ifndef EFX_HUNT_H
#define EFX_HUNT_H

#include "efx_common.h"

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**************************************************************************
 *
 * Hardware data structures and sizing
 *
 ***************************************************************************/

#define EFX_EV_SIZE(_nevs)     ((_nevs) * sizeof(efx_qword_t))
#define EFX_EVQ_NBUFS(_nevs)    (EFX_EV_SIZE(_nevs) / EFX_BUF_ALIGN)

#define	EFX_RXQ_SIZE(_ndescs)	((_ndescs) * sizeof(efx_qword_t))
#define	EFX_RXQ_NBUFS(_ndescs)	(EFX_RXQ_SIZE(_ndescs) / EFX_BUF_ALIGN)

#define	EFX_TXQ_SIZE(_ndescs)	((_ndescs) * sizeof(efx_qword_t))
#define	EFX_TXQ_NBUFS(_ndescs)	(EFX_TXQ_SIZE(_ndescs) / EFX_BUF_ALIGN)

/** MCDI request structure */
struct efx_mcdi_req_s {
	unsigned int    emr_cmd;
	efx_dword_t     *emr_in_buf;
	size_t          emr_in_length;
	int             emr_rc;
	efx_dword_t     *emr_out_buf;
	size_t          emr_out_length;
	size_t          emr_out_length_used;
};

/*******************************************************************************
 *
 *
 * Hardware API
 *
 *
 ******************************************************************************/

extern void efx_hunt_free_special_buffer(void *buf, int bytes);

/* Data path entry points */
extern int efx_hunt_transmit(struct net_device *netdev, struct io_buffer *iob);
extern void efx_hunt_poll(struct net_device *netdev);
extern void efx_hunt_irq(struct net_device *netdev, int enable);

/* Initialisation */
extern int efx_hunt_ev_init(struct net_device *netdev, dma_addr_t *dma_addr);
extern int efx_hunt_rx_init(struct net_device *netdev, dma_addr_t *dma_addr);
extern int efx_hunt_tx_init(struct net_device *netdev, dma_addr_t *dma_addr);
extern int efx_hunt_open(struct net_device *netdev);
extern void efx_hunt_close(struct net_device *netdev);

#endif /* EFX_HUNT_H */
