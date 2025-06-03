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
#include "atl_hw.h"
#include <compiler.h>


int atl_hw_reset_flb_ ( struct atl_nic *nic ) {
	uint32_t val;
	int k = 0;

	ATL_WRITE_REG ( ATL_GLB_CTRL2_MBOX_ERR_UP_RUN_STALL, ATL_GLB_CTRL2 );
	mdelay ( ATL_DELAY_50_MNS );

	/* Cleanup SPI */
	val = ATL_READ_REG ( ATL_GLB_NVR_PROV4 );
	ATL_WRITE_REG ( val | ATL_GBL_NVR_PROV4_RESET, ATL_GLB_NVR_PROV4 );

	ATL_WRITE_REG( ( ATL_READ_REG ( ATL_GLB_STD_CTRL ) &
		      ~ATL_GLB_CTRL_RST_DIS ) | ATL_GLB_STD_CTRL_RESET,
			ATL_GLB_STD_CTRL );

	/* Kickstart MAC */
	ATL_WRITE_REG ( ATL_GLB_CTRL2_FW_RESET, ATL_GLB_CTRL2 );
	ATL_WRITE_REG ( ATL_MIF_PWR_GATING_EN_CTRL_RESET,
			ATL_MIF_PWR_GATING_EN_CTRL );

	ATL_WRITE_REG ( ATL_GEN_PROV9_ENABLE, ATL_GEN_PROV9 );

	/* Reset SPI again because of possible interrupted SPI burst */
	val = ATL_READ_REG ( ATL_GLB_NVR_PROV4 );
	ATL_WRITE_REG ( val | ATL_GBL_NVR_PROV4_RESET, ATL_GLB_NVR_PROV4 );
	mdelay ( ATL_DELAY_10_MNS );
	/* Clear SPI reset state */
	ATL_WRITE_REG ( val & ~ATL_GBL_NVR_PROV4_RESET, ATL_GLB_NVR_PROV4 );

	/* MAC Kickstart */
	ATL_WRITE_REG ( ATL_GLB_CTRL2_MAC_KICK_START, ATL_GLB_CTRL2 );

	for (k = 0; k < 1000; k++) {
		uint32_t flb_status = ATL_READ_REG ( ATL_MPI_DAISY_CHAIN_STS );

		flb_status = flb_status & FLB_LOAD_STS;
		if ( flb_status )
			break;
		mdelay ( ATL_DELAY_10_MNS );
	}
	if ( k == 1000 ) {
		DBGC (nic, "MAC kickstart failed\n" );
		return -EIO;
	}

	/* FW reset */
	ATL_WRITE_REG ( ATL_GLB_CTRL2_FW_RESET, ATL_GLB_CTRL2 );
	mdelay ( ATL_DELAY_50_MNS );

	ATL_WRITE_REG ( ATL_GBL_MCP_SEM1_RELEASE, ATL_GLB_MCP_SEM1 );

	/* Global software reset*/
	ATL_WRITE_REG ( ATL_READ_REG ( ATL_RX_CTRL ) &
			~ATL_RX_CTRL_RST_DIS, ATL_RX_CTRL );
	ATL_WRITE_REG ( ATL_READ_REG ( ATL_TX_CTRL ) &
			~ATL_TX_CTRL_RST_DIS, ATL_TX_CTRL );

	ATL_WRITE_REG ( ATL_READ_REG ( ATL_MAC_PHY_CTRL ) &
			~ATL_MAC_PHY_CTRL_RST_DIS, ATL_MAC_PHY_CTRL );

	ATL_WRITE_REG ( ( ATL_READ_REG ( ATL_GLB_STD_CTRL ) &
			~ATL_GLB_CTRL_RST_DIS ) | ATL_GLB_STD_CTRL_RESET,
			ATL_GLB_STD_CTRL );

	for (k = 0; k < 1000; k++) {
		u32 fw_state = ATL_READ_REG ( ATL_FW_VER );

		if ( fw_state )
			break;
		mdelay ( ATL_DELAY_10_MNS );
	}
	if ( k == 1000 ) {
		DBGC ( nic, "FW kickstart failed\n" );
		return -EIO;
	}
	/* Old FW requires fixed delay after init */
	mdelay ( ATL_DELAY_15_MNS );

	return 0;
}

