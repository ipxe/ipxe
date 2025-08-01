
FILE_LICENCE ( GPL2_ONLY );

#include <mii.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include <ipxe/iobuf.h>
#include <ipxe/dma.h>
#include <ipxe/timer.h>
#include <ipxe/malloc.h>
#include <ipxe/if_ether.h>
#include <ipxe/ethernet.h>
#include <ipxe/netdevice.h>
#include "bnxt.h"
#include "bnxt_dbg.h"

static void bnxt_service_cq ( struct net_device *dev );
static void bnxt_tx_complete ( struct net_device *dev, u16 hw_idx );
static void bnxt_adv_cq_index ( struct bnxt *bp, u16 cnt );
static void bnxt_adv_cq_index ( struct bnxt *bp, u16 cnt );
static int bnxt_rx_complete ( struct net_device *dev, struct rx_pkt_cmpl *rx );
void bnxt_link_evt ( struct bnxt *bp, struct hwrm_async_event_cmpl *evt );

static struct pci_device_id bnxt_nics[] = {
	PCI_ROM( 0x14e4, 0x1604, "14e4-1604", "Broadcom BCM957454", 0 ),
	PCI_ROM( 0x14e4, 0x1605, "14e4-1605", "Broadcom BCM957454 RDMA", 0 ),
	PCI_ROM( 0x14e4, 0x1606, "14e4-1606", "Broadcom BCM957454 RDMA VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1607, "bcm957454-1607", "Broadcom BCM957454 HV VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1608, "bcm957454-1608", "Broadcom BCM957454 RDMA HV VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1609, "14e4-1609", "Broadcom BCM957454 VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1614, "14e4-1614", "Broadcom BCM957454", 0 ),
	PCI_ROM( 0x14e4, 0x16bd, "bcm95741x-16bd", "Broadcom BCM95741x RDMA_HV_VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x16c0, "14e4-16c0", "Broadcom BCM957417", 0 ),
	PCI_ROM( 0x14e4, 0x16c1, "14e4-16c1", "Broadcom BCM95741x VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x16c5, "bcm95741x-16c5", "Broadcom BCM95741x HV VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x16c8, "14e4-16c8", "Broadcom BCM957301", 0 ),
	PCI_ROM( 0x14e4, 0x16c9, "14e4-16c9", "Broadcom BCM957302", 0 ),
	PCI_ROM( 0x14e4, 0x16ca, "14e4-16ca", "Broadcom BCM957304", 0 ),
	PCI_ROM( 0x14e4, 0x16cc, "14e4-16cc", "Broadcom BCM957417 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16cd, "14e4-16cd", "Broadcom BCM958700", 0 ),
	PCI_ROM( 0x14e4, 0x16ce, "14e4-16ce", "Broadcom BCM957311", 0 ),
	PCI_ROM( 0x14e4, 0x16cf, "14e4-16cf", "Broadcom BCM957312", 0 ),
	PCI_ROM( 0x14e4, 0x16d0, "14e4-16d0", "Broadcom BCM957402", 0 ),
	PCI_ROM( 0x14e4, 0x16d1, "14e4-16d1", "Broadcom BCM957404", 0 ),
	PCI_ROM( 0x14e4, 0x16d2, "14e4-16d2", "Broadcom BCM957406", 0 ),
	PCI_ROM( 0x14e4, 0x16d4, "14e4-16d4", "Broadcom BCM957402 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16d5, "14e4-16d5", "Broadcom BCM957407", 0 ),
	PCI_ROM( 0x14e4, 0x16d6, "14e4-16d6", "Broadcom BCM957412", 0 ),
	PCI_ROM( 0x14e4, 0x16d7, "14e4-16d7", "Broadcom BCM957414", 0 ),
	PCI_ROM( 0x14e4, 0x16d8, "14e4-16d8", "Broadcom BCM957416", 0 ),
	PCI_ROM( 0x14e4, 0x16d9, "14e4-16d9", "Broadcom BCM957417", 0 ),
	PCI_ROM( 0x14e4, 0x16da, "14e4-16da", "Broadcom BCM957402", 0 ),
	PCI_ROM( 0x14e4, 0x16db, "14e4-16db", "Broadcom BCM957404", 0 ),
	PCI_ROM( 0x14e4, 0x16dc, "14e4-16dc", "Broadcom BCM95741x VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x16de, "14e4-16de", "Broadcom BCM957412 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16df, "14e4-16df", "Broadcom BCM957314", 0 ),
	PCI_ROM( 0x14e4, 0x16e0, "14e4-16e0", "Broadcom BCM957317", 0 ),
	PCI_ROM( 0x14e4, 0x16e2, "14e4-16e2", "Broadcom BCM957417", 0 ),
	PCI_ROM( 0x14e4, 0x16e3, "14e4-16e3", "Broadcom BCM957416", 0 ),
	PCI_ROM( 0x14e4, 0x16e4, "14e4-16e4", "Broadcom BCM957317", 0 ),
	PCI_ROM( 0x14e4, 0x16e7, "14e4-16e7", "Broadcom BCM957404 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16e8, "14e4-16e8", "Broadcom BCM957406 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16e9, "14e4-16e9", "Broadcom BCM957407", 0 ),
	PCI_ROM( 0x14e4, 0x16ea, "14e4-16ea", "Broadcom BCM957407 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16eb, "14e4-16eb", "Broadcom BCM957412 RDMA MF", 0 ),
	PCI_ROM( 0x14e4, 0x16ec, "14e4-16ec", "Broadcom BCM957414 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16ed, "14e4-16ed", "Broadcom BCM957414 RDMA MF", 0 ),
	PCI_ROM( 0x14e4, 0x16ee, "14e4-16ee", "Broadcom BCM957416 MF", 0 ),
	PCI_ROM( 0x14e4, 0x16ef, "14e4-16ef", "Broadcom BCM957416 RDMA MF", 0 ),
	PCI_ROM( 0x14e4, 0x16f0, "14e4-16f0", "Broadcom BCM957320", 0 ),
	PCI_ROM( 0x14e4, 0x16f1, "14e4-16f1", "Broadcom BCM957320", 0 ),
	PCI_ROM( 0x14e4, 0x1750, "14e4-1750", "Broadcom BCM957508", 0 ),
	PCI_ROM( 0x14e4, 0x1751, "14e4-1751", "Broadcom BCM957504", 0 ),
	PCI_ROM( 0x14e4, 0x1752, "14e4-1752", "Broadcom BCM957502", 0 ),
	PCI_ROM( 0x14e4, 0x1760, "14e4-1760", "Broadcom BCM957608", 0 ),
	PCI_ROM( 0x14e4, 0x1800, "14e4-1800", "Broadcom BCM957502 MF", 0 ),
	PCI_ROM( 0x14e4, 0x1801, "14e4-1801", "Broadcom BCM957504 MF", 0 ),
	PCI_ROM( 0x14e4, 0x1802, "14e4-1802", "Broadcom BCM957508 MF", 0 ),
	PCI_ROM( 0x14e4, 0x1803, "14e4-1803", "Broadcom BCM957502 RDMA MF", 0 ),
	PCI_ROM( 0x14e4, 0x1804, "14e4-1804", "Broadcom BCM957504 RDMA MF", 0 ),
	PCI_ROM( 0x14e4, 0x1805, "14e4-1805", "Broadcom BCM957508 RDMA MF", 0 ),
	PCI_ROM( 0x14e4, 0x1806, "14e4-1806", "Broadcom BCM9575xx VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1807, "14e4-1807", "Broadcom BCM9575xx RDMA VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1808, "14e4-1808", "Broadcom BCM9575xx HV VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1809, "14e4-1809", "Broadcom BCM9575xx RDMA HV VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x1819, "bcm95760x-1819", "Broadcom BCM95760x VF", BNXT_FLAG_PCI_VF ),
	PCI_ROM( 0x14e4, 0x181b, "bcm95760x-181b", "Broadcom BCM95760x HV VF", BNXT_FLAG_PCI_VF ),
};

/**
 * Check if Virtual Function
 */
u8 bnxt_is_pci_vf ( struct pci_device *pdev )
{
	if ( FLAG_TEST ( pdev->id->driver_data, BNXT_FLAG_PCI_VF ) ) {
		return 1;
	}
	return 0;
}

static void bnxt_down_pci ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	if ( bp->bar2 ) {
		iounmap ( bp->bar2 );
		bp->bar2 = NULL;
	}
	if ( bp->bar1 ) {
		iounmap ( bp->bar1 );
		bp->bar1 = NULL;
	}
	if ( bp->bar0 ) {
		iounmap ( bp->bar0 );
		bp->bar0 = NULL;
	}
}

static void *bnxt_pci_base ( struct pci_device *pdev, unsigned int reg )
{
	unsigned long reg_base, reg_size;

	reg_base = pci_bar_start ( pdev, reg );
	reg_size = pci_bar_size ( pdev, reg );
	return pci_ioremap ( pdev, reg_base, reg_size );
}

static int bnxt_get_pci_info ( struct bnxt *bp )
{
	u16 cmd_reg = 0;

	DBGP ( "%s\n", __func__ );
	/* Disable Interrupt */
	pci_read_config_word ( bp->pdev, PCI_COMMAND, &bp->cmd_reg );
	cmd_reg = bp->cmd_reg | PCI_COMMAND_INTX_DISABLE;
	pci_write_config_word ( bp->pdev, PCI_COMMAND, cmd_reg );
	pci_read_config_word ( bp->pdev, PCI_COMMAND, &cmd_reg );

	/* SSVID */
	pci_read_config_word ( bp->pdev,
			PCI_SUBSYSTEM_VENDOR_ID,
			&bp->subsystem_vendor );

	/* SSDID */
	pci_read_config_word ( bp->pdev,
			PCI_SUBSYSTEM_ID,
			&bp->subsystem_device );

	/* Function Number */
	pci_read_config_byte ( bp->pdev,
			PCICFG_ME_REGISTER,
			&bp->pf_num );

	/* Get Bar Address */
	bp->bar0 = bnxt_pci_base ( bp->pdev, PCI_BASE_ADDRESS_0 );
	bp->bar1 = bnxt_pci_base ( bp->pdev, PCI_BASE_ADDRESS_2 );
	bp->bar2 = bnxt_pci_base ( bp->pdev, PCI_BASE_ADDRESS_4 );

	/* Virtual function */
	bp->vf = bnxt_is_pci_vf ( bp->pdev );

	dbg_pci ( bp, __func__, cmd_reg );
	return STATUS_SUCCESS;
}

static int bnxt_get_device_address ( struct bnxt *bp )
{
	struct net_device *dev = bp->dev;

	DBGP ( "%s\n", __func__ );
	memcpy ( &dev->hw_addr[0], ( char * )&bp->mac_addr[0], ETH_ALEN );
	if ( !is_valid_ether_addr ( &dev->hw_addr[0] ) ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return -EINVAL;
	}

	return STATUS_SUCCESS;
}

static void bnxt_set_link ( struct bnxt *bp )
{
	if ( bp->link_status == STATUS_LINK_ACTIVE )
		netdev_link_up ( bp->dev );
	else
		netdev_link_down ( bp->dev );
}

static void dev_p5_db ( struct bnxt *bp, u32 idx, u32 xid, u32 flag )
{
	void *off;
	u64  val;

	if ( bp->vf )
		off = ( void * ) ( bp->bar1 + DB_OFFSET_VF );
	else
		off = ( void * ) ( bp->bar1 + DB_OFFSET_PF );

	val = ( ( u64 )DBC_MSG_XID ( xid, flag ) << 32 ) |
		( u64 )DBC_MSG_IDX ( idx );
	writeq ( val, off );
}

static void dev_p7_db ( struct bnxt *bp, u32 idx, u32 xid, u32 flag, u32 epoch, u32 toggle )
{
	void *off;
	u64  val;

	off = ( void * ) ( bp->bar1 );

	val = ( ( u64 )DBC_MSG_XID ( xid, flag ) << 32 ) |
	        ( u64 )DBC_MSG_IDX ( idx ) |
	        ( u64 )DBC_MSG_EPCH ( epoch ) |
	        ( u64 )DBC_MSG_TOGGLE ( toggle );
	writeq ( val, off );
}

static void bnxt_db_nq ( struct bnxt *bp )
{
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P7 ) )
		dev_p7_db ( bp, ( u32 )bp->nq.cons_id,
			 ( u32 )bp->nq_ring_id, DBC_DBC_TYPE_NQ_ARM,
                         ( u32 )bp->nq.epoch, 0 );
	else if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5 ) )
		dev_p5_db ( bp, ( u32 )bp->nq.cons_id,
			 ( u32 )bp->nq_ring_id, DBC_DBC_TYPE_NQ_ARM );
	else
		writel ( CMPL_DOORBELL_KEY_CMPL, ( bp->bar1 + 0 ) );
}

