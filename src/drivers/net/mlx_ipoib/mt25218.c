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

struct mlx_nic {
};

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"

#include "mt_version.c"
#include "mt25218_imp.c"


int prompt_key(int secs, unsigned char *ch_p)
{
	unsigned long tmo;
	unsigned char ch;

	for (tmo = currticks() + secs * TICKS_PER_SEC; currticks() < tmo;) {
		if (iskey()) {
			ch = getchar();
			/* toupper does not work ... */
			if (ch == 'v')
				ch = 'V';
			if (ch == 'i')
				ch = 'I';
			if ((ch=='V') || (ch=='I')) {
				*ch_p = ch;
				return 1;
			}
		}
	}

	return 0;
}

#if 0
/**************************************************************************
IRQ - handle interrupts
***************************************************************************/
static void mt25218_irq(struct nic *nic, irq_action_t action)
{
	/* This routine is somewhat optional.  Etherboot itself
	 * doesn't use interrupts, but they are required under some
	 * circumstances when we're acting as a PXE stack.
	 *
	 * If you don't implement this routine, the only effect will
	 * be that your driver cannot be used via Etherboot's UNDI
	 * API.  This won't affect programs that use only the UDP
	 * portion of the PXE API, such as pxelinux.
	 */

	if (0) {
		nic = NULL;
	}
	switch (action) {
	case DISABLE:
	case ENABLE:
		/* Set receive interrupt enabled/disabled state */
		/*
		   outb ( action == ENABLE ? IntrMaskEnabled : IntrMaskDisabled,
		   nic->ioaddr + IntrMaskRegister );
		 */
		break;
	case FORCE:
		/* Force NIC to generate a receive interrupt */
		/*
		   outb ( ForceInterrupt, nic->ioaddr + IntrForceRegister );
		 */
		break;
	}
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int mt25218_poll(struct nic *nic, int retrieve)
{
	/* Work out whether or not there's an ethernet packet ready to
	 * read.  Return 0 if not.
	 */
	/* 
	   if ( ! <packet_ready> ) return 0;
	 */

	/* retrieve==0 indicates that we are just checking for the
	 * presence of a packet but don't want to read it just yet.
	 */
	/*
	   if ( ! retrieve ) return 1;
	 */

	/* Copy data to nic->packet.  Data should include the
	 * link-layer header (dest MAC, source MAC, type).
	 * Store length of data in nic->packetlen.
	 * Return true to indicate a packet has been read.
	 */
	/* 
	   nic->packetlen = <packet_length>;
	   memcpy ( nic->packet, <packet_data>, <packet_length> );
	   return 1;
	 */
	unsigned int size;
	int rc;
	rc = poll_imp(nic, retrieve, &size);
	if (rc) {
		return 0;
	}

	if (size == 0) {
		return 0;
	}

	nic->packetlen = size;

	return 1;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void mt25218_transmit(struct nic *nic, const char *dest,	/* Destination */
			     unsigned int type,	/* Type */
			     unsigned int size,	/* size */
			     const char *packet)
{				/* Packet */
	int rc;

	/* Transmit packet to dest MAC address.  You will need to
	 * construct the link-layer header (dest MAC, source MAC,
	 * type).
	 */
	if (nic) {
		rc = transmit_imp(dest, type, packet, size);
		if (rc)
			eprintf("tranmit error");
	}
}
#endif

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

#warning "Broadcast address?"
static uint8_t ib_broadcast[IB_ALEN] = { 0xff, };


/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int mlx_transmit ( struct net_device *netdev,
			  struct io_buffer *iobuf ) {
	struct ibhdr *ibhdr = iobuf->data;

	( void ) netdev;

	iob_pull ( iobuf, sizeof ( *ibhdr ) );	

	if ( memcmp ( ibhdr->peer, ib_broadcast, IB_ALEN ) == 0 ) {
		printf ( "Sending broadcast packet\n" );
		return send_bcast_packet ( ntohs ( ibhdr->proto ),
					   iobuf->data, iob_len ( iobuf ) );
	} else {
		printf ( "Sending unicast packet\n" );
		return send_ucast_packet ( ibhdr->peer,
					   ntohs ( ibhdr->proto ),
					   iobuf->data, iob_len ( iobuf ) );
	}
}

/**
 * Handle TX completion
 *
 * @v netdev		Network device
 * @v cqe		Completion queue entry
 */
static void mlx_tx_complete ( struct net_device *netdev,
			      struct ib_cqe_st *cqe ) {
	netdev_tx_complete_next_err ( netdev,
				      ( cqe->is_error ? -EIO : 0 ) );
}

/**
 * Handle RX completion
 *
 * @v netdev		Network device
 * @v cqe		Completion queue entry
 */
static void mlx_rx_complete ( struct net_device *netdev,
			      struct ib_cqe_st *cqe ) {
	unsigned int len;
	struct io_buffer *iobuf;
	void *buf;

	/* Check for errors */
	if ( cqe->is_error ) {
		netdev_rx_err ( netdev, NULL, -EIO );
		return;
	}

	/* Allocate I/O buffer */
	len = cqe->count;
	iobuf = alloc_iob ( len );
	if ( ! iobuf ) {
		netdev_rx_err ( netdev, NULL, -ENOMEM );
		return;
	}
	buf = get_rcv_wqe_buf ( cqe->wqe, 1 );
	memcpy ( iob_put ( iobuf, len ), buf, len );
	//	DBG ( "Received packet header:\n" );
	//	struct recv_wqe_st *rcv_wqe = ib_cqe.wqe;
	//	DBG_HD ( get_rcv_wqe_buf(ib_cqe.wqe, 0),
	//		 be32_to_cpu(rcv_wqe->mpointer[0].byte_count) );
	//	DBG ( "Received packet:\n" );
	//	DBG_HD ( iobuf->data, iob_len ( iobuf ) );
	netdev_rx ( netdev, iobuf );
}

/**
 * Poll completion queue
 *
 * @v netdev		Network device
 * @v cq		Completion queue
 */
static void mlx_poll_cq ( struct net_device *netdev,
			  struct cq_st *cq ) {
	struct mlx_nic *mlx = netdev->priv;
	struct ib_cqe_st cqe;
	uint8_t num_cqes;

	while ( 1 ) {

		unsigned long cons_idx;
		union cqe_st *temp;

		cons_idx = ( cq->cons_counter & ( cq->num_cqes - 1 ) );
		temp = &cq->cq_buf[cons_idx];
		if ( EX_FLD_BE ( temp, arbelprm_completion_queue_entry_st,
				 owner ) == 0 ) {
			DBG ( "software owned\n" );
			DBGC_HD ( mlx, temp, sizeof ( *temp ) );
			DBG ( "my_qpn=%lx, g=%ld, s=%ld, op=%02lx, cnt=%lx\n",
			      EX_FLD_BE ( temp, arbelprm_completion_queue_entry_st, my_qpn ),
			      EX_FLD_BE ( temp, arbelprm_completion_queue_entry_st, g ),
			      EX_FLD_BE ( temp, arbelprm_completion_queue_entry_st, s ),
			      EX_FLD_BE ( temp, arbelprm_completion_queue_entry_st, opcode ),
			      EX_FLD_BE ( temp, arbelprm_completion_queue_entry_st, byte_cnt ) );
		}

		/* Poll for single completion queue entry */
		ib_poll_cq ( cq, &cqe, &num_cqes );

		/* Return if no entries in the queue */
		if ( ! num_cqes )
			return;

		DBGC ( mlx, "MLX %p cpl in %p: err %x send %x "
		       "wqe %p count %lx\n", mlx, cq, cqe.is_error,
		       cqe.is_send, cqe.wqe, cqe.count );

		/* Handle TX/RX completion */
		if ( cqe.is_send ) {
			mlx_tx_complete ( netdev, &cqe );
		} else {
			mlx_rx_complete ( netdev, &cqe );
		}
		
		/* Free associated work queue entry */
		free_wqe ( cqe.wqe );
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void mlx_poll ( struct net_device *netdev ) {
	int rc;

	if ( ( rc = poll_error_buf() ) != 0 ) {
		DBG ( "poll_error_buf() failed: %s\n", strerror ( rc ) );
		return;
	}

	if ( ( rc = drain_eq() ) != 0 ) {
		DBG ( "drain_eq() failed: %s\n", strerror ( rc ) );
		return;
	}

	//	mlx_poll_cq ( netdev, ipoib_data.snd_cqh );
	mlx_poll_cq ( netdev, ipoib_data.rcv_cqh );
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
	.transmit	= mlx_transmit,
	.poll		= mlx_poll,
	.irq		= mlx_irq,
};

#if 0
/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void mt25218_disable(struct nic *nic)
{
	/* put the card in its initial state */
	/* This function serves 3 purposes.
	 * This disables DMA and interrupts so we don't receive
	 *  unexpected packets or interrupts from the card after
	 *  etherboot has finished. 
	 * This frees resources so etherboot may use
	 *  this driver on another interface
	 * This allows etherboot to reinitialize the interface
	 *  if something is something goes wrong.
	 */
	if (nic || 1) {		// ????
		disable_imp();
	}
}
#endif

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void mlx_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );

	unregister_netdev ( netdev );
	ipoib_close(0);
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

#if 0
static struct nic_operations mt25218_operations = {
	.connect	= dummy_connect,
	.poll		= mt25218_poll,
	.transmit	= mt25218_transmit,
	.irq		= mt25218_irq,
};

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

static int mt25218_probe(struct nic *nic, struct pci_device *pci)
{
	int rc;
	unsigned char user_request;

	if (pci->vendor != MELLANOX_VENDOR_ID) {
		eprintf("");
		return 0;
	}

	printf("\n");
	printf("Mellanox Technologies LTD - Boot over IB implementaion\n");
	printf("Build version = %s\n\n", build_revision);

	verbose_messages = 0;
        print_info = 0;
	printf("Press within 3 seconds:\n");
	printf("V - to increase verbosity\n");
	printf("I - to print information\n");
	if (prompt_key(3, &user_request)) {
		if (user_request == 'V') {
			printf("User selected verbose messages\n");
			verbose_messages = 1;
		}
		else if (user_request == 'I') {
			printf("User selected to print information\n");
			print_info = 1;
		}
	}
	printf("\n");

	adjust_pci_device(pci);

	nic->priv_data = NULL;
	rc = probe_imp(pci, nic);

	/* give the user a chance to look at the info */
	if (print_info)
		sleep(5);

	if (!rc) {
		/* store NIC parameters */
		nic->ioaddr = pci->ioaddr & ~3;
		nic->irqno = pci->irq;
		/* point to NIC specific routines */
		nic->nic_op = &mt25218_operations;

		uint8_t fixed_node_addr[ETH_ALEN] = { 0x00, 0x02, 0xc9,
						      0x20, 0xf5, 0x95 };
		memcpy ( nic->node_addr, fixed_node_addr, ETH_ALEN );

		return 1;
	}
	/* else */
	return 0;
}
#endif

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @v id		PCI ID
 * @ret rc		Return status code
 */
static int mlx_probe ( struct pci_device *pci,
		       const struct pci_device_id *id __unused ) {
	struct net_device *netdev;
	struct mlx_nic *mlx;
	struct ib_mac *mac;
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
	if ( ( rc = ipoib_init ( pci ) ) != 0 )
		goto err_ipoib_init;
	mac = ( ( struct ib_mac * ) netdev->ll_addr );
	mac->qpn = htonl ( ipoib_data.ipoib_qpn );
	memcpy ( &mac->gid, ipoib_data.port_gid_raw, sizeof ( mac->gid ) );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	return 0;

 err_register_netdev:
 err_ipoib_init:
	ipoib_close(0);
	netdev_nullify ( netdev );
	netdev_put ( netdev );
	return rc;
}

static struct pci_device_id mlx_nics[] = {
	PCI_ROM(0x15b3, 0x6282, "MT25218", "MT25218 HCA driver"),
	PCI_ROM(0x15b3, 0x6274, "MT25204", "MT25204 HCA driver"),
};

struct pci_driver mlx_driver __pci_driver = {
	.ids = mlx_nics,
	.id_count = ( sizeof ( mlx_nics ) / sizeof ( mlx_nics[0] ) ),
	.probe = mlx_probe,
	.remove = mlx_remove,
};
