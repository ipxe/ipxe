/**************************************************************************
 *
 * Device driver for Solarflare Communications EF10 devices
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/pci.h>
#include <ipxe/malloc.h>
#include <ipxe/ethernet.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include "efx_hunt.h"
#include "efx_bitfield.h"
#include "ef10_regs.h"
#include "mc_driver_pcol.h"
#include <ipxe/if_ether.h>

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define	HUNTINGTON_NVRAM_CHUNK 0x80
#define HUNTINGTON_NVS_MAX_LENGTH 0x1000

#define EMCDI_IO(code)	EUNIQ(EINFO_EIO, (code))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#endif

struct hunt_nic *primary_nics = NULL;

struct hunt_nic {
	struct efx_nic efx;

	/* PHY information */
	unsigned int phy_cap_mask;
	unsigned int phy_cap;
	unsigned long link_poll_timer;

	/* resource housekeeping */
	uint64_t uc_filter_id;
	uint64_t mc_filter_id;
	u8 mac[ETH_ALEN];

	struct {
		/* Common payload for all MCDI requests */
		unsigned int seqno;

		size_t resp_hdr_len;
		size_t resp_data_len;

		struct io_buffer *iob;
		uint64_t dma_addr;
	} mcdi;

	struct hunt_nic *primary;
	struct hunt_nic *next_primary;
	u32 flags;
};

static int hunt_nic_is_primary(struct hunt_nic *hunt)
{
	return (hunt->flags & (1 << MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_PRIMARY));
}

/*******************************************************************************
 *
 *
 * MCDI transport
 *
 * This has been based on the implementation of MCDI in the common code driver.
 *
 *
 ******************************************************************************/

static int hunt_mcdi_init(struct hunt_nic *hunt)
{
	size_t max_msg_size;
	int rc;

	/* The MCDI message has two 32-bit headers (the MCDI header and the
	 * MCDI v2 extended command) and then up to MCDI_CTL_SDU_LEN_MAX_V2
	 * bytes of payload
	 */
	max_msg_size = 2 * sizeof(efx_dword_t) + MCDI_CTL_SDU_LEN_MAX_V2;

	hunt->mcdi.iob = alloc_iob(max_msg_size);
	if (!hunt->mcdi.iob) {
		rc = -ENOMEM;
		return rc;
	}
	return 0;
}

static void hunt_mcdi_copyin(struct hunt_nic *hunt,
			     unsigned int cmd,
			     uint8_t *inbuf,
			     size_t inlen)
{
	efx_dword_t hdr[2];
	uint32_t seqno;
	unsigned int xflags;
	size_t hdr_len;
	u8 *pdu = hunt->mcdi.iob->data;

	seqno = hunt->mcdi.seqno & MCDI_SEQ_MASK;

	xflags = 0;

	EFX_POPULATE_DWORD_7(hdr[0],
			     MCDI_HEADER_CODE, MC_CMD_V2_EXTN,
			     MCDI_HEADER_RESYNC, 1,
			     MCDI_HEADER_DATALEN, 0,
			     MCDI_HEADER_SEQ, seqno,
			     MCDI_HEADER_ERROR, 0,
			     MCDI_HEADER_RESPONSE, 0,
			     MCDI_HEADER_XFLAGS, xflags);
	EFX_POPULATE_DWORD_2(hdr[1],
			     MC_CMD_V2_EXTN_IN_EXTENDED_CMD, cmd,
			     MC_CMD_V2_EXTN_IN_ACTUAL_LEN, inlen);

	hdr_len = sizeof(hdr);

	memcpy(pdu, &hdr, hdr_len);
	assert(inlen <= MCDI_CTL_SDU_LEN_MAX_V2);
	memcpy(pdu + hdr_len, inbuf, inlen);

	wmb();	/* Sync the data before ringing the doorbell */

	/* Ring the doorbell to post the command DMA address to the MC */
	hunt->mcdi.dma_addr = virt_to_bus(hunt->mcdi.iob->data);

	assert((hunt->mcdi.dma_addr & 0xFF) == 0);

	_efx_writel(&hunt->efx,
		   cpu_to_le32((u64)hunt->mcdi.dma_addr >> 32),
		   ER_DZ_MC_DB_LWRD);

	_efx_writel(&hunt->efx,
		   cpu_to_le32((u32)hunt->mcdi.dma_addr),
		   ER_DZ_MC_DB_HWRD);
}

static void hunt_mcdi_copyout(struct hunt_nic *hunt,
			      uint8_t *outbuf, size_t outlen)
{
	size_t offset;
	const u8 *pdu = hunt->mcdi.iob->data;

	offset =  hunt->mcdi.resp_hdr_len;

	if (outlen > 0)
		memcpy(outbuf, pdu+offset, outlen);
}

