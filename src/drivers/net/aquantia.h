#ifndef _AQUANTIA_H
#define _AQUANTIA_H

/** @file
*
* aQuantia AQC network card driver
*
*/

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

#include <stdint.h>
#include <ipxe/if_ether.h>
#include <ipxe/nvs.h>

#define AQ_BAR_SIZE   0xA000
#define AQ_RING_SIZE  16
#define AQ_RING_ALIGN 128
#define AQ_RX_MAX_LEN 2048

#define AQ_IRQ_TX   0x00000001U
#define AQ_IRQ_RX   0x00000002U

/*IRQ Statur Register*/
#define AQ_IRQ_STAT_REG 0x00002000U

/* Interrupt Vector Allocation Register */
#define AQ_IRQ_CTRL             0x00002300U
#define AQ_IRQ_CTRL_COR_EN      0x00000080U /*IRQ clear on read */
#define AQ_IRQ_CTRL_REG_RST_DIS 0x20000000U /*Register reset disable */

/*TX/RX Interruprt Mapping*/
#define AQ_IRQ_MAP_REG1        0x00002100U /*IRQ mapping register */

#define AQ_IRQ_MAP_REG1_RX0_EN 0x00008000U /*IRQ RX0 enable*/
#define AQ_IRQ_MAP_REG1_RX0    0x00000100U /*IRQ RX0*/

#define AQ_IRQ_MAP_REG1_TX0_EN 0x80000000U /*IRQ TX0 enable*/
#define AQ_IRQ_MAP_REG1_TX0    0x00000000U /*IRQ TX0*/

/*TX interrupt ctrl reg*/
#define AQ_TX_IRQ_CTRL       0x00007B40U
#define AQ_TX_IRQ_CTRL_WB_EN 0x00000002U
//#define AQ_TX_IRQ_CTRL_PCKT_TRANSM_EN 0x00000008U

/*RX interrupt ctrl reg*/
#define AQ_RX_IRQ_CTRL       0x00005A30U
#define AQ_RX_IRQ_CTRL_WB_EN 0x00000002U
//#define AQ_RX_IRQ_CTRL_PCKT_TRANSM_EN 0x00000008U

#define AQ_GLB_CTRL  0x00000000U

#define AQ_PCI_CTRL         0x00001000U
#define AQ_PCI_CTRL_RST_DIS 0x20000000U

#define AQ_RX_CTRL         0x00005000U
#define AQ_RX_CTRL_RST_DIS 0x20000000U /*RPB reset disable */
#define AQ_TX_CTRL         0x00007000U
#define AQ_TX_CTRL_RST_DIS 0x20000000U /*TPB reset disable */

/*RX data path control registers*/
#define AQ_RPF2_CTRL    0x00005040U
#define AQ_RPF2_CTRL_EN 0x000F0000U /* RPF2 enable*/

#define AQ_RPF_CTRL1            0x00005100U
#define AQ_RPF_CTRL1_BRC_EN     0x00000001U /*Allow broadcast receive*/
#define AQ_RPF_CTRL1_L2_PROMISC 0x00000008U /*L2 promiscious*/
#define AQ_RPF_CTRL1_ACTION     0x00001000U /*Action to host*/
#define AQ_RPF_CTRL1_BRC_TSH    0x00010000U /*Broadcast threshold in 256 units per sec*/

#define AQ_RPF_CTRL2              0x00005280U
#define AQ_RPF_CTRL2_VLAN_PROMISC 0x00000002U /*VLAN promisc*/

#define AQ_RPB_CTRL         0x00005700U
#define AQ_RPB_CTRL_EN      0x00000001U /*RPB Enable*/
#define AQ_RPB_CTRL_FC      0x00000010U /*RPB Enable*/
#define AQ_RPB_CTRL_TC_MODE 0x00000100U /*RPB Traffic Class Mode*/

#define AQ_RPB0_CTRL1      0x00005710U
#define AQ_RPB0_CTRL1_SIZE 0x00000140U /*RPB size (in unit 1KB) \*/

#define AQ_RPB0_CTRL2          0x00005714U
#define AQ_RPB0_CTRL2_LOW_TSH  0x00000C00U /*Buffer Low Threshold (70% of RPB size in unit 32B)*/
#define AQ_RPB0_CTRL2_HIGH_TSH 0x1C000000U /*Buffer High Threshold(30% of RPB size in unit 32B)*/
#define AQ_RPB0_CTRL2_FC_EN    0x80000000U /*Flow control Enable*/

