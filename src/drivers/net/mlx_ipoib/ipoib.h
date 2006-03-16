/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
*/

#ifndef __ipoib_h__
#define __ipoib_h__

#define ARP_PROT_TYPE 0x806
#define IPV4_PROT_TYPE 0x800

#define IPOIB_HW_TYPE 0x20
#define ETH_HW_TYPE 1

#define ARP_OP_REQUESET 1
#define ARP_OP_REPLY    2

#define MLX_ETH_3BYTE_PREFIX 0x2c9	/* 00,02,c9 */
#define MLX_ETH_BYTE0 0
#define MLX_ETH_BYTE1 2
#define MLX_ETH_BYTE2 0xC9

#define IP_PROT_UDP 17
#define DHCP_TYPE_REQUEST 1
#define DHCP_TYPE_RESPONSE 2
#define DHCP_TYPE_ACK 5

struct ipoib_mac_st {
	__u32 qpn:24;
	__u32 r0:8;
	__u8 gid[16];
} __attribute__ ((packed));

struct arp_packet_st {
	__u16 arp_prot_type;
	__u16 hw_type;
	__u16 opcode;
	__u8 prot_size;
	__u8 hw_len;
	struct ipoib_mac_st sender_mac;
	__u32 sender_ip;
	struct ipoib_mac_st target_mac;
	__u32 target_ip;
} __attribute__ ((packed));

/* this struct is used to translate between ipoib and
   ethernet mac addresses */
struct mac_xlation_st {
	__u8 valid;		/* 1=entry valid 0=entry free */
	__u32 youth;		/* youth of this entry the lowest the
				   number the older in age */
	__u8 eth_mac_lsb[3];	/* three bytes Ethernet MAC
				   LS bytes are constants */
	union ib_gid_u gid;
	__u32 qpn;
	ud_av_t av;		/* address vector representing neighbour */
};

static inline __u16 get_prot_type(void *data)
{
	__u8 *ptr = data;

	return be16_to_cpu(*((__u16 *) ptr));
}

static inline __u8 get_hw_len(void *data)
{
	return ((__u8 *) data)[8];
}

static inline __u8 get_prot_size(void *data)
{
	return ((__u8 *) data)[9];
}

static inline __u16 get_opcode(const void *data)
{
	return be16_to_cpu(*((__u16 *) (&(((__u8 *) data)[10]))));
}

static inline __u32 get_sender_qpn(void *data)
{
	__u8 *ptr = data;

	return (ptr[13] << 16) | (ptr[14] << 8) | ptr[15];
}

static inline const __u8 *get_sender_gid(void *data)
{
	return &(((__u8 *) data)[16]);
}

static inline void *arp_mac6_get_sender_ip(const void *data)
{
	return (__u8 *) data + 14;
}

static inline const void *arp_mac6_get_target_ip(const void *data)
{
	return data + 24;
}

static inline void arp_mac20_set_sender_ip(const void *ip, void *data)
{
	memcpy(((__u8 *) data) + 28, ip, 4);
}

static inline void arp_mac20_set_target_ip(const void *ip, void *data)
{
	memcpy(((__u8 *) data) + 52, ip, 4);
}

static inline void arp_mac20_set_sender_mac(const void *qpn, const void *gid,
					    void *data)
{
	memcpy(((__u8 *) data) + 9, qpn, 3);
	memcpy(((__u8 *) data) + 12, gid, 16);
}

static inline void arp_mac20_set_target_mac(void *qpn, void *gid, void *data)
{
	memcpy(((__u8 *) data) + 33, qpn, 3);
	memcpy(((__u8 *) data) + 36, gid, 16);
}

static inline const void *arp_mac6_get_opcode(const void *data)
{
	return data + 6;
}

static inline void arp_mac20_set_opcode(const void *opcode, void *data)
{
	memcpy(data + 6, opcode, 2);
}

static inline const void *arp_mac6_get_target_mac(const void *data)
{
	return data + 18;
}