static void bnxt_db_cq ( struct bnxt *bp )
{
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P7 ) )
		dev_p7_db ( bp, ( u32 )bp->cq.cons_id,
			 ( u32 )bp->cq_ring_id, DBC_DBC_TYPE_CQ_ARMALL,
                         ( u32 )bp->cq.epoch, ( u32 )bp->nq.toggle );
	else if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5 ) )
		dev_p5_db ( bp, ( u32 )bp->cq.cons_id,
			 ( u32 )bp->cq_ring_id, DBC_DBC_TYPE_CQ_ARMALL );
	else
		writel ( CQ_DOORBELL_KEY_IDX ( bp->cq.cons_id ),
			( bp->bar1 + 0 ) );
}

static void bnxt_db_rx ( struct bnxt *bp, u32 idx )
{
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P7 ) )
		dev_p7_db ( bp, idx, ( u32 )bp->rx_ring_id, DBC_DBC_TYPE_SRQ,
                         ( u32 )bp->rx.epoch, 0 );
	else if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5 ) )
		dev_p5_db ( bp, idx, ( u32 )bp->rx_ring_id, DBC_DBC_TYPE_SRQ );
	else
		writel ( RX_DOORBELL_KEY_RX | idx, ( bp->bar1 + 0 ) );
}

static void bnxt_db_tx ( struct bnxt *bp, u32 idx )
{
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P7 ) )
		dev_p7_db ( bp, idx, ( u32 )bp->tx_ring_id, DBC_DBC_TYPE_SQ,
                         ( u32 )bp->tx.epoch, 0 );
	else if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5 ) )
		dev_p5_db ( bp, idx, ( u32 )bp->tx_ring_id, DBC_DBC_TYPE_SQ );
	else
		writel ( ( u32 ) ( TX_DOORBELL_KEY_TX | idx ),
			( bp->bar1 + 0 ) );
}

void bnxt_add_vlan ( struct io_buffer *iob, u16 vlan )
{
	char *src = ( char * )iob->data;
	u16 len = iob_len ( iob );

	memmove ( ( char * )&src[MAC_HDR_SIZE + VLAN_HDR_SIZE],
			 ( char * )&src[MAC_HDR_SIZE],
			 ( len - MAC_HDR_SIZE ) );

	* ( u16 * ) ( &src[MAC_HDR_SIZE] ) = BYTE_SWAP_S ( ETHERTYPE_VLAN );
	* ( u16 * ) ( &src[MAC_HDR_SIZE + 2] ) = BYTE_SWAP_S ( vlan );
	iob_put ( iob, VLAN_HDR_SIZE );
}

static u16 bnxt_get_pkt_vlan ( char *src )
{
	if ( * ( ( u16 * )&src[MAC_HDR_SIZE] ) == BYTE_SWAP_S ( ETHERTYPE_VLAN ) )
		return BYTE_SWAP_S ( * ( ( u16 * )&src[MAC_HDR_SIZE + 2] ) );
	return 0;
}

static inline u32 bnxt_tx_avail ( struct bnxt *bp )
{
	u32 avail;
	u32 use;

	barrier (  );
	avail = TX_AVAIL ( bp->tx.ring_cnt );
	use = TX_IN_USE ( bp->tx.prod_id, bp->tx.cons_id, bp->tx.ring_cnt );
	dbg_tx_avail ( bp, avail, use );
	return ( avail-use );
}

void bnxt_set_txq ( struct bnxt *bp, int entry, physaddr_t mapping, int len )
{
	struct tx_bd_short *prod_bd;

	prod_bd = ( struct tx_bd_short * )BD_NOW ( bp->tx.bd_virt,
			entry, sizeof ( struct tx_bd_short ) );
	if ( len < 512 )
		prod_bd->flags_type = TX_BD_SHORT_FLAGS_LHINT_LT512;
	else if ( len < 1024 )
		prod_bd->flags_type = TX_BD_SHORT_FLAGS_LHINT_LT1K;
	else if ( len < 2048 )
		prod_bd->flags_type = TX_BD_SHORT_FLAGS_LHINT_LT2K;
	else
		prod_bd->flags_type = TX_BD_SHORT_FLAGS_LHINT_GTE2K;
	prod_bd->flags_type |= TX_BD_FLAGS;
	prod_bd->dma      = mapping;
	prod_bd->len	  = len;
	prod_bd->opaque   = ( u32 )entry;
}

static void bnxt_tx_complete ( struct net_device *dev, u16 hw_idx )
{
	struct bnxt *bp = dev->priv;
	struct io_buffer *iob;

	iob = bp->tx.iob[hw_idx];
	dbg_tx_done ( iob->data, iob_len ( iob ), hw_idx );
	netdev_tx_complete ( dev, iob );
	bp->tx.cons_id = NEXT_IDX ( hw_idx, bp->tx.ring_cnt );
	bp->tx.cnt++;
	dump_tx_stat ( bp );
}

int bnxt_free_rx_iob ( struct bnxt *bp )
{
	unsigned int i;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_RX_IOB ) ) )
		return STATUS_SUCCESS;

	for ( i = 0; i < bp->rx.buf_cnt; i++ ) {
		if ( bp->rx.iob[i] ) {
			free_rx_iob ( bp->rx.iob[i] );
			bp->rx.iob[i] = NULL;
		}
	}
	bp->rx.iob_cnt = 0;

	FLAG_RESET ( bp->flag_hwrm, VALID_RX_IOB );
	return STATUS_SUCCESS;
}

static void bnxt_set_rx_desc ( u8 *buf, struct io_buffer *iob,
				u16 cid, u32 idx )
{
	struct rx_prod_pkt_bd *desc;
	u16 off = cid * sizeof ( struct rx_prod_pkt_bd );

	desc = ( struct rx_prod_pkt_bd * )&buf[off];
	desc->flags_type = RX_PROD_PKT_BD_TYPE_RX_PROD_PKT;
	desc->len   	 = MAX_ETHERNET_PACKET_BUFFER_SIZE;
	desc->opaque	 = idx;
	desc->dma        = iob_dma ( iob );
}

static int bnxt_alloc_rx_iob ( struct bnxt *bp, u16 cons_id, u16 iob_idx )
{
	struct io_buffer *iob;

	iob = alloc_rx_iob ( BNXT_RX_STD_DMA_SZ, bp->dma );
	if ( !iob ) {
		DBGP ( "- %s (  ): alloc_iob Failed\n", __func__ );
		return -ENOMEM;
	}

	dbg_alloc_rx_iob ( iob, iob_idx, cons_id );
	bnxt_set_rx_desc ( ( u8 * )bp->rx.bd_virt, iob, cons_id,
			( u32 ) iob_idx );
	bp->rx.iob[iob_idx] = iob;
	return 0;
}

int bnxt_post_rx_buffers ( struct bnxt *bp )
{
	u16 cons_id = ( bp->rx.cons_id % bp->rx.ring_cnt );
	u16 iob_idx;

	while ( bp->rx.iob_cnt < bp->rx.buf_cnt ) {
		iob_idx = ( cons_id % bp->rx.buf_cnt );
		if ( !bp->rx.iob[iob_idx] ) {
			if ( bnxt_alloc_rx_iob ( bp, cons_id, iob_idx ) < 0 ) {
				dbg_alloc_rx_iob_fail ( iob_idx, cons_id );
				break;
			}
		}
		cons_id = NEXT_IDX ( cons_id, bp->rx.ring_cnt );
		/* If the ring has wrapped, flip the epoch bit */
		if ( iob_idx > cons_id )
		        bp->rx.epoch ^= 1;
		bp->rx.iob_cnt++;
	}

	if ( cons_id != bp->rx.cons_id ) {
		dbg_rx_cid ( bp->rx.cons_id, cons_id );
		bp->rx.cons_id = cons_id;
		bnxt_db_rx ( bp, ( u32 )cons_id );
	}

	FLAG_SET ( bp->flag_hwrm, VALID_RX_IOB );
	return STATUS_SUCCESS;
}

u8 bnxt_rx_drop ( struct bnxt *bp, struct io_buffer *iob,
	struct rx_pkt_cmpl *rx_cmp,
	struct rx_pkt_cmpl_hi *rx_cmp_hi, u16 rx_len )
{
	struct rx_pkt_v3_cmpl *rx_cmp_v3 = ( struct rx_pkt_v3_cmpl * )rx_cmp;
	struct rx_pkt_v3_cmpl_hi  *rx_cmp_hi_v3 = ( struct rx_pkt_v3_cmpl_hi * )rx_cmp_hi;
	u8  *rx_buf = ( u8 * )iob->data;
	u16 err_flags;
	u8  ignore_chksum_err = 0;
	int i;

	if ( ( rx_cmp_v3->flags_type & RX_PKT_V3_CMPL_TYPE_MASK ) ==
	     RX_PKT_V3_CMPL_TYPE_RX_L2_V3 ) {
	        err_flags = rx_cmp_hi_v3->errors_v2 >> RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_SFT;
	} else
	        err_flags = rx_cmp_hi->errors_v2 >> RX_PKT_CMPL_ERRORS_BUFFER_ERROR_SFT;
	if ( rx_cmp_hi->errors_v2 == 0x20 || rx_cmp_hi->errors_v2 == 0x21 )
		ignore_chksum_err = 1;

	if ( err_flags && !ignore_chksum_err ) {
		bp->rx.drop_err++;
		return 1;
	}

	for ( i = 0; i < 6; i++ ) {
		if ( rx_buf[6 + i] != bp->mac_addr[i] )
			break;
	}

	/* Drop the loopback packets */
	if ( i == 6 ) {
		bp->rx.drop_lb++;
		return 2;
	}

	iob_put ( iob, rx_len );

	bp->rx.good++;
	return 0;
}

static void bnxt_adv_cq_index ( struct bnxt *bp, u16 cnt )
{
	u16 cons_id;

	cons_id = bp->cq.cons_id + cnt;
	if ( cons_id >= bp->cq.ring_cnt) {
		/* Toggle completion bit when the ring wraps. */
		bp->cq.completion_bit ^= 1;
		bp->cq.epoch ^= 1;
		cons_id = cons_id - bp->cq.ring_cnt;
	}
	bp->cq.cons_id = cons_id;
}

void bnxt_rx_process ( struct net_device *dev, struct bnxt *bp,
	struct rx_pkt_cmpl *rx_cmp, struct rx_pkt_cmpl_hi *rx_cmp_hi )
{
	u32 desc_idx = rx_cmp->opaque;
	struct io_buffer *iob = bp->rx.iob[desc_idx];
	u8 drop;