#define AQ_RPB_CTRL_SIZE 0x00005b18U
#define AQ_RPB_CTRL_ADDR 0x00005b00U

/*TX data path  control registers*/
#define AQ_TPO2_CTRL 0x00007040U
#define AQ_TPO2_EN   0x00010000U /*TPO2 Enable*/

#define AQ_TPB_CTRL         0x00007900U
#define AQ_TPB_CTRL_EN      0x00000001U /*TPB enable*/
#define AQ_TPB_CTRL_PAD_EN  0x00000004U /*Tx pad insert enable*/
#define AQ_TPB_CTRL_TC_MODE 0x00000100U /*Tx traffic Class Mode*/

#define AQ_TPB0_CTRL1      0x00007910U
#define AQ_TPB0_CTRL1_SIZE 0x000000A0U /*TPB Size (in unit 1KB)*/

#define AQ_TPB0_CTRL2          0x00007914U
#define AQ_TPB0_CTRL2_LOW_TSH  0x00000600U /*Buffer Low Threshold (70% of RPB size in unit 32B)*/
#define AQ_TPB0_CTRL2_HIGH_TSH 0x0E000000U /*Buffer High Threshold(30% of RPB size in unit 32B)*/

#define AQ_TPB_CTRL_ADDR 0x00007c00U

/*Rings control registers*/
#define AQ_RING_TX_CTRL    0x00007c08U
#define AQ_RING_TX_CTRL_EN 0x80000000U /*Tx descriptor Enable*/

#define AQ_RING_RX_CTRL    0x00005b08U
#define AQ_RING_RX_CTRL_EN 0x80000000U /*Rx descriptor Enable*/

#define AQ_RING_TAIL     0x00007c10U
#define AQ_RING_TAIL_PTR 0x00005b10U

/*IRQ control registers*/
#define AQ_ITR_MSKS     0x00002060U
#define AQ_ITR_MSKS_LSW 0x0000000CU
#define AQ_ITR_MSKC     0x00002070U
#define AQ_ITR_MSKC_LSW 0x0000000CU

/*Link advertising*/
#define AQ_LINK_ADV           0x00000368U
#define AQ_LINK_ADV_AUTONEG   0x003B0000U
#define AQ_LINK_ADV_DOWNSHIFT 0xC0000000U
#define AQ_LINK_ADV_CMD       0x00000002U

#define AQ_LINK_ADV_EN 0xFFFF0002U /*??????????????*/
#define AQ_LINK_ST     0x0000036CU

/*Semaphores*/
#define AQ_SEM_RAM 0x000003a8U

/*Mailbox*/
#define AQ_MBOX_ADDR  0x00000360U
#define AQ_MBOX_CTRL1 0x00000200U
#define AQ_MBOX_CTRL3 0x00000208U
#define AQ_MBOX_CTRL5 0x0000020cU

struct aq_desc_tx {
    uint64_t address;
    
    union {
        struct {
            uint32_t dx_type : 3;
            uint32_t rsvd1 : 1;
            uint32_t buf_len : 16;
            uint32_t dd : 1;
            uint32_t eop : 1;
            uint32_t cmd : 8;
            uint32_t rsvd2 : 2;
            uint32_t rsvd3 : 14;
            uint32_t pay_len : 18;
        };
        uint64_t flags;
    };
} __attribute__((packed));

struct aq_desc_tx_wb {
    uint64_t rsvd1;
    uint32_t rsvd2 : 20;
    uint32_t dd : 1;
    uint32_t rsvd3 : 11;
    uint32_t rsvd4;
} __attribute__((packed));

struct aq_desc_rx {
    uint64_t data_addr;
    uint64_t hdr_addr;

} __attribute__((packed));

struct aq_desc_rx_wb {
    uint64_t rsvd2;
    uint16_t dd : 1;
    uint16_t eop : 1;
    uint16_t rsvd3 : 14;
    uint16_t pkt_len;
    uint32_t rsvd4;
} __attribute__((packed));

struct aq_ring {
    unsigned int sw_tail;
    unsigned int sw_head;
    void * ring;
    unsigned int length;
};

/** An aQuanita network card */
struct aq_nic {
    /** Registers */
    void *regs;
    /** Port number (for multi-port devices) */
    unsigned int port;
    /** Flags */
    unsigned int flags;
    struct aq_ring tx_ring;
    struct aq_ring rx_ring;
    struct io_buffer *iobufs[AQ_RING_SIZE];
    unsigned int mbox_addr;
};

struct aq_hw_stats
{
    uint32_t version;
    uint32_t tid;
};



#endif /* _AQUANTIA_H */
