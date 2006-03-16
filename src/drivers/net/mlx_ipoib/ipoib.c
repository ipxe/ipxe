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

#include "ipoib.h"
#include "ib_driver.h"
#include "ib_mad.h"

static const __u8 arp_packet_template[] = {
	0x00, 0x20,		/* hardware type */
	0x08, 0x00,		/* protocol type */
	20,			/* hw size */
	4,			/* protocol size */
	0x00, 0x00,		/* opcode */

	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,		/* sender's mac */
	0, 0, 0, 0,		/* sender's IP address */

	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,		/* Target's mac */
	0, 0, 0, 0		/* targets's IP address */
};

struct ipoib_data_st {
	__u32 ipoib_qpn;
	udqp_t ipoib_qph;
	ud_av_t bcast_av;
	cq_t snd_cqh;
	cq_t rcv_cqh;
	__u8 *port_gid_raw;
} ipoib_data;

#define NUM_MAC_ENTRIES (NUM_AVS+2)

static struct mac_xlation_st mac_tbl[NUM_MAC_ENTRIES];
static __u32 mac_counter = 1;
static __u32 youth_counter = 0;

#define EQUAL_GUIDS(g1, g2) (       \
			((g1)[0]==(g2)[0]) &&   \
			((g1)[1]==(g2)[1]) &&   \
			((g1)[2]==(g2)[2]) &&   \
			((g1)[3]==(g2)[3]) &&   \
			((g1)[4]==(g2)[4]) &&   \
			((g1)[5]==(g2)[5]) &&   \
			((g1)[6]==(g2)[6]) &&   \
			((g1)[7]==(g2)[7]) )

#define MAC_IDX(i) (((mac_tbl[i].eth_mac_lsb[0])<<16) | \
					((mac_tbl[i].eth_mac_lsb[0])<<8) |  \
					(mac_tbl[i].eth_mac_lsb[0]))

static inline const void *qpn2buf(__u32 qpn, const void *buf)
{
	((__u8 *) buf)[0] = qpn >> 16;
	((__u8 *) buf)[1] = (qpn >> 8) & 0xff;
	((__u8 *) buf)[2] = qpn & 0xff;
	return buf;
}

static inline __u32 buf2qpn(const void *buf)
{
	__u32 qpn;

	qpn = ((((__u8 *) buf)[0]) << 16) +
	    ((((__u8 *) buf)[1]) << 8) + (((__u8 *) buf)[2]);

	return qpn;
}

static int is_bcast_mac(const char *dest)
{
	int i;
	__u8 mac = 0xff;

	for (i = 0; i < 6; ++i)
		mac &= dest[i];

	return mac == 0xff;
}

/* find a free entry. if not found kick
 * another entry.
 */
static int find_free_entry(void)
{
	__u32 youth = 0xffffffff;
	__u8 i, remove_idx = NUM_MAC_ENTRIES;

	/* find a free entry */
	for (i = 0; i < NUM_MAC_ENTRIES; ++i) {
		if (!mac_tbl[i].valid) {
			mac_tbl[i].valid = 1;
			mac_tbl[i].youth = youth_counter;
			youth_counter++;
			return i;
		}
	}

	for (i = 0; i < NUM_MAC_ENTRIES; ++i) {
		if ((mac_tbl[i].av == NULL) && (mac_tbl[i].youth < youth)) {
			youth = mac_tbl[i].youth;
			remove_idx = i;
		}
	}

	if (remove_idx < NUM_MAC_ENTRIES) {
		/* update the new youth value */
		mac_tbl[remove_idx].youth = youth_counter;
		youth_counter++;
		return remove_idx;
	} else {
		tprintf("did not find an entry to kick");
		return -1;
	}
}

static int find_qpn_gid(__u32 qpn, const __u8 * gid)
{
	__u16 i;

	for (i = 0; i < NUM_MAC_ENTRIES; ++i) {
		if (mac_tbl[i].valid &&
		    (mac_tbl[i].qpn == qpn) &&
		    !memcmp(mac_tbl[i].gid.raw, gid, 16)) {
			return i;
		}
	}
	return -1;
}

