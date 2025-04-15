/*
 * Copyright © 2018 Broadcom. All Rights Reserved. 
 * The term “Broadcom” refers to Broadcom Inc. and/or its subsidiaries.

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

#undef ERRFILE
#define ERRFILE ERRFILE_tg3

#define __le16  u16
#define __le32  u32
#define __le64  u64
#define __be16  u16
#define __be32  u32
#define __be64  u64

#define dma_addr_t unsigned long

union dma_addr64_t {
	dma_addr_t addr;
	u64 as_u64;
};

#include "bnxt_hsi.h"

#define DRV_MODULE_NAME              "bnxt"
#define IPXE_VERSION_MAJOR           1
#define IPXE_VERSION_MINOR           0
#define IPXE_VERSION_UPDATE          0

/*
 * Broadcom ethernet driver defines.
 */
#define FLAG_SET(f, b)                          ((f) |= (b))
#define FLAG_TEST(f, b)                         ((f) & (b))
#define FLAG_RESET(f, b)                        ((f) &= ~(b))
#define BNXT_FLAG_HWRM_SHORT_CMD_SUPP           0x0001
#define BNXT_FLAG_HWRM_SHORT_CMD_REQ            0x0002
#define BNXT_FLAG_RESOURCE_QCAPS_SUPPORT        0x0004
#define BNXT_FLAG_MULTI_HOST                    0x0008
#define BNXT_FLAG_NPAR_MODE                     0x0010
#define BNXT_FLAG_ATOMICS_ENABLE                0x0020
#define BNXT_FLAG_PCI_VF                        0x0040
#define BNXT_FLAG_LINK_SPEEDS2                  0x0080
#define BNXT_FLAG_IS_CHIP_P5                    0x0100
#define BNXT_FLAG_IS_CHIP_P5_PLUS               0x0200
#define BNXT_FLAG_IS_CHIP_P7                    0x0400
/*******************************************************************************
 * Status codes.
 ******************************************************************************/
#define STATUS_SUCCESS                          0
#define STATUS_FAILURE                          1
#define STATUS_NO_RESOURCE                      2
#define STATUS_INVALID_PARAMETER                3
#define STATUS_LINK_ACTIVE                      4
#define STATUS_LINK_DOWN                        5
#define STATUS_LINK_SETTING_MISMATCH            6
#define STATUS_TOO_MANY_FRAGMENTS               7
#define STATUS_TRANSMIT_ABORTED                 8
#define STATUS_TRANSMIT_ERROR                   9
#define STATUS_RECEIVE_ABORTED                  10
#define STATUS_RECEIVE_ERROR                    11
#define STATUS_INVALID_PACKET_SIZE              12
#define STATUS_NO_MAP_REGISTER                  13
#define STATUS_UNKNOWN_ADAPTER                  14
#define STATUS_NO_COALESCE_BUFFER               15
#define STATUS_UNKNOWN_PHY                      16
#define STATUS_PENDING                          17
#define STATUS_NO_TX_DESC                       18
#define STATUS_NO_TX_BD                         19
#define STATUS_UNKNOWN_MEDIUM                   20
#define STATUS_RESOURCE                         21
#define STATUS_ABORT_REASON_DISCONNECT          22
#define STATUS_ABORT_REASON_UPLOAD              23
#define STATUS_TIMEOUT                          0xffff
/*******************************************************************************
 * Receive filter masks.
 ******************************************************************************/
#define RX_MASK_ACCEPT_NONE                     0x0000
#define RX_MASK_ACCEPT_UNICAST                  0x0001
#define RX_MASK_ACCEPT_MULTICAST                0x0002
#define RX_MASK_ACCEPT_ALL_MULTICAST            0x0004
#define RX_MASK_ACCEPT_BROADCAST                0x0008
#define RX_MASK_ACCEPT_ERROR_PACKET             0x0010
#define RX_MASK_PROMISCUOUS_MODE                0x10000
/*******************************************************************************
 * media speed.
 ******************************************************************************/
#define MEDIUM_SPEED_AUTONEG                    0x0000L
#define MEDIUM_SPEED_UNKNOWN                    0x0000L
#define MEDIUM_SPEED_10MBPS                     0x0100L
#define MEDIUM_SPEED_100MBPS                    0x0200L
#define MEDIUM_SPEED_1000MBPS                   0x0300L
#define MEDIUM_SPEED_2500MBPS                   0x0400L
#define MEDIUM_SPEED_10GBPS                     0x0600L
#define MEDIUM_SPEED_20GBPS                     0x0700L
#define MEDIUM_SPEED_25GBPS                     0x0800L
#define MEDIUM_SPEED_40GBPS                     0x0900L
#define MEDIUM_SPEED_50GBPS                     0x0a00L
#define MEDIUM_SPEED_100GBPS                    0x0b00L
#define MEDIUM_SPEED_200GBPS                    0x0c00L
#define MEDIUM_SPEED_50PAM4GBPS                 0x0d00L
#define MEDIUM_SPEED_100PAM4GBPS                0x0e00L
#define MEDIUM_SPEED_100PAM4_112GBPS            0x0f00L
#define MEDIUM_SPEED_200PAM4_112GBPS            0x1000L
#define MEDIUM_SPEED_400PAM4GBPS                0x2000L
#define MEDIUM_SPEED_400PAM4_112GBPS            0x3000L
#define MEDIUM_SPEED_AUTONEG_1G_FALLBACK        0x8000L /* Serdes */
#define MEDIUM_SPEED_AUTONEG_2_5G_FALLBACK      0x8100L /* Serdes */
#define MEDIUM_SPEED_HARDWARE_DEFAULT           0xff00L /* Serdes nvram def.*/
#define MEDIUM_SPEED_MASK                       0xff00L
#define GET_MEDIUM_SPEED(m)                     ((m) & MEDIUM_SPEED_MASK)
#define SET_MEDIUM_SPEED(bp, s) ((bp->medium & ~MEDIUM_SPEED_MASK) | s)
#define MEDIUM_UNKNOWN_DUPLEX                   0x00000L
#define MEDIUM_FULL_DUPLEX                      0x00000L
#define MEDIUM_HALF_DUPLEX                      0x10000L
#define GET_MEDIUM_DUPLEX(m)                    ((m) & MEDIUM_HALF_DUPLEX)
#define SET_MEDIUM_DUPLEX(bp, d) ((bp->medium & ~MEDIUM_HALF_DUPLEX) | d)
#define MEDIUM_SELECTIVE_AUTONEG                0x01000000L
#define GET_MEDIUM_AUTONEG_MODE(m)              ((m) & 0xff000000L)
#define PCICFG_ME_REGISTER                      0x98
#define GRC_COM_CHAN_BASE                       0
#define GRC_COM_CHAN_TRIG                       0x100
#define GRC_IND_BAR_0_ADDR                      0x78
#define GRC_IND_BAR_1_ADDR                      0x7C
#define GRC_IND_BAR_0_DATA                      0x80
#define GRC_IND_BAR_1_DATA                      0x84
#define GRC_BASE_WIN_0                          0x400
#define GRC_DATA_WIN_0                          0x1000
#define HWRM_CMD_DEFAULT_TIMEOUT                500 /* in Miliseconds  */
#define HWRM_CMD_POLL_WAIT_TIME                 100 /* In MicroeSconds */
#define HWRM_CMD_DEFAULT_MULTIPLAYER(a)         ((a) * 10)
#define HWRM_CMD_FLASH_MULTIPLAYER(a)           ((a) * 100)
#define HWRM_CMD_FLASH_ERASE_MULTIPLAYER(a)     ((a) * 1000)
#define HWRM_CMD_WAIT(b) ((bp->hwrm_cmd_timeout) * (b))
#define MAX_ETHERNET_PACKET_BUFFER_SIZE         1536
#define DEFAULT_NUMBER_OF_CMPL_RINGS            0x01
#define DEFAULT_NUMBER_OF_TX_RINGS              0x01
#define DEFAULT_NUMBER_OF_RX_RINGS              0x01
#define DEFAULT_NUMBER_OF_RING_GRPS             0x01
#define DEFAULT_NUMBER_OF_STAT_CTXS             0x01
#define NUM_RX_BUFFERS                          2 /* From 8 */
/* This is to fix the HTTP hanging issue https://github.com/ipxe/ipxe/issues/1023#issuecomment-2188474322 */
#define MAX_RX_DESC_CNT                         16
#define MAX_TX_DESC_CNT                         16
#define MAX_CQ_DESC_CNT                         64
#define TX_RING_BUFFER_SIZE (MAX_TX_DESC_CNT * sizeof(struct tx_bd_short))
#define RX_RING_BUFFER_SIZE \
	(MAX_RX_DESC_CNT * sizeof(struct rx_prod_pkt_bd))
