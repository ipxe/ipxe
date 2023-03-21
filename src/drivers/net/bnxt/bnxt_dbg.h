/*
 * Copyright © 2018 Broadcom. All Rights Reserved. 
 * The term Broadcom refers to Broadcom Inc. and/or its subsidiaries.

 * This program is free software; you can redistribute it and/or modify it under
 * the terms of version 2 of the GNU General Public License as published by the
 * Free Software Foundation.

 * This program is distributed in the hope that it will be useful.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING 
 * ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS
 * ARE HELD TO BE LEGALLY INVALID. See the GNU General Public License for more
 * details, a copy of which can be found in the file COPYING included with this
 * package.
 */

//#define DEBUG_DRV
//#define DEBUG_KEY
//#define DEBUG_PCI
//#define DEBUG_MEMORY
//#define DEBUG_LINK
//#define DEBUG_CHIP
//#define DEBUG_FAIL
//#define DEBUG_HWRM_CMDS
//#define DEBUG_HWRM_DUMP
//#define DEBUG_CQ
//#define DEBUG_CQ_DUMP
//#define DEBUG_TX
//#define DEBUG_TX_DUMP
//#define DEBUG_RX
//#define DEBUG_RX_DUMP

#if \
	defined(DEBUG_DRV) || \
	defined(DEBUG_PCI) || \
	defined(DEBUG_CHIP) || \
	defined(DEBUG_MEMORY) || \
	defined(DEBUG_LINK) || \
	defined(DEBUG_FAIL) || \
	defined(DEBUG_HWRM_CMDS) || \
	defined(DEBUG_HWRM_DUMP) || \
	defined(DEBUG_CQ) || \
	defined(DEBUG_CQ_DUMP) || \
	defined(DEBUG_TX) || \
	defined(DEBUG_TX_DUMP) || \
	defined(DEBUG_RX) || \
	defined(DEBUG_RX_DUMP)
#define DEBUG_DEFAULT
#endif
#if defined(DEBUG_DEFAULT)
#define dbg_prn          printf

void pause_drv(void)
{
#if defined(DEBUG_KEY)
	dbg_prn(" Press a key...");
	getchar();
#endif
	dbg_prn("\n");
}

#define MAX_CHAR_SIZE(a) (u32)((1 << (a)) - 1)
#define DISP_U8          0x00
#define DISP_U16         0x01
#define DISP_U32         0x02
#define DISP_U64         0x03

void dumpmemory1(u8 *buffer, u32 length, u8 flag)
{
	u32 jj = 0;
	u8  i, c;

	dbg_prn("\n  %p:", buffer);
	for (jj = 0; jj < 16; jj++) {
		if (!(jj & MAX_CHAR_SIZE(flag)))
			dbg_prn(" ");
		if (jj < length)
			dbg_prn("%02x", buffer[jj]);
		else
			dbg_prn("  ");
		if ((jj & 0xF) == 0xF) {
			dbg_prn(" ");
			for (i = 0; i < 16; i++) {
				if (i < length) {
					c = buffer[jj + i - 15];
					if (c >= 0x20 && c < 0x7F)
						;
					else
						c = '.';
					dbg_prn("%c", c);
				}
			}
		}
	}
}

void dump_mem(u8 *buffer, u32 length, u8 flag)
{
	u32 length16, remlen, jj;

	length16 = length & 0xFFFFFFF0;
	remlen   = length & 0xF;
	for (jj = 0; jj < length16; jj += 16)
		dumpmemory1((u8 *)&buffer[jj], 16, flag);
	if (remlen)
		dumpmemory1((u8 *)&buffer[length16], remlen, flag);
	if (length16 || remlen)
		dbg_prn("\n");
}
#else
#define dbg_prn(func)
#endif