	dump_rx_bd ( rx_cmp, rx_cmp_hi, desc_idx );
	assert ( iob );
	drop = bnxt_rx_drop ( bp, iob, rx_cmp, rx_cmp_hi, rx_cmp->len );
	dbg_rxp ( iob->data, rx_cmp->len, drop );
	if ( drop )
		netdev_rx_err ( dev, iob, -EINVAL );
	else
		netdev_rx ( dev, iob );

	bp->rx.cnt++;
	bp->rx.iob[desc_idx] = NULL;
	bp->rx.iob_cnt--;
	bnxt_post_rx_buffers ( bp );
	bnxt_adv_cq_index ( bp, 2 ); /* Rx completion is 2 entries. */
	dbg_rx_stat ( bp );
}

static int bnxt_rx_complete ( struct net_device *dev,
		struct rx_pkt_cmpl *rx_cmp )
{
	struct bnxt *bp = dev->priv;
	struct rx_pkt_cmpl_hi *rx_cmp_hi;
	u8  cmpl_bit = bp->cq.completion_bit;

	if ( bp->cq.cons_id == ( bp->cq.ring_cnt - 1 ) ) {
		rx_cmp_hi = ( struct rx_pkt_cmpl_hi * ) CQ_DMA_ADDR ( bp );
		cmpl_bit ^= 0x1; /* Ring has wrapped. */
	} else
		rx_cmp_hi = ( struct rx_pkt_cmpl_hi * ) ( rx_cmp+1 );

	if ( ! ( ( rx_cmp_hi->errors_v2 & RX_PKT_CMPL_V2 ) ^ cmpl_bit ) ) {
		bnxt_rx_process ( dev, bp, rx_cmp, rx_cmp_hi );
		return SERVICE_NEXT_CQ_BD;
	} else
		return NO_MORE_CQ_BD_TO_SERVICE;
}

void bnxt_mm_init ( struct bnxt *bp, const char *func )
{
	DBGP ( "%s\n", __func__ );
	memset ( bp->hwrm_addr_req,  0, REQ_BUFFER_SIZE );
	memset ( bp->hwrm_addr_resp, 0, RESP_BUFFER_SIZE );
	memset ( bp->hwrm_addr_dma,  0, DMA_BUFFER_SIZE );
	memset ( bp->tx.bd_virt,  0, TX_RING_BUFFER_SIZE );
	memset ( bp->rx.bd_virt,  0, RX_RING_BUFFER_SIZE );
	memset ( bp->cq.bd_virt,  0, CQ_RING_BUFFER_SIZE );
	memset ( bp->nq.bd_virt,  0, NQ_RING_BUFFER_SIZE );

	bp->link_status = STATUS_LINK_DOWN;
	bp->wait_link_timeout = LINK_DEFAULT_TIMEOUT;
	bp->mtu = MAX_ETHERNET_PACKET_BUFFER_SIZE;
	bp->hwrm_max_req_len  = HWRM_MAX_REQ_LEN;
	bp->nq.ring_cnt 	  = MAX_NQ_DESC_CNT;
	bp->cq.ring_cnt 	  = MAX_CQ_DESC_CNT;
	bp->tx.ring_cnt 	  = MAX_TX_DESC_CNT;
	bp->rx.ring_cnt 	  = MAX_RX_DESC_CNT;
	bp->rx.buf_cnt  	  = NUM_RX_BUFFERS;
	dbg_mem ( bp, func );
}

void bnxt_mm_nic ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	memset ( bp->cq.bd_virt, 0, CQ_RING_BUFFER_SIZE );
	memset ( bp->tx.bd_virt, 0, TX_RING_BUFFER_SIZE );
	memset ( bp->rx.bd_virt, 0, RX_RING_BUFFER_SIZE );
	memset ( bp->nq.bd_virt, 0, NQ_RING_BUFFER_SIZE );
	bp->nq.cons_id		= 0;
	bp->nq.completion_bit	= 0x1;
	bp->nq.epoch            = 0;
	bp->nq.toggle           = 0;
	bp->cq.cons_id		= 0;
	bp->cq.completion_bit	= 0x1;
	bp->cq.epoch            = 0;
	bp->tx.prod_id		= 0;
	bp->tx.cons_id		= 0;
	bp->tx.epoch            = 0;
	bp->rx.cons_id		= 0;
	bp->rx.iob_cnt		= 0;
	bp->rx.epoch            = 0;

	bp->link_status		= STATUS_LINK_DOWN;
	bp->wait_link_timeout	= LINK_DEFAULT_TIMEOUT;
	bp->mtu			= MAX_ETHERNET_PACKET_BUFFER_SIZE;
	bp->hwrm_max_req_len	= HWRM_MAX_REQ_LEN;
	bp->nq.ring_cnt		= MAX_NQ_DESC_CNT;
	bp->cq.ring_cnt		= MAX_CQ_DESC_CNT;
	bp->tx.ring_cnt		= MAX_TX_DESC_CNT;
	bp->rx.ring_cnt		= MAX_RX_DESC_CNT;
	bp->rx.buf_cnt		= NUM_RX_BUFFERS;
}

void bnxt_free_mem ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	if ( bp->nq.bd_virt ) {
		dma_free ( &bp->nq_mapping, bp->nq.bd_virt, NQ_RING_BUFFER_SIZE );
		bp->nq.bd_virt = NULL;
	}

	if ( bp->cq.bd_virt ) {
		dma_free ( &bp->cq_mapping, bp->cq.bd_virt, CQ_RING_BUFFER_SIZE );
		bp->cq.bd_virt = NULL;
	}

	if ( bp->rx.bd_virt ) {
		dma_free ( &bp->rx_mapping, bp->rx.bd_virt, RX_RING_BUFFER_SIZE );
		bp->rx.bd_virt = NULL;
	}

	if ( bp->tx.bd_virt ) {
		dma_free ( &bp->tx_mapping, bp->tx.bd_virt, TX_RING_BUFFER_SIZE );
		bp->tx.bd_virt = NULL;
	}

	if ( bp->hwrm_addr_dma ) {
		dma_free ( &bp->dma_mapped, bp->hwrm_addr_dma, DMA_BUFFER_SIZE );
		bp->hwrm_addr_dma = NULL;
	}

	if ( bp->hwrm_addr_resp ) {
		dma_free ( &bp->resp_mapping, bp->hwrm_addr_resp, RESP_BUFFER_SIZE );
		bp->hwrm_addr_resp = NULL;
	}

	if ( bp->hwrm_addr_req ) {
		dma_free ( &bp->req_mapping, bp->hwrm_addr_req, REQ_BUFFER_SIZE );
		bp->hwrm_addr_req = NULL;
	}
	DBGP ( "- %s (  ): - Done\n", __func__ );
}

int bnxt_alloc_mem ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	bp->hwrm_addr_req  = dma_alloc ( bp->dma, &bp->req_mapping,
					 REQ_BUFFER_SIZE, REQ_BUFFER_SIZE );
	bp->hwrm_addr_resp = dma_alloc ( bp->dma, &bp->resp_mapping,
					 RESP_BUFFER_SIZE, RESP_BUFFER_SIZE );
	bp->hwrm_addr_dma  = dma_alloc ( bp->dma, &bp->dma_mapped,
					 DMA_BUFFER_SIZE, DMA_BUFFER_SIZE);
	bp->tx.bd_virt = dma_alloc ( bp->dma, &bp->tx_mapping,
				     TX_RING_BUFFER_SIZE, DMA_ALIGN_4K );
	bp->rx.bd_virt = dma_alloc ( bp->dma, &bp->rx_mapping,
				     RX_RING_BUFFER_SIZE, DMA_ALIGN_4K );
	bp->cq.bd_virt = dma_alloc ( bp->dma, &bp->cq_mapping,
				     CQ_RING_BUFFER_SIZE, BNXT_DMA_ALIGNMENT );
	bp->nq.bd_virt = dma_alloc ( bp->dma, &bp->nq_mapping,
				     NQ_RING_BUFFER_SIZE, BNXT_DMA_ALIGNMENT );
	if ( bp->hwrm_addr_req &&
		bp->hwrm_addr_resp &&
		bp->hwrm_addr_dma &&
		bp->tx.bd_virt &&
		bp->rx.bd_virt &&
		bp->nq.bd_virt &&
		bp->cq.bd_virt ) {
		bnxt_mm_init ( bp, __func__ );
		return STATUS_SUCCESS;
	}

	DBGP ( "- %s (  ): Failed\n", __func__ );
	bnxt_free_mem ( bp );
	return -ENOMEM;
}

static void hwrm_init ( struct bnxt *bp, struct input *req, u16 cmd, u16 len )
{
	memset ( req, 0, len );
	req->req_type  = cmd;
	req->cmpl_ring = ( u16 )HWRM_NA_SIGNATURE;
	req->target_id = ( u16 )HWRM_NA_SIGNATURE;
	req->resp_addr = RESP_DMA_ADDR ( bp );
	req->seq_id    = bp->seq_id++;
}

static void hwrm_write_req ( struct bnxt *bp, void *req, u32 cnt )
{
	u32 i = 0;

	for ( i = 0; i < cnt; i++ ) {
		writel ( ( ( u32 * )req )[i],
			 ( bp->bar0 + GRC_COM_CHAN_BASE + ( i * 4 ) ) );
	}
	writel ( 0x1, ( bp->bar0 + GRC_COM_CHAN_BASE + GRC_COM_CHAN_TRIG ) );
}

static void short_hwrm_cmd_req ( struct bnxt *bp, u16 len )
{
	struct hwrm_short_input sreq;

	memset ( &sreq, 0, sizeof ( struct hwrm_short_input ) );
	sreq.req_type  = ( u16 ) ( ( struct input * ) REQ_DMA_ADDR (bp ) )->req_type;
	sreq.signature = SHORT_REQ_SIGNATURE_SHORT_CMD;
	sreq.size      = len;
	sreq.req_addr  = REQ_DMA_ADDR ( bp );
	mdelay ( 100 );
	dbg_short_cmd ( ( u8 * )&sreq, __func__,
			sizeof ( struct hwrm_short_input ) );
	hwrm_write_req ( bp, &sreq, sizeof ( struct hwrm_short_input ) / 4 );
}

static int wait_resp ( struct bnxt *bp, u32 tmo, u16 len, const char *func )
{
	struct input *req = ( struct input * ) REQ_DMA_ADDR ( bp );
	struct output *resp = ( struct output * ) RESP_DMA_ADDR ( bp );
	u8  *ptr = ( u8 * )resp;
	u32 idx;
	u32 wait_cnt = HWRM_CMD_DEFAULT_MULTIPLAYER ( ( u32 )tmo );
	u16 resp_len = 0;
	u16 ret = STATUS_TIMEOUT;

	if ( len > bp->hwrm_max_req_len )
		short_hwrm_cmd_req ( bp, len );
	else
		hwrm_write_req ( bp, req, ( u32 ) ( len / 4 ) );

	for ( idx = 0; idx < wait_cnt; idx++ ) {
		resp_len = resp->resp_len;
		if ( resp->seq_id == req->seq_id &&
			resp->req_type == req->req_type &&
			ptr[resp_len - 1] == 1 ) {
			bp->last_resp_code = resp->error_code;
			ret = resp->error_code;
			break;
		}
		udelay ( HWRM_CMD_POLL_WAIT_TIME );
	}
	dbg_hw_cmd ( bp, func, len, resp_len, tmo, ret );
	return ( int )ret;
}