int atl_hw_reset_rbl_ ( struct atl_nic *nic ) {
	uint32_t val, rbl_status;
	int k;

	ATL_WRITE_REG ( ATL_GLB_CTRL2_MBOX_ERR_UP_RUN_STALL, ATL_GLB_CTRL2 );
	ATL_WRITE_REG ( ATL_GBL_MCP_SEM1_RELEASE, ATL_GLB_MCP_SEM1 );
	ATL_WRITE_REG ( ATL_MIF_PWR_GATING_EN_CTRL_RESET,
			ATL_MIF_PWR_GATING_EN_CTRL );

	/* Alter RBL status */
	ATL_WRITE_REG ( POISON_SIGN, ATL_MPI_BOOT_EXIT_CODE );

	/* Cleanup SPI */
	val = ATL_READ_REG ( ATL_GLB_NVR_PROV4 );
	ATL_WRITE_REG ( val | ATL_GBL_NVR_PROV4_RESET, ATL_GLB_NVR_PROV4 );

	/* Global software reset*/
	ATL_WRITE_REG ( ATL_READ_REG ( ATL_RX_CTRL ) & ~ATL_RX_CTRL_RST_DIS,
			ATL_RX_CTRL );
	ATL_WRITE_REG ( ATL_READ_REG ( ATL_TX_CTRL ) & ~ATL_TX_CTRL_RST_DIS,
			ATL_TX_CTRL );
	ATL_WRITE_REG ( ATL_READ_REG ( ATL_MAC_PHY_CTRL ) &
			~ATL_MAC_PHY_CTRL_RST_DIS, ATL_MAC_PHY_CTRL );

	ATL_WRITE_REG ( ( ATL_READ_REG ( ATL_GLB_STD_CTRL ) &
			~ATL_GLB_CTRL_RST_DIS ) | ATL_GLB_STD_CTRL_RESET,
			ATL_GLB_STD_CTRL );

	ATL_WRITE_REG ( ATL_GLB_CTRL2_MBOX_ERR_UP_RUN_NORMAL, ATL_GLB_CTRL2 );

	/* Wait for RBL boot */
	for ( k = 0; k < 1000; k++ ) {
		rbl_status = ATL_READ_REG ( ATL_MPI_BOOT_EXIT_CODE ) & 0xFFFF;
		if ( rbl_status && rbl_status != POISON_SIGN )
			break;
		mdelay ( ATL_DELAY_10_MNS );
	}
	if ( !rbl_status || rbl_status == POISON_SIGN ) {
		DBGC ( nic, "RBL Restart failed\n" );
		return -EIO;
	}

	if ( rbl_status == FW_NOT_SUPPORT )
		return -ENOTSUP;

	for ( k = 0; k < 1000; k++ ) {
		u32 fw_state = ATL_READ_REG ( ATL_FW_VER );

		if ( fw_state )
			break;
		mdelay ( ATL_DELAY_10_MNS );
	}
	if ( k == 1000 ) {
		DBGC ( nic, "FW kickstart failed\n" );
		return -EIO;
	}
	/* Old FW requires fixed delay after init */
	mdelay ( ATL_DELAY_15_MNS );

	return 0;
}

