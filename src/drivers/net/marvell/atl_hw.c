#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include "aquantia.h"
#include "atl_hw.h"

int atl_hw_reset_flb_(struct atl_nic *nic)
{
	uint32_t val;
	int k = 0;

	ATL_WRITE_REG(0x40e1, ATL_GLB_CTRL2);
	mdelay(50);

	/* Cleanup SPI */
	val = ATL_READ_REG(ATL_GLB_NVR_PROV4);
	ATL_WRITE_REG(val | 0x10, ATL_GLB_NVR_PROV4);

	ATL_WRITE_REG((ATL_READ_REG(ATL_GLB_STD_CTRL) &
		~ATL_GLB_CTRL_RST_DIS) | 0x8000, ATL_GLB_STD_CTRL);

	/* Kickstart MAC */
	ATL_WRITE_REG(0x80e0, ATL_GLB_CTRL2);
	ATL_WRITE_REG(0x0, 0x32a8);

	ATL_WRITE_REG(0x1, ATL_GEN_PROV9);

	/* Reset SPI again because of possible interrupted SPI burst */
	val = ATL_READ_REG(ATL_GLB_NVR_PROV4);
	ATL_WRITE_REG(val | 0x10, ATL_GLB_NVR_PROV4);
	mdelay(10);
	/* Clear SPI reset state */
	ATL_WRITE_REG(val & ~0x10, ATL_GLB_NVR_PROV4);

	/* MAC Kickstart */
	ATL_WRITE_REG(0x180e0, ATL_GLB_CTRL2);

	for (k = 0; k < 1000; k++) {
		u32 flb_status = ATL_READ_REG(ATL_MPI_DAISY_CHAIN_STS);

		flb_status = flb_status & 0x10;
		if (flb_status)
			break;
		mdelay(10);
	}
	if (k == 1000) {
		printf("MAC kickstart failed\n");
		return -EIO;
	}

	/* FW reset */
	ATL_WRITE_REG(0x80e0, ATL_GLB_CTRL2);
	mdelay(50);

	ATL_WRITE_REG(0x1, ATL_GLB_MCP_SEM1);

	/* Kickstart PHY - skipped */

	/* Global software reset*/
	ATL_WRITE_REG(ATL_READ_REG(ATL_RX_CTRL) & ~ATL_RX_CTRL_RST_DIS,
		ATL_RX_CTRL);
	ATL_WRITE_REG(ATL_READ_REG(ATL_TX_CTRL) & ~ATL_TX_CTRL_RST_DIS,
		ATL_TX_CTRL);

	ATL_WRITE_REG(ATL_READ_REG(ATL_MAC_PHY_CTRL) &
		~ATL_MAC_PHY_CTRL_RST_DIS, ATL_MAC_PHY_CTRL);

	ATL_WRITE_REG((ATL_READ_REG(ATL_GLB_STD_CTRL) &
		~ATL_GLB_CTRL_RST_DIS) | 0x8000, ATL_GLB_STD_CTRL);

	for (k = 0; k < 1000; k++) {
		u32 fw_state = ATL_READ_REG(ATL_FW_VER);

		if (fw_state)
			break;
		mdelay(10);
	}
	if (k == 1000) {
		printf("FW kickstart failed\n");
		return -EIO;
	}
	/* Old FW requires fixed delay after init */
	mdelay(15);

	return 0;
}

int atl_hw_reset_rbl_(struct atl_nic *nic)
{
	uint32_t val, rbl_status;
	int k;

	ATL_WRITE_REG(0x40e1, ATL_GLB_CTRL2);
	ATL_WRITE_REG(0x1, ATL_GLB_MCP_SEM1);
	ATL_WRITE_REG(0x0, ATL_MIF_PWR_GATING_EN_CTRL);

	/* Alter RBL status */
	ATL_WRITE_REG(0xDEAD, ATL_MPI_BOOT_EXIT_CODE);

	/* Cleanup SPI */
	val = ATL_READ_REG(ATL_GLB_NVR_PROV4);
	ATL_WRITE_REG(val | 0x10, ATL_GLB_NVR_PROV4);

	/* Global software reset*/
	ATL_WRITE_REG(ATL_READ_REG(ATL_RX_CTRL) & ~ATL_RX_CTRL_RST_DIS,
		ATL_RX_CTRL);
	ATL_WRITE_REG(ATL_READ_REG(ATL_TX_CTRL) & ~ATL_TX_CTRL_RST_DIS,
		ATL_TX_CTRL);
	
	ATL_WRITE_REG(ATL_READ_REG(ATL_MAC_PHY_CTRL) &
		~ATL_MAC_PHY_CTRL_RST_DIS, ATL_MAC_PHY_CTRL);

	ATL_WRITE_REG((ATL_READ_REG(ATL_GLB_STD_CTRL) &
		~ATL_GLB_CTRL_RST_DIS) | 0x8000, ATL_GLB_STD_CTRL);
	
	ATL_WRITE_REG(0x40e0, ATL_GLB_CTRL2);

	/* Wait for RBL boot */
	for (k = 0; k < 1000; k++) {
		rbl_status = ATL_READ_REG(ATL_MPI_BOOT_EXIT_CODE) & 0xFFFF;
		if (rbl_status && rbl_status != 0xDEAD)
			break;
		mdelay(10);
	}
	if (!rbl_status || rbl_status == 0xDEAD) {
		printf("RBL Restart failed");
		return -EIO;
	}

	if (rbl_status == 0xF1A7) {
		return -ENOTSUP;
	}

	for (k = 0; k < 1000; k++) {
		u32 fw_state = ATL_READ_REG(ATL_FW_VER);

		if (fw_state)
			break;
		mdelay(10);
	}
	if (k == 1000) {
		printf("FW kickstart failed\n");
		return -EIO;
	}
	/* Old FW requires fixed delay after init */
	mdelay(15);

	return 0;
}