static int bnxt_hwrm_ver_get ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_ver_get_input );
	struct hwrm_ver_get_input *req;
	struct hwrm_ver_get_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_ver_get_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_ver_get_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_VER_GET, cmd_len );
	req->hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req->hwrm_intf_min = HWRM_VERSION_MINOR;
	req->hwrm_intf_upd = HWRM_VERSION_UPDATE;
	rc = wait_resp ( bp, HWRM_CMD_DEFAULT_TIMEOUT, cmd_len, __func__ );
	if ( rc )
		return STATUS_FAILURE;

	bp->hwrm_spec_code =
		resp->hwrm_intf_maj_8b << 16 |
		resp->hwrm_intf_min_8b << 8 |
		resp->hwrm_intf_upd_8b;
	bp->hwrm_cmd_timeout = ( u32 )resp->def_req_timeout;
	if ( !bp->hwrm_cmd_timeout )
		bp->hwrm_cmd_timeout = ( u32 )HWRM_CMD_DEFAULT_TIMEOUT;
	if ( resp->hwrm_intf_maj_8b >= 1 )
		bp->hwrm_max_req_len = resp->max_req_win_len;
	bp->chip_id =
		resp->chip_rev << 24 |
		resp->chip_metal << 16 |
		resp->chip_bond_id << 8 |
		resp->chip_platform_type;
	bp->chip_num = resp->chip_num;
	if ( ( resp->dev_caps_cfg & SHORT_CMD_SUPPORTED ) &&
		 ( resp->dev_caps_cfg & SHORT_CMD_REQUIRED ) )
		FLAG_SET ( bp->flags, BNXT_FLAG_HWRM_SHORT_CMD_SUPP );
	bp->hwrm_max_ext_req_len = resp->max_ext_req_len;
	if ( ( bp->chip_num == CHIP_NUM_57508 ) ||
	     ( bp->chip_num == CHIP_NUM_57504 ) ||
	     ( bp->chip_num == CHIP_NUM_57502 ) ) {
		FLAG_SET ( bp->flags, BNXT_FLAG_IS_CHIP_P5 );
		FLAG_SET ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS );
	}
	if ( bp->chip_num == CHIP_NUM_57608 ) {
		FLAG_SET ( bp->flags, BNXT_FLAG_IS_CHIP_P7 );
		FLAG_SET ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS );
	}
	dbg_fw_ver ( resp, bp->hwrm_cmd_timeout );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_func_resource_qcaps ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_resource_qcaps_input );
	struct hwrm_func_resource_qcaps_input *req;
	struct hwrm_func_resource_qcaps_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_func_resource_qcaps_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_func_resource_qcaps_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_RESOURCE_QCAPS,
		cmd_len );
	req->fid = ( u16 )HWRM_NA_SIGNATURE;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc != STATUS_SUCCESS )
		return STATUS_SUCCESS;

	FLAG_SET ( bp->flags, BNXT_FLAG_RESOURCE_QCAPS_SUPPORT );

	// VFs
	if ( !bp->vf ) {
		bp->max_vfs = resp->max_vfs;
		bp->vf_res_strategy = resp->vf_reservation_strategy;
	}

	// vNICs
	bp->min_vnics = resp->min_vnics;
	bp->max_vnics = resp->max_vnics;

	// MSI-X
	bp->max_msix = resp->max_msix;

	// Ring Groups
	bp->min_hw_ring_grps = resp->min_hw_ring_grps;
	bp->max_hw_ring_grps = resp->max_hw_ring_grps;

	// TX Rings
	bp->min_tx_rings = resp->min_tx_rings;
	bp->max_tx_rings = resp->max_tx_rings;

	// RX Rings
	bp->min_rx_rings = resp->min_rx_rings;
	bp->max_rx_rings = resp->max_rx_rings;

	// Completion Rings
	bp->min_cp_rings = resp->min_cmpl_rings;
	bp->max_cp_rings = resp->max_cmpl_rings;

	// RSS Contexts
	bp->min_rsscos_ctxs = resp->min_rsscos_ctx;
	bp->max_rsscos_ctxs = resp->max_rsscos_ctx;

	// L2 Contexts
	bp->min_l2_ctxs = resp->min_l2_ctxs;
	bp->max_l2_ctxs = resp->max_l2_ctxs;

	// Statistic Contexts
	bp->min_stat_ctxs = resp->min_stat_ctx;
	bp->max_stat_ctxs = resp->max_stat_ctx;
	dbg_func_resource_qcaps ( bp );
	return STATUS_SUCCESS;
}

static u32 bnxt_set_ring_info ( struct bnxt *bp )
{
	u32 enables = 0;

	DBGP ( "%s\n", __func__ );
	bp->num_cmpl_rings   = DEFAULT_NUMBER_OF_CMPL_RINGS;
	bp->num_tx_rings	 = DEFAULT_NUMBER_OF_TX_RINGS;
	bp->num_rx_rings	 = DEFAULT_NUMBER_OF_RX_RINGS;
	bp->num_hw_ring_grps = DEFAULT_NUMBER_OF_RING_GRPS;
	bp->num_stat_ctxs    = DEFAULT_NUMBER_OF_STAT_CTXS;

	if ( bp->min_cp_rings <= DEFAULT_NUMBER_OF_CMPL_RINGS )
		bp->num_cmpl_rings = bp->min_cp_rings;

	if ( bp->min_tx_rings <= DEFAULT_NUMBER_OF_TX_RINGS )
		bp->num_tx_rings = bp->min_tx_rings;

	if ( bp->min_rx_rings <= DEFAULT_NUMBER_OF_RX_RINGS )
		bp->num_rx_rings = bp->min_rx_rings;

	if ( bp->min_hw_ring_grps <= DEFAULT_NUMBER_OF_RING_GRPS )
		bp->num_hw_ring_grps = bp->min_hw_ring_grps;

	if ( bp->min_stat_ctxs <= DEFAULT_NUMBER_OF_STAT_CTXS )
		bp->num_stat_ctxs = bp->min_stat_ctxs;

	dbg_num_rings ( bp );
	enables = ( FUNC_CFG_REQ_ENABLES_NUM_CMPL_RINGS |
			   FUNC_CFG_REQ_ENABLES_NUM_TX_RINGS |
			   FUNC_CFG_REQ_ENABLES_NUM_RX_RINGS |
			   FUNC_CFG_REQ_ENABLES_NUM_STAT_CTXS |
			   FUNC_CFG_REQ_ENABLES_NUM_HW_RING_GRPS );
	return enables;
}

static void bnxt_hwrm_assign_resources ( struct bnxt *bp )
{
	struct hwrm_func_cfg_input *req;
	u32 enables = 0;

	DBGP ( "%s\n", __func__ );
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_RESOURCE_QCAPS_SUPPORT ) )
		enables = bnxt_set_ring_info ( bp );

	req = ( struct hwrm_func_cfg_input * ) REQ_DMA_ADDR ( bp );
	req->num_cmpl_rings   = bp->num_cmpl_rings;
	req->num_tx_rings     = bp->num_tx_rings;
	req->num_rx_rings     = bp->num_rx_rings;
	req->num_stat_ctxs    = bp->num_stat_ctxs;
	req->num_hw_ring_grps = bp->num_hw_ring_grps;
	req->enables = enables;
}

static int bnxt_hwrm_func_qcaps_req ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_qcaps_input );
	struct hwrm_func_qcaps_input *req;
	struct hwrm_func_qcaps_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( bp->vf )
		return STATUS_SUCCESS;

	req = ( struct hwrm_func_qcaps_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_func_qcaps_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_QCAPS, cmd_len );
	req->fid = ( u16 )HWRM_NA_SIGNATURE;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	bp->fid = resp->fid;
	bp->port_idx = ( u8 )resp->port_id;

	/* Get MAC address for this PF */
	memcpy ( &bp->mac_addr[0], &resp->mac_address[0], ETH_ALEN );
	dbg_func_qcaps ( bp );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_func_qcfg_req ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_qcfg_input );
	struct hwrm_func_qcfg_input *req;
	struct hwrm_func_qcfg_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_func_qcfg_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_func_qcfg_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_QCFG, cmd_len );
	req->fid = ( u16 )HWRM_NA_SIGNATURE;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	if ( resp->flags & FUNC_QCFG_RESP_FLAGS_MULTI_HOST )
		FLAG_SET ( bp->flags, BNXT_FLAG_MULTI_HOST );

	if ( resp->port_partition_type &
		FUNC_QCFG_RESP_PORT_PARTITION_TYPE_NPAR1_0 )
		FLAG_SET ( bp->flags, BNXT_FLAG_NPAR_MODE );

	bp->ordinal_value = ( u8 )resp->pci_id & 0x0F;
	bp->stat_ctx_id   = resp->stat_ctx_id;

	/* If VF is set to TRUE, then use some data from func_qcfg (  ). */
	if ( bp->vf ) {
		bp->fid	= resp->fid;
		bp->port_idx = ( u8 )resp->port_id;
		bp->vlan_id  = resp->vlan;

		/* Get MAC address for this VF */
		memcpy ( bp->mac_addr, resp->mac_address, ETH_ALEN );
	}
	dbg_func_qcfg ( bp );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_port_phy_qcaps_req ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_port_phy_qcaps_input );
	struct hwrm_port_phy_qcaps_input *req;
	struct hwrm_port_phy_qcaps_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );

	req = ( struct hwrm_port_phy_qcaps_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_port_phy_qcaps_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_PORT_PHY_QCAPS, cmd_len );
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "-s %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	if ( resp->flags2 & PORT_PHY_QCAPS_RESP_FLAGS2_SPEEDS2_SUPPORTED )
                FLAG_SET ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 );

	return STATUS_SUCCESS;
}

static int bnxt_hwrm_func_reset_req ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_reset_input );
	struct hwrm_func_reset_input *req;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_func_reset_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_RESET, cmd_len );
	if ( !bp->vf )
		req->func_reset_level = FUNC_RESET_REQ_FUNC_RESET_LEVEL_RESETME;

	return wait_resp ( bp, HWRM_CMD_WAIT ( 6 ), cmd_len, __func__ );
}

static int bnxt_hwrm_func_cfg_req ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_cfg_input );
	struct hwrm_func_cfg_input *req;

	DBGP ( "%s\n", __func__ );
	if ( bp->vf )
		return STATUS_SUCCESS;

	req = ( struct hwrm_func_cfg_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_CFG, cmd_len );
	req->fid = ( u16 )HWRM_NA_SIGNATURE;
	bnxt_hwrm_assign_resources ( bp );
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) {
		req->enables |= ( FUNC_CFG_REQ_ENABLES_NUM_MSIX |
				  FUNC_CFG_REQ_ENABLES_NUM_VNICS |
				  FUNC_CFG_REQ_ENABLES_EVB_MODE );
		req->num_msix  = 1;
		req->num_vnics = 1;
		req->evb_mode  = FUNC_CFG_REQ_EVB_MODE_NO_EVB;
	}
	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_hwrm_func_drv_rgtr ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_drv_rgtr_input );
	struct hwrm_func_drv_rgtr_input *req;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_func_drv_rgtr_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_DRV_RGTR, cmd_len );

	/* Register with HWRM */
	req->enables = FUNC_DRV_RGTR_REQ_ENABLES_OS_TYPE |
			FUNC_DRV_RGTR_REQ_ENABLES_ASYNC_EVENT_FWD |
			FUNC_DRV_RGTR_REQ_ENABLES_VER;
	req->async_event_fwd[0] |= 0x01;
	req->os_type = FUNC_DRV_RGTR_REQ_OS_TYPE_OTHER;
	req->ver_maj = IPXE_VERSION_MAJOR;
	req->ver_min = IPXE_VERSION_MINOR;
	req->ver_upd = IPXE_VERSION_UPDATE;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_SET ( bp->flag_hwrm, VALID_DRIVER_REG );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_func_drv_unrgtr ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_drv_unrgtr_input );
	struct hwrm_func_drv_unrgtr_input *req;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_DRIVER_REG ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_func_drv_unrgtr_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_DRV_UNRGTR, cmd_len );
	req->flags = FUNC_DRV_UNRGTR_REQ_FLAGS_PREPARE_FOR_SHUTDOWN;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc )
		return STATUS_FAILURE;

	FLAG_RESET ( bp->flag_hwrm, VALID_DRIVER_REG );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_set_async_event ( struct bnxt *bp )
{
	int rc;
	u16 idx;

	DBGP ( "%s\n", __func__ );
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) )
		idx = bp->nq_ring_id;
	else
		idx = bp->cq_ring_id;
	if ( bp->vf ) {
		u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_vf_cfg_input );
		struct hwrm_func_vf_cfg_input *req;

		req = ( struct hwrm_func_vf_cfg_input * ) REQ_DMA_ADDR ( bp );
		hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_VF_CFG,
			cmd_len );
		req->enables = VF_CFG_ENABLE_FLAGS;
		req->async_event_cr = idx;
		req->mtu = bp->mtu;
		req->guest_vlan = bp->vlan_id;
		memcpy ( ( char * )&req->dflt_mac_addr[0], bp->mac_addr,
			ETH_ALEN );
		rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	} else {
		u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_cfg_input );
		struct hwrm_func_cfg_input *req;

		req = ( struct hwrm_func_cfg_input * ) REQ_DMA_ADDR ( bp );
		hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_CFG, cmd_len );
		req->fid = ( u16 )HWRM_NA_SIGNATURE;
		req->enables = FUNC_CFG_REQ_ENABLES_ASYNC_EVENT_CR;
		req->async_event_cr = idx;
		rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	}
	return rc;
}