static int hunt_mcdi_request_poll(struct hunt_nic *hunt, bool quiet)
{
	unsigned int resplen, respseq, error;
	unsigned long finish;
	efx_dword_t errdword;
	efx_qword_t qword;
	const efx_dword_t *pdu = hunt->mcdi.iob->data;
	const u8 *pdu1 = hunt->mcdi.iob->data;
	int delay, rc;

	/* Spin for up to 5s, polling at intervals of 10us, 20us, ... ~100ms  */
	finish = currticks() + (5 * TICKS_PER_SEC);
	delay = 10;
	while (1) {
		udelay(delay);

		/* Check for an MCDI response */
		if (EFX_DWORD_FIELD(*pdu, MCDI_HEADER_RESPONSE))
			break;

		if (currticks() >= finish)
			return -ETIMEDOUT;

		if (delay < 100000)
			delay *= 2;
	}

	memcpy(&qword, pdu1, 8);

	/* qword.dword[0] is the MCDI header; qword.dword[1] is the MCDI v2
	 * extended command
	 */
	respseq = EFX_DWORD_FIELD(qword.dword[0], MCDI_HEADER_SEQ);
	error = EFX_DWORD_FIELD(qword.dword[0], MCDI_HEADER_ERROR);
	resplen = EFX_DWORD_FIELD(qword.dword[1], MC_CMD_V2_EXTN_IN_ACTUAL_LEN);

	if (error && resplen == 0) {
		if (!quiet)
			DBGC(hunt, "MC rebooted\n");
		return -EIO;
	} else if ((respseq ^ hunt->mcdi.seqno) & MCDI_SEQ_MASK) {
		if (!quiet)
			DBGC(hunt, "MC response mismatch rxseq 0x%x txseq "
			     "0x%x\n", respseq, hunt->mcdi.seqno);
		return -EIO;
	} else if (error) {
		memcpy(&errdword, pdu1 + 8, 4);
		rc = EFX_DWORD_FIELD(errdword, EFX_DWORD_0);
		switch (rc) {
		case MC_CMD_ERR_ENOENT:
			return -ENOENT;
		case MC_CMD_ERR_EINTR:
			return -EINTR;
		case MC_CMD_ERR_EACCES:
			return -EACCES;
		case MC_CMD_ERR_EBUSY:
			return -EBUSY;
		case MC_CMD_ERR_EINVAL:
			return -EINVAL;
		case MC_CMD_ERR_EDEADLK:
			return -EDEADLK;
		case MC_CMD_ERR_ENOSYS:
			return -ENOSYS;
		case MC_CMD_ERR_ETIME:
			return -ETIME;
		case MC_CMD_ERR_EPERM:
			return -EPERM;
		default:
			/* Return the MC error in an I/O error. */
			return EMCDI_IO(rc & 0xff);
		}
	}
	hunt->mcdi.resp_hdr_len = 8;
	hunt->mcdi.resp_data_len = resplen;

	return 0;
}

static void hunt_mcdi_fini(struct hunt_nic *hunt)
{
	free_iob(hunt->mcdi.iob);
}

int _hunt_mcdi(struct efx_nic *efx, unsigned int cmd,
	       const efx_dword_t *inbuf, size_t inlen,
	       efx_dword_t *outbuf, size_t outlen,
	       size_t *outlen_actual, bool quiet)
{
	int rc;
	struct hunt_nic *hunt = (struct hunt_nic *) efx;
	size_t local_outlen_actual;

	if (outlen_actual == NULL)
		outlen_actual = &local_outlen_actual;

	++hunt->mcdi.seqno;
	hunt_mcdi_copyin(hunt, cmd, (uint8_t *) inbuf, inlen);

	rc = hunt_mcdi_request_poll(hunt, quiet);
	if (rc != 0) {
		if (!quiet)
			DBGC(hunt, "MC response to cmd 0x%x: %s\n",
			     cmd, strerror(rc));
		return rc;
	}

	*outlen_actual = hunt->mcdi.resp_data_len;

	hunt_mcdi_copyout(hunt, (uint8_t *) outbuf, outlen);

	return 0;
}

static int hunt_mcdi(struct hunt_nic *hunt, struct efx_mcdi_req_s *req)
{
	return _hunt_mcdi(&hunt->efx, req->emr_cmd,
			  (const efx_dword_t *) req->emr_in_buf,
			  req->emr_in_length,
			  (efx_dword_t *) req->emr_out_buf, req->emr_out_length,
			  &req->emr_out_length_used, false);
}

static int hunt_mcdi_quiet(struct hunt_nic *hunt, struct efx_mcdi_req_s *req)
{
	return _hunt_mcdi(&hunt->efx, req->emr_cmd,
			  (const efx_dword_t *) req->emr_in_buf,
			  req->emr_in_length,
			  (efx_dword_t *) req->emr_out_buf, req->emr_out_length,
			  &req->emr_out_length_used, true);
}

/*******************************************************************************
 *
 *
 * Hardware initialization
 *
 *
 ******************************************************************************/
static int hunt_get_workarounds(struct hunt_nic *hunt, uint32_t *implemented,
				uint32_t *enabled)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_WORKAROUNDS_OUT_LEN);
	int rc;

	*implemented = *enabled = 0;

	req.emr_cmd = MC_CMD_GET_WORKAROUNDS;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	rc = hunt_mcdi(hunt, &req);

	if (rc)
		return rc;

	if (req.emr_out_length_used < MC_CMD_GET_WORKAROUNDS_OUT_LEN)
		return -EMSGSIZE;

	*implemented = MCDI_DWORD(outbuf, GET_WORKAROUNDS_OUT_IMPLEMENTED);
	*enabled = MCDI_DWORD(outbuf, GET_WORKAROUNDS_OUT_ENABLED);
	return 0;
}

