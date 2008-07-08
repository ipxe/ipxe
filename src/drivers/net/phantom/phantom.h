#ifndef _PHANTOM_H
#define _PHANTOM_H

/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 * Copyright (C) 2008 NetXen, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * NetXen Phantom NICs
 *
 */

#include <stdint.h>

/* Drag in hardware definitions */
#include "nx_bitops.h"
#include "phantom_hw.h"
struct phantom_rds { NX_PSEUDO_BIT_STRUCT ( struct phantom_rds_pb ) };
struct phantom_sds { NX_PSEUDO_BIT_STRUCT ( struct phantom_sds_pb ) };
union phantom_cds { NX_PSEUDO_BIT_STRUCT ( union phantom_cds_pb ) };

/* Drag in firmware interface definitions */
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef uint32_t nx_rcode_t;
#define NXHAL_VERSION 1
#include "nxhal_nic_interface.h"

/** SPI controller maximum block size */
#define UNM_SPI_BLKSIZE 4

/** DMA buffer alignment */
#define UNM_DMA_BUFFER_ALIGN 16

/** Mark structure as DMA-aligned */
#define __unm_dma_aligned __attribute__ (( aligned ( UNM_DMA_BUFFER_ALIGN ) ))

/** Dummy DMA buffer size */
#define UNM_DUMMY_DMA_SIZE 1024

/******************************************************************************
 *
 * Register definitions
 *
 */

#define UNM_128M_CRB_WINDOW		0x6110210UL
#define UNM_32M_CRB_WINDOW		0x0110210UL
#define UNM_2M_CRB_WINDOW		0x0130060UL

/**
 * Phantom register blocks
 *
 * The upper address bits vary between cards.  We define an abstract
 * address space in which the upper 8 bits of the 32-bit register
 * address encode the register block.  This gets translated to a bus
 * address by the phantom_crb_access_xxx() methods.
 */
enum unm_reg_blocks {
	UNM_CRB_BLK_PCIE,
	UNM_CRB_BLK_CAM,
	UNM_CRB_BLK_ROMUSB,
	UNM_CRB_BLK_TEST,
};
#define UNM_CRB_BASE(blk)		( (blk) << 24 )
#define UNM_CRB_BLK(reg)		( (reg) >> 24 )
#define UNM_CRB_OFFSET(reg)		( (reg) & 0x00ffffff )

#define UNM_CRB_PCIE			UNM_CRB_BASE ( UNM_CRB_BLK_PCIE )
#define UNM_PCIE_SEM2_LOCK		( UNM_CRB_PCIE + 0x1c010 )
#define UNM_PCIE_SEM2_UNLOCK		( UNM_CRB_PCIE + 0x1c014 )

#define UNM_CRB_CAM			UNM_CRB_BASE ( UNM_CRB_BLK_CAM )

#define UNM_CAM_RAM			( UNM_CRB_CAM + 0x02000 )
#define UNM_CAM_RAM_PORT_MODE		( UNM_CAM_RAM + 0x00024 )
#define UNM_CAM_RAM_PORT_MODE_AUTO_NEG		4
#define UNM_CAM_RAM_PORT_MODE_AUTO_NEG_1G	5
#define UNM_CAM_RAM_DMESG_HEAD(n)	( UNM_CAM_RAM + 0x00030 + (n) * 0x10 )
#define UNM_CAM_RAM_DMESG_LEN(n)	( UNM_CAM_RAM + 0x00034 + (n) * 0x10 )
#define UNM_CAM_RAM_DMESG_TAIL(n)	( UNM_CAM_RAM + 0x00038 + (n) * 0x10 )
#define UNM_CAM_RAM_DMESG_SIG(n)	( UNM_CAM_RAM + 0x0003c + (n) * 0x10 )
#define UNM_CAM_RAM_DMESG_SIG_MAGIC		0xcafebabeUL
#define UNM_CAM_RAM_NUM_DMESG_BUFFERS		5
#define UNM_CAM_RAM_WOL_PORT_MODE	( UNM_CAM_RAM + 0x00198 )
#define UNM_CAM_RAM_MAC_ADDRS		( UNM_CAM_RAM + 0x001c0 )
#define UNM_CAM_RAM_COLD_BOOT		( UNM_CAM_RAM + 0x001fc )
#define UNM_CAM_RAM_COLD_BOOT_MAGIC		0x55555555UL

