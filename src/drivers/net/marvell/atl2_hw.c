/** @file
 *
 * Marvell AQtion family network card driver, hardware-specific functions.
 *
 * Copyright(C) 2017-2024 Marvell
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO,THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

FILE_LICENCE ( BSD2 );

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include "aqc1xx.h"
#include "atl2_hw.h"

static int atl2_hw_boot_completed_ ( struct atl_nic *nic ) {
	uint32_t reset_status = ATL_READ_REG ( ATL2_GLB_RST_CTRL2 );

	return ( reset_status & ATL2_RESET_STATUS_BOOT_COMPLETED_MASK ) ||
		   ( ATL_READ_REG ( ATL2_HOST_ITR_REQ )
		   & ATL2_FW_HOST_INTERRUPT_REQUEST_READY );
}

void atl2_hw_read_shared_in_ ( struct atl_nic *nic, uint32_t offset,
								uint32_t *data, uint32_t len ) {
	uint32_t i;

	for (i = 0; i < len; ++i )
	{
		data[i] = ATL_READ_REG ( ATL2_MIF_SHARED_BUF_IN + offset + i * 4 );
	}
}

void atl2_hw_write_shared_in_ ( struct atl_nic *nic, uint32_t offset,
							  uint32_t *data, uint32_t len ) {
	uint32_t i;

	for ( i = 0; i < len; ++i )
	{
		ATL_WRITE_REG ( data[i], ATL2_MIF_SHARED_BUF_IN + offset + i * 4 );
	}
}

int atl2_hw_finish_ack_ ( struct atl_nic *nic, uint32_t ms ) {
	uint32_t i;
	int err = 0;

	ATL_WRITE_REG ( ATL_READ_REG ( ATL2_HOST_FINISHED_WRITE )
	| 1, ATL2_HOST_FINISHED_WRITE );

	for ( i = 0; i < ( ms / 100 ); ++i )
	{
		if ( ( ATL_READ_REG ( ATL2_MCP_BUSY_WRITE ) & 1 ) == 0 )
		{
			break;
		}
		udelay ( ATL2_DELAY_100 );
	}
	if (i == ( ms / 100 ) )
		err = -ETIME;

	return err;
}

int atl2_hw_fw_init_ ( struct atl_nic *nic ) {
	uint32_t val;
	int err = 0;

	atl2_hw_read_shared_in_ ( nic, ATL2_LINK_CTRL_IN_OFF, &val, 1 );
	val |= ( ATL2_HOST_MODE_ACTIVE | ( 1U << 13 ) );
	atl2_hw_write_shared_in_ ( nic, ATL2_LINK_CTRL_IN_OFF, &val, 1 );

	atl2_hw_read_shared_in_ ( nic, ATL2_MTU_IN_OFF, &val, 1 );
	val = 16352;
	atl2_hw_write_shared_in_ ( nic, ATL2_MTU_IN_OFF, &val, 1 );

	atl2_hw_read_shared_in_ ( nic, ATL2_LINK_OPTS_IN_OFF, &val, 1 );
	val = 0;
	atl2_hw_write_shared_in_( nic, ATL2_LINK_OPTS_IN_OFF, &val, 1 );
	err = atl2_hw_finish_ack_ ( nic, 50000000 );

	return err;
}

int atl2_hw_reset ( struct atl_nic *nic ) {
	int completed = 0;
	uint32_t status = 0;
	uint32_t request;
	int err = 0;
	int i;

	request = ATL2_RESET_STATUS_REQ_GSR;

	ATL_WRITE_REG ( request, ATL2_GLB_RST_CTRL2 );

	/* Wait for boot code started every 10us, 200 ms */
	for ( i = 0; i < 20000; ++i )
	{
		status = ATL_READ_REG ( ATL2_GLB_RST_CTRL2 );

		if ( ( ( status & ATL2_RESET_STATUS_BC_STARTED ) &&
			 ( status != 0xFFFFFFFFu ) ) )
			break;

		udelay ( ATL2_DELAY_10 );
	}
	if ( i == 20000 )
	{
		DBGC ( nic, "Boot code hanged" );
		err = -EIO;
		goto err_exit;
	}

	/* Wait for boot succeed, failed or host request every 10us, 480ms */
	for ( i = 0; i < 48000; ++i )
	{
		completed = atl2_hw_boot_completed_ ( nic );
		if ( completed )
			break;

		udelay ( ATL2_DELAY_10 );
	}

	if ( !completed )
	{
		DBGC ( nic, "FW Restart timed out" );
		err = -ETIME;
		goto err_exit;
	}

	status = ATL_READ_REG ( ATL2_GLB_RST_CTRL2 );

	if ( status & ATL2_RESET_STATUS_BOOT_FAILED_MASK )
	{
		err = -EIO;
		DBGC ( nic, "FW Restart failed" );
		DBGC ( nic, "status = 0x%x", status );
		goto err_exit;
	}

	if ( ATL_READ_REG ( ATL2_HOST_ITR_REQ )
			& ATL2_FW_HOST_INTERRUPT_REQUEST_READY )
	{
		err = -ENOTSUP;
		DBGC ( nic, "Dynamic FW load not implemented" );
		goto err_exit;
	}

	err = atl2_hw_fw_init_ ( nic );

err_exit:
	return err;
}

int atl2_hw_start ( struct atl_nic *nic ) {
	uint32_t val;

	atl2_hw_read_shared_in_ ( nic, ATL2_LINK_OPTS_IN_OFF, &val, 1 );
	val = 0x4B00FFE1;
	atl2_hw_write_shared_in_ ( nic, ATL2_LINK_OPTS_IN_OFF, &val, 1 );

	return atl2_hw_finish_ack_ ( nic, 100000 );
}

int atl2_hw_stop ( struct atl_nic *nic ) {
	uint32_t val;

	atl2_hw_read_shared_in_ ( nic, ATL2_LINK_OPTS_IN_OFF, &val, 1 );
	val = 0;
	atl2_hw_write_shared_in_ ( nic, ATL2_LINK_OPTS_IN_OFF, &val, 1 );

	return atl2_hw_finish_ack_ ( nic, 100000 );
}

int atl2_hw_get_link ( struct atl_nic *nic ) {
	uint32_t val;

	val = ATL_READ_REG ( ATL2_MIF_SHARED_BUF_OUT + ATL2_LINK_STS_OUT_OFF );

	return ( ( val & 0xf ) != 0 ) && ( ( val & 0xF0 ) != 0 );
}

int atl2_hw_get_mac ( struct atl_nic *nic, uint8_t *mac ) {
	uint32_t mac_addr[2] = {0};

	atl2_hw_read_shared_in_ ( nic, ATL2_MAC_ADDR_IN_OFF, mac_addr, 2 );

	memcpy ( mac, ( uint8_t * )mac_addr, 6 );

	return 0;
}

struct atl_hw_ops atl2_hw = {
	.reset = atl2_hw_reset,
	.start = atl2_hw_start,
	.stop = atl2_hw_stop,
	.get_link = atl2_hw_get_link,
	.get_mac = atl2_hw_get_mac,
};