static int bnxt_hwrm_cfa_l2_filter_alloc ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_cfa_l2_filter_alloc_input );
	struct hwrm_cfa_l2_filter_alloc_input *req;
	struct hwrm_cfa_l2_filter_alloc_output *resp;
	int rc;
	u32 flags = CFA_L2_FILTER_ALLOC_REQ_FLAGS_PATH_RX;
	u32 enables;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_cfa_l2_filter_alloc_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_cfa_l2_filter_alloc_output * ) RESP_DMA_ADDR ( bp );
	if ( bp->vf )
		flags |= CFA_L2_FILTER_ALLOC_REQ_FLAGS_OUTERMOST;
	enables = CFA_L2_FILTER_ALLOC_REQ_ENABLES_DST_ID |
		CFA_L2_FILTER_ALLOC_REQ_ENABLES_L2_ADDR |
		CFA_L2_FILTER_ALLOC_REQ_ENABLES_L2_ADDR_MASK;

	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_CFA_L2_FILTER_ALLOC,
		cmd_len );
	req->flags = flags;
	req->enables = enables;
	memcpy ( ( char * )&req->l2_addr[0], ( char * )&bp->mac_addr[0],
		ETH_ALEN );
	memset ( ( char * )&req->l2_addr_mask[0], 0xff, ETH_ALEN );
	if ( !bp->vf ) {
		memcpy ( ( char * )&req->t_l2_addr[0], bp->mac_addr, ETH_ALEN );
		memset ( ( char * )&req->t_l2_addr_mask[0], 0xff, ETH_ALEN );
	}
	req->src_type = CFA_L2_FILTER_ALLOC_REQ_SRC_TYPE_NPORT;
	req->src_id   = ( u32 )bp->port_idx;
	req->dst_id   = bp->vnic_id;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc )
		return STATUS_FAILURE;

	FLAG_SET ( bp->flag_hwrm, VALID_L2_FILTER );
	bp->l2_filter_id = resp->l2_filter_id;
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_cfa_l2_filter_free ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_cfa_l2_filter_free_input );
	struct hwrm_cfa_l2_filter_free_input *req;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_L2_FILTER ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_cfa_l2_filter_free_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_CFA_L2_FILTER_FREE,
		cmd_len );
	req->l2_filter_id = bp->l2_filter_id;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_RESET ( bp->flag_hwrm, VALID_L2_FILTER );
	return STATUS_SUCCESS;
}

u32 set_rx_mask ( u32 rx_mask )
{
	u32 mask = 0;

	if ( !rx_mask )
		return mask;

	mask = CFA_L2_SET_RX_MASK_REQ_MASK_BCAST;
	if ( rx_mask != RX_MASK_ACCEPT_NONE ) {
		if ( rx_mask & RX_MASK_ACCEPT_MULTICAST )
			mask |= CFA_L2_SET_RX_MASK_REQ_MASK_MCAST;
		if ( rx_mask & RX_MASK_ACCEPT_ALL_MULTICAST )
			mask |= CFA_L2_SET_RX_MASK_REQ_MASK_ALL_MCAST;
		if ( rx_mask & RX_MASK_PROMISCUOUS_MODE )
			mask |= CFA_L2_SET_RX_MASK_REQ_MASK_PROMISCUOUS;
	}
	return mask;
}

static int bnxt_hwrm_set_rx_mask ( struct bnxt *bp, u32 rx_mask )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_cfa_l2_set_rx_mask_input );
	struct hwrm_cfa_l2_set_rx_mask_input *req;
	u32 mask = set_rx_mask ( rx_mask );

	req = ( struct hwrm_cfa_l2_set_rx_mask_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_CFA_L2_SET_RX_MASK,
		cmd_len );
	req->vnic_id = bp->vnic_id;
	req->mask    = mask;

	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_hwrm_port_phy_qcfg ( struct bnxt *bp, u16 idx )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_port_phy_qcfg_input );
	struct hwrm_port_phy_qcfg_input *req;
	struct hwrm_port_phy_qcfg_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_port_phy_qcfg_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_port_phy_qcfg_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_PORT_PHY_QCFG, cmd_len );
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	if ( idx & SUPPORT_SPEEDS )
		bp->support_speeds = resp->support_speeds;

	if ( idx & SUPPORT_SPEEDS2 )
                if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) )
		        bp->auto_link_speeds2_mask = resp->auto_link_speeds2;

	if ( idx & DETECT_MEDIA )
		bp->media_detect = resp->module_status;

	if ( idx & PHY_SPEED )
		bp->current_link_speed = resp->link_speed;

	if ( idx & PHY_STATUS ) {
		if ( resp->link == PORT_PHY_QCFG_RESP_LINK_LINK )
			bp->link_status = STATUS_LINK_ACTIVE;
		else
			bp->link_status = STATUS_LINK_DOWN;
	}
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_nvm_get_variable_req ( struct bnxt *bp,
	u16 data_len, u16 option_num, u16 dimensions, u16 index_0 )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_nvm_get_variable_input );
	struct hwrm_nvm_get_variable_input *req;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_nvm_get_variable_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_NVM_GET_VARIABLE, cmd_len );
	req->dest_data_addr = DMA_DMA_ADDR ( bp );
	req->data_len   	= data_len;
	req->option_num 	= option_num;
	req->dimensions 	= dimensions;
	req->index_0		= index_0;
	return wait_resp ( bp,
		HWRM_CMD_FLASH_MULTIPLAYER ( bp->hwrm_cmd_timeout ),
		cmd_len, __func__ );
}

static int bnxt_get_link_speed ( struct bnxt *bp )
{
	u32 *ptr32 = ( u32 * ) DMA_DMA_ADDR ( bp );

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST (bp->flags, BNXT_FLAG_IS_CHIP_P7 ) ) ) {
	        if ( bnxt_hwrm_nvm_get_variable_req ( bp, 4,
		        ( u16 )LINK_SPEED_DRV_NUM,
		        1, ( u16 )bp->port_idx ) != STATUS_SUCCESS )
		        return STATUS_FAILURE;
	        bp->link_set = SET_LINK ( *ptr32, SPEED_DRV_MASK, SPEED_DRV_SHIFT );
	        if ( bnxt_hwrm_nvm_get_variable_req ( bp, 4,
		        ( u16 )D3_LINK_SPEED_FW_NUM, 1,
		        ( u16 )bp->port_idx ) != STATUS_SUCCESS )
		        return STATUS_FAILURE;
	        bp->link_set |= SET_LINK ( *ptr32, D3_SPEED_FW_MASK,
				D3_SPEED_FW_SHIFT );
	}
	if ( bnxt_hwrm_nvm_get_variable_req ( bp, 4,
		( u16 )LINK_SPEED_FW_NUM,
		1, ( u16 )bp->port_idx ) != STATUS_SUCCESS )
		return STATUS_FAILURE;
	bp->link_set |= SET_LINK ( *ptr32, SPEED_FW_MASK, SPEED_FW_SHIFT );
	if ( bnxt_hwrm_nvm_get_variable_req ( bp, 1,
		 ( u16 )PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_NUM,
		1, ( u16 )bp->port_idx ) != STATUS_SUCCESS )
		return STATUS_FAILURE;
	bp->link_set |= SET_LINK ( *ptr32,
		MEDIA_AUTO_DETECT_MASK, MEDIA_AUTO_DETECT_SHIFT );

	/* Use LINK_SPEED_FW_xxx which is valid for CHIP_P7 and earlier devices */
	switch ( bp->link_set & LINK_SPEED_FW_MASK ) {
	case LINK_SPEED_FW_1G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_1000MBPS );
		break;
	case LINK_SPEED_FW_2_5G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_2500MBPS );
		break;
	case LINK_SPEED_FW_10G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_10GBPS );
		break;
	case LINK_SPEED_FW_25G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_25GBPS );
		break;
	case LINK_SPEED_FW_40G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_40GBPS );
		break;
	case LINK_SPEED_FW_50G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_50GBPS );
		break;
	case LINK_SPEED_FW_50G_PAM4:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_50PAM4GBPS );
		break;
	case LINK_SPEED_FW_100G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_100GBPS );
		break;
	case LINK_SPEED_FW_100G_PAM4:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_100PAM4GBPS );
		break;
	case LINK_SPEED_FW_100G_PAM4_112:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_100PAM4_112GBPS );
		break;
	case LINK_SPEED_FW_200G:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_200GBPS );
		break;
	case LINK_SPEED_FW_200G_PAM4_112:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_200PAM4_112GBPS );
		break;
	case LINK_SPEED_FW_400G_PAM4:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_400PAM4GBPS );
		break;
	case LINK_SPEED_FW_400G_PAM4_112:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_400PAM4_112GBPS );
		break;
	case LINK_SPEED_FW_AUTONEG:
		bp->medium = SET_MEDIUM_SPEED ( bp, MEDIUM_SPEED_AUTONEG );
		break;
	default:
		bp->medium = SET_MEDIUM_DUPLEX ( bp, MEDIUM_FULL_DUPLEX );
		break;
	}
	prn_set_speed ( bp->link_set );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_backing_store_qcfg ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_backing_store_qcfg_input );
	struct hwrm_func_backing_store_qcfg_input *req;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_func_backing_store_qcfg_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_BACKING_STORE_QCFG,
		cmd_len );
	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_hwrm_backing_store_cfg ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_func_backing_store_cfg_input );
	struct hwrm_func_backing_store_cfg_input *req;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_func_backing_store_cfg_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_FUNC_BACKING_STORE_CFG,
		cmd_len );
	req->flags   = FUNC_BACKING_STORE_CFG_REQ_FLAGS_PREBOOT_MODE;
	req->enables = 0;
	return wait_resp ( bp, HWRM_CMD_WAIT ( 6 ), cmd_len, __func__ );
}