#define CQ_RING_BUFFER_SIZE (MAX_CQ_DESC_CNT * sizeof(struct cmpl_base))
#define BNXT_DMA_ALIGNMENT                      256 //64
#define DMA_ALIGN_4K                            4096 //thor tx & rx
#define REQ_BUFFER_SIZE                         1024
#define RESP_BUFFER_SIZE                        1024
#define DMA_BUFFER_SIZE                         1024
#define LM_PAGE_BITS(a)                         (a)
#define BNXT_RX_STD_DMA_SZ                      (1536 + 64 + 2)
#define NEXT_IDX(N, S)                          (((N) + 1) & ((S) - 1))
#define BD_NOW(bd, entry, len) (&((u8 *)(bd))[(entry) * (len)])
#define BNXT_CQ_INTR_MODE(vf) (\
	((vf) ? RING_ALLOC_REQ_INT_MODE_MSIX : RING_ALLOC_REQ_INT_MODE_POLL))
/* Set default link timeout period to 1 second */
#define LINK_DEFAULT_TIMEOUT                    1000
#define LINK_POLL_WAIT_TIME                     100    /* In Miliseconds */
#define RX_MASK (\
	RX_MASK_ACCEPT_BROADCAST | \
	RX_MASK_ACCEPT_ALL_MULTICAST | \
	RX_MASK_ACCEPT_MULTICAST)
#define MAX_NQ_DESC_CNT                         64
#define NQ_RING_BUFFER_SIZE (MAX_NQ_DESC_CNT * sizeof(struct cmpl_base))
#define RX_RING_QID (FLAG_TEST(bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS) ? bp->queue_id : 0)
#define STAT_CTX_ID ((bp->vf || FLAG_TEST(bp->flags, BNXT_FLAG_IS_CHIP_P5_PLUS)) ? bp->stat_ctx_id : 0)
#define TX_AVAIL(r)                      (r - 1)
#define TX_IN_USE(a, b, c) ((a - b) & (c - 1))
#define NO_MORE_NQ_BD_TO_SERVICE         1
#define SERVICE_NEXT_NQ_BD               0
#define NO_MORE_CQ_BD_TO_SERVICE         1
#define SERVICE_NEXT_CQ_BD               0
#define MAC_HDR_SIZE     12
#define VLAN_HDR_SIZE    4
#define ETHERTYPE_VLAN   0x8100
#define BYTE_SWAP_S(w) (\
	(((w) & 0xff00) >> 8) | \
	(((w) & 0x00ff) << 8))
#define DB_OFFSET_PF           0x10000
#define DB_OFFSET_VF           0x4000
#define DBC_MSG_IDX(idx)       (\
	((idx) << DBC_DBC_INDEX_SFT) & DBC_DBC_INDEX_MASK)
#define DBC_MSG_XID(xid, flg)  (\
	(((xid) << DBC_DBC_XID_SFT) & DBC_DBC_XID_MASK) | \
	DBC_DBC_PATH_L2 | (FLAG_TEST ( bp->flags, BNXT_FLAG_IS_CHIP_P7 ) ? DBC_DBC_VALID : 0) | (flg))
#define DBC_MSG_EPCH(idx)      (\
        ((idx) << DBC_DBC_EPOCH_SFT))
#define DBC_MSG_TOGGLE(idx)    (\
        ((idx) << DBC_DBC_TOGGLE_SFT) & DBC_DBC_TOGGLE_MASK)
#define PHY_STATUS         0x0001
#define PHY_SPEED          0x0002
#define DETECT_MEDIA       0x0004
#define SUPPORT_SPEEDS     0x0008
#define SUPPORT_SPEEDS2    0x0010
#define QCFG_PHY_ALL   (\
	SUPPORT_SPEEDS | SUPPORT_SPEEDS2 | \
        DETECT_MEDIA | PHY_SPEED | PHY_STATUS)
#define str_mbps           "Mbps"
#define str_gbps           "Gbps"
/*
 * Broadcom ethernet driver nvm defines.
 */