static void allocate_new_mac6(__u8 * mac_lsb)
{
	__u32 eth_counter;

	eth_counter = mac_counter;
	mac_counter = (mac_counter + 1) & 0xffffff;

	mac_lsb[0] = eth_counter >> 16;
	mac_lsb[1] = eth_counter >> 8;
	mac_lsb[2] = eth_counter & 0xff;
	tprintf("add mac: %x:%x:%x", mac_lsb[0], mac_lsb[1], mac_lsb[2]);
}

static void modify_arp_reply(__u8 * eth_mac_lsb, void *data)
{
	__u8 *packet;

	/* skip 4 bytes */
	packet = ((__u8 *) data) + 4;

	/* modify hw type */
	packet[0] = 0;
	packet[1] = 1;

	/* modify hw size */
	packet[4] = 6;

	/* modify sender's mac */
	packet[8] = MLX_ETH_BYTE0;
	packet[9] = MLX_ETH_BYTE1;
	packet[10] = MLX_ETH_BYTE2;
	packet[11] = eth_mac_lsb[0];
	packet[12] = eth_mac_lsb[1];
	packet[13] = eth_mac_lsb[2];

	/* move sender's IP address */
	memcpy(packet + 14, packet + 28, 4);

	/* set target MAC - that's us */
	packet[18] = MLX_ETH_BYTE0;
	packet[19] = MLX_ETH_BYTE1;
	packet[20] = MLX_ETH_BYTE2;
	packet[21] = 0;
	packet[22] = 0;
	packet[23] = 0;

	/* move target's IP address */
	memcpy(packet + 24, packet + 52, 4);
}

static void modify_arp_request(__u8 * eth_mac_lsb, void *data)
{
	__u8 *packet;

	/* skip 4 bytes */
	packet = ((__u8 *) data) + 4;

	/* modify hw type */
	packet[0] = 0;
	packet[1] = 1;

	/* modify hw size */
	packet[4] = 6;

	/* modify sender's mac */
	packet[8] = MLX_ETH_BYTE0;
	packet[9] = MLX_ETH_BYTE1;
	packet[10] = MLX_ETH_BYTE2;
	packet[11] = eth_mac_lsb[0];
	packet[12] = eth_mac_lsb[1];
	packet[13] = eth_mac_lsb[2];

	/* move sender's IP address */
	memcpy(packet + 14, packet + 28, 4);

	/* set target MAC - that's us */
	packet[18] = 0;
	packet[19] = 0;
	packet[20] = 0;
	packet[21] = 0;
	packet[22] = 0;
	packet[23] = 0;

	/* move target's IP address */
	memcpy(packet + 24, packet + 52, 4);
}

static int handle_arp_packet(void *buf, void **out_buf_p,
			     unsigned int *new_size_p)
{
	__u16 opcode;
	const void *p;
	const __u8 *gid;
	__u32 qpn;
	int idx;

	opcode = get_opcode(buf);
	switch (opcode) {
	case ARP_OP_REQUESET:
	case ARP_OP_REPLY:
		break;

	default:
		return -1;
	}

	p = arp_mac20_get_sender_qpn(buf);
	qpn = buf2qpn(p);
	gid = arp_mac20_get_sender_gid(buf);

	if (!memcmp(gid, get_port_gid(), 16)) {
		/* my own gid */
		*out_buf_p = NULL;
		return 0;
	}

	idx = find_qpn_gid(qpn, gid);
	if (idx == -1) {
		/* entry not in the table */
		idx = find_free_entry();
		if (idx == -1) {
			eprintf("we're in broch\n");
			return -1;
		}
		allocate_new_mac6(mac_tbl[idx].eth_mac_lsb);
		mac_tbl[idx].av = NULL;	// free the av id it exists ?? !!
		mac_tbl[idx].qpn = qpn;
		memcpy(mac_tbl[idx].gid.raw, gid, 16);
	}

	if (opcode == ARP_OP_REQUESET) {
		modify_arp_request(mac_tbl[idx].eth_mac_lsb, buf);
	} else {
		/* we want to filter possible broadcast arp
		   replies not directed to us */
		p = arp_mac20_get_target_qpn(buf);
		qpn = buf2qpn(p);
		gid = arp_mac20_get_target_gid(buf);

		if ((qpn != ipoib_data.ipoib_qpn) ||
		    (memcmp(gid, get_port_gid(), 16))) {
			*out_buf_p = NULL;
			return 0;
		}

		modify_arp_reply(mac_tbl[idx].eth_mac_lsb, buf);
		{
			__u8 i;
			tprintf("arp reply dump:\n");
			for (i = 4; i < 32; ++i) {
				tprintf("%x: ", ((__u8 *) buf)[i]);
			}
			tprintf("\n");
		}
	}
	*out_buf_p = ((__u8 *) buf) + 4;
	*new_size_p = 28;	/* size of eth arp packet */

	tprintf("");

	return 0;
}