#if defined(DEBUG_PCI)
void dbg_pci(struct bnxt *bp, const char *func, u16 cmd_reg)
{
	struct pci_device *pdev = bp->pdev;

	dbg_prn("- %s()\n", func);
	dbg_prn("  Bus:Dev:Func       : %04X\n", pdev->busdevfn);
	dbg_prn("  Vendor id          : %04X\n", pdev->vendor);
	dbg_prn("  Device id          : %04X (%cF)\n",
		pdev->device, (bp->vf) ? 'V' : 'P');
	dbg_prn("  Irq                : %d\n", pdev->irq);
	dbg_prn("  PCI Command Reg    : %04X\n", cmd_reg);
	dbg_prn("  Sub Vendor id      : %04X\n", bp->subsystem_vendor);
	dbg_prn("  Sub Device id      : %04X\n", bp->subsystem_device);
	dbg_prn("  PF Number          : %X\n", bp->pf_num);
	dbg_prn("  BAR (0)            : %p %lx\n",
		bp->bar0, pci_bar_start(pdev, PCI_BASE_ADDRESS_0));
	dbg_prn("  BAR (1)            : %p %lx\n",
		bp->bar1, pci_bar_start(pdev, PCI_BASE_ADDRESS_2));
	dbg_prn("  BAR (2)            : %p %lx\n",
		bp->bar2, pci_bar_start(pdev, PCI_BASE_ADDRESS_4));
	dbg_prn(" ");
	pause_drv();
}
#else
#define dbg_pci(bp, func, creg)
#endif