/* nvm cfg 203 - u32 link_settings */
#define LINK_SPEED_DRV_NUM                                      203
#define LINK_SPEED_DRV_MASK                                     0x0000000F
#define LINK_SPEED_DRV_SHIFT                                    0
#define LINK_SPEED_DRV_AUTONEG                                  0x0
#define NS_LINK_SPEED_DRV_AUTONEG                               0x0
#define LINK_SPEED_DRV_1G                                       0x1
#define NS_LINK_SPEED_DRV_1G                                    0x1
#define LINK_SPEED_DRV_10G                                      0x2
#define NS_LINK_SPEED_DRV_10G                                   0x2
#define LINK_SPEED_DRV_25G                                      0x3
#define NS_LINK_SPEED_DRV_25G                                   0x3
#define LINK_SPEED_DRV_40G                                      0x4
#define NS_LINK_SPEED_DRV_40G                                   0x4
#define LINK_SPEED_DRV_50G                                      0x5
#define NS_LINK_SPEED_DRV_50G                                   0x5
#define LINK_SPEED_DRV_100G                                     0x6
#define NS_LINK_SPEED_DRV_100G                                  0x6
#define LINK_SPEED_DRV_200G                                     0x7
#define NS_LINK_SPEED_DRV_200G                                  0x7
#define LINK_SPEED_DRV_2_5G                                     0xE
#define NS_LINK_SPEED_DRV_2_5G                                  0xE
#define LINK_SPEED_DRV_100M                                     0xF
#define NS_LINK_SPEED_DRV_100M                                  0xF
/* nvm cfg 201 - u32 speed_cap_mask */
#define SPEED_CAPABILITY_DRV_MASK                               0x0000FFFF
#define SPEED_CAPABILITY_DRV_SHIFT                              0
#define SPEED_CAPABILITY_DRV_1G                                 0x1
#define NS_SPEED_CAPABILITY_DRV_1G                              0x1
#define SPEED_CAPABILITY_DRV_10G                                0x2
#define NS_SPEED_CAPABILITY_DRV_10G                             0x2
#define SPEED_CAPABILITY_DRV_25G                                0x4
#define NS_SPEED_CAPABILITY_DRV_25G                             0x4
#define SPEED_CAPABILITY_DRV_40G                                0x8
#define NS_SPEED_CAPABILITY_DRV_40G                             0x8
#define SPEED_CAPABILITY_DRV_50G                                0x10
#define NS_SPEED_CAPABILITY_DRV_50G                             0x10
#define SPEED_CAPABILITY_DRV_100G                               0x20
#define NS_SPEED_CAPABILITY_DRV_100G                            0x20
#define SPEED_CAPABILITY_DRV_200G                               0x40
#define NS_SPEED_CAPABILITY_DRV_200G                            0x40
#define SPEED_CAPABILITY_DRV_2_5G                               0x4000
#define NS_SPEED_CAPABILITY_DRV_2_5G                            0x4000
#define SPEED_CAPABILITY_DRV_100M                               0x8000
#define NS_SPEED_CAPABILITY_DRV_100M                            0x8000
/* nvm cfg 202 */
#define SPEED_CAPABILITY_FW_MASK                                0xFFFF0000
#define SPEED_CAPABILITY_FW_SHIFT                               16
#define SPEED_CAPABILITY_FW_1G                                  (0x1L << 16)
#define NS_SPEED_CAPABILITY_FW_1G                               (0x1)
#define SPEED_CAPABILITY_FW_10G                                 (0x2L << 16)
#define NS_SPEED_CAPABILITY_FW_10G                              (0x2)
#define SPEED_CAPABILITY_FW_25G                                 (0x4L << 16)
#define NS_SPEED_CAPABILITY_FW_25G                              (0x4)
#define SPEED_CAPABILITY_FW_40G                                 (0x8L << 16)
#define NS_SPEED_CAPABILITY_FW_40G                              (0x8)
#define SPEED_CAPABILITY_FW_50G                                 (0x10L << 16)
#define NS_SPEED_CAPABILITY_FW_50G                              (0x10)
#define SPEED_CAPABILITY_FW_100G                                (0x20L << 16)
#define NS_SPEED_CAPABILITY_FW_100G                             (0x20)
#define SPEED_CAPABILITY_FW_200G                                (0x40L << 16)
#define NS_SPEED_CAPABILITY_FW_200G                             (0x40)
#define SPEED_CAPABILITY_FW_2_5G                                (0x4000L << 16)
#define NS_SPEED_CAPABILITY_FW_2_5G                             (0x4000)
#define SPEED_CAPABILITY_FW_100M                                (0x8000UL << 16)
#define NS_SPEED_CAPABILITY_FW_100M                             (0x8000)
/* nvm cfg 205 */
#define LINK_SPEED_FW_NUM                                       205
#define LINK_SPEED_FW_MASK                                      0x00000780
#define LINK_SPEED_FW_SHIFT                                     7
#define LINK_SPEED_FW_AUTONEG                                   (0x0L << 7)
#define NS_LINK_SPEED_FW_AUTONEG                                (0x0)
#define LINK_SPEED_FW_1G                                        (0x1L << 7)
#define NS_LINK_SPEED_FW_1G                                     (0x1)
#define LINK_SPEED_FW_10G                                       (0x2L << 7)
#define NS_LINK_SPEED_FW_10G                                    (0x2)
#define LINK_SPEED_FW_25G                                       (0x3L << 7)
#define NS_LINK_SPEED_FW_25G                                    (0x3)
#define LINK_SPEED_FW_40G                                       (0x4L << 7)
#define NS_LINK_SPEED_FW_40G                                    (0x4)
#define LINK_SPEED_FW_50G                                       (0x5L << 7)
#define NS_LINK_SPEED_FW_50G                                    (0x5)
#define LINK_SPEED_FW_100G                                      (0x6L << 7)
#define NS_LINK_SPEED_FW_100G                                   (0x6)
#define LINK_SPEED_FW_200G                                      (0x7L << 7)
#define NS_LINK_SPEED_FW_200G                                   (0x7)
#define LINK_SPEED_FW_50G_PAM4                                  (0x8L << 7)
#define NS_LINK_SPEED_FW_50G_PAM4                               (0x8)
#define LINK_SPEED_FW_100G_PAM4                                 (0x9L << 7)
#define NS_LINK_SPEED_FW_100G_PAM4                              (0x9)
#define LINK_SPEED_FW_100G_PAM4_112                             (0xAL << 7)
#define NS_LINK_SPEED_FW_100G_PAM4_112                          (0xA)
#define LINK_SPEED_FW_200G_PAM4_112                             (0xBL << 7)
#define NS_LINK_SPEED_FW_200G_PAM4_112                          (0xB)
#define LINK_SPEED_FW_400G_PAM4                                 (0xCL << 7)
#define NS_LINK_SPEED_FW_400G_PAM4                              (0xC)
#define LINK_SPEED_FW_400G_PAM4_112                             (0xDL << 7)
#define NS_LINK_SPEED_FW_400G_PAM4_112                          (0xD)
#define LINK_SPEED_FW_2_5G                                      (0xEL << 7)
#define NS_LINK_SPEED_FW_2_5G                                   (0xE)
#define LINK_SPEED_FW_100M                                      (0xFL << 7)
#define NS_LINK_SPEED_FW_100M                                   (0xF)
/* nvm cfg 210 */
#define D3_LINK_SPEED_FW_NUM                                    210
#define D3_LINK_SPEED_FW_MASK                                   0x000F0000
#define D3_LINK_SPEED_FW_SHIFT                                  16
#define D3_LINK_SPEED_FW_AUTONEG                                (0x0L << 16)
#define NS_D3_LINK_SPEED_FW_AUTONEG                             (0x0)
#define D3_LINK_SPEED_FW_1G                                     (0x1L << 16)
#define NS_D3_LINK_SPEED_FW_1G                                  (0x1)
#define D3_LINK_SPEED_FW_10G                                    (0x2L << 16)
#define NS_D3_LINK_SPEED_FW_10G                                 (0x2)
#define D3_LINK_SPEED_FW_25G                                    (0x3L << 16)
#define NS_D3_LINK_SPEED_FW_25G                                 (0x3)
#define D3_LINK_SPEED_FW_40G                                    (0x4L << 16)
#define NS_D3_LINK_SPEED_FW_40G                                 (0x4)
#define D3_LINK_SPEED_FW_50G                                    (0x5L << 16)
#define NS_D3_LINK_SPEED_FW_50G                                 (0x5)
#define D3_LINK_SPEED_FW_100G                                   (0x6L << 16)
#define NS_D3_LINK_SPEED_FW_100G                                (0x6)
#define D3_LINK_SPEED_FW_200G                                   (0x7L << 16)
#define NS_D3_LINK_SPEED_FW_200G                                (0x7)
#define D3_LINK_SPEED_FW_2_5G                                   (0xEL << 16)
#define NS_D3_LINK_SPEED_FW_2_5G                                (0xE)
#define D3_LINK_SPEED_FW_100M                                   (0xFL << 16)
#define NS_D3_LINK_SPEED_FW_100M                                (0xF)
/* nvm cfg 211 */
#define D3_FLOW_CONTROL_FW_NUM                                  211
#define D3_FLOW_CONTROL_FW_MASK                                 0x00700000
#define D3_FLOW_CONTROL_FW_SHIFT                                20
#define D3_FLOW_CONTROL_FW_AUTO                                 (0x0L << 20)
#define NS_D3_FLOW_CONTROL_FW_AUTO                              (0x0)
#define D3_FLOW_CONTROL_FW_TX                                   (0x1L << 20)
#define NS_D3_FLOW_CONTROL_FW_TX                                (0x1)
#define D3_FLOW_CONTROL_FW_RX                                   (0x2L << 20)
#define NS_D3_FLOW_CONTROL_FW_RX                                (0x2)
#define D3_FLOW_CONTROL_FW_BOTH                                 (0x3L << 20)
#define NS_D3_FLOW_CONTROL_FW_BOTH                              (0x3)
#define D3_FLOW_CONTROL_FW_NONE                                 (0x4L << 20)
#define NS_D3_FLOW_CONTROL_FW_NONE                              (0x4)
/* nvm cfg 213 */
#define PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_NUM            213
#define PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_MASK           0x02000000
#define PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_SHIFT          25
#define PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_DISABLED       (0x0L << 25)
#define NS_PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_DISABLED    (0x0)
#define PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_ENABLED        (0x1L << 25)
#define NS_PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_ENABLED     (0x1)
/* nvm cfg 357 - u32 mba_cfg2 */
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_VALUE_NUM                    357
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_VALUE_MASK                   0x0000FFFF
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_VALUE_SHIFT                  0
/* nvm cfg 358 - u32 mba_cfg2 */
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_NUM                          358
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_MASK                         0x00010000
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_SHIFT                        16
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_DISABLED                     (0x0L << 16)
#define NS_FUNC_CFG_PRE_BOOT_MBA_VLAN_DISABLED                  (0x0)
#define FUNC_CFG_PRE_BOOT_MBA_VLAN_ENABLED                      (0x1L << 16)
#define NS_FUNC_CFG_PRE_BOOT_MBA_VLAN_ENABLED                   (0x1)