static int hunt_enable_workaround_35388(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(payload, MC_CMD_WORKAROUND_IN_LEN);

	req.emr_cmd = MC_CMD_WORKAROUND;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_WORKAROUND_IN_LEN;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, WORKAROUND_IN_TYPE,
		       MC_CMD_WORKAROUND_BUG35388);
	MCDI_SET_DWORD(req.emr_in_buf, WORKAROUND_IN_ENABLED, 1);

	/* If the firmware doesn't support this workaround, hunt_mcdi() will
	 * return -EINVAL from hunt_mcdi_request_poll().
	 */
	return hunt_mcdi(hunt, &req);
}

static int hunt_workaround_35388(struct hunt_nic *hunt)
{
	uint32_t implemented, enabled;
	int rc = hunt_get_workarounds(hunt, &implemented, &enabled);

	if (rc < 0)
		return 0;
	if (!(implemented & MC_CMD_GET_WORKAROUNDS_OUT_BUG35388))
		return 0;
	if (enabled & MC_CMD_GET_WORKAROUNDS_OUT_BUG35388)
		return 1;

	rc = hunt_enable_workaround_35388(hunt);
	if (rc == 0)
		return 1; /* Workaround is enabled */
	else
		return 0;
}

static int hunt_get_port_assignment(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN);
	int rc;

	req.emr_cmd = MC_CMD_GET_PORT_ASSIGNMENT;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	hunt->efx.port = MCDI_DWORD(req.emr_out_buf,
				    GET_PORT_ASSIGNMENT_OUT_PORT);
	return 0;
}

static int hunt_mac_addr(struct hunt_nic *hunt, uint8_t *ll_addr)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_MAC_ADDRESSES_OUT_LEN);
	int rc;

	req.emr_cmd = MC_CMD_GET_MAC_ADDRESSES;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = MC_CMD_GET_MAC_ADDRESSES_OUT_LEN;

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used < MC_CMD_GET_MAC_ADDRESSES_OUT_LEN)
		return -EMSGSIZE;

	memcpy(ll_addr,
	       MCDI_PTR(req.emr_out_buf, GET_MAC_ADDRESSES_OUT_MAC_ADDR_BASE),
	       ETH_ALEN);

	return 0;
}

static int hunt_get_phy_cfg(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_PHY_CFG_OUT_LEN);
	int rc;

	req.emr_cmd = MC_CMD_GET_PHY_CFG;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used < MC_CMD_GET_PHY_CFG_OUT_LEN)
		return -EMSGSIZE;

	hunt->phy_cap_mask = hunt->phy_cap =
		MCDI_DWORD(req.emr_out_buf, GET_PHY_CFG_OUT_SUPPORTED_CAP);
	DBGC2(hunt, "GET_PHY_CFG: flags=%x, caps=%x\n", rc, hunt->phy_cap);
	return 0;
}

static int hunt_driver_attach(struct hunt_nic *hunt, int attach)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(inbuf, MC_CMD_DRV_ATTACH_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_DRV_ATTACH_EXT_OUT_LEN);
	int rc;

	req.emr_cmd = MC_CMD_DRV_ATTACH;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	/* Set the PREBOOT flag to indicate later instances of attach should
	 * force an ENTITY RESET
	 */
	if (attach)
		attach |= 1 << MC_CMD_DRV_PREBOOT_LBN;

	MCDI_SET_DWORD(req.emr_in_buf, DRV_ATTACH_IN_NEW_STATE, attach);
	MCDI_SET_DWORD(req.emr_in_buf, DRV_ATTACH_IN_UPDATE, 1);
	MCDI_SET_DWORD(req.emr_in_buf, DRV_ATTACH_IN_FIRMWARE_ID,
		       MC_CMD_FW_DONT_CARE);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used < MC_CMD_DRV_ATTACH_OUT_LEN)
		return -EMSGSIZE;

	hunt->flags = MCDI_DWORD(outbuf, DRV_ATTACH_EXT_OUT_FUNC_FLAGS);

	return 0;
}

static int hunt_reset(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(inbuf, MC_CMD_ENTITY_RESET_IN_LEN);

	req.emr_cmd = MC_CMD_ENTITY_RESET;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_POPULATE_DWORD_1(req.emr_in_buf, ENTITY_RESET_IN_FLAG,
			      ENTITY_RESET_IN_FUNCTION_RESOURCE_RESET, 1);
	return hunt_mcdi(hunt, &req);
}

static void hunt_clear_udp_tunnel_ports(struct hunt_nic *hunt)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LENMAX);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_LEN);
	struct efx_mcdi_req_s req;
	int rc;

	memset(inbuf, 0, MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LENMAX);
	MCDI_SET_DWORD(inbuf, SET_TUNNEL_ENCAP_UDP_PORTS_IN_FLAGS,
		(1 << MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_UNLOADING_LBN));

	req.emr_cmd = MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	rc = hunt_mcdi_quiet(hunt, &req);
	if (rc)
		return;

	if (MCDI_DWORD(outbuf, SET_TUNNEL_ENCAP_UDP_PORTS_OUT_FLAGS) &
	    (1 << MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_RESETTING_LBN)) {
		DBGC(hunt,
		     "Rebooting MC due to clearing UDP tunnel port list\n");
		/* Delay for the MC reboot to complete. */
		mdelay(100);
	}
}