int atl_hw_reset(struct atl_nic *nic)
{
	uint32_t boot_exit_code = 0;
	int k;
	int rbl_enabled;

	for (k = 0; k < 1000; ++k) {
		uint32_t flb_status = ATL_READ_REG(ATL_MPI_DAISY_CHAIN_STS);
		boot_exit_code = ATL_READ_REG(ATL_MPI_BOOT_EXIT_CODE);
		if (flb_status != 0x06000000 || boot_exit_code != 0)
			break;
	}

	if (k == 1000) {
		printf("Neither RBL nor FLB firmware started\n");
		return -ENOTSUP;
	}

	rbl_enabled = (boot_exit_code != 0);

	/* FW 1.x may bootup in an invalid POWER state (WOL feature).
	 * We should work around this by forcing its state back to DEINIT
	 */
	/*if (ver & 0xff000000)) {
		int err = 0;

		hw_atl_utils_mpi_set_state(self, MPI_DEINIT);
		err = readx_poll_timeout_atomic(hw_atl_utils_mpi_get_state,
						self, val,
						(val & HW_ATL_MPI_STATE_MSK) ==
						 MPI_DEINIT,
						10, 100000U);
		if (err)
			return err;
	}*/

	if (rbl_enabled)
		return atl_hw_reset_rbl_(nic);
	else
		return atl_hw_reset_flb_(nic);
}

int atl_hw_start(struct atl_nic *nic)
{
	ATL_WRITE_REG(ATL_LINK_ADV_AUTONEG, ATL_LINK_ADV);
	return 0;
}

int atl_hw_stop(struct atl_nic *nic)
{
	ATL_WRITE_REG(0x0, ATL_LINK_ADV);
	return 0;
}

int atl_hw_get_link(struct atl_nic *nic)
{
	return (ATL_READ_REG(ATL_LINK_ST) & ATL_LINK_ADV_AUTONEG) != 0;
}

int atl_hw_read_mem(struct atl_nic *nic, uint32_t addr, uint32_t *buffer, uint32_t size)
{
	uint32_t i;
	printf("AQUANTIA: atl_hw_read_mem\n");

	for (i = 0; i < 100; ++i) {
		if (ATL_READ_REG(ATL_SEM_RAM))
			break;
		mdelay(1);
	}
	if (i == 100)
		goto err;
	
	ATL_WRITE_REG(addr, ATL_MBOX_CTRL3);

	for (i = 0; i < size; ++i, addr += 4) {
		uint32_t j;

		ATL_WRITE_REG(0x8000, ATL_MBOX_CTRL1);
		for (j = 0; j < 10000; ++j) {
			if (ATL_READ_REG(ATL_MBOX_CTRL3) != addr)
				break;
			udelay(10);
		}
		if (j == 10000)
			goto err;

		buffer[i] = ATL_READ_REG(ATL_MBOX_CTRL5);
	}

	ATL_WRITE_REG(1, ATL_SEM_RAM);

	return 0;
err:
	printf("AQUANTIA: download_dwords error\n");
	return -1;
}

int atl_hw_get_mac(struct atl_nic *nic, uint8_t *mac)
{
	uint32_t mac_addr[2] = {0};
	int err = 0;
	uint32_t efuse_addr = ATL_READ_REG(ATL_GLB_MCP_SP26);

	if (efuse_addr != 0) {
		uint32_t mac_efuse_addr = efuse_addr + 40 * sizeof(uint32_t);
		err = atl_hw_read_mem(nic, mac_efuse_addr, mac_addr, 2);
		if (err != 0) 
			return err;
		
		mac_addr[0] = __bswap_32(mac_addr[0]);
		mac_addr[1] = __bswap_32(mac_addr[1]);
		
		memcpy(mac, (uint8_t *)mac_addr, 6);
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