struct tx_doorbell {
	u32 key_idx;
#define TX_DOORBELL_IDX_MASK      0xffffffUL
#define TX_DOORBELL_IDX_SFT       0
#define TX_DOORBELL_KEY_MASK      0xf0000000UL
#define TX_DOORBELL_KEY_SFT       28
    #define TX_DOORBELL_KEY_TX    (0x0UL << 28)
    #define TX_DOORBELL_KEY_LAST  TX_DOORBELL_KEY_TX
};

struct rx_doorbell {
	u32 key_idx;
#define RX_DOORBELL_IDX_MASK      0xffffffUL
#define RX_DOORBELL_IDX_SFT       0
#define RX_DOORBELL_KEY_MASK      0xf0000000UL
#define RX_DOORBELL_KEY_SFT       28
    #define RX_DOORBELL_KEY_RX    (0x1UL << 28)
    #define RX_DOORBELL_KEY_LAST  RX_DOORBELL_KEY_RX
};

struct cmpl_doorbell {
	u32  key_mask_valid_idx;
#define CMPL_DOORBELL_IDX_MASK      0xffffffUL
#define CMPL_DOORBELL_IDX_SFT       0
#define CMPL_DOORBELL_IDX_VALID     0x4000000UL
#define CMPL_DOORBELL_MASK          0x8000000UL
#define CMPL_DOORBELL_KEY_MASK      0xf0000000UL
#define CMPL_DOORBELL_KEY_SFT       28
    #define CMPL_DOORBELL_KEY_CMPL  (0x2UL << 28)
    #define CMPL_DOORBELL_KEY_LAST  CMPL_DOORBELL_KEY_CMPL
};

/* dbc_dbc (size:64b/8B) */
struct dbc_dbc {
	__le32  index;
	#define DBC_DBC_INDEX_MASK          0xffffffUL
	#define DBC_DBC_INDEX_SFT           0
	#define DBC_DBC_EPOCH               0x1000000UL
	#define DBC_DBC_EPOCH_SFT           24
	#define DBC_DBC_TOGGLE_MASK         0x6000000UL
	#define DBC_DBC_TOGGLE_SFT          25
	__le32  type_path_xid;
	#define DBC_DBC_XID_MASK            0xfffffUL
	#define DBC_DBC_XID_SFT             0
	#define DBC_DBC_PATH_MASK           0x3000000UL
	#define DBC_DBC_PATH_SFT            24
	#define DBC_DBC_PATH_ROCE           (0x0UL << 24)
	#define DBC_DBC_PATH_L2             (0x1UL << 24)
	#define DBC_DBC_PATH_ENGINE         (0x2UL << 24)
	#define DBC_DBC_PATH_LAST           DBC_DBC_PATH_ENGINE
	#define DBC_DBC_VALID               0x4000000UL
	#define DBC_DBC_DEBUG_TRACE         0x8000000UL
	#define DBC_DBC_TYPE_MASK           0xf0000000UL
	#define DBC_DBC_TYPE_SFT            28
	#define DBC_DBC_TYPE_SQ             (0x0UL << 28)
	#define DBC_DBC_TYPE_RQ             (0x1UL << 28)
	#define DBC_DBC_TYPE_SRQ            (0x2UL << 28)
	#define DBC_DBC_TYPE_SRQ_ARM        (0x3UL << 28)
	#define DBC_DBC_TYPE_CQ             (0x4UL << 28)
	#define DBC_DBC_TYPE_CQ_ARMSE       (0x5UL << 28)
	#define DBC_DBC_TYPE_CQ_ARMALL      (0x6UL << 28)
	#define DBC_DBC_TYPE_CQ_ARMENA      (0x7UL << 28)
	#define DBC_DBC_TYPE_SRQ_ARMENA     (0x8UL << 28)
	#define DBC_DBC_TYPE_CQ_CUTOFF_ACK  (0x9UL << 28)
	#define DBC_DBC_TYPE_NQ             (0xaUL << 28)
	#define DBC_DBC_TYPE_NQ_ARM         (0xbUL << 28)
	#define DBC_DBC_TYPE_NULL           (0xfUL << 28)
	#define DBC_DBC_TYPE_LAST           DBC_DBC_TYPE_NULL
};

/*******************************************************************************
 * Transmit info.
 *****************************************************************************/
struct tx_bd_short {
	u16 flags_type;
#define TX_BD_SHORT_TYPE_MASK               0x3fUL
#define TX_BD_SHORT_TYPE_SFT                0
#define TX_BD_SHORT_TYPE_TX_BD_SHORT        0x0UL
#define TX_BD_SHORT_TYPE_LAST               TX_BD_SHORT_TYPE_TX_BD_SHORT
#define TX_BD_SHORT_FLAGS_MASK              0xffc0UL
#define TX_BD_SHORT_FLAGS_SFT               6
#define TX_BD_SHORT_FLAGS_PACKET_END        0x40UL
#define TX_BD_SHORT_FLAGS_NO_CMPL           0x80UL
#define TX_BD_SHORT_FLAGS_BD_CNT_MASK       0x1f00UL
#define TX_BD_SHORT_FLAGS_BD_CNT_SFT        8
#define TX_BD_SHORT_FLAGS_LHINT_MASK        0x6000UL
#define TX_BD_SHORT_FLAGS_LHINT_SFT         13
#define TX_BD_SHORT_FLAGS_LHINT_LT512       (0x0UL << 13)
#define TX_BD_SHORT_FLAGS_LHINT_LT1K        (0x1UL << 13)
#define TX_BD_SHORT_FLAGS_LHINT_LT2K        (0x2UL << 13)
#define TX_BD_SHORT_FLAGS_LHINT_GTE2K       (0x3UL << 13)
#define TX_BD_SHORT_FLAGS_LHINT_LAST        TX_BD_SHORT_FLAGS_LHINT_GTE2K
#define TX_BD_SHORT_FLAGS_COAL_NOW          0x8000UL
	u16 len;
	u32 opaque;
	union dma_addr64_t dma;
};