static int hunt_set_mac(struct hunt_nic *hunt)
{
	struct net_device *netdev = hunt->efx.netdev;
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(payload, MC_CMD_SET_MAC_IN_LEN);
	unsigned int fcntl;
	int rc;

	req.emr_cmd = MC_CMD_SET_MAC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MAC_IN_LEN;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, SET_MAC_IN_MTU,
		       EFX_MAC_FRAME_LEN(ETH_FRAME_LEN));
	MCDI_SET_DWORD(req.emr_in_buf, SET_MAC_IN_DRAIN, 0);
	memcpy(MCDI_PTR(req.emr_in_buf, SET_MAC_IN_ADDR),
	       netdev->ll_addr, ETH_ALEN);
	MCDI_SET_DWORD(req.emr_in_buf, SET_MAC_IN_REJECT, 0);

	/* If the PHY supports autnegotiation, then configure the MAC to match
	 * the negotiated settings. Otherwise force the MAC to TX and RX flow
	 * control.
	 */
	if (hunt->phy_cap_mask & (1 << MC_CMD_PHY_CAP_AN_LBN))
		fcntl = MC_CMD_FCNTL_AUTO;
	else
		fcntl = MC_CMD_FCNTL_BIDIR;
	MCDI_SET_DWORD(req.emr_in_buf, SET_MAC_IN_FCNTL, fcntl);

	rc = hunt_mcdi(hunt, &req);
	/* Ignore failure for permissions reasons */
	if (rc == -EPERM)
		rc = 0;
	return rc;
}

static int hunt_alloc_vis(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(inbuf, MC_CMD_ALLOC_VIS_IN_LEN);

	req.emr_cmd = MC_CMD_ALLOC_VIS;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, ALLOC_VIS_IN_MIN_VI_COUNT, 1);
	MCDI_SET_DWORD(req.emr_in_buf, ALLOC_VIS_IN_MAX_VI_COUNT, 1);

	return hunt_mcdi(hunt, &req);
}

static void hunt_free_vis(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	int rc;

	req.emr_cmd = MC_CMD_FREE_VIS;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		DBGC(hunt, "MC_CMD_FREE_VIS Failed\n");
}

/*******************************************************************************
 *
 *
 * Link state handling
 *
 *
 ******************************************************************************/
static int hunt_check_link(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_LINK_OUT_LEN);
	unsigned int flags, speed;
	bool up;
	int rc;
	static bool link_state = false;

	req.emr_cmd = MC_CMD_GET_LINK;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used < MC_CMD_GET_LINK_OUT_LEN)
		return -EMSGSIZE;

	flags = MCDI_DWORD(req.emr_out_buf, GET_LINK_OUT_FLAGS);
	up = !!(flags & (1 << MC_CMD_GET_LINK_OUT_LINK_UP_LBN));
	speed = MCDI_DWORD(req.emr_out_buf, GET_LINK_OUT_LINK_SPEED);

	/* Set netdev_link_*() based on the link status from the MC */
	if (up && speed)
		netdev_link_up(hunt->efx.netdev);
	else
		netdev_link_down(hunt->efx.netdev);

	if (up != link_state) {
		DBGC(hunt, "Link %s, flags=%x, our caps=%x, lpa=%x, speed=%d, fcntl=%x, mac_fault=%x\n",
		     (up? "up": "down"), flags,
		     MCDI_DWORD(req.emr_out_buf, GET_LINK_OUT_CAP),
		     MCDI_DWORD(req.emr_out_buf, GET_LINK_OUT_LP_CAP),
		     speed,
		     MCDI_DWORD(req.emr_out_buf, GET_LINK_OUT_FCNTL),
		     MCDI_DWORD(req.emr_out_buf, GET_LINK_OUT_MAC_FAULT));
		link_state = up;
	}

	return 0;
}

#define MCDI_PORT_SPEED_CAPS   ((1 << MC_CMD_PHY_CAP_10HDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_10FDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_100HDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_100FDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_1000HDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_1000FDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_10000FDX_LBN) | \
				(1 << MC_CMD_PHY_CAP_40000FDX_LBN))

/*******************************************************************************
 *
 *
 * TX
 *
 *
 ******************************************************************************/