static void modify_udp_csum(void *buf, __u16 size)
{
	__u8 *ptr = (__u8 *) buf;
	__u32 csum = 0;
	__u16 chksum;
	__u16 buf_size;
	__u16 *tmp;
	int i;

	buf_size = (size & 1) ? size + 1 : size;
	tmp = (__u16 *) (ptr + 12);	/* src and dst ip addresses */
	for (i = 0; i < 4; ++i) {
		csum += tmp[i];
	}

	csum += 0x1100;		// udp protocol

	tmp = (__u16 *) (ptr + 26);
	tmp[0] = 0;		/* zero the checksum */

	tmp = (__u16 *) (ptr + 24);
	csum += tmp[0];

	tmp = (__u16 *) (ptr + 20);

	for (i = 0; i < ((buf_size - 20) >> 1); ++i) {
		csum += tmp[i];
	}

	chksum = ~((__u16) ((csum & 0xffff) + (csum >> 16)));

	tmp = (__u16 *) (ptr + 26);
	tmp[0] = chksum;	/* set the checksum */
}

static void modify_dhcp_resp(void *buf, __u16 size)
{
	set_eth_hwtype(buf);
	set_eth_hwlen(buf);
	set_own_mac(buf);
	modify_udp_csum(buf, size);
}

static void get_my_client_id(__u8 * my_client_id)
{

	my_client_id[0] = 0;
	qpn2buf(ipoib_data.ipoib_qpn, my_client_id + 1);
	memcpy(my_client_id + 4, ipoib_data.port_gid_raw, 16);
}

static const __u8 *get_client_id(const void *buf, int len)
{
	const __u8 *ptr;
	int delta;

	if (len < 268)
		return NULL;

	/* pointer to just after magic cookie */
	ptr = (const __u8 *)buf + 268;

	/* find last client identifier option */
	do {
		if (ptr[0] == 255) {
			/* found end of options list */
			return NULL;
		}

		if (ptr[0] == 0x3d) {
			/* client identifer option */
			return ptr + 3;
		}

		delta = ptr[1] + 2;
		ptr += delta;
		len -= delta;
	} while (len > 0);

	return NULL;
}

static int handle_ipv4_packet(void *buf, void **out_buf_p,
			      unsigned int *new_size_p, int *is_bcast_p)
{
	void *new_buf;
	__u16 new_size;
	__u8 msg_type;
	__u8 my_client_id[20];

	new_buf = (void *)(((__u8 *) buf) + 4);
	new_size = (*new_size_p) - 4;
	*out_buf_p = new_buf;
	*new_size_p = new_size;

	if (get_ip_protocl(new_buf) == IP_PROT_UDP) {
		__u16 udp_dst_port;
		const __u8 *client_id;

		udp_dst_port = get_udp_dst_port(new_buf);

		if (udp_dst_port == 67) {
			/* filter dhcp requests */
			*out_buf_p = 0;
			return 0;
		}

		if (udp_dst_port == 68) {
			get_my_client_id(my_client_id);

			/* packet client id */
			client_id = get_client_id(new_buf, new_size);
			if (!client_id) {
				*out_buf_p = 0;
				return 0;
			}

			if (memcmp(client_id, my_client_id, 20)) {
				*out_buf_p = 0;
				return 0;
			}
		}
	}

	msg_type = get_dhcp_msg_type(new_buf);
	if ((get_ip_protocl(new_buf) == IP_PROT_UDP) &&
	    (get_udp_dst_port(new_buf) == 68) &&
	    ((msg_type == DHCP_TYPE_RESPONSE) || (msg_type == DHCP_TYPE_ACK))
	    ) {
		*is_bcast_p = 1;
		modify_dhcp_resp(new_buf, new_size);
	}

	return 0;
}

