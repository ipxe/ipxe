/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Bochs Pseudo NIC driver for Etherboot
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * See pnic_api.h for an explanation of the Bochs Pseudo NIC.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"

/* PNIC API */
#include "pnic_api.h"

/* Private data structure */
typedef struct {
	uint16_t api_version;
} pnic_priv_data_t;

/* Function prototypes */
static int pnic_api_check ( uint16_t api_version );

/* NIC specific static variables go here */
static uint8_t tx_buffer[ETH_FRAME_LEN];

/* 
 * Utility functions: issue a PNIC command, retrieve result.  Use
 * pnic_command_quiet if you don't want failure codes to be
 * automatically printed.  Returns the PNIC status code.
 * 
 * Set output_length to NULL only if you expect to receive exactly
 * output_max_length bytes, otherwise it'll complain that you didn't
 * get enough data (on the assumption that if you not interested in
 * discovering the output length then you're expecting a fixed amount
 * of data).
 */

static uint16_t pnic_command_quiet ( struct nic *nic, uint16_t command,
				     void *input, uint16_t input_length,
				     void *output, uint16_t output_max_length,
				     uint16_t *output_length ) {
	int i;
	uint16_t status;
	uint16_t _output_length;

	if ( input != NULL ) {
		/* Write input length */
		outw ( input_length, nic->ioaddr + PNIC_REG_LEN );
		/* Write input data */
		for ( i = 0; i < input_length; i++ ) {
			outb( ((char*)input)[i], nic->ioaddr + PNIC_REG_DATA );
		}
	}
	/* Write command */
	outw ( command, nic->ioaddr + PNIC_REG_CMD );
	/* Retrieve status */
	status = inw ( nic->ioaddr + PNIC_REG_STAT );
	/* Retrieve output length */
	_output_length = inw ( nic->ioaddr + PNIC_REG_LEN );
	if ( output_length == NULL ) {
		if ( _output_length != output_max_length ) {
			printf ( "pnic_command %#hx: wrong data length "
				 "returned (expected %d, got %d)\n", command,
				 output_max_length, _output_length );
		}
	} else {
		*output_length = _output_length;
	}
	if ( output != NULL ) {
		if ( _output_length > output_max_length ) {
			printf ( "pnic_command %#hx: output buffer too small "
				 "(have %d, need %d)\n", command,
				 output_max_length, _output_length );
			_output_length = output_max_length;
		}
		/* Retrieve output data */
		for ( i = 0; i < _output_length; i++ ) {
			((char*)output)[i] =
				inb ( nic->ioaddr + PNIC_REG_DATA );
		}
	}
	return status;
}

static uint16_t pnic_command ( struct nic *nic, uint16_t command,
			       void *input, uint16_t input_length,
			       void *output, uint16_t output_max_length,
			       uint16_t *output_length ) {
	pnic_priv_data_t *priv = (pnic_priv_data_t*)nic->priv_data;
	uint16_t status = pnic_command_quiet ( nic, command,
					       input, input_length,
					       output, output_max_length,
					       output_length );
	if ( status == PNIC_STATUS_OK ) return status;
	printf ( "PNIC command %#hx (len %#hx) failed with status %#hx\n",
		 command, input_length, status );
	if ( priv->api_version ) pnic_api_check(priv->api_version);
	return status;
}