static int bnxt_hwrm_queue_qportcfg ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_queue_qportcfg_input );
	struct hwrm_queue_qportcfg_input *req;
	struct hwrm_queue_qportcfg_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_queue_qportcfg_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_queue_qportcfg_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_QUEUE_QPORTCFG, cmd_len );
	req->flags   = 0;
	req->port_id = 0;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	bp->queue_id = resp->queue_id0;
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_port_mac_cfg ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_port_mac_cfg_input );
	struct hwrm_port_mac_cfg_input *req;

	DBGP ( "%s\n", __func__ );
	if ( bp->vf )
		return STATUS_SUCCESS;

	req = ( struct hwrm_port_mac_cfg_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_PORT_MAC_CFG, cmd_len );
	req->lpbk = PORT_MAC_CFG_REQ_LPBK_NONE;
	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_hwrm_port_phy_cfg ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_port_phy_cfg_input );
	struct hwrm_port_phy_cfg_input *req;
	u32 flags;
	u32 enables = 0;
	u16 force_link_speed = 0;
	u16 force_link_speeds2 = 0;
	u16 force_pam4_link_speed = 0;
	u16 auto_link_speed_mask = 0;
	u16 auto_link_speeds2_mask = 0;
	u8  auto_mode = 0;
	u8  auto_pause = 0;
	u8  auto_duplex = 0;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_port_phy_cfg_input * ) REQ_DMA_ADDR ( bp );
	flags = PORT_PHY_CFG_REQ_FLAGS_FORCE |
		PORT_PHY_CFG_REQ_FLAGS_RESET_PHY;

	switch ( GET_MEDIUM_SPEED ( bp->medium ) ) {
	case MEDIUM_SPEED_1000MBPS:
		force_link_speed = PORT_PHY_CFG_REQ_FORCE_LINK_SPEED_1GB;
		break;
	case MEDIUM_SPEED_10GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_10GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_link_speed = PORT_PHY_CFG_REQ_FORCE_LINK_SPEED_10GB;
		}
		break;
	case MEDIUM_SPEED_25GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_25GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_link_speed = PORT_PHY_CFG_REQ_FORCE_LINK_SPEED_25GB;
		}
		break;
	case MEDIUM_SPEED_40GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_40GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_link_speed = PORT_PHY_CFG_REQ_FORCE_LINK_SPEED_40GB;
		}
		break;
	case MEDIUM_SPEED_50GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_50GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_link_speed = PORT_PHY_CFG_REQ_FORCE_LINK_SPEED_50GB;
		}
		break;
	case MEDIUM_SPEED_50PAM4GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_50GB_PAM4_56;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_pam4_link_speed = PORT_PHY_CFG_REQ_FORCE_PAM4_LINK_SPEED_50GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_PAM4_LINK_SPEED;
		}
		break;
	case MEDIUM_SPEED_100GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_100GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_link_speed = PORT_PHY_CFG_REQ_FORCE_LINK_SPEED_100GB;
		}
		break;
	case MEDIUM_SPEED_100PAM4GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_100GB_PAM4_56;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
		        force_pam4_link_speed = PORT_PHY_CFG_REQ_FORCE_PAM4_LINK_SPEED_100GB;
                        enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_PAM4_LINK_SPEED;
		}
		break;
	case MEDIUM_SPEED_100PAM4_112GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_100GB_PAM4_112;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		}
		break;
	case MEDIUM_SPEED_200GBPS:
		if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
			force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_200GB_PAM4_56;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
		} else {
			force_pam4_link_speed = PORT_PHY_CFG_REQ_FORCE_PAM4_LINK_SPEED_200GB;
			enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_PAM4_LINK_SPEED;
		}
		break;
	case MEDIUM_SPEED_200PAM4_112GBPS:
                if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
                        force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_200GB_PAM4_112;
                        enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
                }
                break;
	case MEDIUM_SPEED_400PAM4GBPS:
                if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
                        force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_400GB_PAM4_56;
                        enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
                }
		break;
	case MEDIUM_SPEED_400PAM4_112GBPS:
                if ( FLAG_TEST ( bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) ) {
                        force_link_speeds2 = PORT_PHY_CFG_REQ_FORCE_LINK_SPEEDS2_400GB_PAM4_112;
                        enables |= PORT_PHY_CFG_REQ_ENABLES_FORCE_LINK_SPEEDS2;
                }
                break;
	default:
		auto_mode = PORT_PHY_CFG_REQ_AUTO_MODE_SPEED_MASK;
		flags &= ~PORT_PHY_CFG_REQ_FLAGS_FORCE;
		enables |= PORT_PHY_CFG_REQ_ENABLES_AUTO_MODE |
			PORT_PHY_CFG_REQ_ENABLES_AUTO_DUPLEX |
			PORT_PHY_CFG_REQ_ENABLES_AUTO_PAUSE;
		if ( FLAG_TEST (bp->flags, BNXT_FLAG_LINK_SPEEDS2 ) )
			enables |= PORT_PHY_CFG_REQ_ENABLES_AUTO_LINK_SPEEDS2_MASK;
		else
			enables |= PORT_PHY_CFG_REQ_ENABLES_AUTO_LINK_SPEED_MASK;
		auto_pause = PORT_PHY_CFG_REQ_AUTO_PAUSE_TX |
				PORT_PHY_CFG_REQ_AUTO_PAUSE_RX;
		auto_duplex = PORT_PHY_CFG_REQ_AUTO_DUPLEX_BOTH;
		auto_link_speed_mask = bp->support_speeds;
		auto_link_speeds2_mask = bp->auto_link_speeds2_mask;
		break;
	}

	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_PORT_PHY_CFG, cmd_len );
	req->flags = flags;
	req->enables = enables;
	req->port_id = bp->port_idx;
	req->force_link_speed = force_link_speed;
	req->force_pam4_link_speed = force_pam4_link_speed;
	req->force_link_speeds2 = force_link_speeds2;
	req->auto_mode = auto_mode;
	req->auto_duplex = auto_duplex;
	req->auto_pause = auto_pause;
	req->auto_link_speed_mask = auto_link_speed_mask;
	req->auto_link_speeds2_mask = auto_link_speeds2_mask;

	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_query_phy_link ( struct bnxt *bp )
{
	u16 flag = PHY_STATUS | PHY_SPEED | DETECT_MEDIA;

	DBGP ( "%s\n", __func__ );
	/* Query Link Status */
	if ( bnxt_hwrm_port_phy_qcfg ( bp, QCFG_PHY_ALL ) != STATUS_SUCCESS ) {
			return STATUS_FAILURE;
	}

	if ( bp->link_status == STATUS_LINK_ACTIVE )
		return STATUS_SUCCESS;

	/* If VF is set to TRUE, Do not issue the following commands */
	if ( bp->vf )
		return STATUS_SUCCESS;

	/* If multi_host or NPAR, Do not issue bnxt_get_link_speed */
	if ( FLAG_TEST ( bp->flags, PORT_PHY_FLAGS ) ) {
		dbg_flags ( __func__, bp->flags );
		return STATUS_SUCCESS;
	}

	/* HWRM_NVM_GET_VARIABLE - speed */
	if ( bnxt_get_link_speed ( bp ) != STATUS_SUCCESS ) {
			return STATUS_FAILURE;
	}

	/* Configure link if it is not up */
	bnxt_hwrm_port_phy_cfg ( bp );

	/* refresh link speed values after bringing link up */
	return bnxt_hwrm_port_phy_qcfg ( bp, flag );
}

static int bnxt_get_phy_link ( struct bnxt *bp )
{
	u16 i;
	u16 flag = PHY_STATUS | PHY_SPEED | DETECT_MEDIA;

	DBGP ( "%s\n", __func__ );
	dbg_chip_info ( bp );
	for ( i = 0; i < ( bp->wait_link_timeout / 100 ); i++ ) {
		if ( bnxt_hwrm_port_phy_qcfg ( bp, flag ) != STATUS_SUCCESS )
			break;

		if ( bp->link_status == STATUS_LINK_ACTIVE )
			break;

//		if ( bp->media_detect )
//			break;
		mdelay ( LINK_POLL_WAIT_TIME );
	}
	dbg_link_state ( bp, ( u32 ) ( ( i + 1 ) * 100 ) );
	bnxt_set_link ( bp );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_stat_ctx_alloc ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_stat_ctx_alloc_input );
	struct hwrm_stat_ctx_alloc_input *req;
	struct hwrm_stat_ctx_alloc_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_stat_ctx_alloc_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_stat_ctx_alloc_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_STAT_CTX_ALLOC, cmd_len );
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_SET ( bp->flag_hwrm, VALID_STAT_CTX );
	bp->stat_ctx_id = ( u16 )resp->stat_ctx_id;
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_stat_ctx_free ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_stat_ctx_free_input );
	struct hwrm_stat_ctx_free_input *req;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_STAT_CTX ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_stat_ctx_free_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_STAT_CTX_FREE, cmd_len );
	req->stat_ctx_id = ( u32 )bp->stat_ctx_id;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_RESET ( bp->flag_hwrm, VALID_STAT_CTX );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_ring_free_grp ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_ring_grp_free_input );
	struct hwrm_ring_grp_free_input *req;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_RING_GRP ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_ring_grp_free_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_RING_GRP_FREE, cmd_len );
	req->ring_group_id = ( u32 )bp->ring_grp_id;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_RESET ( bp->flag_hwrm, VALID_RING_GRP );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_ring_alloc_grp ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_ring_grp_alloc_input );
	struct hwrm_ring_grp_alloc_input *req;
	struct hwrm_ring_grp_alloc_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_ring_grp_alloc_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_ring_grp_alloc_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_RING_GRP_ALLOC, cmd_len );
	req->cr = bp->cq_ring_id;
	req->rr = bp->rx_ring_id;
	req->ar = ( u16 )HWRM_NA_SIGNATURE;
	if ( bp->vf )
		req->sc = bp->stat_ctx_id;

	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_SET ( bp->flag_hwrm, VALID_RING_GRP );
	bp->ring_grp_id = ( u16 )resp->ring_group_id;
	return STATUS_SUCCESS;
}

int bnxt_hwrm_ring_free ( struct bnxt *bp, u16 ring_id, u8 ring_type )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_ring_free_input );
	struct hwrm_ring_free_input *req;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_ring_free_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_RING_FREE, cmd_len );
	req->ring_type = ring_type;
	req->ring_id   = ring_id;
	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_hwrm_ring_alloc ( struct bnxt *bp, u8 type )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_ring_alloc_input );
	struct hwrm_ring_alloc_input *req;
	struct hwrm_ring_alloc_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_ring_alloc_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_ring_alloc_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_RING_ALLOC, cmd_len );
	req->ring_type = type;
	switch ( type ) {
	case RING_ALLOC_REQ_RING_TYPE_NQ:
		req->page_size  = LM_PAGE_BITS ( 12 );
		req->int_mode   = BNXT_CQ_INTR_MODE ( ( (FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P7) ) || bp->vf ) );
		req->length 	= ( u32 )bp->nq.ring_cnt;
		req->logical_id = 0xFFFF; // Required value for Thor FW?
		req->page_tbl_addr = NQ_DMA_ADDR ( bp );
		break;
	case RING_ALLOC_REQ_RING_TYPE_L2_CMPL:
		req->page_size = LM_PAGE_BITS ( 8 );
		req->int_mode  = BNXT_CQ_INTR_MODE ( bp->vf );
		req->length    = ( u32 )bp->cq.ring_cnt;
		req->page_tbl_addr = CQ_DMA_ADDR ( bp );
	        if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
			break;
		req->enables = RING_ALLOC_REQ_ENABLES_NQ_RING_ID_VALID;
		req->nq_ring_id = bp->nq_ring_id;
		req->cq_handle = ( u64 )bp->nq_ring_id;
		break;
	case RING_ALLOC_REQ_RING_TYPE_TX:
		req->page_size   = LM_PAGE_BITS ( 8 );
		req->int_mode    = RING_ALLOC_REQ_INT_MODE_POLL;
		req->length 	 = ( u32 )bp->tx.ring_cnt;
		req->queue_id    = ( u16 )bp->queue_id;
		req->stat_ctx_id = ( u32 )bp->stat_ctx_id;
		req->cmpl_ring_id  = bp->cq_ring_id;
		req->page_tbl_addr = TX_DMA_ADDR ( bp );
		break;
	case RING_ALLOC_REQ_RING_TYPE_RX:
		req->page_size   = LM_PAGE_BITS ( 8 );
		req->int_mode    = RING_ALLOC_REQ_INT_MODE_POLL;
		req->length 	 = ( u32 )bp->rx.ring_cnt;
		req->stat_ctx_id = ( u32 )STAT_CTX_ID;
		req->cmpl_ring_id  = bp->cq_ring_id;
		req->page_tbl_addr = RX_DMA_ADDR ( bp );
	        if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
			break;
		req->queue_id    = ( u16 )RX_RING_QID;
		req->rx_buf_size = MAX_ETHERNET_PACKET_BUFFER_SIZE;
		req->enables	 = RING_ALLOC_REQ_ENABLES_RX_BUF_SIZE_VALID;
		break;
	default:
		return STATUS_SUCCESS;
	}
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed, type = %x\n", __func__, type );
		return STATUS_FAILURE;
	}

	if ( type == RING_ALLOC_REQ_RING_TYPE_L2_CMPL ) {
		FLAG_SET ( bp->flag_hwrm, VALID_RING_CQ );
		bp->cq_ring_id = resp->ring_id;
	} else if ( type == RING_ALLOC_REQ_RING_TYPE_TX ) {
		FLAG_SET ( bp->flag_hwrm, VALID_RING_TX );
		bp->tx_ring_id = resp->ring_id;
	} else if ( type == RING_ALLOC_REQ_RING_TYPE_RX ) {
		FLAG_SET ( bp->flag_hwrm, VALID_RING_RX );
		bp->rx_ring_id = resp->ring_id;
	} else if ( type == RING_ALLOC_REQ_RING_TYPE_NQ ) {
		FLAG_SET ( bp->flag_hwrm, VALID_RING_NQ );
		bp->nq_ring_id = resp->ring_id;
	}
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_ring_alloc_cq ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	return bnxt_hwrm_ring_alloc ( bp, RING_ALLOC_REQ_RING_TYPE_L2_CMPL );
}