#define UNM_NIC_REG			( UNM_CRB_CAM + 0x02200 )
#define UNM_NIC_REG_NX_CDRP		( UNM_NIC_REG + 0x00018 )
#define UNM_NIC_REG_NX_ARG1		( UNM_NIC_REG + 0x0001c )
#define UNM_NIC_REG_NX_ARG2		( UNM_NIC_REG + 0x00020 )
#define UNM_NIC_REG_NX_ARG3		( UNM_NIC_REG + 0x00024 )
#define UNM_NIC_REG_NX_SIGN		( UNM_NIC_REG + 0x00028 )
#define UNM_NIC_REG_DUMMY_BUF_ADDR_HI	( UNM_NIC_REG + 0x0003c )
#define UNM_NIC_REG_DUMMY_BUF_ADDR_LO	( UNM_NIC_REG + 0x00040 )
#define UNM_NIC_REG_CMDPEG_STATE	( UNM_NIC_REG + 0x00050 )
#define UNM_NIC_REG_CMDPEG_STATE_INITIALIZED	0xff01
#define UNM_NIC_REG_CMDPEG_STATE_INITIALIZE_ACK	0xf00f
#define UNM_NIC_REG_DUMMY_BUF		( UNM_NIC_REG + 0x000fc )
#define UNM_NIC_REG_DUMMY_BUF_INIT		0
#define UNM_NIC_REG_XG_STATE_P3		( UNM_NIC_REG + 0x00098 )
#define UNM_NIC_REG_XG_STATE_P3_LINK( port, state_p3 ) \
	( ( (state_p3) >> ( (port) * 4 ) ) & 0x0f )
#define UNM_NIC_REG_XG_STATE_P3_LINK_UP		0x01
#define UNM_NIC_REG_XG_STATE_P3_LINK_DOWN	0x02
#define UNM_NIC_REG_RCVPEG_STATE	( UNM_NIC_REG + 0x0013c )
#define UNM_NIC_REG_RCVPEG_STATE_INITIALIZED	0xff01
#define UNM_NIC_REG_SW_INT_MASK_0	( UNM_NIC_REG + 0x001d8 )
#define UNM_NIC_REG_SW_INT_MASK_1	( UNM_NIC_REG + 0x001e0 )
#define UNM_NIC_REG_SW_INT_MASK_2	( UNM_NIC_REG + 0x001e4 )
#define UNM_NIC_REG_SW_INT_MASK_3	( UNM_NIC_REG + 0x001e8 )

#define UNM_CRB_ROMUSB			UNM_CRB_BASE ( UNM_CRB_BLK_ROMUSB )

#define UNM_ROMUSB_GLB			( UNM_CRB_ROMUSB + 0x00000 )
#define UNM_ROMUSB_GLB_STATUS		( UNM_ROMUSB_GLB + 0x00004 )
#define UNM_ROMUSB_GLB_STATUS_ROM_DONE		( 1 << 1 )
#define UNM_ROMUSB_GLB_SW_RESET		( UNM_ROMUSB_GLB + 0x00008 )
#define UNM_ROMUSB_GLB_SW_RESET_MAGIC		0x0080000fUL
#define UNM_ROMUSB_GLB_PEGTUNE_DONE	( UNM_ROMUSB_GLB + 0x0005c )

#define UNM_ROMUSB_ROM			( UNM_CRB_ROMUSB + 0x10000 )
#define UNM_ROMUSB_ROM_INSTR_OPCODE	( UNM_ROMUSB_ROM + 0x00004 )
#define UNM_ROMUSB_ROM_ADDRESS		( UNM_ROMUSB_ROM + 0x00008 )
#define UNM_ROMUSB_ROM_WDATA		( UNM_ROMUSB_ROM + 0x0000c )
#define UNM_ROMUSB_ROM_ABYTE_CNT	( UNM_ROMUSB_ROM + 0x00010 )
#define UNM_ROMUSB_ROM_DUMMY_BYTE_CNT	( UNM_ROMUSB_ROM + 0x00014 )
#define UNM_ROMUSB_ROM_RDATA		( UNM_ROMUSB_ROM + 0x00018 )

#define UNM_CRB_TEST			UNM_CRB_BASE ( UNM_CRB_BLK_TEST )

#define UNM_TEST_CONTROL		( UNM_CRB_TEST + 0x00090 )
#define UNM_TEST_CONTROL_START			0x01
#define UNM_TEST_CONTROL_ENABLE			0x02
#define UNM_TEST_CONTROL_BUSY			0x08
#define UNM_TEST_ADDR_LO		( UNM_CRB_TEST + 0x00094 )
#define UNM_TEST_ADDR_HI		( UNM_CRB_TEST + 0x00098 )
#define UNM_TEST_RDDATA_LO		( UNM_CRB_TEST + 0x000a8 )
#define UNM_TEST_RDDATA_HI		( UNM_CRB_TEST + 0x000ac )