/* Check API version matches that of NIC */
static int pnic_api_check ( uint16_t api_version ) {
	if ( api_version != PNIC_API_VERSION ) {
		printf ( "Warning: API version mismatch! "
			 "(NIC's is %d.%d, ours is %d.%d)\n",
			 api_version >> 8, api_version & 0xff,
			 PNIC_API_VERSION >> 8, PNIC_API_VERSION & 0xff );
	}
	if ( api_version < PNIC_API_VERSION ) {
		printf ( "*** You may need to update your copy of Bochs ***\n" );
	}
	return ( api_version == PNIC_API_VERSION );
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int pnic_poll(struct nic *nic, int retrieve)
{
	uint16_t length;
	uint16_t qlen;

	/* Check receive queue length to see if there's anything to
	 * get.  Necessary since once we've called PNIC_CMD_RECV we
	 * have to read out the packet, otherwise it's lost forever.
	 */
	if ( pnic_command ( nic, PNIC_CMD_RECV_QLEN, NULL, 0,
			    &qlen, sizeof(qlen), NULL )
	     != PNIC_STATUS_OK ) return ( 0 );
	if ( qlen == 0 ) return ( 0 );

	/* There is a packet ready.  Return 1 if we're only checking. */
	if ( ! retrieve ) return ( 1 );

	/* Retrieve the packet */
	if ( pnic_command ( nic, PNIC_CMD_RECV, NULL, 0,
			    nic->packet, ETH_FRAME_LEN, &length )
	     != PNIC_STATUS_OK ) return ( 0 );
	nic->packetlen = length;
	return ( 1 );
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void pnic_transmit(
	struct nic *nic,
	const char *dest,		/* Destination */
	unsigned int type,		/* Type */
	unsigned int size,		/* size */
	const char *data)		/* Packet */
{
	unsigned int nstype = htons ( type );

	if ( ( ETH_HLEN + size ) >= ETH_FRAME_LEN ) {
		printf ( "pnic_transmit: packet too large\n" );
		return;
	}

	/* Assemble packet */
	memcpy ( tx_buffer, dest, ETH_ALEN );
	memcpy ( tx_buffer + ETH_ALEN, nic->node_addr, ETH_ALEN );
	memcpy ( tx_buffer + 2 * ETH_ALEN, &nstype, 2 );
	memcpy ( tx_buffer + ETH_HLEN, data, size );

	pnic_command ( nic, PNIC_CMD_XMIT, tx_buffer, ETH_HLEN + size,
		       NULL, 0, NULL );
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void pnic_disable(struct dev *dev)
{
	struct nic *nic = (struct nic *)dev;
	pnic_command ( nic, PNIC_CMD_RESET, NULL, 0, NULL, 0, NULL );
}

/**************************************************************************
IRQ - Handle card interrupt status
***************************************************************************/
static void pnic_irq ( struct nic *nic, irq_action_t action )
{
	uint8_t enabled;

	switch ( action ) {
	case DISABLE :
	case ENABLE :
		enabled = ( action == ENABLE ? 1 : 0 );
		pnic_command ( nic, PNIC_CMD_MASK_IRQ,
			       &enabled, sizeof(enabled), NULL, 0, NULL );
		break;
	case FORCE :
		pnic_command ( nic, PNIC_CMD_FORCE_IRQ,
			       NULL, 0, NULL, 0, NULL );
		break;
	}
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

static int pnic_probe(struct dev *dev, struct pci_device *pci)
{
	struct nic *nic = (struct nic *)dev;
	static pnic_priv_data_t priv;
	uint16_t status;

	printf(" - ");

	/* Clear private data structure and chain it in */
	memset ( &priv, 0, sizeof(priv) );
	nic->priv_data = &priv;

	/* Mask the bit that says "this is an io addr" */
	nic->ioaddr = pci->ioaddr & ~3;
	nic->irqno = pci->irq;
	/* Not sure what this does, but the rtl8139 driver does it */
	adjust_pci_device(pci);

	status = pnic_command_quiet( nic, PNIC_CMD_API_VER, NULL, 0,
				     &priv.api_version,
				     sizeof(priv.api_version), NULL );
	if ( status != PNIC_STATUS_OK ) {
		printf ( "PNIC failed installation check, code %#hx\n",
			 status );
		return 0;
	}
	pnic_api_check(priv.api_version);
	status = pnic_command ( nic, PNIC_CMD_READ_MAC, NULL, 0,
				nic->node_addr, ETH_ALEN, NULL );
	printf ( "Detected Bochs Pseudo NIC MAC %! (API v%d.%d) at %#hx\n",
		 nic->node_addr, priv.api_version>>8, priv.api_version&0xff,
		 nic->ioaddr );

	/* point to NIC specific routines */
	dev->disable  = pnic_disable;
	nic->poll     = pnic_poll;
	nic->transmit = pnic_transmit;
	nic->irq      = pnic_irq;
	return 1;
}

static struct pci_id pnic_nics[] = {
/* genrules.pl doesn't let us use macros for PCI IDs...*/
PCI_ROM(0xfefe, 0xefef, "pnic", "Bochs Pseudo NIC Adaptor"),
};

static struct pci_driver pnic_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "PNIC",
	.probe    = pnic_probe,
	.ids      = pnic_nics,
	.id_count = sizeof(pnic_nics)/sizeof(pnic_nics[0]),
	.class    = 0,
};