static int bnxt_hwrm_ring_alloc_tx ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	return bnxt_hwrm_ring_alloc ( bp, RING_ALLOC_REQ_RING_TYPE_TX );
}

static int bnxt_hwrm_ring_alloc_rx ( struct bnxt *bp )
{
	DBGP ( "%s\n", __func__ );
	return bnxt_hwrm_ring_alloc ( bp, RING_ALLOC_REQ_RING_TYPE_RX );
}

static int bnxt_hwrm_ring_free_cq ( struct bnxt *bp )
{
	int ret = STATUS_SUCCESS;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_RING_CQ ) ) )
		return ret;

	ret = RING_FREE ( bp, bp->cq_ring_id, RING_FREE_REQ_RING_TYPE_L2_CMPL );
	if ( ret == STATUS_SUCCESS )
		FLAG_RESET ( bp->flag_hwrm, VALID_RING_CQ );

	return ret;
}

static int bnxt_hwrm_ring_free_tx ( struct bnxt *bp )
{
	int ret = STATUS_SUCCESS;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_RING_TX ) ) )
		return ret;

	ret = RING_FREE ( bp, bp->tx_ring_id, RING_FREE_REQ_RING_TYPE_TX );
	if ( ret == STATUS_SUCCESS )
		FLAG_RESET ( bp->flag_hwrm, VALID_RING_TX );

	return ret;
}

static int bnxt_hwrm_ring_free_rx ( struct bnxt *bp )
{
	int ret = STATUS_SUCCESS;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_RING_RX ) ) )
		return ret;

	ret = RING_FREE ( bp, bp->rx_ring_id, RING_FREE_REQ_RING_TYPE_RX );
	if ( ret == STATUS_SUCCESS )
		FLAG_RESET ( bp->flag_hwrm, VALID_RING_RX );

	return ret;
}

static int bnxt_hwrm_ring_alloc_nq ( struct bnxt *bp )
{
	if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
		return STATUS_SUCCESS;
	return bnxt_hwrm_ring_alloc ( bp, RING_ALLOC_REQ_RING_TYPE_NQ );
}

static int bnxt_hwrm_ring_free_nq ( struct bnxt *bp )
{
	int ret = STATUS_SUCCESS;

	if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
		return STATUS_SUCCESS;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_RING_NQ ) ) )
		return ret;

	ret = RING_FREE ( bp, bp->nq_ring_id, RING_FREE_REQ_RING_TYPE_NQ );
	if ( ret == STATUS_SUCCESS )
		FLAG_RESET ( bp->flag_hwrm, VALID_RING_NQ );

	return ret;
}

static int bnxt_hwrm_vnic_alloc ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_vnic_alloc_input );
	struct hwrm_vnic_alloc_input *req;
	struct hwrm_vnic_alloc_output *resp;
	int rc;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_vnic_alloc_input * ) REQ_DMA_ADDR ( bp );
	resp = ( struct hwrm_vnic_alloc_output * ) RESP_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_VNIC_ALLOC, cmd_len );
	req->flags = VNIC_ALLOC_REQ_FLAGS_DEFAULT;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_SET ( bp->flag_hwrm, VALID_VNIC_ID );
	bp->vnic_id = resp->vnic_id;
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_vnic_free ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_vnic_free_input );
	struct hwrm_vnic_free_input *req;
	int rc;

	DBGP ( "%s\n", __func__ );
	if ( ! ( FLAG_TEST ( bp->flag_hwrm, VALID_VNIC_ID ) ) )
		return STATUS_SUCCESS;

	req = ( struct hwrm_vnic_free_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_VNIC_FREE, cmd_len );
	req->vnic_id = bp->vnic_id;
	rc = wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
	if ( rc ) {
		DBGP ( "- %s (  ): Failed\n", __func__ );
		return STATUS_FAILURE;
	}

	FLAG_RESET ( bp->flag_hwrm, VALID_VNIC_ID );
	return STATUS_SUCCESS;
}

static int bnxt_hwrm_vnic_cfg ( struct bnxt *bp )
{
	u16 cmd_len = ( u16 )sizeof ( struct hwrm_vnic_cfg_input );
	struct hwrm_vnic_cfg_input *req;

	DBGP ( "%s\n", __func__ );
	req = ( struct hwrm_vnic_cfg_input * ) REQ_DMA_ADDR ( bp );
	hwrm_init ( bp, ( void * )req, ( u16 )HWRM_VNIC_CFG, cmd_len );
	req->enables = VNIC_CFG_REQ_ENABLES_MRU;
	req->mru	 = bp->mtu;

	if ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) {
		req->enables |= ( VNIC_CFG_REQ_ENABLES_DEFAULT_RX_RING_ID |
				VNIC_CFG_REQ_ENABLES_DEFAULT_CMPL_RING_ID );
		req->default_rx_ring_id   = bp->rx_ring_id;
		req->default_cmpl_ring_id = bp->cq_ring_id;
	} else {
		req->enables |= VNIC_CFG_REQ_ENABLES_DFLT_RING_GRP;
		req->dflt_ring_grp = bp->ring_grp_id;
	}

	req->vnic_id = bp->vnic_id;
	return wait_resp ( bp, bp->hwrm_cmd_timeout, cmd_len, __func__ );
}

static int bnxt_set_rx_mask ( struct bnxt *bp )
{
	return bnxt_hwrm_set_rx_mask ( bp, RX_MASK );
}

static int bnxt_reset_rx_mask ( struct bnxt *bp )
{
	return bnxt_hwrm_set_rx_mask ( bp, 0 );
}

typedef int ( *hwrm_func_t ) ( struct bnxt *bp );

hwrm_func_t bring_down_chip[] = {
	bnxt_hwrm_func_drv_unrgtr,	/* HWRM_FUNC_DRV_UNRGTR		*/
	NULL,
};

hwrm_func_t bring_down_nic[] = {
	bnxt_hwrm_cfa_l2_filter_free,	/* HWRM_CFA_L2_FILTER_FREE	*/
	bnxt_reset_rx_mask,
	bnxt_hwrm_vnic_cfg,		/* HWRM_VNIC_CFG		*/
	bnxt_free_rx_iob,		/* HWRM_FREE_IOB		*/
	bnxt_hwrm_vnic_free,		/* HWRM_VNIC_FREE		*/
	bnxt_hwrm_ring_free_grp,	/* HWRM_RING_GRP_FREE		*/
	bnxt_hwrm_ring_free_rx,		/* HWRM_RING_FREE - RX Ring	*/
	bnxt_hwrm_ring_free_tx,		/* HWRM_RING_FREE - TX Ring	*/
	bnxt_hwrm_stat_ctx_free,	/* HWRM_STAT_CTX_FREE		*/
	bnxt_hwrm_ring_free_cq,		/* HWRM_RING_FREE - CQ Ring	*/
	bnxt_hwrm_ring_free_nq,		/* HWRM_RING_FREE - NQ Ring	*/
	NULL,
};
hwrm_func_t bring_up_chip[] = {
	bnxt_hwrm_ver_get,		/* HWRM_VER_GET			*/
	bnxt_hwrm_func_reset_req,	/* HWRM_FUNC_RESET		*/
	bnxt_hwrm_func_drv_rgtr,	/* HWRM_FUNC_DRV_RGTR		*/
	bnxt_hwrm_func_qcaps_req,	/* HWRM_FUNC_QCAPS		*/
	bnxt_hwrm_backing_store_cfg,	/* HWRM_FUNC_BACKING_STORE_CFG  */
	bnxt_hwrm_backing_store_qcfg,	/* HWRM_FUNC_BACKING_STORE_QCFG	*/
	bnxt_hwrm_func_resource_qcaps,	/* HWRM_FUNC_RESOURCE_QCAPS	*/
	bnxt_hwrm_port_phy_qcaps_req,	/* HWRM_PORT_PHY_QCAPS	*/
	bnxt_hwrm_func_qcfg_req,	/* HWRM_FUNC_QCFG		*/
	bnxt_hwrm_port_mac_cfg,		/* HWRM_PORT_MAC_CFG		*/
	bnxt_hwrm_func_cfg_req,		/* HWRM_FUNC_CFG		*/
	bnxt_query_phy_link,		/* HWRM_PORT_PHY_QCFG		*/
	bnxt_get_device_address,	/* HW MAC address		*/
	NULL,
};

hwrm_func_t bring_up_nic[] = {
	bnxt_hwrm_stat_ctx_alloc,	/* HWRM_STAT_CTX_ALLOC		*/
	bnxt_hwrm_queue_qportcfg,	/* HWRM_QUEUE_QPORTCFG		*/
	bnxt_hwrm_ring_alloc_nq,	/* HWRM_RING_ALLOC - NQ Ring	*/
	bnxt_hwrm_ring_alloc_cq,	/* HWRM_RING_ALLOC - CQ Ring	*/
	bnxt_hwrm_ring_alloc_tx,	/* HWRM_RING_ALLOC - TX Ring	*/
	bnxt_hwrm_ring_alloc_rx,	/* HWRM_RING_ALLOC - RX Ring	*/
	bnxt_hwrm_ring_alloc_grp,	/* HWRM_RING_GRP_ALLOC - Group	*/
	bnxt_hwrm_vnic_alloc,		/* HWRM_VNIC_ALLOC		*/
	bnxt_post_rx_buffers,		/* Post RX buffers		*/
	bnxt_hwrm_set_async_event,	/* ENABLES_ASYNC_EVENT_CR	*/
	bnxt_hwrm_vnic_cfg,		/* HWRM_VNIC_CFG		*/
	bnxt_hwrm_cfa_l2_filter_alloc,	/* HWRM_CFA_L2_FILTER_ALLOC	*/
	bnxt_get_phy_link,		/* HWRM_PORT_PHY_QCFG - PhyLink */
	bnxt_set_rx_mask,		/* HWRM_CFA_L2_SET_RX_MASK	*/
	NULL,
};