int atl_hw_reset ( struct atl_nic *nic ) {
	uint32_t boot_exit_code = 0;
	uint32_t k;
	int rbl_enabled;
	uint32_t fw_ver;
	uint32_t sem_timeout;

	for ( k = 0; k < 1000; ++k ) {
		uint32_t flb_status = ATL_READ_REG ( ATL_MPI_DAISY_CHAIN_STS );
		boot_exit_code = ATL_READ_REG ( ATL_MPI_BOOT_EXIT_CODE );
		if ( flb_status != ATL_MPI_DAISY_CHAIN_STS_ERROR_STATUS ||
			boot_exit_code != 0 )
			break;
	}

	if ( k == 1000 ) {
		DBGC ( nic, "Neither RBL nor FLB firmware started\n" );
		return -ENOTSUP;
	}

	rbl_enabled = ( boot_exit_code != 0 );

	fw_ver = ATL_READ_REG ( ATL_FW_VER );
	if ( ( ( fw_ver >> 24 ) & 0xFF ) >= 4 ) {
		sem_timeout = ATL_READ_REG ( ATL_SEM_TIMEOUT );
		if ( sem_timeout > ATL_SEM_MAX_TIMEOUT )
			sem_timeout = ATL_SEM_MAX_TIMEOUT;

		for ( k = 0; k < sem_timeout; ++k ) {
			if ( ATL_READ_REG ( ATL_GLB_MCP_SEM4 ) )
				break;

			mdelay ( ATL_DELAY_1_MNS );
		}
		for ( k = 0; k < sem_timeout; ++k ) {
			if ( ATL_READ_REG ( ATL_GLB_MCP_SEM5 ) )
				break;

			mdelay ( ATL_DELAY_1_MNS );
		}
	}


	if ( rbl_enabled )
		return atl_hw_reset_rbl_ ( nic );
	else
		return atl_hw_reset_flb_ ( nic );
}

int atl_hw_start ( struct atl_nic *nic ) {
	ATL_WRITE_REG ( ATL_LINK_ADV_AUTONEG, ATL_LINK_ADV );
	return 0;
}

int atl_hw_stop ( struct atl_nic *nic ) {
	ATL_WRITE_REG ( ATL_SHUT_LINK, ATL_LINK_ADV );
	return 0;
}

int atl_hw_get_link ( struct atl_nic *nic ) {
	return ( ATL_READ_REG ( ATL_LINK_ST) & ATL_LINK_ADV_AUTONEG ) != 0;
}

int atl_hw_read_mem ( struct atl_nic *nic, uint32_t addr, uint32_t *buffer,
		    uint32_t size ) {
	uint32_t i;

	for ( i = 0; i < 100; ++i ) {
		if ( ATL_READ_REG( ATL_SEM_RAM ) )
			break;
		mdelay ( ATL_DELAY_1_MNS );
	}
	if ( i == 100 ) {
		DBGC ( nic, "Semaphore Register not set\n" );
		return -EIO;
	}

	ATL_WRITE_REG ( addr, ATL_MBOX_CTRL3 );

	for ( i = 0; i < size; ++i, addr += 4 ) {
		uint32_t j;

		ATL_WRITE_REG ( ATL_MBOX_CTRL1_START_MBOX_OPT, ATL_MBOX_CTRL1 );
		for ( j = 0; j < 10000; ++j ) {
			if ( ATL_READ_REG (ATL_MBOX_CTRL3 ) != addr )
				break;
			udelay ( ATL_DELAY_10_MNS );
		}
		if ( j == 10000 ) {
			DBGC ( nic, "Reading from CTRL3 Register Failed\n" );
			return -EIO;
		}

		buffer[i] = ATL_READ_REG ( ATL_MBOX_CTRL5 );
	}

	ATL_WRITE_REG( ATL_SEM_RAM_RESET, ATL_SEM_RAM );

	return 0;
}

int atl_hw_get_mac ( struct atl_nic *nic, uint8_t *mac ) {
	uint32_t mac_addr[2] = {0};
	int err = 0;
	uint32_t efuse_addr = ATL_READ_REG ( ATL_GLB_MCP_SP26 );

	if ( efuse_addr != 0) {
		uint32_t mac_efuse_addr = efuse_addr + 40 * sizeof ( uint32_t );
		err = atl_hw_read_mem ( nic, mac_efuse_addr, mac_addr, 2 );
		if ( err != 0 )
			return err;

		mac_addr[0] = cpu_to_be32 ( mac_addr[0] );
		mac_addr[1] = cpu_to_be32 ( mac_addr[1] );

		memcpy ( mac, ( uint8_t * )mac_addr, ATL_MAC_ADDRESS_SIZE );
	}
	return 0;
}

struct atl_hw_ops atl_hw = {
	.reset = atl_hw_reset,
	.start = atl_hw_start,
	.stop = atl_hw_stop,
	.get_link = atl_hw_get_link,
	.get_mac = atl_hw_get_mac,
};