static int is_valid_arp(void *buf, unsigned int size)
{
	__u8 *ptr = buf;
	__u16 tmp;

	if (size != 60) {
		eprintf("");
		return 0;
	}
	if (be16_to_cpu(*((__u16 *) ptr)) != ARP_PROT_TYPE)
		return 0;

	if (be16_to_cpu(*((__u16 *) (ptr + 4))) != IPOIB_HW_TYPE)
		return 0;

	if (be16_to_cpu(*((__u16 *) (ptr + 6))) != IPV4_PROT_TYPE)
		return 0;

	if (ptr[8] != 20)	/* hw addr len */
		return 0;

	if (ptr[9] != 4)	/* protocol len  = 4 for IP */
		return 0;

	tmp = be16_to_cpu(*((__u16 *) (ptr + 10)));
	if ((tmp != ARP_OP_REQUESET) && (tmp != ARP_OP_REPLY))
		return 0;

	return 1;
}

static int ipoib_handle_rcv(void *buf, void **out_buf_p,
			    unsigned int *new_size_p, int *is_bcast_p)
{
	__u16 prot_type;
	int rc;

	prot_type = get_prot_type(buf);
	switch (prot_type) {
	case ARP_PROT_TYPE:
		tprintf("");
		if (is_valid_arp(buf, *new_size_p)) {
			tprintf("got valid arp");
			rc = handle_arp_packet(buf, out_buf_p, new_size_p);
			if (rc) {
				eprintf("");
				return rc;
			}
			if (!out_buf_p) {
				tprintf("");
			}
			tprintf("arp for me");
			*is_bcast_p = 1;
			return rc;
		} else {
			tprintf("got invalid arp");
			*out_buf_p = NULL;
			return 0;
		}

	case IPV4_PROT_TYPE:
		tprintf("");
		rc = handle_ipv4_packet(buf, out_buf_p, new_size_p, is_bcast_p);
		return rc;
	}
	eprintf("prot=0x%x", prot_type);
	return -1;
}

static int is_null_mac(const __u8 * mac)
{
	__u8 i, tmp = 0;
	__u8 lmac[6];

	memcpy(lmac, mac, 6);

	for (i = 0; i < 6; ++i) {
		tmp |= lmac[i];
	}

	if (tmp == 0)
		return 1;
	else
		return 0;
}

static int find_mac(const __u8 * mac)
{
	int i;
	const __u8 *tmp = mac + 3;

	for (i = 0; i < NUM_MAC_ENTRIES; ++i) {
		tprintf("checking 0x%02x:0x%02x:0x%02x valid=%d",
			mac_tbl[i].eth_mac_lsb[0], mac_tbl[i].eth_mac_lsb[1],
			mac_tbl[i].eth_mac_lsb[2], mac_tbl[i].valid);
		if (mac_tbl[i].valid && !memcmp(mac_tbl[i].eth_mac_lsb, tmp, 3))
			return i;
	}
	tprintf("mac: %x:%x:%x - dumping", tmp[0], tmp[1], tmp[2]);
	for (i = 0; i < NUM_MAC_ENTRIES; ++i) {
		//__u8 *gid= mac_tbl[i].gid.raw;
		//__u8 *m= mac_tbl[i].eth_mac_lsb;
		/*if (mac_tbl[i].valid) {
		   tprintf("%d: qpn=0x%lx, "
		   "gid=%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x, "
		   "av=0x%lx, "
		   "youth= %ld, "
		   "mac=%x:%x:%x\n",
		   i, mac_tbl[i].qpn,
		   gid[0], gid[1], gid[2], gid[3], gid[4], gid[5], gid[6], gid[7], 
		   gid[8], gid[9], gid[10], gid[11], gid[12], gid[13], gid[14], gid[15], 
		   mac_tbl[i].av, mac_tbl[i].youth,
		   m[0], m[1], m[2]); 
		   } */
	}
	return -1;
}

