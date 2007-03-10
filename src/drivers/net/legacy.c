#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/ethernet.h>
#include <gpxe/pkbuff.h>
#include <nic.h>

/*
 * Quick and dirty compatibility layer
 *
 * This should allow old-API PCI drivers to at least function until
 * they are updated.  It will not help non-PCI drivers.
 *
 * No drivers should rely on this code.  It will be removed asap.
 *
 */

struct nic nic;

static int legacy_registered = 0;

static int legacy_transmit ( struct net_device *netdev, struct pk_buff *pkb ) {
	struct nic *nic = netdev->priv;
	struct ethhdr *ethhdr = pkb->data;

	DBG ( "Transmitting %d bytes\n", pkb_len ( pkb ) );
	pkb_pad ( pkb, ETH_ZLEN );
	pkb_pull ( pkb, sizeof ( *ethhdr ) );
	nic->nic_op->transmit ( nic, ( const char * ) ethhdr->h_dest,
				ntohs ( ethhdr->h_protocol ),
				pkb_len ( pkb ), pkb->data );
	netdev_tx_complete ( netdev, pkb );
	return 0;
}

static void legacy_poll ( struct net_device *netdev, unsigned int rx_quota ) {
	struct nic *nic = netdev->priv;
	struct pk_buff *pkb;

	if ( ! rx_quota )
		return;

	pkb = alloc_pkb ( ETH_FRAME_LEN );
	if ( ! pkb )
		return;

	nic->packet = pkb->data;
	if ( nic->nic_op->poll ( nic, 1 ) ) {
		DBG ( "Received %d bytes\n", nic->packetlen );
		pkb_put ( pkb, nic->packetlen );
		netdev_rx ( netdev, pkb );
	} else {
		free_pkb ( pkb );
	}
}

static int legacy_open ( struct net_device *netdev __unused ) {
	return 0;
}

static void legacy_close ( struct net_device *netdev __unused ) {
	/* Nothing to do */
}

int legacy_probe ( void *hwdev,
		   void ( * set_drvdata ) ( void *hwdev, void *priv ),
		   struct device *dev,
		   int ( * probe ) ( struct nic *nic, void *hwdev ),
		   void ( * disable ) ( struct nic *nic, void *hwdev ) ) {
	struct net_device *netdev;
	int rc;

	if ( legacy_registered )
		return -EBUSY;
	
	netdev = alloc_etherdev ( 0 );
	if ( ! netdev )
		return -ENOMEM;
	netdev->priv = &nic;
	memset ( &nic, 0, sizeof ( nic ) );
	set_drvdata ( hwdev, netdev );
	netdev->dev = dev;

	netdev->open = legacy_open;
	netdev->close = legacy_close;
	netdev->transmit = legacy_transmit;
	netdev->poll = legacy_poll;
	nic.node_addr = netdev->ll_addr;

	if ( ! probe ( &nic, hwdev ) ) {
		free_netdev ( netdev );
		return -ENODEV;
	}

	if ( ( rc = register_netdev ( netdev ) ) != 0 ) {
		disable ( &nic, hwdev );
		free_netdev ( netdev );
		return rc;
	}

	/* Do not remove this message */
	printf ( "WARNING: Using legacy NIC wrapper on %s\n",
		 ethernet_protocol.ntoa ( nic.node_addr ) );

	legacy_registered = 1;
	return 0;
}

void legacy_remove ( void *hwdev,
		     void * ( * get_drvdata ) ( void *hwdev ),
		     void ( * disable ) ( struct nic *nic, void *hwdev ) ) {
	struct net_device *netdev = get_drvdata ( hwdev );
	struct nic *nic = netdev->priv;

	unregister_netdev ( netdev );
	disable ( nic, hwdev );
	free_netdev ( netdev );
	legacy_registered = 0;
}

int dummy_connect ( struct nic *nic __unused ) {
	return 1;
}

void dummy_irq ( struct nic *nic __unused, irq_action_t irq_action __unused ) {
	return;
}