struct tx_cmpl {
	u16 flags_type;
#define TX_CMPL_TYPE_MASK        0x3fUL
#define TX_CMPL_TYPE_SFT         0
#define TX_CMPL_TYPE_TX_L2       0x0UL
#define TX_CMPL_TYPE_LAST        TX_CMPL_TYPE_TX_L2
#define TX_CMPL_FLAGS_MASK       0xffc0UL
#define TX_CMPL_FLAGS_SFT        6
#define TX_CMPL_FLAGS_ERROR      0x40UL
#define TX_CMPL_FLAGS_PUSH       0x80UL
	u16 unused_0;
	u32 opaque;
	u16 errors_v;
#define TX_CMPL_V                               0x1UL
#define TX_CMPL_ERRORS_MASK                     0xfffeUL
#define TX_CMPL_ERRORS_SFT                      1
#define TX_CMPL_ERRORS_BUFFER_ERROR_MASK        0xeUL
#define TX_CMPL_ERRORS_BUFFER_ERROR_SFT         1
#define TX_CMPL_ERRORS_BUFFER_ERROR_NO_ERROR    (0x0UL << 1)
#define TX_CMPL_ERRORS_BUFFER_ERROR_BAD_FMT     (0x2UL << 1)
#define TX_CMPL_ERRORS_BUFFER_ERROR_LAST TX_CMPL_ERRORS_BUFFER_ERROR_BAD_FMT
#define TX_CMPL_ERRORS_ZERO_LENGTH_PKT          0x10UL
#define TX_CMPL_ERRORS_EXCESSIVE_BD_LENGTH      0x20UL
#define TX_CMPL_ERRORS_DMA_ERROR                0x40UL
#define TX_CMPL_ERRORS_HINT_TOO_SHORT           0x80UL
#define TX_CMPL_ERRORS_POISON_TLP_ERROR         0x100UL
	u16 unused_1;
	u32 unused_2;
};

struct tx_info {
	void             *bd_virt;
	struct io_buffer *iob[MAX_TX_DESC_CNT];
	u16              prod_id;  /* Tx producer index. */
	u16              cons_id;
	u16              ring_cnt;
	u32              cnt;   /* Tx statistics. */
	u32              cnt_req;
	u8               epoch;
	u8               res[3];
};

struct cmpl_base {
	u16 type;
#define CMPL_BASE_TYPE_MASK              0x3fUL
#define CMPL_BASE_TYPE_SFT               0
#define CMPL_BASE_TYPE_TX_L2             0x0UL
#define CMPL_BASE_TYPE_RX_L2             0x11UL
#define CMPL_BASE_TYPE_RX_AGG            0x12UL
#define CMPL_BASE_TYPE_RX_TPA_START      0x13UL
#define CMPL_BASE_TYPE_RX_TPA_END        0x15UL
#define CMPL_BASE_TYPE_RX_L2_V3          0x17UL
#define CMPL_BASE_TYPE_STAT_EJECT        0x1aUL
#define CMPL_BASE_TYPE_HWRM_DONE         0x20UL
#define CMPL_BASE_TYPE_HWRM_FWD_REQ      0x22UL
#define CMPL_BASE_TYPE_HWRM_FWD_RESP     0x24UL
#define CMPL_BASE_TYPE_HWRM_ASYNC_EVENT  0x2eUL
#define CMPL_BASE_TYPE_CQ_NOTIFICATION   0x30UL
#define CMPL_BASE_TYPE_SRQ_EVENT         0x32UL
#define CMPL_BASE_TYPE_DBQ_EVENT         0x34UL
#define CMPL_BASE_TYPE_QP_EVENT          0x38UL
#define CMPL_BASE_TYPE_FUNC_EVENT        0x3aUL
#define CMPL_BASE_TYPE_LAST CMPL_BASE_TYPE_FUNC_EVENT
	u16 info1;
	u32 info2;
	u32 info3_v;
#define CMPL_BASE_V          0x1UL
#define CMPL_BASE_INFO3_MASK 0xfffffffeUL
#define CMPL_BASE_INFO3_SFT  1
	u32 info4;
};

struct cmp_info {
	void      *bd_virt;
	u16       cons_id;
	u16       ring_cnt;
	u8        completion_bit;
	u8        epoch;
	u8        res[2];
};

/* Completion Queue Notification */
/* nq_cn (size:128b/16B) */
struct nq_base {
	u16	type;
/*
 * This field indicates the exact type of the completion.
 * By convention, the LSB identifies the length of the
 * record in 16B units.  Even values indicate 16B
 * records.  Odd values indicate 32B
 * records.
 */
#define NQ_CN_TYPE_MASK           0x3fUL
#define NQ_CN_TYPE_SFT            0
#define NQ_CN_TOGGLE_MASK         0xc0UL
#define NQ_CN_TOGGLE_SFT          6
/* CQ Notification */
	    #define NQ_CN_TYPE_CQ_NOTIFICATION  0x30UL
	    #define NQ_CN_TYPE_LAST            NQ_CN_TYPE_CQ_NOTIFICATION
	u16	reserved16;
/*
 * This is an application level ID used to identify the
 * CQ.  This field carries the lower 32b of the value.
 */
	u32	cq_handle_low;
	u32	v;
/*
 * This value is written by the NIC such that it will be different
 * for each pass through the completion queue.   The even passes
 * will write 1.  The odd passes will write 0.
 */
#define NQ_CN_V     0x1UL
/*
 * This is an application level ID used to identify the
 * CQ.  This field carries the upper 32b of the value.
 */
	u32	cq_handle_high;
};

struct nq_info {
	void      *bd_virt;
	u16       cons_id;
	u16       ring_cnt;
	u8        completion_bit;
	u8        epoch;
	u8        toggle;
	u8        res[1];
};