static int send_bcast_packet(__u16 protocol, const void *data, __u16 size)
{
	ud_send_wqe_t snd_wqe, tmp_wqe;
	int rc;
	int is_good;
	void *send_buffer;

	snd_wqe = alloc_send_wqe(ipoib_data.ipoib_qph);
	if (!snd_wqe) {
		eprintf("");
		return -1;
	}

	send_buffer = get_send_wqe_buf(snd_wqe, 0);
	*((__u32 *) send_buffer) = cpu_to_be32(protocol << 16);
	prep_send_wqe_buf(ipoib_data.ipoib_qph, ipoib_data.bcast_av,
			  snd_wqe, data, 4, size, 0);

	rc = post_send_req(ipoib_data.ipoib_qph, snd_wqe, 1);
	if (rc) {
		eprintf("");
		goto ex;
	}

	rc = poll_cqe_tout(ipoib_data.snd_cqh, SEND_CQE_POLL_TOUT, &tmp_wqe,
			   &is_good);
	if (rc) {
		eprintf("");
		goto ex;
	}
	if (!is_good) {
		eprintf("");
		rc = -1;
		goto ex;
	}
	if (tmp_wqe != snd_wqe) {
		eprintf("");
		rc = -1;
		goto ex;
	}

      ex:free_wqe(snd_wqe);
	return rc;
}

static int send_ucast_packet(const __u8 * mac, __u16 protocol, const void *data,
			     __u16 size)
{
	ud_send_wqe_t snd_wqe, tmp_wqe;
	ud_av_t av;
	udqp_t qph;
	__u16 dlid;
	__u8 sl, rate;
	int rc;
	int i;
	int is_good;

	i = find_mac(mac);
	if (i < 0) {
		tprintf("");
		return -1;
	}

	if (!mac_tbl[i].av) {
		rc = get_path_record(&mac_tbl[i].gid, &dlid, &sl, &rate);
		if (rc) {
			eprintf("");
			return -1;
		} else {
			tprintf("get_path_record() success dlid=0x%x", dlid);
		}

		/* no av - allocate one */
		av = alloc_ud_av();
		if (!av) {
			eprintf("");
			return -1;
		}
		modify_av_params(av, dlid, 1, sl, rate, &mac_tbl[i].gid,
				 mac_tbl[i].qpn);
		mac_tbl[i].av = av;
	} else {
		av = mac_tbl[i].av;
	}
	qph = ipoib_data.ipoib_qph;
	snd_wqe = alloc_send_wqe(qph);
	if (!snd_wqe) {
		eprintf("");
		return -1;
	}

	*((__u32 *) get_send_wqe_buf(snd_wqe, 0)) = cpu_to_be32(protocol << 16);
	prep_send_wqe_buf(qph, av, snd_wqe, data, 4, size, 0);

	rc = post_send_req(qph, snd_wqe, 1);
	if (rc) {
		eprintf("");
		return -1;
	}

	rc = poll_cqe_tout(ipoib_data.snd_cqh, SEND_CQE_POLL_TOUT, &tmp_wqe,
			   &is_good);
	if (rc) {
		eprintf("");
		goto ex;
	}
	if (!is_good) {
		eprintf("");
		rc = -1;
		goto ex;
	}
	if (tmp_wqe != snd_wqe) {
		eprintf("");
		rc = -1;
		goto ex;
	}

      ex:free_wqe(snd_wqe);
	return rc;
}

static void *alloc_convert_arp6_msg(const void *data,
				    struct arp_packet_st *ipoib_arp)
{
	void *buf;
	const void *p1;
	int idx;
	__u8 qpn[3];

	memcpy(ipoib_arp, arp_packet_template, sizeof arp_packet_template);
	buf = ipoib_arp;

	/* update opcode */
	p1 = arp_mac6_get_opcode(data);
	arp_mac20_set_opcode(p1, buf);

	/* update sender ip */
	p1 = arp_mac6_get_sender_ip(data);
	arp_mac20_set_sender_ip(p1, buf);

	/* update target ip */
	p1 = arp_mac6_get_target_ip(data);
	arp_mac20_set_target_ip(p1, buf);

	/* update sender mac */
	qpn2buf(ipoib_data.ipoib_qpn, qpn);
	arp_mac20_set_sender_mac(qpn, ipoib_data.port_gid_raw, buf);

	/* update target mac */
	p1 = arp_mac6_get_target_mac(data);
	if (!is_null_mac(p1)) {
		idx = find_mac(p1);
		if (idx == -1) {
			__u8 *_ptr = (__u8 *) p1;
			eprintf("could not find mac %x:%x:%x",
				_ptr[3], _ptr[4], _ptr[5]);
			return NULL;
		}
		qpn2buf(mac_tbl[idx].qpn, qpn);
		arp_mac20_set_target_mac(qpn, mac_tbl[idx].gid.raw, buf);
	}