static int
hunt_tx_init(struct net_device *netdev, struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	dma_addr_t dma_addr;
	efx_qword_t *addr;
	MCDI_DECLARE_BUF(inbuf,
			 MC_CMD_INIT_TXQ_IN_LEN(EFX_TXQ_NBUFS(EFX_TXD_SIZE)));
	int rc, npages;

	rc = efx_hunt_tx_init(netdev, &dma_addr);
	if (rc != 0)
		return rc;

	npages = EFX_TXQ_NBUFS(EFX_TXD_SIZE);

	req.emr_cmd = MC_CMD_INIT_TXQ;
	req.emr_in_buf = inbuf;
	req.emr_in_length = MC_CMD_INIT_TXQ_IN_LEN(npages);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, INIT_TXQ_IN_SIZE, EFX_TXD_SIZE);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_TXQ_IN_TARGET_EVQ, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_TXQ_IN_LABEL, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_TXQ_IN_INSTANCE, 0);

	MCDI_POPULATE_DWORD_6(req.emr_in_buf, INIT_TXQ_IN_FLAGS,
			      INIT_TXQ_IN_FLAG_BUFF_MODE, 0,
			      INIT_TXQ_IN_FLAG_IP_CSUM_DIS, 1,
			      INIT_TXQ_IN_FLAG_TCP_CSUM_DIS, 1,
			      INIT_TXQ_IN_FLAG_TCP_UDP_ONLY, 0,
			      INIT_TXQ_IN_CRC_MODE, 0,
			      INIT_TXQ_IN_FLAG_TIMESTAMP, 0);

	MCDI_SET_DWORD(req.emr_in_buf, INIT_TXQ_IN_OWNER_ID, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_TXQ_IN_PORT_ID,
		       EVB_PORT_ID_ASSIGNED);

	addr = (efx_qword_t *) MCDI_PTR(req.emr_in_buf, INIT_TXQ_IN_DMA_ADDR);

	EFX_POPULATE_QWORD_2(*addr,
			     EFX_DWORD_1, (uint32_t)(dma_addr >> 32),
			     EFX_DWORD_0, (uint32_t)(dma_addr & 0xffffffff));

	return hunt_mcdi(hunt, &req);
}

static void hunt_tx_fini(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(inbuf, MC_CMD_FINI_TXQ_IN_LEN);
	struct efx_nic *efx = &hunt->efx;
	struct efx_tx_queue *txq = &efx->txq;
	int rc;

	req.emr_cmd = MC_CMD_FINI_TXQ;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, FINI_TXQ_IN_INSTANCE, 0);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		DBGC(hunt, "MC_CMD_FINI_TXQ Failed\n");

	efx_hunt_free_special_buffer(txq->ring,
				     sizeof(efx_tx_desc_t) * EFX_TXD_SIZE);
	txq->ring = NULL;
}

/*******************************************************************************
 *
 *
 * RX
 *
 *
 ******************************************************************************/
static int hunt_rx_filter_insert(struct net_device *netdev,
				 struct hunt_nic *hunt,
				 int multicast)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(inbuf, MC_CMD_FILTER_OP_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_FILTER_OP_OUT_LEN);
	int rc;
	uint64_t filter_id;
	(void) netdev;

	req.emr_cmd = MC_CMD_FILTER_OP;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_OP,
		       multicast ? MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE
				 : MC_CMD_FILTER_OP_IN_OP_INSERT);
	MCDI_POPULATE_DWORD_1(req.emr_in_buf, FILTER_OP_IN_MATCH_FIELDS,
			      FILTER_OP_IN_MATCH_DST_MAC, 1);
	if (multicast)
		memset(MCDI_PTR(req.emr_in_buf, FILTER_OP_IN_DST_MAC),
		       0xff, ETH_ALEN);
	else
		memcpy(MCDI_PTR(req.emr_in_buf, FILTER_OP_IN_DST_MAC),
		       hunt->mac, ETH_ALEN);

	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_PORT_ID,
		       EVB_PORT_ID_ASSIGNED);
	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_RX_DEST,
			MC_CMD_FILTER_OP_IN_RX_DEST_HOST);
	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_RX_QUEUE, 0);
	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_RX_MODE, 0);
	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_TX_DEST,
		       MC_CMD_FILTER_OP_IN_TX_DEST_DEFAULT);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used <  MC_CMD_FILTER_OP_OUT_LEN)
		return -EIO;

	filter_id = MCDI_QWORD(req.emr_out_buf, FILTER_OP_OUT_HANDLE);
	if (multicast)
		hunt->mc_filter_id = filter_id;
	else
		hunt->uc_filter_id = filter_id;

	return 0;
}

static int hunt_rx_filter_remove(struct hunt_nic *hunt,
				 int multicast)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(inbuf, MC_CMD_FILTER_OP_IN_LEN);

	req.emr_cmd = MC_CMD_FILTER_OP;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, FILTER_OP_IN_OP,
		       multicast ? MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE :
		       MC_CMD_FILTER_OP_IN_OP_REMOVE);
	MCDI_SET_QWORD(req.emr_in_buf, FILTER_OP_IN_HANDLE,
			  multicast ? hunt->mc_filter_id :
				      hunt->uc_filter_id);
	return hunt_mcdi(hunt, &req);
}

static int hunt_get_mac(struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_MAC_ADDRESSES_OUT_LEN);
	int rc;

	req.emr_cmd = MC_CMD_GET_MAC_ADDRESSES;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used < MC_CMD_INIT_EVQ_OUT_LEN)
		return -EMSGSIZE;

	memcpy(hunt->mac, MCDI_PTR(outbuf, GET_MAC_ADDRESSES_OUT_MAC_ADDR_BASE),
	       ETH_ALEN);
	return 0;
}

