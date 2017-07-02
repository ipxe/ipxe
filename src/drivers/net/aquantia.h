#ifndef _aq_H
#define _aq_H

/** @file
 *
 * aQuantia AQC network card driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/if_ether.h>
#include <ipxe/nvs.h>

#define AQUANITA_BAR_SIZE 0xA000
#define AQ_RING_SIZE    16
#define AQ_RING_ALIGN   128
#define AQ_RX_MAX_LEN   2044

#define AQ_IRQ_TX   0x1
#define AQ_IRQ_RX   0x2
#define AQ_IRQ_LINK   0x4
struct aq_desc_tx {
    uint64_t address;
    
    uint32_t dx_type : 3;
    uint32_t rsvd2 : 1;
    uint32_t buf_len : 16;
    uint32_t dd : 1;
    uint32_t eop : 1;
    uint32_t cmd: 8;
    uint32_t rsvd3 : 2;
    uint32_t rsvd1 : 12;
    uint32_t pay_len : 18;
} __attribute__((packed));

struct aq_desc_tx_wb {
    uint64_t rsvd4;
    uint32_t rsvd5 : 20;
    uint32_t dd : 1;
    uint32_t rsvd6 : 11;
    uint32_t rsvd7;
} __attribute__((packed));

struct aq_desc_rx {
    uint64_t address;
    uint64_t rsvd1;

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



#endif /* _aq_H */