	return buf;
}

static __u16 set_client_id(__u8 * packet)
{
	__u8 *ptr;
	__u8 y[3];
	__u16 new_size;

	/* pointer to just after magic cookie */
	ptr = packet + 268;

	/* find last option */
	do {
		if (ptr[0] == 255) {
			/* found end of options list */
			break;
		}
		ptr = ptr + ptr[1] + 2;
	} while (1);

	ptr[0] = 61;		/* client id option identifier */
	ptr[1] = 21;		/* length of the option */
	ptr[2] = IPOIB_HW_TYPE;
	ptr[3] = 0;
	qpn2buf(ipoib_data.ipoib_qpn, y);
	memcpy(ptr + 4, y, 3);
	memcpy(ptr + 7, ipoib_data.port_gid_raw, 16);
	ptr[23] = 255;
	new_size = (__u16) (ptr + 24 - packet);
	if (new_size & 3) {
		new_size += (4 - (new_size & 3));
	}
	return new_size;
}

static __u16 calc_udp_csum(__u8 * packet)
{
	__u16 *ptr;
	int i;
	__u32 sum = 0;
	__u16 udp_length, udp_csum;

	/* src ip, dst ip */
	ptr = (__u16 *) (packet + 12);
	for (i = 0; i < 4; ++i) {
		sum += be16_to_cpu(ptr[i]);
	}

	/* udp protocol */
	sum += IP_PROT_UDP;

	/* udp length */
	ptr = (__u16 *) (packet + 24);
	udp_length = be16_to_cpu(*ptr);
	sum += udp_length;

	/* udp part */
	ptr = (__u16 *) (packet + 20);
	do {
		sum += be16_to_cpu(*ptr);
		ptr++;
		udp_length -= 2;
	} while (udp_length);

	udp_csum = ~((__u16) ((sum & 0xffff) + (sum >> 16)));
	return udp_csum;
}

static __u16 modify_dhcp_request(__u8 * packet, __u16 size)
{
	__u16 csum, new_size;

	set_hw_type(packet);
	zero_hw_len(packet);
	zero_chaddr(packet);
	set_bcast_flag(packet);
	new_size = set_client_id(packet);
	if (new_size > size) {
		add_udp_len(packet, new_size - size);
	} else
		new_size = size;
	set_udp_csum(packet, 0);
	csum = calc_udp_csum(packet);
	set_udp_csum(packet, csum);
	return new_size;
}

static __u16 copy_dhcp_message(__u8 * buf, const void *packet, __u16 size)
{
	memcpy(buf, packet, size);
	return size;
}

static void modify_ip_hdr(__u8 * buf, __u16 add_size)
{
	__u16 *ptr, ip_csum;
	__u16 tmp;
	__u32 sum = 0;
	__u8 i;

	/* update ip length */
	ptr = (__u16 *) buf;
	tmp = be16_to_cpu(ptr[1]);
	ptr[1] = cpu_to_be16(tmp + add_size);

	ptr[5] = 0;		/* zero csum */
	for (i = 0; i < 10; ++i) {
		sum += be16_to_cpu(ptr[i]);
	}

	ip_csum = ~((__u16) ((sum & 0xffff) + (sum >> 16)));
	ptr[5] = cpu_to_be16(ip_csum);

}

static void *update_dhcp_request(const void *packet, unsigned int size,
				 __u16 * new_size_p)
{
	__u8 ip_proto, dhcp_message_type;
	__u16 dest_port, new_size, orig_size;
	static __u8 dhcp_send_buffer[576];

	ip_proto = get_ip_protocl_type(packet);
	if (ip_proto != IP_PROT_UDP) {
		return NULL;
	}

	dest_port = get_udp_dest_port(packet);
	if (dest_port != 0x4300 /*67 */ )
		return NULL;

	dhcp_message_type = get_dhcp_message_type(packet);
	if (dhcp_message_type != DHCP_TYPE_REQUEST)
		return NULL;

	memset(dhcp_send_buffer, 0, sizeof dhcp_send_buffer);
	orig_size = copy_dhcp_message(dhcp_send_buffer, packet, size);

	new_size = modify_dhcp_request(dhcp_send_buffer, orig_size);
	if (new_size != orig_size) {
		modify_ip_hdr(dhcp_send_buffer, new_size - orig_size);
	}
	*new_size_p = new_size;
	return dhcp_send_buffer;
}