static int hunt_rx_filter_init(struct net_device *netdev,
			       struct hunt_nic *hunt)
{
	int rc = hunt_get_mac(hunt);

	if (rc != 0)
		return rc;

	rc = hunt_rx_filter_insert(netdev, hunt, 0);
	if (rc != 0)
		return rc;

	rc = hunt_rx_filter_insert(netdev, hunt, 1);
	if (rc != 0)
		hunt_rx_filter_remove(hunt, 0);

	return rc;
}

static int
hunt_rx_init(struct net_device *netdev,
	     struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	dma_addr_t dma_addr;
	efx_qword_t *addr;
	MCDI_DECLARE_BUF(inbuf,
			 MC_CMD_INIT_RXQ_IN_LEN(EFX_RXQ_NBUFS(EFX_RXD_SIZE)));
	int rc, npages;

	rc = efx_hunt_rx_init(netdev, &dma_addr);
	if (rc != 0)
		return rc;

	npages = EFX_RXQ_NBUFS(EFX_RXD_SIZE);

	req.emr_cmd = MC_CMD_INIT_RXQ;
	req.emr_in_buf = inbuf;
	req.emr_in_length = MC_CMD_INIT_RXQ_IN_LEN(npages);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, INIT_RXQ_IN_SIZE, EFX_RXD_SIZE);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_RXQ_IN_TARGET_EVQ, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_RXQ_IN_LABEL, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_RXQ_IN_INSTANCE, 0);
	MCDI_POPULATE_DWORD_5(req.emr_in_buf, INIT_RXQ_IN_FLAGS,
				 INIT_RXQ_IN_FLAG_BUFF_MODE, 0,
				 INIT_RXQ_IN_FLAG_HDR_SPLIT, 0,
				 INIT_RXQ_IN_FLAG_TIMESTAMP, 0,
				 INIT_RXQ_IN_CRC_MODE, 0,
				 INIT_RXQ_IN_FLAG_PREFIX, 1);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_RXQ_IN_OWNER_ID, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_RXQ_IN_PORT_ID,
		       EVB_PORT_ID_ASSIGNED);

	addr = (efx_qword_t *) MCDI_PTR(req.emr_in_buf, INIT_RXQ_IN_DMA_ADDR);

	EFX_POPULATE_QWORD_2(*addr,
			     EFX_DWORD_1, (uint32_t)(dma_addr >> 32),
			     EFX_DWORD_0, (uint32_t)(dma_addr & 0xffffffff));
	return hunt_mcdi(hunt, &req);
}

static void hunt_rx_filter_fini(struct hunt_nic *hunt)
{
	hunt_rx_filter_remove(hunt, 0);
	hunt_rx_filter_remove(hunt, 1);
}

static void hunt_rx_fini(struct hunt_nic *hunt)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_FINI_RXQ_IN_LEN);
	struct efx_mcdi_req_s req;
	struct efx_nic *efx = &hunt->efx;
	struct efx_rx_queue *rxq = &efx->rxq;
	int rc;

	req.emr_cmd = MC_CMD_FINI_RXQ;
	req.emr_in_buf = inbuf;
	req.emr_in_length = MC_CMD_FINI_RXQ_IN_LEN;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, FINI_RXQ_IN_INSTANCE, 0);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		DBGC(hunt, "MC_CMD_FINI_RXQ Failed\n");

	efx_hunt_free_special_buffer(rxq->ring,
				     sizeof(efx_rx_desc_t) * EFX_RXD_SIZE);
	rxq->ring = NULL;
}

/*******************************************************************************
 *
 *
 * Event queues and interrupts
 *
 *
 ******************************************************************************/
static int
hunt_ev_init(struct net_device *netdev,
	     struct hunt_nic *hunt)
{
	struct efx_mcdi_req_s req;
	dma_addr_t dma_addr;
	efx_qword_t *addr;
	MCDI_DECLARE_BUF(inbuf,
			 MC_CMD_INIT_EVQ_IN_LEN(EFX_EVQ_NBUFS(EFX_EVQ_SIZE)));
	MCDI_DECLARE_BUF(outbuf, MC_CMD_INIT_EVQ_OUT_LEN);
	int rc, npages;

	rc = efx_hunt_ev_init(netdev, &dma_addr);
	if (rc != 0)
		return rc;

	npages = EFX_EVQ_NBUFS(EFX_EVQ_SIZE);