struct rx_pkt_cmpl {
	u16 flags_type;
#define RX_PKT_CMPL_TYPE_MASK                    0x3fUL
#define RX_PKT_CMPL_TYPE_SFT                     0
#define RX_PKT_CMPL_TYPE_RX_L2                   0x11UL
#define RX_PKT_CMPL_TYPE_LAST                    RX_PKT_CMPL_TYPE_RX_L2
#define RX_PKT_CMPL_FLAGS_MASK                   0xffc0UL
#define RX_PKT_CMPL_FLAGS_SFT                    6
#define RX_PKT_CMPL_FLAGS_ERROR                  0x40UL
#define RX_PKT_CMPL_FLAGS_PLACEMENT_MASK         0x380UL
#define RX_PKT_CMPL_FLAGS_PLACEMENT_SFT          7
#define RX_PKT_CMPL_FLAGS_PLACEMENT_NORMAL       (0x0UL << 7)
#define RX_PKT_CMPL_FLAGS_PLACEMENT_JUMBO        (0x1UL << 7)
#define RX_PKT_CMPL_FLAGS_PLACEMENT_HDS          (0x2UL << 7)
#define RX_PKT_CMPL_FLAGS_PLACEMENT_LAST RX_PKT_CMPL_FLAGS_PLACEMENT_HDS
#define RX_PKT_CMPL_FLAGS_RSS_VALID              0x400UL
#define RX_PKT_CMPL_FLAGS_UNUSED                 0x800UL
#define RX_PKT_CMPL_FLAGS_ITYPE_MASK             0xf000UL
#define RX_PKT_CMPL_FLAGS_ITYPE_SFT              12
#define RX_PKT_CMPL_FLAGS_ITYPE_NOT_KNOWN          (0x0UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_IP                 (0x1UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_TCP                (0x2UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_UDP                (0x3UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_FCOE               (0x4UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_ROCE               (0x5UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_ICMP               (0x7UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_PTP_WO_TIMESTAMP   (0x8UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP    (0x9UL << 12)
#define RX_PKT_CMPL_FLAGS_ITYPE_LAST RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP
	u16 len;
	u32 opaque;
	u8  agg_bufs_v1;
#define RX_PKT_CMPL_V1            0x1UL
#define RX_PKT_CMPL_AGG_BUFS_MASK 0x3eUL
#define RX_PKT_CMPL_AGG_BUFS_SFT  1
#define RX_PKT_CMPL_UNUSED1_MASK  0xc0UL
#define RX_PKT_CMPL_UNUSED1_SFT   6
	u8  rss_hash_type;
	u8  payload_offset;
	u8  unused1;
	u32 rss_hash;
};