static inline const void *arp_mac20_get_sender_qpn(void *data)
{
	return ((__u8 *) data) + 13;
}

static inline const void *arp_mac20_get_sender_gid(void *data)
{
	return ((__u8 *) data) + 16;
}

static inline const void *arp_mac20_get_target_qpn(void *data)
{
	return ((__u8 *) data) + 37;
}

static inline const void *arp_mac20_get_target_gid(void *data)
{
	return ((__u8 *) data) + 40;
}

static inline const void *get_lptr_by_off(const void *packet, __u16 offset)
{
	return packet + offset;
}

static inline __u8 get_ip_protocl_type(const void *packet)
{
	const void *ptr;
	__u8 prot;

	ptr = get_lptr_by_off(packet, 9);

	memcpy(&prot, ptr, 1);
	return prot;
}

static inline __u16 get_udp_dest_port(const void *packet)
{
	const void *ptr;
	__u16 port;

	ptr = get_lptr_by_off(packet, 22);

	memcpy(&port, ptr, 2);
	return port;
}

static inline __u8 get_dhcp_message_type(const void *packet)
{
	const void *ptr;
	__u8 type;

	ptr = get_lptr_by_off(packet, 28);

	memcpy(&type, ptr, 1);
	return type;
}

static inline void set_hw_type(__u8 * packet)
{
	packet[29] = IPOIB_HW_TYPE;
}

static inline void zero_hw_len(__u8 * packet)
{
	packet[30] = 0;
}

static inline void set_udp_csum(__u8 * packet, __u16 val)
{
	__u16 *csum_ptr;

	csum_ptr = (__u16 *) (packet + 26);

	*csum_ptr = htons(val);
}

static inline void zero_chaddr(__u8 * packet)
{
	memset(packet + 56, 0, 16);
}

static inline void set_bcast_flag(__u8 * packet)
{
	packet[38] = 0x80;
}

static inline __u8 get_ip_protocl(void *buf)
{
	return ((__u8 *) buf)[9];
}

static inline __u16 get_udp_dst_port(void *buf)
{
	return be16_to_cpu(*((__u16 *) (((__u8 *) buf) + 0x16)));
}

static inline __u8 get_dhcp_msg_type(void *buf)
{
	return ((__u8 *) buf)[0x1c];
}

static inline void set_eth_hwtype(void *buf)
{
	((__u8 *) buf)[0x1d] = ETH_HW_TYPE;
}

static inline void set_eth_hwlen(void *buf)
{
	((__u8 *) buf)[0x1e] = 6;
}

static inline void add_udp_len(void *buf, __u16 size_add)
{
	__u16 old_len, *len_ptr;

	len_ptr = (__u16 *) (((__u8 *) buf) + 24);
	old_len = ntohs(*len_ptr);
	*len_ptr = htons(old_len + size_add);
}

static inline void set_own_mac(void *buf)
{
	((__u8 *) buf)[0x38] = 0xff;	//MLX_ETH_BYTE0;
	((__u8 *) buf)[0x39] = 0xff;	//MLX_ETH_BYTE1;
	((__u8 *) buf)[0x3a] = 0xff;	//MLX_ETH_BYTE2;
	((__u8 *) buf)[0x3b] = 0xff;	//0;
	((__u8 *) buf)[0x3c] = 0xff;	//0;
	((__u8 *) buf)[0x3d] = 0xff;	//0;
}

static int ipoib_handle_rcv(void *buf, void **out_buf_p,
			    unsigned int *new_size_p, int *bcast_p);
static int ipoib_send_packet(const __u8 * mac, __u16 protocol, const void *data,
			     unsigned int size);
static int ipoib_init(struct pci_device *pci);
static u8 *get_port_gid(void);
static int ipoib_read_packet(__u16 * prot_p, void *data, unsigned int *size_p,
			     int *is_bcast_p);

#endif				/* __ipoib_h__ */