	req.emr_cmd = MC_CMD_INIT_EVQ;
	req.emr_in_buf = inbuf;
	req.emr_in_length = MC_CMD_INIT_EVQ_IN_LEN(npages);
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof(outbuf);

	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_SIZE, EFX_EVQ_SIZE);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_INSTANCE, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_IRQ_NUM, 0);

	MCDI_POPULATE_DWORD_6(req.emr_in_buf, INIT_EVQ_IN_FLAGS,
				 INIT_EVQ_IN_FLAG_INTERRUPTING, 1,
				 INIT_EVQ_IN_FLAG_RPTR_DOS, 0,
				 INIT_EVQ_IN_FLAG_INT_ARMD, 0,
				 INIT_EVQ_IN_FLAG_CUT_THRU, 0,
				 INIT_EVQ_IN_FLAG_RX_MERGE, 0,
				 INIT_EVQ_IN_FLAG_TX_MERGE, 0);

	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_TMR_MODE,
			  MC_CMD_INIT_EVQ_IN_TMR_MODE_DIS);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_TMR_LOAD, 0);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_TMR_RELOAD, 0);

	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_COUNT_MODE,
			  MC_CMD_INIT_EVQ_IN_COUNT_MODE_DIS);
	MCDI_SET_DWORD(req.emr_in_buf, INIT_EVQ_IN_COUNT_THRSHLD, 0);

	addr = (efx_qword_t *) MCDI_PTR(req.emr_in_buf, INIT_EVQ_IN_DMA_ADDR);

	EFX_POPULATE_QWORD_2(*addr,
			     EFX_DWORD_1, (uint32_t)(dma_addr >> 32),
			     EFX_DWORD_0, (uint32_t)(dma_addr & 0xffffffff));
	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		return rc;

	if (req.emr_out_length_used < MC_CMD_INIT_EVQ_OUT_LEN)
		return -EMSGSIZE;

	return 0;
}

static void hunt_ev_fini(struct hunt_nic *hunt)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_FINI_EVQ_IN_LEN);
	struct efx_mcdi_req_s req;
	struct efx_nic *efx = &hunt->efx;
	struct efx_ev_queue *evq = &efx->evq;
	int rc;

	req.emr_cmd = MC_CMD_FINI_EVQ;
	req.emr_in_buf = inbuf;
	req.emr_in_length = sizeof(inbuf);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_SET_DWORD(req.emr_in_buf, FINI_EVQ_IN_INSTANCE, 0);

	rc = hunt_mcdi(hunt, &req);
	if (rc != 0)
		DBGC(hunt, "MC_CMD_FINI_EVQ Failed\n");

	efx_hunt_free_special_buffer(evq->ring,
				     sizeof(efx_event_t) * EFX_EVQ_SIZE);
	evq->ring = NULL;
}

static void
hunt_poll(struct net_device *netdev)
{
	struct hunt_nic *hunt = netdev_priv(netdev);

	/* If called while already polling, return immediately */
	if (hunt->efx.state & EFX_STATE_POLLING)
		return;
	hunt->efx.state |= EFX_STATE_POLLING;

	/* Poll link state */
	if (hunt->link_poll_timer + TICKS_PER_SEC < currticks()) {
		hunt->link_poll_timer = currticks();
		hunt_check_link(hunt);
	}

	/* Poll data path */
	efx_hunt_poll(netdev);

	hunt->efx.state &= ~EFX_STATE_POLLING;
}

/*******************************************************************************
 *
 *
 * Netdevice operations
 *
 *
 ******************************************************************************/
static int hunt_open(struct net_device *netdev)
{
	struct hunt_nic *hunt = netdev_priv(netdev);
	int rc;

	/* Allocate VIs */
	rc = hunt_alloc_vis(hunt);
	if (rc != 0)
		goto fail2;

	/* Initialize data path */
	rc = hunt_ev_init(netdev, hunt);
	if (rc != 0)
		goto fail3;

	rc = hunt_rx_init(netdev, hunt);
	if (rc != 0)
		goto fail4;

	rc = hunt_rx_filter_init(netdev, hunt);
	if (rc != 0)
		goto fail5;

	rc = hunt_tx_init(netdev, hunt);
	if (rc != 0)
		goto fail6;

	rc = efx_hunt_open(netdev);
	if (rc)
		goto fail7;

	rc = hunt_set_mac(hunt);
	if (rc)
		goto fail8;

	/* Mark the link as down before checking the link state because the
	 * latter might fail.
	 */
	netdev_link_down(netdev);
	hunt_check_link(hunt);

	DBGC2(hunt, "%s: open ok\n", netdev->name);
	return 0;

fail8:
	efx_hunt_close(netdev);
fail7:
	hunt_tx_fini(hunt);
fail6:
	hunt_rx_filter_fini(hunt);
fail5:
	hunt_rx_fini(hunt);
fail4:
	hunt_ev_fini(hunt);
fail3:
	hunt_free_vis(hunt);
fail2:
	DBGC2(hunt, "%s: %s\n", netdev->name, strerror(rc));
	return rc;
}


static void hunt_close(struct net_device *netdev)
{
	struct hunt_nic *hunt = netdev_priv(netdev);

	/* Stop datapath */
	efx_hunt_close(netdev);

	hunt_tx_fini(hunt);
	hunt_rx_fini(hunt);
	hunt_rx_filter_fini(hunt);
	hunt_ev_fini(hunt);

	hunt_free_vis(hunt);

	/* Reset hardware and detach */
	hunt_reset(hunt);
}


/*******************************************************************************
 *
 *
 * Public operations
 *
 *
 ******************************************************************************/

static struct net_device_operations hunt_operations = {
	.open                  = hunt_open,
	.close                 = hunt_close,
	.transmit              = efx_hunt_transmit,
	.poll                  = hunt_poll,
	.irq                   = efx_hunt_irq,
};