struct rx_pkt_cmpl_hi {
	u32  flags2;
#define RX_PKT_CMPL_FLAGS2_IP_CS_CALC         0x1UL
#define RX_PKT_CMPL_FLAGS2_L4_CS_CALC         0x2UL
#define RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC       0x4UL
#define RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC       0x8UL
#define RX_PKT_CMPL_FLAGS2_META_FORMAT_MASK   0xf0UL
#define RX_PKT_CMPL_FLAGS2_META_FORMAT_SFT    4
#define RX_PKT_CMPL_FLAGS2_META_FORMAT_NONE   (0x0UL << 4)
#define RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN   (0x1UL << 4)
#define RX_PKT_CMPL_FLAGS2_META_FORMAT_LAST   \
	RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN
#define RX_PKT_CMPL_FLAGS2_IP_TYPE            0x100UL
	u32 metadata;
#define RX_PKT_CMPL_METADATA_VID_MASK         0xfffUL
#define RX_PKT_CMPL_METADATA_VID_SFT          0
#define RX_PKT_CMPL_METADATA_DE               0x1000UL
#define RX_PKT_CMPL_METADATA_PRI_MASK         0xe000UL
#define RX_PKT_CMPL_METADATA_PRI_SFT          13
#define RX_PKT_CMPL_METADATA_TPID_MASK        0xffff0000UL
#define RX_PKT_CMPL_METADATA_TPID_SFT         16
	u16 errors_v2;
#define RX_PKT_CMPL_V2                        0x1UL
#define RX_PKT_CMPL_ERRORS_MASK               0xfffeUL
#define RX_PKT_CMPL_ERRORS_SFT                1
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_MASK  0xeUL
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_SFT   1
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_NO_BUFFER   (0x0UL << 1)
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_DID_NOT_FIT (0x1UL << 1)
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_NOT_ON_CHIP (0x2UL << 1)
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_BAD_FORMAT  (0x3UL << 1)
#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_LAST \
	RX_PKT_CMPL_ERRORS_BUFFER_ERROR_BAD_FORMAT
#define RX_PKT_CMPL_ERRORS_IP_CS_ERROR              0x10UL
#define RX_PKT_CMPL_ERRORS_L4_CS_ERROR              0x20UL
#define RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR            0x40UL
#define RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR            0x80UL
#define RX_PKT_CMPL_ERRORS_CRC_ERROR                0x100UL
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_MASK         0xe00UL
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_SFT          9
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_NO_ERROR     (0x0UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_VERSION   (0x1UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_HDR_LEN   (0x2UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_TUNNEL_TOTAL_ERROR (0x3UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_IP_TOTAL_ERROR   (0x4UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_UDP_TOTAL_ERROR  (0x5UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL       (0x6UL << 9)
#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_LAST \
	RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_MASK                 0xf000UL
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_SFT                  12
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_NO_ERROR             (0x0UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_VERSION       (0x1UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_HDR_LEN       (0x2UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_TTL           (0x3UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_IP_TOTAL_ERROR       (0x4UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_UDP_TOTAL_ERROR          (0x5UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN           (0x6UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN_TOO_SMALL (0x7UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN           (0x8UL << 12)
#define RX_PKT_CMPL_ERRORS_PKT_ERROR_LAST \
	RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN
	u16 cfa_code;
	u32 reorder;
#define RX_PKT_CMPL_REORDER_MASK 0xffffffUL
#define RX_PKT_CMPL_REORDER_SFT  0
};

struct rx_pkt_v3_cmpl {
	u16	flags_type;
	#define RX_PKT_V3_CMPL_TYPE_MASK                      0x3fUL
	#define RX_PKT_V3_CMPL_TYPE_SFT                       0
	/*
	 * RX L2 V3 completion:
	 * Completion of and L2 RX packet. Length = 32B
	 * This is the new version of the RX_L2 completion used in Thor2
	 * and later chips.
	 */
	#define RX_PKT_V3_CMPL_TYPE_RX_L2_V3                    0x17UL
	#define RX_PKT_V3_CMPL_TYPE_LAST                       RX_PKT_V3_CMPL_TYPE_RX_L2_V3
	#define RX_PKT_V3_CMPL_FLAGS_MASK                     0xffc0UL
	#define RX_PKT_V3_CMPL_FLAGS_SFT                      6
	#define RX_PKT_V3_CMPL_FLAGS_ERROR                     0x40UL
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_MASK            0x380UL
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_SFT             7
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_NORMAL            (0x0UL << 7)
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_JUMBO             (0x1UL << 7)
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_HDS               (0x2UL << 7)
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_TRUNCATION        (0x3UL << 7)
	#define RX_PKT_V3_CMPL_FLAGS_PLACEMENT_LAST             RX_PKT_V3_CMPL_FLAGS_PLACEMENT_TRUNCATION
	#define RX_PKT_V3_CMPL_FLAGS_RSS_VALID                 0x400UL
	#define RX_PKT_V3_CMPL_FLAGS_PKT_METADATA_PRESENT      0x800UL
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_MASK                0xf000UL
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_SFT                 12
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_NOT_KNOWN             (0x0UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_IP                    (0x1UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_TCP                   (0x2UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_UDP                   (0x3UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_FCOE                  (0x4UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_ROCE                  (0x5UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_ICMP                  (0x7UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_PTP_WO_TIMESTAMP      (0x8UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP       (0x9UL << 12)
	#define RX_PKT_V3_CMPL_FLAGS_ITYPE_LAST                 RX_PKT_V3_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP
	u16	len;
	u32	opaque;
	u16	rss_hash_type_agg_bufs_v1;
	#define RX_PKT_V3_CMPL_V1                   0x1UL
	#define RX_PKT_V3_CMPL_AGG_BUFS_MASK        0x3eUL
	#define RX_PKT_V3_CMPL_AGG_BUFS_SFT         1
	#define RX_PKT_V3_CMPL_UNUSED1              0x40UL
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_MASK   0xff80UL
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_SFT    7
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_0   (0x0UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_1   (0x1UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_3   (0x3UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_4   (0x4UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_5   (0x5UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_6   (0x6UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_7   (0x7UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_8   (0x8UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_9   (0x9UL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_10  (0xaUL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_11  (0xbUL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_12  (0xcUL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_13  (0xdUL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_14  (0xeUL << 7)
	#define RX_PKT_V3_CMPL_RSS_HASH_TYPE_LAST    RX_PKT_V3_CMPL_RSS_HASH_TYPE_ENUM_14
	u16	metadata1_payload_offset;
	#define RX_PKT_V3_CMPL_PAYLOAD_OFFSET_MASK        0x1ffUL
	#define RX_PKT_V3_CMPL_PAYLOAD_OFFSET_SFT         0
	#define RX_PKT_V3_CMPL_METADATA1_MASK             0xf000UL
	#define RX_PKT_V3_CMPL_METADATA1_SFT              12
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_MASK     0x7000UL
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_SFT      12
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPID88A8   (0x0UL << 12)
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPID8100   (0x1UL << 12)
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPID9100   (0x2UL << 12)
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPID9200   (0x3UL << 12)
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPID9300   (0x4UL << 12)
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPIDCFG    (0x5UL << 12)
	#define RX_PKT_V3_CMPL_METADATA1_TPID_SEL_LAST      RX_PKT_V3_CMPL_METADATA1_TPID_SEL_TPIDCFG
	#define RX_PKT_V3_CMPL_METADATA1_VALID             0x8000UL
	u32	rss_hash;
};

struct rx_pkt_v3_cmpl_hi {
	u32	flags2;
	#define RX_PKT_V3_CMPL_HI_FLAGS2_IP_CS_CALC                 0x1UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_L4_CS_CALC                 0x2UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_T_IP_CS_CALC               0x4UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_T_L4_CS_CALC               0x8UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_MASK           0xf0UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_SFT            4
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_NONE             (0x0UL << 4)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_ACT_REC_PTR      (0x1UL << 4)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_TUNNEL_ID        (0x2UL << 4)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_CHDR_DATA        (0x3UL << 4)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_HDR_OFFSET       (0x4UL << 4)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_LAST            RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_HDR_OFFSET
	#define RX_PKT_V3_CMPL_HI_FLAGS2_IP_TYPE                    0x100UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_COMPLETE_CHECKSUM_CALC     0x200UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_T_IP_TYPE                  0x400UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_T_IP_TYPE_IPV4               (0x0UL << 10)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_T_IP_TYPE_IPV6               (0x1UL << 10)
	#define RX_PKT_V3_CMPL_HI_FLAGS2_T_IP_TYPE_LAST              RX_PKT_V3_CMPL_HI_FLAGS2_T_IP_TYPE_IPV6
	#define RX_PKT_V3_CMPL_HI_FLAGS2_COMPLETE_CHECKSUM_MASK     0xffff0000UL
	#define RX_PKT_V3_CMPL_HI_FLAGS2_COMPLETE_CHECKSUM_SFT      16
	u32	metadata2;
	u16	errors_v2;
	#define RX_PKT_V3_CMPL_HI_V2                                       0x1UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_MASK                              0xfffeUL
	#define RX_PKT_V3_CMPL_HI_ERRORS_SFT                               1
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_MASK                  0xeUL
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_SFT                   1
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_NO_BUFFER               (0x0UL << 1)
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_DID_NOT_FIT             (0x1UL << 1)
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_NOT_ON_CHIP             (0x2UL << 1)
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_BAD_FORMAT              (0x3UL << 1)
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_FLUSH                   (0x5UL << 1)
	#define RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_LAST                   RX_PKT_V3_CMPL_HI_ERRORS_BUFFER_ERROR_FLUSH
	#define RX_PKT_V3_CMPL_HI_ERRORS_IP_CS_ERROR                        0x10UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_L4_CS_ERROR                        0x20UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_IP_CS_ERROR                      0x40UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_L4_CS_ERROR                      0x80UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_CRC_ERROR                          0x100UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_MASK                   0xe00UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_SFT                    9
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_NO_ERROR                 (0x0UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_L3_BAD_VERSION         (0x1UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_L3_BAD_HDR_LEN         (0x2UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_IP_TOTAL_ERROR         (0x3UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_UDP_TOTAL_ERROR        (0x4UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL             (0x5UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_TOTAL_ERROR            (0x6UL << 9)
	#define RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_LAST                    RX_PKT_V3_CMPL_HI_ERRORS_T_PKT_ERROR_T_TOTAL_ERROR
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_MASK                     0xf000UL
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_SFT                      12
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_NO_ERROR                   (0x0UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L3_BAD_VERSION             (0x1UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L3_BAD_HDR_LEN             (0x2UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L3_BAD_TTL                 (0x3UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_IP_TOTAL_ERROR             (0x4UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_UDP_TOTAL_ERROR            (0x5UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN             (0x6UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN_TOO_SMALL   (0x7UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN             (0x8UL << 12)
	#define RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_LAST                      RX_PKT_V3_CMPL_HI_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN
	u16	metadata0;
	#define RX_PKT_V3_CMPL_HI_METADATA0_VID_MASK 0xfffUL
	#define RX_PKT_V3_CMPL_HI_METADATA0_VID_SFT 0
	#define RX_PKT_V3_CMPL_HI_METADATA0_DE      0x1000UL
	/* When meta_format=1, this value is the VLAN PRI. */
	#define RX_PKT_V3_CMPL_HI_METADATA0_PRI_MASK 0xe000UL
	#define RX_PKT_V3_CMPL_HI_METADATA0_PRI_SFT 13
	u32	timestamp;
};

struct rx_prod_pkt_bd {
	u16  flags_type;
#define RX_PROD_PKT_BD_TYPE_MASK          0x3fUL
#define RX_PROD_PKT_BD_TYPE_SFT           0
#define RX_PROD_PKT_BD_TYPE_RX_PROD_PKT   0x4UL
#define RX_PROD_PKT_BD_TYPE_LAST          RX_PROD_PKT_BD_TYPE_RX_PROD_PKT
#define RX_PROD_PKT_BD_FLAGS_MASK         0xffc0UL
#define RX_PROD_PKT_BD_FLAGS_SFT          6
#define RX_PROD_PKT_BD_FLAGS_SOP_PAD      0x40UL
#define RX_PROD_PKT_BD_FLAGS_EOP_PAD      0x80UL
#define RX_PROD_PKT_BD_FLAGS_BUFFERS_MASK 0x300UL
#define RX_PROD_PKT_BD_FLAGS_BUFFERS_SFT  8
	u16  len;
	u32  opaque;
	union dma_addr64_t dma;
};

struct rx_info {
	void              *bd_virt;
	struct io_buffer  *iob[NUM_RX_BUFFERS];
	u16               iob_cnt;
	u16               buf_cnt;  /* Total Rx buffer descriptors. */
	u16               ring_cnt;
	u16               cons_id;  /* Last processed consumer index. */
/* Receive statistics. */
	u32               cnt;
	u32               good;
	u32               drop_err;
	u32               drop_lb;
	u32               drop_vlan;
	u8                epoch;
	u8                res[3];
};

#define VALID_DRIVER_REG          0x0001
#define VALID_STAT_CTX            0x0002
#define VALID_RING_CQ             0x0004
#define VALID_RING_TX             0x0008
#define VALID_RING_RX             0x0010
#define VALID_RING_GRP            0x0020
#define VALID_VNIC_ID             0x0040
#define VALID_RX_IOB              0x0080
#define VALID_L2_FILTER           0x0100
#define VALID_RING_NQ             0x0200

struct bnxt {
/* begin "general, frequently-used members" cacheline section */
/* If the IRQ handler (which runs lockless) needs to be
 * quiesced, the following bitmask state is used. The
 * SYNC flag is set by non-IRQ context code to initiate
 * the quiescence.
 *
 * When the IRQ handler notices that SYNC is set, it
 * disables interrupts and returns.
 *
 * When all outstanding IRQ handlers have returned after
 * the SYNC flag has been set, the setter can be assured
 * that interrupts will no longer get run.
 *
 * In this way all SMP driver locks are never acquired
 * in hw IRQ context, only sw IRQ context or lower.
 */
	unsigned int              irq_sync;
	struct net_device         *dev;
	struct pci_device         *pdev;
	void                      *hwrm_addr_req;
	void                      *hwrm_addr_resp;
	void                      *hwrm_addr_dma;
	dma_addr_t                req_addr_mapping;
	dma_addr_t                resp_addr_mapping;
	dma_addr_t                dma_addr_mapping;
	struct tx_info            tx; /* Tx info. */
	struct rx_info            rx; /* Rx info. */
	struct cmp_info           cq; /* completion info. */
	struct nq_info            nq; /* completion info. */
	u16                       nq_ring_id;
	u8                        queue_id;
	u16                       last_resp_code;
	u16                       seq_id;
	u32                       flag_hwrm;
	u32                       flags;
/* PCI info. */
	u16                       subsystem_vendor;
	u16                       subsystem_device;
	u16                       cmd_reg;
	u8                        pf_num;  /* absolute PF number */
	u8                        vf;
	void                      *bar0;
	void                      *bar1;
	void                      *bar2;
/* Device info. */
	u16                       chip_num;
/* chip num:16-31, rev:12-15, metal:4-11, bond_id:0-3 */
	u32                       chip_id;
	u32                       hwrm_cmd_timeout;
	u16                       hwrm_spec_code;
	u16                       hwrm_max_req_len;
	u16                       hwrm_max_ext_req_len;
	u8                        mac_addr[ETH_ALEN]; /* HW MAC address */
	u16                       fid;
	u8                        port_idx;
	u8                        ordinal_value;
	u16                       mtu;
	u16                       ring_grp_id;
	u16                       cq_ring_id;
	u16                       tx_ring_id;
	u16                       rx_ring_id;
	u16                       current_link_speed;
	u16                       link_status;
	u16                       wait_link_timeout;
	u64                       l2_filter_id;
	u16                       vnic_id;
	u16                       stat_ctx_id;
	u16                       vlan_id;
	u16                       vlan_tx;
	u32                       mba_cfg2;
	u32                       medium;
	u16                       support_speeds;
	u16                       auto_link_speeds2_mask;
	u32                       link_set;
	u8                        media_detect;
	u8                        rsvd;
	u16                       max_vfs;
	u16                       vf_res_strategy;
	u16                       min_vnics;
	u16                       max_vnics;
	u16                       max_msix;
	u16                       min_hw_ring_grps;
	u16                       max_hw_ring_grps;
	u16                       min_tx_rings;
	u16                       max_tx_rings;
	u16                       min_rx_rings;
	u16                       max_rx_rings;
	u16                       min_cp_rings;
	u16                       max_cp_rings;
	u16                       min_rsscos_ctxs;
	u16                       max_rsscos_ctxs;
	u16                       min_l2_ctxs;
	u16                       max_l2_ctxs;
	u16                       min_stat_ctxs;
	u16                       max_stat_ctxs;
	u16                       num_cmpl_rings;
	u16                       num_tx_rings;
	u16                       num_rx_rings;
	u16                       num_stat_ctxs;
	u16                       num_hw_ring_grps;
};

/* defines required to rsolve checkpatch errors / warnings */
#define test_if               if
#define write32               writel
#define write64               writeq
#define pci_read_byte         pci_read_config_byte
#define pci_read_word16       pci_read_config_word
#define pci_write_word        pci_write_config_word
#define SHORT_CMD_SUPPORTED   VER_GET_RESP_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED
#define SHORT_CMD_REQUIRED    VER_GET_RESP_DEV_CAPS_CFG_SHORT_CMD_REQUIRED
#define CQ_DOORBELL_KEY_MASK(a) (\
	CMPL_DOORBELL_KEY_CMPL | \
	CMPL_DOORBELL_IDX_VALID | \
	CMPL_DOORBELL_MASK | \
	(u32)(a))
#define CQ_DOORBELL_KEY_IDX(a) (\
	CMPL_DOORBELL_KEY_CMPL | \
	CMPL_DOORBELL_IDX_VALID | \
	(u32)(a))
#define TX_BD_FLAGS (\
	TX_BD_SHORT_TYPE_TX_BD_SHORT |\
	TX_BD_SHORT_FLAGS_COAL_NOW   |\
	TX_BD_SHORT_FLAGS_PACKET_END |\
	(1 << TX_BD_SHORT_FLAGS_BD_CNT_SFT))
#define PORT_PHY_FLAGS (\
	BNXT_FLAG_NPAR_MODE | \
	BNXT_FLAG_MULTI_HOST)
#define RING_FREE(bp, rid, flag) bnxt_hwrm_ring_free(bp, rid, flag)
#define SET_LINK(p, m, s) ((p & (m >> s)) << s)
#define SET_MBA(p, m, s)  ((p & (m >> s)) << s)
#define SPEED_DRV_MASK    LINK_SPEED_DRV_MASK
#define SPEED_DRV_SHIFT   LINK_SPEED_DRV_SHIFT
#define SPEED_FW_MASK     LINK_SPEED_FW_MASK
#define SPEED_FW_SHIFT    LINK_SPEED_FW_SHIFT
#define D3_SPEED_FW_MASK  D3_LINK_SPEED_FW_MASK
#define D3_SPEED_FW_SHIFT D3_LINK_SPEED_FW_SHIFT
#define MEDIA_AUTO_DETECT_MASK  PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_MASK
#define MEDIA_AUTO_DETECT_SHIFT PORT_CFG_LINK_SETTINGS_MEDIA_AUTO_DETECT_SHIFT
#define VLAN_MASK        FUNC_CFG_PRE_BOOT_MBA_VLAN_MASK
#define VLAN_SHIFT       FUNC_CFG_PRE_BOOT_MBA_VLAN_SHIFT
#define VLAN_VALUE_MASK  FUNC_CFG_PRE_BOOT_MBA_VLAN_VALUE_MASK
#define VLAN_VALUE_SHIFT FUNC_CFG_PRE_BOOT_MBA_VLAN_VALUE_SHIFT
#define VF_CFG_ENABLE_FLAGS (\
	FUNC_VF_CFG_REQ_ENABLES_MTU | \
	FUNC_VF_CFG_REQ_ENABLES_GUEST_VLAN | \
	FUNC_VF_CFG_REQ_ENABLES_ASYNC_EVENT_CR | \
	FUNC_VF_CFG_REQ_ENABLES_DFLT_MAC_ADDR)

#define CHIP_NUM_57508       0x1750
#define CHIP_NUM_57504       0x1751
#define CHIP_NUM_57502       0x1752

#define CHIP_NUM_57608       0x1760