int bnxt_hwrm_run ( hwrm_func_t cmds[], struct bnxt *bp )
{
	hwrm_func_t *ptr;
	int ret;

	for ( ptr = cmds; *ptr; ++ptr ) {
		memset ( ( void * ) REQ_DMA_ADDR ( bp ),  0, REQ_BUFFER_SIZE );
		memset ( ( void * ) RESP_DMA_ADDR ( bp ), 0, RESP_BUFFER_SIZE );
		ret = ( *ptr ) ( bp );
		if ( ret ) {
			DBGP ( "- %s (  ): Failed\n", __func__ );
			return STATUS_FAILURE;
		}
	}
	return STATUS_SUCCESS;
}

#define bnxt_down_chip( bp )	bnxt_hwrm_run ( bring_down_chip, bp )
#define bnxt_up_chip( bp )	bnxt_hwrm_run ( bring_up_chip, bp )
#define bnxt_down_nic( bp )	bnxt_hwrm_run ( bring_down_nic, bp )
#define bnxt_up_nic( bp )	bnxt_hwrm_run ( bring_up_nic, bp )

static int bnxt_open ( struct net_device *dev )
{
	struct bnxt *bp = dev->priv;

	DBGP ( "%s\n", __func__ );
	bnxt_mm_nic ( bp );
	return (bnxt_up_nic ( bp ));
}

static void bnxt_tx_adjust_pkt ( struct bnxt *bp, struct io_buffer *iob )
{
	u16 prev_len = iob_len ( iob );

	bp->vlan_tx = bnxt_get_pkt_vlan ( ( char * )iob->data );
	if ( !bp->vlan_tx && bp->vlan_id )
		bnxt_add_vlan ( iob, bp->vlan_id );

	dbg_tx_vlan ( bp, ( char * )iob->data, prev_len, iob_len ( iob ) );
	if ( iob_len ( iob ) != prev_len )
		prev_len = iob_len ( iob );

}

static int bnxt_tx ( struct net_device *dev, struct io_buffer *iob )
{
	struct bnxt *bp = dev->priv;
	u16 len, entry;
	physaddr_t mapping;

	if ( bnxt_tx_avail ( bp ) < 1 ) {
		DBGP ( "- %s (  ): Failed no bd's available\n", __func__ );
		return -ENOBUFS;
	}

	mapping = iob_dma ( iob );
	bnxt_tx_adjust_pkt ( bp, iob );
	entry = bp->tx.prod_id;
	len = iob_len ( iob );
	bp->tx.iob[entry] = iob;
	bnxt_set_txq ( bp, entry, mapping, len );
	entry = NEXT_IDX ( entry, bp->tx.ring_cnt );
	/* If the ring has wrapped, toggle the epoch bit */
	if ( bp->tx.prod_id > entry )
		bp->tx.epoch ^= 1;
	dump_tx_pkt ( ( u8 * )iob->data, len, bp->tx.prod_id );
	/* Packets are ready, update Tx producer idx local and on card. */
	bnxt_db_tx ( bp, ( u32 )entry );
	bp->tx.prod_id = entry;
	bp->tx.cnt_req++;
	/* memory barrier */
	mb (  );
	return 0;
}

static void bnxt_adv_nq_index ( struct bnxt *bp, u16 cnt )
{
	u16 cons_id;

	cons_id = bp->nq.cons_id + cnt;
	if ( cons_id >= bp->nq.ring_cnt ) {
		/* Toggle completion bit when the ring wraps. */
		bp->nq.completion_bit ^= 1;
		bp->nq.epoch ^= 1;
		cons_id = cons_id - bp->nq.ring_cnt;
	}
	bp->nq.cons_id = cons_id;
}

void bnxt_link_evt ( struct bnxt *bp, struct hwrm_async_event_cmpl *evt )
{
	switch ( evt->event_id ) {
	case ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE:
		if ( evt->event_data1 & 0x01 )
			bp->link_status = STATUS_LINK_ACTIVE;
		else
			bp->link_status = STATUS_LINK_DOWN;
		bnxt_set_link ( bp );
		dbg_link_status ( bp );
		break;
	default:
		break;
	}
}

static void bnxt_service_cq ( struct net_device *dev )
{
	struct bnxt *bp = dev->priv;
	struct cmpl_base *cmp;
	struct tx_cmpl *tx;
	u16 old_cid = bp->cq.cons_id;
	int done = SERVICE_NEXT_CQ_BD;
	u32 cq_type;

	while ( done == SERVICE_NEXT_CQ_BD ) {
		cmp = ( struct cmpl_base * )BD_NOW ( CQ_DMA_ADDR ( bp ),
						bp->cq.cons_id,
						sizeof ( struct cmpl_base ) );

		if ( ( cmp->info3_v & CMPL_BASE_V ) ^ bp->cq.completion_bit )
			break;

		cq_type = cmp->type & CMPL_BASE_TYPE_MASK;
		dump_evt ( ( u8 * )cmp, cq_type, bp->cq.cons_id, 0 );
		dump_cq ( cmp, bp->cq.cons_id, bp->nq.toggle );

		switch ( cq_type ) {
		case CMPL_BASE_TYPE_TX_L2:
			tx = ( struct tx_cmpl * )cmp;
			bnxt_tx_complete ( dev, ( u16 )tx->opaque );
		/* Fall through */
		case CMPL_BASE_TYPE_STAT_EJECT:
			bnxt_adv_cq_index ( bp, 1 );
			break;
		case CMPL_BASE_TYPE_RX_L2:
		case CMPL_BASE_TYPE_RX_L2_V3:
			done = bnxt_rx_complete ( dev,
				( struct rx_pkt_cmpl * )cmp );
			break;
		case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
			bnxt_link_evt ( bp,
				( struct hwrm_async_event_cmpl * )cmp );
			bnxt_adv_cq_index ( bp, 1 );
			break;
		default:
			done = NO_MORE_CQ_BD_TO_SERVICE;
			break;
		}
	}

	if ( bp->cq.cons_id != old_cid )
		bnxt_db_cq ( bp );
}

static void bnxt_service_nq ( struct net_device *dev )
{
	struct bnxt *bp = dev->priv;
	struct nq_base *nqp;
	u16 old_cid = bp->nq.cons_id;
	int done = SERVICE_NEXT_NQ_BD;
	u32 nq_type;

	if ( ! ( FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS ) ) )
		return;

	while ( done == SERVICE_NEXT_NQ_BD ) {
		nqp = ( struct nq_base * )BD_NOW ( NQ_DMA_ADDR ( bp ),
						bp->nq.cons_id,
						sizeof ( struct nq_base ) );
		if ( ( nqp->v & NQ_CN_V ) ^ bp->nq.completion_bit )
			break;
		nq_type = ( nqp->type & NQ_CN_TYPE_MASK );
		bp->nq.toggle = ( ( nqp->type & NQ_CN_TOGGLE_MASK ) >> NQ_CN_TOGGLE_SFT );
		dump_evt ( ( u8 * )nqp, nq_type, bp->nq.cons_id, 1 );
		dump_nq ( nqp, bp->nq.cons_id );

		switch ( nq_type ) {
		case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
			bnxt_link_evt ( bp,
				( struct hwrm_async_event_cmpl * )nqp );
				/* Fall through */
		case NQ_CN_TYPE_CQ_NOTIFICATION:
			bnxt_adv_nq_index ( bp, 1 );
			break;
		default:
			done = NO_MORE_NQ_BD_TO_SERVICE;
			break;
		}
	}

	if ( bp->nq.cons_id != old_cid )
		bnxt_db_nq ( bp );
}

static void bnxt_poll ( struct net_device *dev )
{
	mb (  );
	bnxt_service_nq ( dev );
	bnxt_service_cq ( dev );
}

static void bnxt_close ( struct net_device *dev )
{
	struct bnxt *bp = dev->priv;

	DBGP ( "%s\n", __func__ );
	bnxt_down_nic (bp);

	/* iounmap PCI BAR ( s ) */
	bnxt_down_pci(bp);

	/* Get Bar Address */
	bp->bar0 = bnxt_pci_base ( bp->pdev, PCI_BASE_ADDRESS_0 );
	bp->bar1 = bnxt_pci_base ( bp->pdev, PCI_BASE_ADDRESS_2 );
	bp->bar2 = bnxt_pci_base ( bp->pdev, PCI_BASE_ADDRESS_4 );

}

static struct net_device_operations bnxt_netdev_ops = {
	.open     = bnxt_open,
	.close    = bnxt_close,
	.poll     = bnxt_poll,
	.transmit = bnxt_tx,
};

static int bnxt_init_one ( struct pci_device *pci )
{
	struct net_device *netdev;
	struct bnxt *bp;
	int err = 0;

	DBGP ( "%s\n", __func__ );
	/* Allocate network device */
	netdev = alloc_etherdev ( sizeof ( *bp ) );
	if ( !netdev ) {
		DBGP ( "- %s (  ): alloc_etherdev Failed\n", __func__ );
		err = -ENOMEM;
		goto disable_pdev;
	}

	/* Initialise network device */
	netdev_init ( netdev, &bnxt_netdev_ops );

	/* Driver private area for this device */
	bp = netdev->priv;

	/* Set PCI driver private data */
	pci_set_drvdata ( pci, netdev );

	/* Clear Private area data */
	memset ( bp, 0, sizeof ( *bp ) );
	bp->pdev = pci;
	bp->dev  = netdev;
	netdev->dev = &pci->dev;

	/* Configure DMA */
	bp->dma = &pci->dma;
	netdev->dma = bp->dma;

	/* Enable PCI device */
	adjust_pci_device ( pci );

	/* Get PCI Information */
	bnxt_get_pci_info ( bp );

	/* Allocate and Initialise device specific parameters */
	if ( ( err = bnxt_alloc_mem ( bp ) ) != 0 ) {
		DBGP ( "- %s (  ): bnxt_alloc_mem Failed\n", __func__ );
		goto err_down_pci;
	}

	/* Get device specific information */
	if ( bnxt_up_chip ( bp ) != 0 ) {
		DBGP ( "- %s (  ): bnxt_up_chip Failed\n", __func__ );
		err = -ENODEV;
		goto err_down_chip;
	}

	/* Register Network device */
	if ( ( err = register_netdev ( netdev ) ) != 0 ) {
		DBGP ( "- %s (  ): register_netdev Failed\n", __func__ );
		goto err_down_chip;
	}

	return 0;

	unregister_netdev ( netdev );

err_down_chip:
	bnxt_down_chip (bp);
	bnxt_free_mem ( bp );

err_down_pci:
	bnxt_down_pci ( bp );
	netdev_nullify ( netdev );
	netdev_put ( netdev );

disable_pdev:
	pci_set_drvdata ( pci, NULL );
	return err;
}

static void bnxt_remove_one ( struct pci_device *pci )
{
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct bnxt *bp = netdev->priv;

	DBGP ( "%s\n", __func__ );
	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Bring down Chip */
	bnxt_down_chip(bp);

	/* Free Allocated resource */
	bnxt_free_mem ( bp );

	/* iounmap PCI BAR ( s ) */
	bnxt_down_pci ( bp );

	/* Stop network device */
	netdev_nullify ( netdev );

	/* Drop refernce to network device */
	netdev_put ( netdev );
}

/* Broadcom NXE PCI driver */
struct pci_driver bnxt_pci_driver __pci_driver = {
	.ids		= bnxt_nics,
	.id_count	= ARRAY_SIZE ( bnxt_nics ),
	.probe		= bnxt_init_one,
	.remove		= bnxt_remove_one,
};