static int
hunt_probe(struct pci_device *pci)
{
	struct net_device *netdev;
	struct hunt_nic *hunt;
	struct efx_nic *efx;
	int rc = 0;

	/* Create the network adapter */
	netdev = alloc_etherdev(sizeof(struct hunt_nic));
	if (!netdev) {
		rc = -ENOMEM;
		goto fail1;
	}

	/* Initialise the network adapter, and initialise private storage */
	netdev_init(netdev, &hunt_operations);
	pci_set_drvdata(pci, netdev);
	netdev->dev = &pci->dev;
	netdev->state |= NETDEV_IRQ_UNSUPPORTED;

	hunt = netdev_priv(netdev);
	memset(hunt, 0, sizeof(*hunt));
	efx = &hunt->efx;

	efx->type = &hunt_nic_type;

	/* Initialise efx datapath */
	efx_probe(netdev, EFX_HUNTINGTON);

	/* Initialise MCDI.  In case we are recovering from a crash, first
	 * cancel any outstanding request by sending a special message using the
	 * least significant bits of the 'high' (doorbell) register.
	 */
	_efx_writel(efx, cpu_to_le32(1), ER_DZ_MC_DB_HWRD);
	rc = hunt_mcdi_init(hunt);
	if (rc != 0)
		goto fail2;

	/* Reset (most) configuration for this function */
	rc = hunt_reset(hunt);
	if (rc != 0)
		goto fail3;

	/* Medford has a list of UDP tunnel ports that is populated by the
	 * driver. Avoid dropping any unencapsulated packets. This may cause
	 * an MC reboot.
	 */
	hunt_clear_udp_tunnel_ports(hunt);

	/* Enable the workaround for bug35388, if supported */
	efx->workaround_35388 = hunt_workaround_35388(hunt);

	/* Set the RX packet prefix size */
	efx->rx_prefix_size = ES_DZ_RX_PREFIX_SIZE;

	rc = hunt_get_port_assignment(hunt);
	if (rc != 0)
		goto fail3;

	rc = hunt_mac_addr(hunt, netdev->ll_addr);
	if (rc != 0)
		goto fail4;

	rc = hunt_get_phy_cfg(hunt);
	if (rc != 0)
		goto fail5;

	rc = hunt_driver_attach(hunt, 1);
	if (rc != 0)
		goto fail5;

	/* If not exposing this network device, return successfully here */
	if (hunt->flags & (1 << MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_NO_ACTIVE_PORT))
		return 0;

	if (hunt_nic_is_primary(hunt)) {
		hunt->next_primary = primary_nics;
		primary_nics = hunt;
		hunt->primary = hunt;
	} else {
		struct hunt_nic *other_hunt = primary_nics;

		while (other_hunt && !hunt->primary) {
			struct pci_device *other_pci = (struct pci_device *)
				other_hunt->efx.netdev->dev;
			/* Check if the seg:bus:dev parts match. */
			if (PCI_FIRST_FUNC(other_pci->busdevfn) ==
			    PCI_FIRST_FUNC(pci->busdevfn))
				hunt->primary = other_hunt;

			other_hunt = other_hunt->next_primary;
		}
		if (!hunt->primary) {
			rc = -EIO;
			goto fail6;
		}
	}

	rc = register_netdev(netdev);
	if (rc != 0)
		goto fail8;

	DBG2("%s " PCI_FMT " ok\n", __func__, PCI_ARGS(pci));
	return 0;

fail8:
fail6:
	(void) hunt_driver_attach(hunt, 0);
fail5:
fail4:
fail3:
	hunt_mcdi_fini(hunt);
fail2:
	efx_remove(netdev);
	netdev_put(netdev);
fail1:
	DBG2("%s " PCI_FMT " rc=%d\n", __func__, PCI_ARGS(pci), rc);
	return rc;
}

static void hunt_remove(struct pci_device *pci)
{
	struct net_device *netdev = pci_get_drvdata(pci);
	struct hunt_nic *hunt = netdev_priv(netdev);

	if (!(hunt->flags &
	      (1 << MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_NO_ACTIVE_PORT))) {
		/* The netdevice might still be open, so unregister it now
		 * before ripping stuff out from underneath.
		 */
		unregister_netdev(netdev);
	}

	(void)hunt_driver_attach(hunt, 0);
	hunt_mcdi_fini(hunt);

	/* Destroy data path */
	efx_remove(netdev);

	netdev_nullify(netdev);
	netdev_put(netdev);
}

const struct efx_nic_type hunt_nic_type = {
	.mcdi_rpc = _hunt_mcdi,
};

static struct pci_device_id hunt_nics[] = {
	PCI_ROM(0x1924, 0x0a03, "SFC9220", "Solarflare SFN8xxx Adapter", 0),
	PCI_ROM(0x1924, 0x0b03, "SFC9250", "Solarflare X25xx Adapter", 0),
};

struct pci_driver hunt_driver __pci_driver = {
	.ids = hunt_nics,
	.id_count = ARRAY_SIZE(hunt_nics),
	.probe = hunt_probe,
	.remove = hunt_remove,
};