#if defined(DEBUG_MEMORY)
void dbg_mem(struct bnxt *bp, const char *func)
{
	dbg_prn("- %s()\n", func);
	dbg_prn("  bp Addr            : %p", bp);
	dbg_prn(" Len %4d", (u16)sizeof(struct bnxt));
	dbg_prn(" phy %lx\n", virt_to_bus(bp));
	dbg_prn("  bp->hwrm_req_addr  : %p", bp->hwrm_addr_req);
	dbg_prn(" Len %4d", (u16)REQ_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", bp->req_addr_mapping);
	dbg_prn("  bp->hwrm_resp_addr : %p", bp->hwrm_addr_resp);
	dbg_prn(" Len %4d", (u16)RESP_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", bp->resp_addr_mapping);
	dbg_prn("  bp->dma_addr       : %p", bp->hwrm_addr_dma);
	dbg_prn(" Len %4d", (u16)DMA_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", bp->dma_addr_mapping);
	dbg_prn("  bp->tx.bd_virt     : %p", bp->tx.bd_virt);
	dbg_prn(" Len %4d", (u16)TX_RING_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", virt_to_bus(bp->tx.bd_virt));
	dbg_prn("  bp->rx.bd_virt     : %p", bp->rx.bd_virt);
	dbg_prn(" Len %4d", (u16)RX_RING_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", virt_to_bus(bp->rx.bd_virt));
	dbg_prn("  bp->cq.bd_virt     : %p", bp->cq.bd_virt);
	dbg_prn(" Len %4d", (u16)CQ_RING_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", virt_to_bus(bp->cq.bd_virt));
	dbg_prn("  bp->nq.bd_virt     : %p", bp->nq.bd_virt);
	dbg_prn(" Len %4d", (u16)NQ_RING_BUFFER_SIZE);
	dbg_prn(" phy %lx\n", virt_to_bus(bp->nq.bd_virt));
	dbg_prn(" ");
	pause_drv();
}
#else
#define dbg_mem(bp, func) (func = func)
#endif

#if defined(DEBUG_CHIP)
void dbg_fw_ver(struct hwrm_ver_get_output *resp, u32 tmo)
{
	if (resp->hwrm_intf_maj_8b < 1) {
		dbg_prn("  HWRM interface %d.%d.%d is older than 1.0.0.\n",
			resp->hwrm_intf_maj_8b, resp->hwrm_intf_min_8b,
			resp->hwrm_intf_upd_8b);
		dbg_prn("  Update FW with HWRM interface 1.0.0 or newer.\n");
	}
	dbg_prn("  FW Version         : %d.%d.%d.%d\n",
		resp->hwrm_fw_maj_8b, resp->hwrm_fw_min_8b,
		resp->hwrm_fw_bld_8b, resp->hwrm_fw_rsvd_8b);
	dbg_prn("  cmd timeout        : %d\n", tmo);
	if (resp->hwrm_intf_maj_8b >= 1)
		dbg_prn("  hwrm_max_req_len   : %d\n", resp->max_req_win_len);
	dbg_prn("  hwrm_max_ext_req   : %d\n", resp->max_ext_req_len);
	dbg_prn("  chip_num           : %x\n", resp->chip_num);
	dbg_prn("  chip_id            : %x\n",
		(u32)(resp->chip_rev << 24) |
		(u32)(resp->chip_metal << 16) |
		(u32)(resp->chip_bond_id << 8) |
		(u32)resp->chip_platform_type);
	test_if((resp->dev_caps_cfg & SHORT_CMD_SUPPORTED) &&
		(resp->dev_caps_cfg & SHORT_CMD_REQUIRED))
		dbg_prn("  SHORT_CMD_SUPPORTED\n");
}

void dbg_func_resource_qcaps(struct bnxt *bp)
{
// Ring Groups
	dbg_prn("  min_hw_ring_grps   : %d\n", bp->min_hw_ring_grps);
	dbg_prn("  max_hw_ring_grps   : %d\n", bp->max_hw_ring_grps);
// TX Rings
	dbg_prn("  min_tx_rings       : %d\n", bp->min_tx_rings);
	dbg_prn("  max_tx_rings       : %d\n", bp->max_tx_rings);
// RX Rings
	dbg_prn("  min_rx_rings       : %d\n", bp->min_rx_rings);
	dbg_prn("  max_rx_rings       : %d\n", bp->max_rx_rings);
// Completion Rings
	dbg_prn("  min_cq_rings       : %d\n", bp->min_cp_rings);
	dbg_prn("  max_cq_rings       : %d\n", bp->max_cp_rings);
// Statistic Contexts
	dbg_prn("  min_stat_ctxs      : %d\n", bp->min_stat_ctxs);
	dbg_prn("  max_stat_ctxs      : %d\n", bp->max_stat_ctxs);
}

void dbg_func_qcaps(struct bnxt *bp)
{
	dbg_prn("  Port Number        : %d\n", bp->port_idx);
	dbg_prn("  fid                : 0x%04x\n", bp->fid);
	dbg_prn("  PF MAC             : %02x:%02x:%02x:%02x:%02x:%02x\n",
		bp->mac_addr[0],
		bp->mac_addr[1],
		bp->mac_addr[2],
		bp->mac_addr[3],
		bp->mac_addr[4],
		bp->mac_addr[5]);
}

void dbg_func_qcfg(struct bnxt *bp)
{
	dbg_prn("  ordinal_value      : %d\n", bp->ordinal_value);
	dbg_prn("  stat_ctx_id        : %x\n", bp->stat_ctx_id);
	if (bp->vf) {
		dbg_func_qcaps(bp);
		dbg_prn("  vlan_id            : %d\n", bp->vlan_id);
	}
}

void prn_set_speed(u32 speed)
{
	u32 speed1 = ((speed & LINK_SPEED_DRV_MASK) >> LINK_SPEED_DRV_SHIFT);

	dbg_prn("  Set Link Speed     : ");
	switch (speed & LINK_SPEED_DRV_MASK) {
	case LINK_SPEED_DRV_1G:
		dbg_prn("1 GBPS");
		break;
	case LINK_SPEED_DRV_10G:
		dbg_prn("10 GBPS");
		break;
	case LINK_SPEED_DRV_25G:
		dbg_prn("25 GBPS");
		break;
	case LINK_SPEED_DRV_40G:
		dbg_prn("40 GBPS");
		break;
	case LINK_SPEED_DRV_50G:
		dbg_prn("50 GBPS");
		break;
	case LINK_SPEED_DRV_100G:
		dbg_prn("100 GBPS");
		break;
	case LINK_SPEED_DRV_200G:
		dbg_prn("200 GBPS");
		break;
	case LINK_SPEED_DRV_AUTONEG:
		dbg_prn("AUTONEG");
		break;
	default:
		dbg_prn("%x", speed1);
		break;
	}
	dbg_prn("\n");
}

void dbg_chip_info(struct bnxt *bp)
{
	if (bp->thor)
		dbg_prn("  NQ Ring Id         : %d\n", bp->nq_ring_id);
	else
		dbg_prn("  Grp ID             : %d\n", bp->ring_grp_id);
	dbg_prn("  Stat Ctx ID        : %d\n", bp->stat_ctx_id);
	dbg_prn("  CQ Ring Id         : %d\n", bp->cq_ring_id);
	dbg_prn("  Tx Ring Id         : %d\n", bp->tx_ring_id);
	dbg_prn("  Rx ring Id         : %d\n", bp->rx_ring_id);
	dbg_prn(" ");
	pause_drv();
}

void dbg_num_rings(struct bnxt *bp)
{
	dbg_prn("  num_cmpl_rings     : %d\n", bp->num_cmpl_rings);
	dbg_prn("  num_tx_rings       : %d\n", bp->num_tx_rings);
	dbg_prn("  num_rx_rings       : %d\n", bp->num_rx_rings);
	dbg_prn("  num_ring_grps      : %d\n", bp->num_hw_ring_grps);
	dbg_prn("  num_stat_ctxs      : %d\n", bp->num_stat_ctxs);
}

void dbg_flags(const char *func, u32 flags)
{
	dbg_prn("- %s()\n", func);
	dbg_prn("  bp->flags          : 0x%04x\n", flags);
}

void dbg_bnxt_pause(void)
{
	dbg_prn(" ");
	pause_drv();
}
#else
#define dbg_fw_ver(resp, tmo)
#define dbg_func_resource_qcaps(bp)
#define dbg_func_qcaps(bp)
#define dbg_func_qcfg(bp)
#define prn_set_speed(speed)
#define dbg_chip_info(bp)
#define dbg_num_rings(bp)
#define dbg_flags(func, flags)
#define dbg_bnxt_pause()
#endif

#if defined(DEBUG_HWRM_CMDS) || defined(DEBUG_FAIL)
void dump_hwrm_req(struct bnxt *bp, const char *func, u32 len, u32 tmo)
{
	dbg_prn("- %s(0x%04x) cmd_len %d cmd_tmo %d",
		func, (u16)((struct input *)bp->hwrm_addr_req)->req_type,
		len, tmo);
#if defined(DEBUG_HWRM_DUMP)
	dump_mem((u8 *)bp->hwrm_addr_req, len, DISP_U8);
#else
	dbg_prn("\n");
#endif
}

void debug_resp(struct bnxt *bp, const char *func, u32 resp_len, u16 err)
{
	dbg_prn("- %s(0x%04x) - ",
		func, (u16)((struct input *)bp->hwrm_addr_req)->req_type);
	if (err == STATUS_SUCCESS)
		dbg_prn("Done");
	else if (err != STATUS_TIMEOUT)
		dbg_prn("Fail err 0x%04x", err);
	else
		dbg_prn("timedout");
#if defined(DEBUG_HWRM_DUMP)
	if (err != STATUS_TIMEOUT) {
		dump_mem((u8 *)bp->hwrm_addr_resp, resp_len, DISP_U8);
		sleep(1);
	} else
		dbg_prn("\n");
#else
	resp_len = resp_len;
	dbg_prn("\n");
#endif
}

void dbg_hw_cmd(struct bnxt *bp,
		const char *func, u16 cmd_len,
		u16 resp_len, u32 cmd_tmo, u16 err)
{
#if !defined(DEBUG_HWRM_CMDS)
	if (err)
#endif
	{
		dump_hwrm_req(bp, func, cmd_len, cmd_tmo);
		debug_resp(bp, func, resp_len, err);
	}
}
#else
#define dbg_hw_cmd(bp, func, cmd_len, resp_len, cmd_tmo, err) (func = func)
#endif

#if defined(DEBUG_HWRM_CMDS)
void dbg_short_cmd(u8 *req, const char *func, u32 len)
{
	struct hwrm_short_input *sreq;

	sreq = (struct hwrm_short_input *)req;
	dbg_prn("- %s(0x%04x) short_cmd_len %d",
		func,
		sreq->req_type,
		(int)len);
#if defined(DEBUG_HWRM_DUMP)
	dump_mem((u8 *)sreq, len, DISP_U8);
#else
	dbg_prn("\n");
#endif
}
#else
#define dbg_short_cmd(sreq, func, len)
#endif

#if defined(DEBUG_RX)
void dump_rx_bd(struct rx_pkt_cmpl *rx_cmp,
		struct rx_pkt_cmpl_hi *rx_cmp_hi,
		u32 desc_idx)
{
	dbg_prn("  RX desc_idx %d PktLen %d\n", desc_idx, rx_cmp->len);
	dbg_prn("- rx_cmp    %lx", virt_to_bus(rx_cmp));
#if defined(DEBUG_RX_DUMP)
	dump_mem((u8 *)rx_cmp, (u32)sizeof(struct rx_pkt_cmpl), DISP_U8);
#else
	dbg_prn("\n");
#endif
	dbg_prn("- rx_cmp_hi %lx", virt_to_bus(rx_cmp_hi));
#if defined(DEBUG_RX_DUMP)
	dump_mem((u8 *)rx_cmp_hi, (u32)sizeof(struct rx_pkt_cmpl_hi), DISP_U8);
#else
	dbg_prn("\n");
#endif
}

void dbg_rx_vlan(struct bnxt *bp, u32 meta, u16 f2, u16 rx_vid)
{
	dbg_prn("  Rx VLAN metadata %x flags2 %x\n", meta, f2);
	dbg_prn("  Rx VLAN MBA %d TX %d RX %d\n",
		bp->vlan_id, bp->vlan_tx, rx_vid);
}

void dbg_alloc_rx_iob(struct io_buffer *iob, u16 id, u16 cid)
{
	dbg_prn("  Rx alloc_iob (%d) %p bd_virt (%d)\n",
		id, iob->data, cid);
}

void dbg_rx_cid(u16 idx, u16 cid)
{
	dbg_prn("- RX old cid %d new cid %d\n", idx, cid);
}

void dbg_alloc_rx_iob_fail(u16 iob_idx, u16 cons_id)
{
	dbg_prn("  Rx alloc_iob (%d) ", iob_idx);
	dbg_prn("failed for cons_id %d\n", cons_id);
}

void dbg_rxp(u8 *iob, u16 rx_len, u8 drop)
{
	dbg_prn("- RX iob %lx Len %d ", virt_to_bus(iob), rx_len);
	if (drop == 1)
		dbg_prn("drop ErrPkt ");
	else if (drop == 2)
		dbg_prn("drop LoopBack ");
	else if (drop == 3)
		dbg_prn("drop VLAN");
#if defined(DEBUG_RX_DUMP)
	dump_mem(iob, (u32)rx_len, DISP_U8);
#else
	dbg_prn("\n");
#endif
}

void dbg_rx_stat(struct bnxt *bp)
{
	dbg_prn("- RX Stat Total %d Good %d Drop err %d LB %d VLAN %d\n",
		bp->rx.cnt, bp->rx.good,
		bp->rx.drop_err, bp->rx.drop_lb, bp->rx.drop_vlan);
}
#else
#define dump_rx_bd(rx_cmp, rx_cmp_hi, desc_idx)
#define dbg_rx_vlan(bp, metadata, flags2, rx_vid)
#define dbg_alloc_rx_iob(iob, id, cid)
#define dbg_rx_cid(idx, cid)
#define dbg_alloc_rx_iob_fail(iob_idx, cons_id)
#define dbg_rxp(iob, rx_len, drop)
#define dbg_rx_stat(bp)
#endif

#if defined(DEBUG_CQ)
static void dump_cq(struct cmpl_base *cmp, u16 cid)
{
	dbg_prn("- CQ Type ");
	switch (cmp->type & CMPL_BASE_TYPE_MASK) {
	case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
		dbg_prn("(ae)");
		break;
	case CMPL_BASE_TYPE_STAT_EJECT:
		dbg_prn("(se)");
		break;
	case CMPL_BASE_TYPE_TX_L2:
		dbg_prn("(tx)");
		break;
	case CMPL_BASE_TYPE_RX_L2:
		dbg_prn("(rx)");
		break;
	default:
		dbg_prn("%04x", (u16)(cmp->type & CMPL_BASE_TYPE_MASK));
		break;
	}
	dbg_prn(" cid %d", cid);
#if defined(DEBUG_CQ_DUMP)
	dump_mem((u8 *)cmp, (u32)sizeof(struct cmpl_base), DISP_U8);
#else
	dbg_prn("\n");
#endif
}

static void dump_nq(struct nq_base *nqp, u16 cid)
{
	dbg_prn("- NQ Type %lx cid %d", (nqp->type & NQ_CN_TYPE_MASK), cid);
#if defined(DEBUG_CQ_DUMP)
	dump_mem((u8 *)nqp, (u32)sizeof(struct nq_base), DISP_U8);
#else
	dbg_prn("\n");
#endif
}
#else
#define dump_cq(cq, id)
#define dump_nq(nq, id)
#endif

#if defined(DEBUG_TX)
void dbg_tx_avail(struct bnxt *bp, u32 avail, u16 use)
{
	dbg_prn("- Tx BD %d Avail %d Use %d pid %d cid %d\n",
		bp->tx.ring_cnt,
		avail, use,
		bp->tx.prod_id,
		bp->tx.cons_id);
}

void dbg_tx_vlan(struct bnxt *bp, char *src, u16 plen, u16 len)
{
	dbg_prn("- Tx VLAN PKT %d MBA %d", bp->vlan_tx, bp->vlan_id);
	dbg_prn(" PKT %d",
		BYTE_SWAP_S(*(u16 *)(&src[MAC_HDR_SIZE + 2])));
	dbg_prn(" Pro %x",
		BYTE_SWAP_S(*(u16 *)(&src[MAC_HDR_SIZE])));
	dbg_prn(" old len %d new len %d\n", plen, len);
}

void dbg_tx_pad(u16 plen, u16 len)
{
	if (len != plen)
		dbg_prn("- Tx padded(0) old len %d new len %d\n", plen, len);
}

void dump_tx_stat(struct bnxt *bp)
{
	dbg_prn("  TX stats cnt %d req_cnt %d", bp->tx.cnt, bp->tx.cnt_req);
	dbg_prn(" prod_id %d cons_id %d\n", bp->tx.prod_id, bp->tx.cons_id);
}

void dump_tx_pkt(u8 *pkt, u16 len, u16 idx)
{
	dbg_prn("  TX(%d) Addr %lx Size %d", idx, virt_to_bus(pkt), len);
#if defined(DEBUG_TX_DUMP)
	dump_mem(pkt, (u32)len, DISP_U8);
#else
	dbg_prn("\n");
#endif
}

void dump_tx_bd(struct tx_bd_short *tx_bd, u16 len, int idx)
{
	dbg_prn("  Tx(%d) BD Addr %lx Size %d", idx, virt_to_bus(tx_bd), len);
#if defined(DEBUG_TX_DUMP)
	dump_mem((u8 *)tx_bd, (u32)len, DISP_U8);
#else
	dbg_prn("\n");
#endif
}

void dbg_tx_done(u8 *pkt, u16 len, u16 idx)
{
	dbg_prn("  Tx(%d) Done pkt %lx Size %d\n", idx, virt_to_bus(pkt), len);
}
#else
#define dbg_tx_avail(bp, a, u)
#define dbg_tx_vlan(bp, src, plen, len)
#define dbg_tx_pad(plen, len)
#define dump_tx_stat(bp)
#define dump_tx_pkt(pkt, len, idx)
#define dump_tx_bd(prod_bd, len, idx)
#define dbg_tx_done(pkt, len, idx)
#endif

#if defined(DEBUG_LINK)
static void dump_evt(u8 *cmp, u32 type, u16 cid, u8 ring)
{
	u32 size;
	u8  c;

	if (ring) {
		c = 'N';
		size = sizeof(struct nq_base);
	} else {
		c = 'C';
		size = sizeof(struct cmpl_base);
	}
	switch (type) {
	case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
		break;
	default:
		return;
	}
	dbg_prn("- %cQ Type (ae)  cid %d", c, cid);
	dump_mem(cmp, size, DISP_U8);
}

void dbg_link_info(struct bnxt *bp)
{
	dbg_prn("  Current Speed      : ");
	switch (bp->current_link_speed) {
	case PORT_PHY_QCFG_RESP_LINK_SPEED_200GB:
		dbg_prn("200 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_100GB:
		dbg_prn("100 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_50GB:
		dbg_prn("50 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_40GB:
		dbg_prn("40 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_25GB:
		dbg_prn("25 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_20GB:
		dbg_prn("20 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_10GB:
		dbg_prn("10 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_2_5GB:
		dbg_prn("2.5 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_2GB:
		dbg_prn("2 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_1GB:
		dbg_prn("1 %s", str_gbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_100MB:
		dbg_prn("100 %s", str_mbps);
		break;
	case PORT_PHY_QCFG_RESP_LINK_SPEED_10MB:
		dbg_prn("10 %s", str_mbps);
		break;
	default:
		dbg_prn("%x", bp->current_link_speed);
	}
	dbg_prn("\n");
	dbg_prn("  media_detect       : %x\n", bp->media_detect);
}

void dbg_link_status(struct bnxt *bp)
{
	dbg_prn("  Port(%d)            : Link", bp->port_idx);
	if (bp->link_status == STATUS_LINK_ACTIVE)
		dbg_prn("Up");
	else
		dbg_prn("Down");
	dbg_prn("\n");
}

void dbg_link_state(struct bnxt *bp, u32 tmo)
{
	dbg_link_status(bp);
	dbg_link_info(bp);
	dbg_prn("  Link wait time     : %d ms", tmo);
	pause_drv();
}
#else
#define dump_evt(cq, ty, id, ring)
#define dbg_link_status(bp)
#define dbg_link_state(bp, tmo)
#endif