static int ipoib_send_packet(const __u8 * mac, __u16 protocol, const void *data,
			     unsigned int size)
{
	const void *packet;
	__u16 new_size, dhcp_req_sz;
	void *tmp;
	int rc;
	struct arp_packet_st ipoib_arp;

	tprintf("");

	if (protocol == ARP_PROT_TYPE) {
		/* special treatment for ARP */
		tmp = alloc_convert_arp6_msg(data, &ipoib_arp);
		if (!tmp) {
			eprintf("");
			return -1;
		}
		packet = tmp;
		new_size = sizeof(struct arp_packet_st);
		tprintf("sending arp");
	} else {
		tmp = update_dhcp_request(data, size, &dhcp_req_sz);
		if (tmp) {
			/* it was a dhcp request so we use a special
			   buffer because we may have to enlarge the size of the packet */
			tprintf("sending dhcp");
			packet = tmp;
			new_size = dhcp_req_sz;
		} else {
			packet = data;
			new_size = size;
			tprintf("sending packet");
		}
	}

	//eprintf("press key ..."); getchar();
	if (is_bcast_mac(mac)) {
		tprintf("");
		rc = send_bcast_packet(protocol, packet, new_size);
	} else {
		tprintf("");
		rc = send_ucast_packet(mac, protocol, packet, new_size);
	}

	return rc;
}

static int ipoib_read_packet(__u16 * prot_p, void *data, unsigned int *size_p,
			     int *is_bcast_p)
{
	int rc;
	struct ib_cqe_st ib_cqe;
	__u8 num_cqes;
	unsigned int new_size;
	void *buf, *out_buf;
	__u16 prot_type;

	rc = ib_poll_cq(ipoib_data.rcv_cqh, &ib_cqe, &num_cqes);
	if (rc) {
		return rc;
	}

	if (num_cqes == 0) {
		*size_p = 0;
		return 0;
	}

	if (ib_cqe.is_error) {
		eprintf("");
		rc = -1;
		goto ex_func;
	}

	new_size = ib_cqe.count - GRH_SIZE;
	buf = get_rcv_wqe_buf(ib_cqe.wqe, 1);
	tprintf("buf=%lx", buf);
	rc = ipoib_handle_rcv(buf, &out_buf, &new_size, is_bcast_p);
	if (rc) {
		eprintf("");
		rc = -1;
		goto ex_func;
	}
	if (out_buf) {
		tprintf("");
		prot_type = get_prot_type(buf);
		*size_p = new_size;
		tprintf("new_size=%d", new_size);
		if (new_size > 1560) {
			eprintf("sizzzzzze = %d", new_size);
		} else {
			memcpy(data, out_buf, new_size);
		}
		*prot_p = prot_type;
	} else {
		tprintf("skip message");
		*size_p = 0;
	}

      ex_func:
	if (free_wqe(ib_cqe.wqe)) {
		eprintf("");
	}

	return rc;
}

static int ipoib_init(struct pci_device *pci)
{
	int rc;
	udqp_t qph;
	int i;

	tprintf("");
	rc = ib_driver_init(pci, &qph);
	if (rc)
		return rc;

	tprintf("");
	ipoib_data.ipoib_qph = qph;
	ipoib_data.ipoib_qpn = ib_get_qpn(qph);

	if(print_info)
		printf("local ipoib qpn=0x%x\n", ipoib_data.ipoib_qpn);

	ipoib_data.bcast_av = ib_data.bcast_av;
	ipoib_data.port_gid_raw = ib_data.port_gid.raw;
	ipoib_data.snd_cqh = ib_data.ipoib_snd_cq;
	ipoib_data.rcv_cqh = ib_data.ipoib_rcv_cq;

	mac_counter = 1;
	youth_counter = 0;
	for (i = 0; i < NUM_MAC_ENTRIES; ++i) {
		mac_tbl[i].valid = 0;
		mac_tbl[i].av = NULL;
	}

	return 0;
}

static int ipoib_close(int fw_fatal)
{
	int rc;

	rc = ib_driver_close(fw_fatal);

	return rc;
}