/******************************************************************************
 *
 * Flash layout
 *
 */

/* Board configuration */

#define UNM_BRDCFG_START		0x4000

struct unm_board_info {
	uint32_t header_version;
	uint32_t board_mfg;
	uint32_t board_type;
	uint32_t board_num;
	uint32_t chip_id;
	uint32_t chip_minor;
	uint32_t chip_major;
	uint32_t chip_pkg;
	uint32_t chip_lot;
	uint32_t port_mask;
	uint32_t peg_mask;
	uint32_t icache_ok;
	uint32_t dcache_ok;
	uint32_t casper_ok;
	uint32_t mac_addr_lo_0;
	uint32_t mac_addr_lo_1;
	uint32_t mac_addr_lo_2;
	uint32_t mac_addr_lo_3;
	uint32_t mn_sync_mode;
	uint32_t mn_sync_shift_cclk;
	uint32_t mn_sync_shift_mclk;
	uint32_t mn_wb_en;
	uint32_t mn_crystal_freq;
	uint32_t mn_speed;
	uint32_t mn_org;
	uint32_t mn_depth;
	uint32_t mn_ranks_0;
	uint32_t mn_ranks_1;
	uint32_t mn_rd_latency_0;
	uint32_t mn_rd_latency_1;
	uint32_t mn_rd_latency_2;
	uint32_t mn_rd_latency_3;
	uint32_t mn_rd_latency_4;
	uint32_t mn_rd_latency_5;
	uint32_t mn_rd_latency_6;
	uint32_t mn_rd_latency_7;
	uint32_t mn_rd_latency_8;
	uint32_t mn_dll_val[18];
	uint32_t mn_mode_reg;
	uint32_t mn_ext_mode_reg;
	uint32_t mn_timing_0;
	uint32_t mn_timing_1;
	uint32_t mn_timing_2;
	uint32_t sn_sync_mode;
	uint32_t sn_pt_mode;
	uint32_t sn_ecc_en;
	uint32_t sn_wb_en;
	uint32_t sn_crystal_freq;
	uint32_t sn_speed;
	uint32_t sn_org;
	uint32_t sn_depth;
	uint32_t sn_dll_tap;
	uint32_t sn_rd_latency;
	uint32_t mac_addr_hi_0;
	uint32_t mac_addr_hi_1;
	uint32_t mac_addr_hi_2;
	uint32_t mac_addr_hi_3;
	uint32_t magic;
	uint32_t mn_rdimm;
	uint32_t mn_dll_override;
};

#define UNM_BDINFO_VERSION		1
#define UNM_BRDTYPE_P3_HMEZ		0x0022
#define UNM_BRDTYPE_P3_10G_CX4_LP	0x0023
#define UNM_BRDTYPE_P3_4_GB		0x0024
#define UNM_BRDTYPE_P3_IMEZ		0x0025
#define UNM_BRDTYPE_P3_10G_SFP_PLUS	0x0026
#define UNM_BRDTYPE_P3_10000_BASE_T	0x0027
#define UNM_BRDTYPE_P3_XG_LOM		0x0028
#define UNM_BRDTYPE_P3_10G_CX4		0x0031
#define UNM_BRDTYPE_P3_10G_XFP		0x0032
#define UNM_BDINFO_MAGIC		0x12345678

/* User defined region */

#define UNM_USER_START			0x3e8000

#define UNM_FLASH_NUM_PORTS		4
#define UNM_FLASH_NUM_MAC_PER_PORT	32

struct unm_user_info {
	uint8_t  flash_md5[16 * 64];
	uint32_t bootld_version;
	uint32_t bootld_size;
	uint32_t image_version;
	uint32_t image_size;
	uint32_t primary_status;
	uint32_t secondary_present;
	/* MAC address , 4 ports, 32 address per port */
	uint64_t mac_addr[UNM_FLASH_NUM_PORTS * UNM_FLASH_NUM_MAC_PER_PORT];
	uint32_t sub_sys_id;
	uint8_t  serial_num[32];
	uint32_t bios_version;
	uint32_t pxe_enable;
	uint32_t vlan_tag[UNM_FLASH_NUM_PORTS];
};

#endif /* _PHANTOM_H */
