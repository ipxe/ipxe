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

#include "mt23108.h"
#include "ib_driver.h"
#include "pci.h"

struct device_buffers_st {
	union recv_wqe_u mads_qp_rcv_queue[NUM_MADS_RCV_WQES]
	    __attribute__ ((aligned(RECV_WQE_U_ALIGN)));
	union recv_wqe_u ipoib_qp_rcv_queue[NUM_IPOIB_RCV_WQES]
	    __attribute__ ((aligned(RECV_WQE_U_ALIGN)));
	union ud_send_wqe_u mads_qp_snd_queue[NUM_MADS_SND_WQES]
	    __attribute__ ((aligned(UD_SEND_WQE_U_ALIGN)));
	union ud_send_wqe_u ipoib_qp_snd_queue[NUM_IPOIB_SND_WQES]
	    __attribute__ ((aligned(UD_SEND_WQE_U_ALIGN)));
	u8 inprm_buf[INPRM_BUF_SZ] __attribute__ ((aligned(INPRM_BUF_ALIGN)));
	u8 outprm_buf[OUTPRM_BUF_SZ]
	    __attribute__ ((aligned(OUTPRM_BUF_ALIGN)));
	struct eqe_t eq_buf[1 << LOG2_EQ_SZ]
	    __attribute__ ((aligned(sizeof(struct eqe_t))));
	union cqe_st mads_snd_cq_buf[NUM_MADS_SND_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union cqe_st ipoib_snd_cq_buf[NUM_IPOIB_SND_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union cqe_st mads_rcv_cq_buf[NUM_MADS_RCV_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union cqe_st ipoib_rcv_cq_buf[NUM_IPOIB_RCV_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union ud_av_u av_array[NUM_AVS]
	    __attribute__ ((aligned(ADDRESS_VECTOR_ST_ALIGN)));
} __attribute__ ((packed));

#define STRUCT_ALIGN_SZ 4096
#define SRC_BUF_SZ (sizeof(struct device_buffers_st) + STRUCT_ALIGN_SZ - 1)

/* the following must be kept in this order
   for the memory region to cover the buffers */
static u8 src_buf[SRC_BUF_SZ];
static struct ib_buffers_st ib_buffers;
static __u32 memreg_size;
/* end of order constraint */

static struct dev_pci_struct tavor_pci_dev;
static struct device_buffers_st *dev_buffers_p;
static struct device_ib_data_st dev_ib_data;

static int gw_write_cr(__u32 addr, __u32 data)
{
	writel(htonl(data), tavor_pci_dev.cr_space + addr);
	return 0;
}

static int gw_read_cr(__u32 addr, __u32 * result)
{
	*result = ntohl(readl(tavor_pci_dev.cr_space + addr));
	return 0;
}

static int reset_hca(void)
{
	return gw_write_cr(TAVOR_RESET_OFFSET, 1);
}

static int find_mlx_bridge(__u8 hca_bus, __u8 * br_bus_p, __u8 * br_devfn_p)
{
	int bus;
	int dev;
	int devfn;
	int rc;
	__u16 vendor, dev_id;
	__u8 sec_bus;

	for (bus = 0; bus < 256; ++bus) {
		for (dev = 0; dev < 32; ++dev) {
			devfn = (dev << 3);
			rc = pcibios_read_config_word(bus, devfn, PCI_VENDOR_ID,
						      &vendor);
			if (rc)
				return rc;

			if (vendor != MELLANOX_VENDOR_ID)
				continue;

			rc = pcibios_read_config_word(bus, devfn, PCI_DEVICE_ID,
						      &dev_id);
			if (rc)
				return rc;

			if (dev_id != TAVOR_BRIDGE_DEVICE_ID)
				continue;

			rc = pcibios_read_config_byte(bus, devfn,
						      PCI_SECONDARY_BUS,
						      &sec_bus);
			if (rc)
				return rc;

			if (sec_bus == hca_bus) {
				*br_bus_p = bus;
				*br_devfn_p = devfn;
				return 0;
			}
		}
	}

	return -1;
}

static int ib_device_init(struct pci_device *dev)
{
	int i;
	int rc;
	__u8 br_bus, br_devfn;

	tprintf("");

	memset(&dev_ib_data, 0, sizeof dev_ib_data);

	/* save bars */
	tprintf("bus=%d devfn=0x%x", dev->bus, dev->devfn);
	for (i = 0; i < 6; ++i) {
		tavor_pci_dev.dev.bar[i] =
		    pci_bar_start(dev, PCI_BASE_ADDRESS_0 + (i << 2));
		tprintf("bar[%d]= 0x%08lx", i, tavor_pci_dev.dev.bar[i]);
	}

	tprintf("");
	/* save config space */
	for (i = 0; i < 64; ++i) {
		rc = pci_read_config_dword(dev, i << 2,
					   &tavor_pci_dev.dev.
					   dev_config_space[i]);
		if (rc) {
			eprintf("");
			return rc;
		}
		tprintf("config[%d]= 0x%08lx", i << 2,
			tavor_pci_dev.dev.dev_config_space[i]);
	}

	tprintf("");
	tavor_pci_dev.dev.dev = dev;

	tprintf("");
	if (dev->dev_id == TAVOR_DEVICE_ID) {

		rc = find_mlx_bridge(dev->bus, &br_bus, &br_devfn);
		if (rc) {
			eprintf("");
			return rc;
		}

		tavor_pci_dev.br.bus = br_bus;
		tavor_pci_dev.br.devfn = br_devfn;

		tprintf("bus=%d devfn=0x%x", br_bus, br_devfn);
		/* save config space */
		for (i = 0; i < 64; ++i) {
			rc = pcibios_read_config_dword(br_bus, br_devfn, i << 2,
						       &tavor_pci_dev.br.
						       dev_config_space[i]);
			if (rc) {
				eprintf("");
				return rc;
			}
			tprintf("config[%d]= 0x%08lx", i << 2,
				tavor_pci_dev.br.dev_config_space[i]);
		}
	}

	tprintf("");

	/* map cr-space */
	tavor_pci_dev.cr_space = ioremap(tavor_pci_dev.dev.bar[0], 0x100000);
	if (!tavor_pci_dev.cr_space) {
		eprintf("");
		return -1;
	}

	/* map uar */
	tavor_pci_dev.uar =
	    ioremap(tavor_pci_dev.dev.bar[2] + UAR_IDX * 0x1000, 0x1000);
	if (!tavor_pci_dev.uar) {
		eprintf("");
		return -1;
	}
	tprintf("uar_base (pa:va) = 0x%lx 0x%lx",
		tavor_pci_dev.dev.bar[2] + UAR_IDX * 0x1000, tavor_pci_dev.uar);

	tprintf("");

	return 0;
}

static inline unsigned long lalign(unsigned long buf, unsigned long align)
{
	return (unsigned long)((buf + align - 1) &
			       (~(((unsigned long)align) - 1)));
}

static int init_dev_data(void)
{
	unsigned long tmp;

	tmp = lalign(virt_to_bus(src_buf), STRUCT_ALIGN_SZ);

	dev_buffers_p = bus_to_virt(tmp);
	memreg_size = (__u32) (&memreg_size) - (__u32) dev_buffers_p;
	tprintf("src_buf=0x%lx, dev_buffers_p=0x%lx, memreg_size=0x%x", src_buf,
		dev_buffers_p, memreg_size);

	return 0;
}

static int restore_config(void)
{
	int i;
	int rc;

	if (tavor_pci_dev.dev.dev->dev_id == TAVOR_DEVICE_ID) {
		for (i = 0; i < 64; ++i) {
			rc = pcibios_write_config_dword(tavor_pci_dev.br.bus,
							tavor_pci_dev.br.devfn,
							i << 2,
							tavor_pci_dev.br.
							dev_config_space[i]);
			if (rc) {
				return rc;
			}
		}
	}

	for (i = 0; i < 64; ++i) {
		if (i != 22 && i != 23) {
			rc = pci_write_config_dword(tavor_pci_dev.dev.dev,
						    i << 2,
						    tavor_pci_dev.dev.
						    dev_config_space[i]);
			if (rc) {
				return rc;
			}
		}
	}
	return 0;
}

static void prep_init_hca_buf(const struct init_hca_st *init_hca_p, void *buf)
{
	/*struct init_hca_param_st */ void *p = buf;
	void *tmp;

	memset(buf, 0, MT_STRUCT_SIZE(tavorprm_init_hca_st));

	tmp =
	    p + MT_BYTE_OFFSET(tavorprm_init_hca_st,
			       qpc_eec_cqc_eqc_rdb_parameters);

	INS_FLD(init_hca_p->qpc_base_addr_h, tmp, tavorprm_qpcbaseaddr_st,
		qpc_base_addr_h);
	INS_FLD(init_hca_p->
		qpc_base_addr_l >> (32 -
				    (MT_BIT_SIZE
				     (tavorprm_qpcbaseaddr_st,
				      qpc_base_addr_l))), tmp,
		tavorprm_qpcbaseaddr_st, qpc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_qp, tmp, tavorprm_qpcbaseaddr_st,
		log_num_of_qp);

	INS_FLD(init_hca_p->cqc_base_addr_h, tmp, tavorprm_qpcbaseaddr_st,
		cqc_base_addr_h);
	INS_FLD(init_hca_p->
		cqc_base_addr_l >> (32 -
				    (MT_BIT_SIZE
				     (tavorprm_qpcbaseaddr_st,
				      cqc_base_addr_l))), tmp,
		tavorprm_qpcbaseaddr_st, cqc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_cq, tmp, tavorprm_qpcbaseaddr_st,
		log_num_of_cq);

	INS_FLD(init_hca_p->eqc_base_addr_h, tmp, tavorprm_qpcbaseaddr_st,
		eqc_base_addr_h);
	INS_FLD(init_hca_p->
		eqc_base_addr_l >> (32 -
				    (MT_BIT_SIZE
				     (tavorprm_qpcbaseaddr_st,
				      eqc_base_addr_l))), tmp,
		tavorprm_qpcbaseaddr_st, eqc_base_addr_l);
	INS_FLD(LOG2_EQS, tmp, tavorprm_qpcbaseaddr_st, log_num_eq);

	INS_FLD(init_hca_p->srqc_base_addr_h, tmp, tavorprm_qpcbaseaddr_st,
		srqc_base_addr_h);
	INS_FLD(init_hca_p->
		srqc_base_addr_l >> (32 -
				     (MT_BIT_SIZE
				      (tavorprm_qpcbaseaddr_st,
				       srqc_base_addr_l))), tmp,
		tavorprm_qpcbaseaddr_st, srqc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_srq, tmp, tavorprm_qpcbaseaddr_st,
		log_num_of_srq);

	INS_FLD(init_hca_p->eqpc_base_addr_h, tmp, tavorprm_qpcbaseaddr_st,
		eqpc_base_addr_h);
	INS_FLD(init_hca_p->eqpc_base_addr_l, tmp, tavorprm_qpcbaseaddr_st,
		eqpc_base_addr_l);

	INS_FLD(init_hca_p->eeec_base_addr_h, tmp, tavorprm_qpcbaseaddr_st,
		eeec_base_addr_h);
	INS_FLD(init_hca_p->eeec_base_addr_l, tmp, tavorprm_qpcbaseaddr_st,
		eeec_base_addr_l);

	tmp = p + MT_BYTE_OFFSET(tavorprm_init_hca_st, multicast_parameters);

	INS_FLD(init_hca_p->mc_base_addr_h, tmp, tavorprm_multicastparam_st,
		mc_base_addr_h);
	INS_FLD(init_hca_p->mc_base_addr_l, tmp, tavorprm_multicastparam_st,
		mc_base_addr_l);

	INS_FLD(init_hca_p->log_mc_table_entry_sz, tmp,
		tavorprm_multicastparam_st, log_mc_table_entry_sz);
	INS_FLD(init_hca_p->log_mc_table_sz, tmp, tavorprm_multicastparam_st,
		log_mc_table_sz);
	INS_FLD(init_hca_p->mc_table_hash_sz, tmp, tavorprm_multicastparam_st,
		mc_table_hash_sz);

	tmp = p + MT_BYTE_OFFSET(tavorprm_init_hca_st, tpt_parameters);

	INS_FLD(init_hca_p->mpt_base_addr_h, tmp, tavorprm_tptparams_st,
		mpt_base_adr_h);
	INS_FLD(init_hca_p->mpt_base_addr_l, tmp, tavorprm_tptparams_st,
		mpt_base_adr_l);
	INS_FLD(init_hca_p->log_mpt_sz, tmp, tavorprm_tptparams_st, log_mpt_sz);

	INS_FLD(init_hca_p->mtt_base_addr_h, tmp, tavorprm_tptparams_st,
		mtt_base_addr_h);
	INS_FLD(init_hca_p->mtt_base_addr_l, tmp, tavorprm_tptparams_st,
		mtt_base_addr_l);

	tmp = p + MT_BYTE_OFFSET(tavorprm_init_hca_st, uar_parameters);
	INS_FLD(tavor_pci_dev.dev.bar[3], tmp, tavorprm_uar_params_st,
		uar_base_addr_h);
	INS_FLD(tavor_pci_dev.dev.bar[2] & 0xfff00000, tmp,
		tavorprm_uar_params_st, uar_base_addr_l);

}

static void prep_sw2hw_mpt_buf(void *buf, __u32 mkey)
{
	INS_FLD(1, buf, tavorprm_mpt_st, m_io);
	INS_FLD(1, buf, tavorprm_mpt_st, lw);
	INS_FLD(1, buf, tavorprm_mpt_st, lr);
	INS_FLD(1, buf, tavorprm_mpt_st, pa);
	INS_FLD(1, buf, tavorprm_mpt_st, r_w);

	INS_FLD(mkey, buf, tavorprm_mpt_st, mem_key);
	INS_FLD(GLOBAL_PD, buf, tavorprm_mpt_st, pd);

	INS_FLD(virt_to_bus(dev_buffers_p), buf, tavorprm_mpt_st,
		start_address_l);
	INS_FLD(memreg_size, buf, tavorprm_mpt_st, reg_wnd_len_l);
}

static void prep_sw2hw_eq_buf(void *buf, struct eqe_t *eq)
{
	memset(buf, 0, MT_STRUCT_SIZE(tavorprm_eqc_st));

	INS_FLD(2, buf, tavorprm_eqc_st, st); /* fired */
	INS_FLD(virt_to_bus(eq), buf, tavorprm_eqc_st, start_address_l);
	INS_FLD(LOG2_EQ_SZ, buf, tavorprm_eqc_st, log_eq_size);
	INS_FLD(UAR_IDX, buf, tavorprm_eqc_st, usr_page);
	INS_FLD(GLOBAL_PD, buf, tavorprm_eqc_st, pd);
	INS_FLD(dev_ib_data.mkey, buf, tavorprm_eqc_st, lkey);
}

static void init_eq_buf(void *eq_buf)
{
	int num_eqes = 1 << LOG2_EQ_SZ;

	memset(eq_buf, 0xff, num_eqes * sizeof(struct eqe_t));
}

static void prep_init_ib_buf(void *buf)
{
	__u32 *ptr = (__u32 *) buf;

	ptr[0] = 0x4310;
	ptr[1] = 1;
	ptr[2] = 64;
}

static void prep_sw2hw_cq_buf(void *buf, __u8 eqn, __u32 cqn,
			      union cqe_st *cq_buf)
{
	__u32 *ptr = (__u32 *) buf;

	ptr[2] = virt_to_bus(cq_buf);
	ptr[3] = (LOG2_CQ_SZ << 24) | UAR_IDX;
	ptr[4] = eqn;
	ptr[5] = eqn;
	ptr[6] = dev_ib_data.pd;
	ptr[7] = dev_ib_data.mkey;
	ptr[12] = cqn;
}

static void prep_rst2init_qpee_buf(void *buf, __u32 snd_cqn, __u32 rcv_cqn,
				   __u32 qkey)
{
	struct qp_ee_state_tarnisition_st *prm;
	void *tmp;

	prm = (struct qp_ee_state_tarnisition_st *)buf;

	INS_FLD(3, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st, st);	/* service type = UD */
	INS_FLD(3, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st, pm_state);	/* required for UD QP */
	INS_FLD(UAR_IDX, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st,
		usr_page);
	INS_FLD(dev_ib_data.pd, &prm->ctx,
		tavorprm_queue_pair_ee_context_entry_st, pd);
	INS_FLD(dev_ib_data.mkey, &prm->ctx,
		tavorprm_queue_pair_ee_context_entry_st, wqe_lkey);
	INS_FLD(1, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st, ssc);	/* generate send CQE */
	INS_FLD(1, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st, rsc);	/* generate receive CQE */
	INS_FLD(snd_cqn, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st,
		cqn_snd);
	INS_FLD(rcv_cqn, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st,
		cqn_rcv);
	INS_FLD(qkey, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st,
		q_key);

	tmp =
	    (void *)(&prm->ctx) +
	    MT_BYTE_OFFSET(tavorprm_queue_pair_ee_context_entry_st,
			   primary_address_path);
	INS_FLD(dev_ib_data.port, tmp, tavorprm_address_path_st, port_number);

	INS_FLD(4, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st, mtu);
	INS_FLD(0xb, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st,
		msg_max);
}

static void prep_init2rtr_qpee_buf(void *buf)
{
	struct qp_ee_state_tarnisition_st *prm;

	prm = (struct qp_ee_state_tarnisition_st *)buf;

	INS_FLD(4, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st, mtu);
	INS_FLD(0xb, &prm->ctx, tavorprm_queue_pair_ee_context_entry_st,
		msg_max);
}

static void init_av_array()
{
	int i;

	dev_ib_data.udav.av_array = dev_buffers_p->av_array;
	dev_ib_data.udav.udav_next_free = FL_EOL;
	for (i = 0; i < NUM_AVS; ++i) {
		dev_ib_data.udav.av_array[i].ud_av.next_free =
		    dev_ib_data.udav.udav_next_free;
		dev_ib_data.udav.udav_next_free = i;
	}
	tprintf("dev_ib_data.udav.udav_next_free=%d", i);
}

static int setup_hca(__u8 port, void **eq_p)
{
	int rc;
	__u32 key, in_key;
	__u32 *inprm;
	struct eqe_t *eq_buf;
	__u32 event_mask;
	void *cfg;
	int ret = 0;
	__u8 eqn;
	struct dev_lim_st dev_lim;
	struct init_hca_st init_hca;
	__u32 offset, base_h, base_l;
	const __u32 delta = 0x400000;
	struct query_fw_st qfw;

	tprintf("called");

	init_dev_data();

	rc = reset_hca();
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	} else {
		tprintf("reset_hca() success");
	}

	mdelay(1000);		/* wait for 1 sec */

	rc = restore_config();
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	} else {
		tprintf("restore_config() success");
	}

	dev_ib_data.pd = GLOBAL_PD;
	dev_ib_data.port = port;

	/* execute system enable command */
	rc = cmd_sys_en();
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	} else {
		tprintf("cmd_sys_en() success");
	}

	rc= cmd_query_fw(&qfw);
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	} else {
		tprintf("cmd_query_fw() success");

		if (print_info) {
			printf("FW ver = %d.%d.%d\n",
				qfw.fw_rev_major,
				qfw.fw_rev_minor,
				qfw.fw_rev_subminor);
		}
		tprintf("fw_rev_major=%d", qfw.fw_rev_major);
		tprintf("fw_rev_minor=%d", qfw.fw_rev_minor);
		tprintf("fw_rev_subminor=%d", qfw.fw_rev_subminor);
		tprintf("error_buf_start_h=0x%x", qfw.error_buf_start_h);
		tprintf("error_buf_start_l=0x%x", qfw.error_buf_start_l);
		tprintf("error_buf_size=%d", qfw.error_buf_size);
	}

	if (qfw.error_buf_start_h) {
		eprintf("too high physical address");
		ret = -1;
		goto exit;
	}

	dev_ib_data.error_buf_addr= ioremap(qfw.error_buf_start_l,
										qfw.error_buf_size*4);
	dev_ib_data.error_buf_size= qfw.error_buf_size;
	if (!dev_ib_data.error_buf_addr) {
		eprintf("");
		ret = -1;
		goto exit;
	}


	rc = cmd_query_dev_lim(&dev_lim);
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	} else {
		tprintf("cmd_query_dev_lim() success");
		tprintf("log2_rsvd_qps=%x", dev_lim.log2_rsvd_qps);
		tprintf("qpc_entry_sz=%x", dev_lim.qpc_entry_sz);
		tprintf("log2_rsvd_srqs=%x", dev_lim.log2_rsvd_srqs);
		tprintf("srq_entry_sz=%x", dev_lim.srq_entry_sz);
		tprintf("log2_rsvd_ees=%x", dev_lim.log2_rsvd_ees);
		tprintf("eec_entry_sz=%x", dev_lim.eec_entry_sz);
		tprintf("log2_rsvd_cqs=%x", dev_lim.log2_rsvd_cqs);
		tprintf("cqc_entry_sz=%x", dev_lim.cqc_entry_sz);
		tprintf("log2_rsvd_mtts=%x", dev_lim.log2_rsvd_mtts);
		tprintf("mtt_entry_sz=%x", dev_lim.mtt_entry_sz);
		tprintf("log2_rsvd_mrws=%x", dev_lim.log2_rsvd_mrws);
		tprintf("mpt_entry_sz=%x", dev_lim.mpt_entry_sz);
		tprintf("eqc_entry_sz=%x", dev_lim.eqc_entry_sz);
	}

	/* set the qp and cq numbers according
	   to the results of query_dev_lim */
	dev_ib_data.mads_qp.qpn = (1 << dev_lim.log2_rsvd_qps) +
	    +QPN_BASE + MADS_QPN_SN;
	dev_ib_data.ipoib_qp.qpn = (1 << dev_lim.log2_rsvd_qps) +
	    +QPN_BASE + IPOIB_QPN_SN;

	dev_ib_data.mads_qp.snd_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    MADS_SND_CQN_SN;
	dev_ib_data.mads_qp.rcv_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    MADS_RCV_CQN_SN;

	dev_ib_data.ipoib_qp.snd_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    IPOIB_SND_CQN_SN;
	dev_ib_data.ipoib_qp.rcv_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    IPOIB_RCV_CQN_SN;

	/* disable SRQ */
	cfg = (void *)dev_buffers_p->inprm_buf;
	memset(cfg, 0, MT_STRUCT_SIZE(tavorprm_mod_stat_cfg_st));
	INS_FLD(1, cfg, tavorprm_mod_stat_cfg_st, srq_m);	//cfg->srq_m = 1;
	rc = cmd_mod_stat_cfg(cfg);
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	} else {
		tprintf("cmd_mod_stat_cfg() success");
	}

	/* prepare the init_hca params to pass
	   to prep_init_hca_buf */
	memset(&init_hca, 0, sizeof init_hca);
	offset = 0;
	base_h = tavor_pci_dev.dev.bar[5] & 0xfffffff0;
	base_l = tavor_pci_dev.dev.bar[4] & 0xfffffff0;

	tprintf("base_h=0x%lx, base_l=0x%lx", base_h, base_l);

	init_hca.qpc_base_addr_h = base_h;
	init_hca.qpc_base_addr_l = base_l + offset;
	init_hca.log_num_of_qp = dev_lim.log2_rsvd_qps + 1;
	offset += delta;

	init_hca.eec_base_addr_h = base_h;
	init_hca.eec_base_addr_l = base_l + offset;
	init_hca.log_num_of_ee = dev_lim.log2_rsvd_ees;
	offset += delta;

	init_hca.srqc_base_addr_h = base_h;
	init_hca.srqc_base_addr_l = base_l + offset;
	init_hca.log_num_of_srq = dev_lim.log2_rsvd_srqs;
	offset += delta;

	init_hca.cqc_base_addr_h = base_h;
	init_hca.cqc_base_addr_l = base_l + offset;
	init_hca.log_num_of_cq = dev_lim.log2_rsvd_cqs + 1;
	offset += delta;

	init_hca.eqpc_base_addr_h = base_h;
	init_hca.eqpc_base_addr_l = base_l + offset;
	offset += delta;

	init_hca.eeec_base_addr_h = base_h;
	init_hca.eeec_base_addr_l = base_l + offset;
	offset += delta;

	init_hca.eqc_base_addr_h = base_h;
	init_hca.eqc_base_addr_l = base_l + offset;
	init_hca.log_num_of_eq = LOG2_EQS;
	offset += delta;

	init_hca.rdb_base_addr_h = base_h;
	init_hca.rdb_base_addr_l = base_l + offset;
	offset += delta;

	init_hca.mc_base_addr_h = base_h;
	init_hca.mc_base_addr_l = base_l + offset;
	init_hca.log_mc_table_entry_sz = LOG2_MC_ENTRY;
	init_hca.mc_table_hash_sz = 0;
	init_hca.log_mc_table_sz = LOG2_MC_GROUPS;
	offset += delta;

	init_hca.mpt_base_addr_h = base_h;
	init_hca.mpt_base_addr_l = base_l + offset;
	init_hca.log_mpt_sz = dev_lim.log2_rsvd_mrws + 1;
	offset += delta;

	init_hca.mtt_base_addr_h = base_h;
	init_hca.mtt_base_addr_l = base_l + offset;

	/* this buffer is used for all the commands */
	inprm = (void *)dev_buffers_p->inprm_buf;
	/* excute init_hca command */
	prep_init_hca_buf(&init_hca, inprm);

	rc = cmd_init_hca(inprm, MT_STRUCT_SIZE(tavorprm_init_hca_st));
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sys_en;
	} else
		tprintf("cmd_init_hca() success");

	/* register a single memory region which covers
	   4 GB of the address space which will be used
	   throughout the driver */
	memset(inprm, 0, SW2HW_MPT_IBUF_SZ);
	in_key = MKEY_PREFIX + (1 << dev_lim.log2_rsvd_mrws);
	prep_sw2hw_mpt_buf(inprm, in_key);
	rc = cmd_sw2hw_mpt(&key, in_key, inprm, SW2HW_MPT_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_init_hca;
	} else {
		tprintf("cmd_sw2hw_mpt() success, key=0x%lx", key);
	}
	dev_ib_data.mkey = key;

	eqn = EQN;
	/* allocate a single EQ which will receive 
	   all the events */
	eq_buf = dev_buffers_p->eq_buf;
	init_eq_buf(eq_buf);	/* put in HW ownership */
	prep_sw2hw_eq_buf(inprm, eq_buf);
	rc = cmd_sw2hw_eq(SW2HW_EQ_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sw2hw_mpt;
	} else
		tprintf("cmd_sw2hw_eq() success");

	event_mask = (1 << XDEV_EV_TYPE_CQ_COMP) |
	    (1 << XDEV_EV_TYPE_CQ_ERR) |
	    (1 << XDEV_EV_TYPE_LOCAL_WQ_CATAS_ERR) |
	    (1 << XDEV_EV_TYPE_PORT_ERR) |
	    (1 << XDEV_EV_TYPE_LOCAL_WQ_INVALID_REQ_ERR) |
	    (1 << XDEV_EV_TYPE_LOCAL_WQ_ACCESS_VIOL_ERR) |
	    (1 << TAVOR_IF_EV_TYPE_OVERRUN);
	rc = cmd_map_eq(eqn, event_mask, 1);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sw2hw_eq;
	} else
		tprintf("cmd_map_eq() success");

	dev_ib_data.eq.eqn = eqn;
	dev_ib_data.eq.eq_buf = eq_buf;
	dev_ib_data.eq.cons_idx = 0;
	dev_ib_data.eq.eq_size = 1 << LOG2_EQ_SZ;
	*eq_p = &dev_ib_data.eq;

	memset(inprm, 0, INIT_IB_IBUF_SZ);
	prep_init_ib_buf(inprm);
	rc = cmd_init_ib(port, inprm, INIT_IB_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sw2hw_eq;
	} else
		tprintf("cmd_init_ib() success");

	init_av_array();
	tprintf("init_av_array() done");

	goto exit;

      undo_sw2hw_eq:
	rc = cmd_hw2sw_eq(EQN);
	if (rc) {
		eprintf("");
	} else
		tprintf("cmd_hw2sw_eq() success");

      undo_sw2hw_mpt:
	rc = cmd_hw2sw_mpt(key);
	if (rc)
		eprintf("");
	else
		tprintf("cmd_hw2sw_mpt() success key=0x%lx", key);

      undo_init_hca:
	rc = cmd_close_hca(0);
	if (rc) {
		eprintf("");
		goto undo_sys_en;
	} else
		tprintf("cmd_close_hca() success");

      undo_sys_en:
	rc = cmd_sys_dis();
	if (rc) {
		eprintf("");
		goto undo_sys_en;
	} else
		tprintf("cmd_sys_dis() success");
	goto exit;

      exit:
	return ret;
}


static int unset_hca(void)
{
	int rc = 0;

	if (!fw_fatal) {
		rc = cmd_sys_dis();
		if (rc)
			eprintf("");
	}

	return rc;
}

static void *get_inprm_buf(void)
{
	return dev_buffers_p->inprm_buf;
}

static void *get_outprm_buf(void)
{
	return dev_buffers_p->outprm_buf;
}

static void *get_send_wqe_buf(void *wqe, __u8 index)
{
	struct ud_send_wqe_st *snd_wqe = wqe;

	return bus_to_virt(snd_wqe->mpointer[index].local_addr_l);
}

static void *get_rcv_wqe_buf(void *wqe, __u8 index)
{
	struct recv_wqe_st *rcv_wqe = wqe;

	return bus_to_virt(be32_to_cpu(rcv_wqe->mpointer[index].local_addr_l));
}

static void modify_av_params(struct ud_av_st *av,
			     __u16 dlid,
			     __u8 g,
			     __u8 sl, __u8 rate, union ib_gid_u *gid, __u32 qpn)
{
	memset(&av->av, 0, sizeof av->av);
	INS_FLD(dev_ib_data.port, &av->av, tavorprm_ud_address_vector_st,
		port_number);
	INS_FLD(dev_ib_data.pd, &av->av, tavorprm_ud_address_vector_st, pd);
	INS_FLD(dlid, &av->av, tavorprm_ud_address_vector_st, rlid);
	INS_FLD(g, &av->av, tavorprm_ud_address_vector_st, g);
	INS_FLD(sl, &av->av, tavorprm_ud_address_vector_st, sl);
	INS_FLD(3, &av->av, tavorprm_ud_address_vector_st, msg);

	if (rate >= 3)
		INS_FLD(0, &av->av, tavorprm_ud_address_vector_st, max_stat_rate);	/* 4x */
	else
		INS_FLD(1, &av->av, tavorprm_ud_address_vector_st, max_stat_rate);	/* 1x */

	cpu_to_be_buf(&av->av, sizeof(av->av));
	if (g) {
		if (gid) {
			INS_FLD(*((__u32 *) (&gid->raw[0])), &av->av,
				tavorprm_ud_address_vector_st, rgid_127_96);
			INS_FLD(*((__u32 *) (&gid->raw[4])), &av->av,
				tavorprm_ud_address_vector_st, rgid_95_64);
			INS_FLD(*((__u32 *) (&gid->raw[8])), &av->av,
				tavorprm_ud_address_vector_st, rgid_63_32);
			INS_FLD(*((__u32 *) (&gid->raw[12])), &av->av,
				tavorprm_ud_address_vector_st, rgid_31_0);
		} else {
			INS_FLD(0, &av->av, tavorprm_ud_address_vector_st,
				rgid_127_96);
			INS_FLD(0, &av->av, tavorprm_ud_address_vector_st,
				rgid_95_64);
			INS_FLD(0, &av->av, tavorprm_ud_address_vector_st,
				rgid_63_32);
			INS_FLD(0, &av->av, tavorprm_ud_address_vector_st,
				rgid_31_0);
		}
	} else {
		INS_FLD(0, &av->av, tavorprm_ud_address_vector_st, rgid_127_96);
		INS_FLD(0, &av->av, tavorprm_ud_address_vector_st, rgid_95_64);
		INS_FLD(0, &av->av, tavorprm_ud_address_vector_st, rgid_63_32);
		INS_FLD(2, &av->av, tavorprm_ud_address_vector_st, rgid_31_0);
	}
	av->dest_qp = qpn;
}

static void init_cq_buf(union cqe_st *cq_buf, __u8 num_cqes)
{
	memset(cq_buf, 0xff, num_cqes * sizeof cq_buf[0]);
}

static int post_rcv_buf(struct udqp_st *qp, struct recv_wqe_st *rcv_wqe)
{
	struct recv_doorbell_st dbell;
	int rc;
	__u32 tmp[2];
	struct recv_wqe_st *tmp_wqe = (struct recv_wqe_st *)tmp;
	__u32 *ptr_dst;

	memset(&dbell, 0, sizeof dbell);
	INS_FLD(sizeof(*rcv_wqe) >> 4, &dbell, tavorprm_receive_doorbell_st,
		nds);
	INS_FLD(virt_to_bus(rcv_wqe) >> 6, &dbell, tavorprm_receive_doorbell_st,
		nda);
	INS_FLD(qp->qpn, &dbell, tavorprm_receive_doorbell_st, qpn);
	INS_FLD(1, &dbell, tavorprm_receive_doorbell_st, credits);

	if (qp->last_posted_rcv_wqe) {
		memcpy(tmp, qp->last_posted_rcv_wqe, sizeof(tmp));
		be_to_cpu_buf(tmp, sizeof(tmp));
		INS_FLD(1, tmp_wqe->next, wqe_segment_next_st, dbd);
		INS_FLD(sizeof(*rcv_wqe) >> 4, tmp_wqe->next,
			wqe_segment_next_st, nds);
		INS_FLD(virt_to_bus(rcv_wqe) >> 6, tmp_wqe->next,
			wqe_segment_next_st, nda_31_6);
		/* this is not really opcode but since the struct
		   is used for both send and receive, in receive this bit must be 1
		   which coinsides with nopcode */
		INS_FLD(1, tmp_wqe->next, wqe_segment_next_st, nopcode);

		cpu_to_be_buf(tmp, sizeof(tmp));

		ptr_dst = (__u32 *) (qp->last_posted_rcv_wqe);
		ptr_dst[0] = tmp[0];
		ptr_dst[1] = tmp[1];
	}
	rc = cmd_post_doorbell(&dbell, POST_RCV_OFFSET);
	if (!rc) {
		qp->last_posted_rcv_wqe = rcv_wqe;
	}

	return rc;
}

static int post_send_req(void *qph, void *wqeh, __u8 num_gather)
{
	struct send_doorbell_st dbell;
	int rc;
	struct udqp_st *qp = qph;
	struct ud_send_wqe_st *snd_wqe = wqeh;
	struct next_control_seg_st tmp;
	__u32 *psrc, *pdst;
	__u32 nds;

	tprintf("snd_wqe=0x%lx, virt_to_bus(snd_wqe)=0x%lx", snd_wqe,
		virt_to_bus(snd_wqe));

	memset(&dbell, 0, sizeof dbell);
	INS_FLD(XDEV_NOPCODE_SEND, &dbell, tavorprm_send_doorbell_st, nopcode);
	INS_FLD(1, &dbell, tavorprm_send_doorbell_st, f);
	INS_FLD(virt_to_bus(snd_wqe) >> 6, &dbell, tavorprm_send_doorbell_st,
		nda);
	nds =
	    (sizeof(snd_wqe->next) + sizeof(snd_wqe->udseg) +
	     sizeof(snd_wqe->mpointer[0]) * num_gather) >> 4;
	INS_FLD(nds, &dbell, tavorprm_send_doorbell_st, nds);
	INS_FLD(qp->qpn, &dbell, tavorprm_send_doorbell_st, qpn);

	tprintf("0= %lx", ((__u32 *) ((void *)(&dbell)))[0]);
	tprintf("1= %lx", ((__u32 *) ((void *)(&dbell)))[1]);

	if (qp->last_posted_snd_wqe) {
		memcpy(&tmp, &qp->last_posted_snd_wqe->next, sizeof tmp);
		be_to_cpu_buf(&tmp, sizeof tmp);
		INS_FLD(1, &tmp, wqe_segment_next_st, dbd);
		INS_FLD(virt_to_bus(snd_wqe) >> 6, &tmp, wqe_segment_next_st,
			nda_31_6);
		INS_FLD(nds, &tmp, wqe_segment_next_st, nds);

		psrc = (__u32 *) (&tmp);
		pdst = (__u32 *) (&qp->last_posted_snd_wqe->next);
		pdst[0] = htonl(psrc[0]);
		pdst[1] = htonl(psrc[1]);
	}

	rc = cmd_post_doorbell(&dbell, POST_SND_OFFSET);
	if (!rc) {
		qp->last_posted_snd_wqe = snd_wqe;
	}

	return rc;
}

static int create_mads_qp(void **qp_pp, void **snd_cq_pp, void **rcv_cq_pp)
{
	__u8 i;
	int rc;
	struct udqp_st *qp;

	qp = &dev_ib_data.mads_qp;

	/* set the pointer to the receive WQEs buffer */
	qp->rcv_wq = dev_buffers_p->mads_qp_rcv_queue;

	qp->send_buf_sz = MAD_BUF_SZ;
	qp->rcv_buf_sz = MAD_BUF_SZ;

	qp->recv_wqe_alloc_idx = 0;
	qp->max_recv_wqes = NUM_MADS_RCV_WQES;
	qp->recv_wqe_cur_free = NUM_MADS_RCV_WQES;

	/* iterrate through the list */
	for (i = 0; i < NUM_MADS_RCV_WQES; ++i) {
		/* clear the WQE */
		memset(&qp->rcv_wq[i], 0, sizeof(qp->rcv_wq[i]));

		qp->rcv_wq[i].wqe_cont.qp = qp;
		qp->rcv_bufs[i] = ib_buffers.rcv_mad_buf[i];
	}

	/* set the pointer to the send WQEs buffer */
	qp->snd_wq = dev_buffers_p->mads_qp_snd_queue;

	qp->snd_wqe_alloc_idx = 0;
	qp->max_snd_wqes = NUM_MADS_SND_WQES;
	qp->snd_wqe_cur_free = NUM_MADS_SND_WQES;

	/* iterrate through the list */
	for (i = 0; i < NUM_MADS_SND_WQES; ++i) {
		/* clear the WQE */
		memset(&qp->snd_wq[i], 0, sizeof(qp->snd_wq[i]));

		/* link the WQE to the free list */
		qp->snd_wq[i].wqe_cont.qp = qp;
		qp->snd_bufs[i] = ib_buffers.send_mad_buf[i];
	}

	/* qp number and cq numbers are already set up */
	qp->snd_cq.cq_buf = dev_buffers_p->mads_snd_cq_buf;
	qp->rcv_cq.cq_buf = dev_buffers_p->mads_rcv_cq_buf;
	qp->snd_cq.num_cqes = NUM_MADS_SND_CQES;
	qp->rcv_cq.num_cqes = NUM_MADS_RCV_CQES;
	qp->qkey = GLOBAL_QKEY;
	rc = create_udqp(qp);
	if (!rc) {
		*qp_pp = qp;
		*snd_cq_pp = &qp->snd_cq;
		*rcv_cq_pp = &qp->rcv_cq;
	}

	return rc;
}

static int create_ipoib_qp(void **qp_pp,
			   void **snd_cq_pp, void **rcv_cq_pp, __u32 qkey)
{
	__u8 i;
	int rc;
	struct udqp_st *qp;
	qp = &dev_ib_data.ipoib_qp;

	/* set the pointer to the receive WQEs buffer */
	qp->rcv_wq = dev_buffers_p->ipoib_qp_rcv_queue;

	qp->rcv_buf_sz = IPOIB_RCV_BUF_SZ;

	qp->recv_wqe_alloc_idx = 0;
	qp->max_recv_wqes = NUM_IPOIB_RCV_WQES;
	qp->recv_wqe_cur_free = NUM_IPOIB_RCV_WQES;

	/* iterrate through the list */
	for (i = 0; i < NUM_IPOIB_RCV_WQES; ++i) {
		/* clear the WQE */
		memset(&qp->rcv_wq[i], 0, sizeof(qp->rcv_wq[i]));

		/* update data */
		qp->rcv_wq[i].wqe_cont.qp = qp;
		qp->rcv_bufs[i] = ib_buffers.ipoib_rcv_buf[i];
		tprintf("rcv_buf=%lx", qp->rcv_bufs[i]);
	}

	/* init send queue WQEs list */
	/* set the list empty */
	qp->snd_wqe_alloc_idx = 0;
	qp->max_snd_wqes = NUM_IPOIB_SND_WQES;
	qp->snd_wqe_cur_free = NUM_IPOIB_SND_WQES;

	/* set the pointer to the send WQEs buffer */
	qp->snd_wq = dev_buffers_p->ipoib_qp_snd_queue;

	/* iterrate through the list */
	for (i = 0; i < NUM_IPOIB_SND_WQES; ++i) {
		/* clear the WQE */
		memset(&qp->snd_wq[i], 0, sizeof(qp->snd_wq[i]));

		/* update data */
		qp->snd_wq[i].wqe_cont.qp = qp;
		qp->snd_bufs[i] = ib_buffers.send_ipoib_buf[i];
		qp->send_buf_sz = 4;
	}

	/* qp number and cq numbers are already set up */

	qp->snd_cq.cq_buf = dev_buffers_p->ipoib_snd_cq_buf;
	qp->rcv_cq.cq_buf = dev_buffers_p->ipoib_rcv_cq_buf;
	qp->snd_cq.num_cqes = NUM_IPOIB_SND_CQES;
	qp->rcv_cq.num_cqes = NUM_IPOIB_RCV_CQES;
	qp->qkey = qkey;
	rc = create_udqp(qp);
	if (!rc) {
		*qp_pp = qp;
		*snd_cq_pp = &qp->snd_cq;
		*rcv_cq_pp = &qp->rcv_cq;
	}

	return rc;
}

static int create_udqp(struct udqp_st *qp)
{
	int rc, ret = 0;
	void *inprm;
	struct recv_wqe_st *rcv_wqe;

	inprm = dev_buffers_p->inprm_buf;

	/* create send CQ */
	init_cq_buf(qp->snd_cq.cq_buf, qp->snd_cq.num_cqes);
	qp->snd_cq.cons_idx = 0;
	memset(inprm, 0, SW2HW_CQ_IBUF_SZ);
	prep_sw2hw_cq_buf(inprm, dev_ib_data.eq.eqn, qp->snd_cq.cqn,
			  qp->snd_cq.cq_buf);
	rc = cmd_sw2hw_cq(qp->snd_cq.cqn, inprm, SW2HW_CQ_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	}

	/* create receive CQ */
	init_cq_buf(qp->rcv_cq.cq_buf, qp->rcv_cq.num_cqes);
	qp->rcv_cq.cons_idx = 0;
	memset(inprm, 0, SW2HW_CQ_IBUF_SZ);
	prep_sw2hw_cq_buf(inprm, dev_ib_data.eq.eqn, qp->rcv_cq.cqn,
			  qp->rcv_cq.cq_buf);
	rc = cmd_sw2hw_cq(qp->rcv_cq.cqn, inprm, SW2HW_CQ_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_snd_cq;
	}

	memset(inprm, 0, QPCTX_IBUF_SZ);
	prep_rst2init_qpee_buf(inprm, qp->snd_cq.cqn, qp->rcv_cq.cqn, qp->qkey);
	rc = cmd_rst2init_qpee(qp->qpn, inprm, QPCTX_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_rcv_cq;
	}

	qp->last_posted_rcv_wqe = NULL;
	qp->last_posted_snd_wqe = NULL;

	/* post all the buffers to the receive queue */
	while (1) {
		/* allocate wqe */
		rcv_wqe = alloc_rcv_wqe(qp);
		if (!rcv_wqe)
			break;

		/* post the buffer */
		rc = post_rcv_buf(qp, rcv_wqe);
		if (rc) {
			ret = -1;
			eprintf("");
			goto undo_rcv_cq;
		}
	}

	memset(inprm, 0, QPCTX_IBUF_SZ);
	prep_init2rtr_qpee_buf(inprm);
	rc = cmd_init2rtr_qpee(qp->qpn, inprm, QPCTX_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_rcv_cq;
	}

	memset(inprm, 0, QPCTX_IBUF_SZ);
	rc = cmd_rtr2rts_qpee(qp->qpn, inprm, QPCTX_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_rcv_cq;
	}

	goto exit;

      undo_rcv_cq:
	rc = cmd_hw2sw_cq(qp->rcv_cq.cqn);
	if (rc)
		eprintf("");

      undo_snd_cq:
	rc = cmd_hw2sw_cq(qp->snd_cq.cqn);
	if (rc)
		eprintf("");

      exit:
	return ret;
}

static int destroy_udqp(struct udqp_st *qp)
{
	int rc;

	rc = cmd_2err_qpee(qp->qpn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_2err_qpee(0x%lx) success", qp->qpn);

	rc = cmd_2rst_qpee(qp->qpn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_2rst_qpee(0x%lx) success", qp->qpn);

	rc = cmd_hw2sw_cq(qp->rcv_cq.cqn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_hw2sw_cq(0x%lx) success", qp->snd_cq.cqn);

	rc = cmd_hw2sw_cq(qp->snd_cq.cqn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_hw2sw_cq(0x%lx) success", qp->rcv_cq.cqn);

	return rc;
}

static void prep_send_wqe_buf(void *qph,
			      void *avh,
			      void *wqeh,
			      const void *buf,
			      unsigned int offset, __u16 len, __u8 e)
{
	struct udqp_st *qp = qph;
	struct ud_av_st *av = avh;
	struct ud_send_wqe_st *wqe = wqeh;

	INS_FLD(e, wqe->next.control, wqe_segment_ctrl_send_st, e);
	INS_FLD(1, wqe->next.control, wqe_segment_ctrl_send_st, always1);

	wqe->udseg.av_add_h = 0;
	wqe->udseg.av_add_l = virt_to_bus(&av->av);
	wqe->udseg.dest_qp = av->dest_qp;
	wqe->udseg.lkey = dev_ib_data.mkey;
	wqe->udseg.qkey = qp->qkey;

	if (buf) {
		memcpy(bus_to_virt(wqe->mpointer[0].local_addr_l) + offset, buf,
		       len);
		len += offset;
	}
	wqe->mpointer[0].byte_count = len;
	wqe->mpointer[0].lkey = dev_ib_data.mkey;

	cpu_to_be_buf(wqe, sizeof *wqe);
}

static void *alloc_ud_av(void)
{
	u8 next_free;

	if (dev_ib_data.udav.udav_next_free == FL_EOL) {
		return NULL;
	}

	next_free = dev_ib_data.udav.udav_next_free;
	dev_ib_data.udav.udav_next_free =
	    dev_buffers_p->av_array[next_free].ud_av.next_free;
	tprintf("allocated udav %d", next_free);
	return &dev_buffers_p->av_array[next_free].ud_av;
}

static void free_ud_av(void *avh)
{
	union ud_av_u *avu;
	__u8 idx, old_idx;
	struct ud_av_st *av = avh;

	avu = (union ud_av_u *)av;

	idx = avu - dev_buffers_p->av_array;
	tprintf("freeing udav idx=%d", idx);
	old_idx = dev_ib_data.udav.udav_next_free;
	dev_ib_data.udav.udav_next_free = idx;
	avu->ud_av.next_free = old_idx;
}

static int update_cq_cons_idx(struct cq_st *cq)
{
	struct cq_dbell_st dbell;
	int rc;

	memset(&dbell, 0, sizeof dbell);
	INS_FLD(cq->cqn, &dbell, tavorprm_cq_cmd_doorbell_st, cqn);
	INS_FLD(CQ_DBELL_CMD_INC_CONS_IDX, &dbell, tavorprm_cq_cmd_doorbell_st,
		cq_cmd);
	rc = cmd_post_doorbell(&dbell, CQ_DBELL_OFFSET);
	return rc;
}

static int poll_cq(void *cqh, union cqe_st *cqe_p, u8 * num_cqes)
{
	union cqe_st cqe;
	int rc;
	u32 *ptr;
	struct cq_st *cq = cqh;

	if (cq->cqn < 0x80 || cq->cqn > 0x83) {
		eprintf("");
		return -1;
	}
	ptr = (u32 *) (&(cq->cq_buf[cq->cons_idx]));
	barrier();
	if ((ptr[7] & 0x80000000) == 0) {
		cqe = cq->cq_buf[cq->cons_idx];
		be_to_cpu_buf(&cqe, sizeof(cqe));
		*cqe_p = cqe;
		ptr[7] = 0x80000000;
		barrier();
		cq->cons_idx = (cq->cons_idx + 1) % cq->num_cqes;
		rc = update_cq_cons_idx(cq);
		if (rc) {
			return rc;
		}
		*num_cqes = 1;
	} else
		*num_cqes = 0;

	return 0;
}

static void dev2ib_cqe(struct ib_cqe_st *ib_cqe_p, union cqe_st *cqe_p)
{
	__u8 opcode;
	__u32 wqe_addr_ba;

	opcode =
	    EX_FLD(cqe_p->good_cqe, tavorprm_completion_queue_entry_st, opcode);
	if (opcode >= CQE_ERROR_OPCODE)
		ib_cqe_p->is_error = 1;
	else
		ib_cqe_p->is_error = 0;

	ib_cqe_p->is_send =
	    EX_FLD(cqe_p->good_cqe, tavorprm_completion_queue_entry_st, s);
	wqe_addr_ba =
	    EX_FLD(cqe_p->good_cqe, tavorprm_completion_queue_entry_st,
		   wqe_adr) << 6;
	ib_cqe_p->wqe = bus_to_virt(wqe_addr_ba);

//      if (ib_cqe_p->is_send) {
//              be_to_cpu_buf(ib_cqe_p->wqe, sizeof(struct ud_send_wqe_st));
//      }
//      else {
//              be_to_cpu_buf(ib_cqe_p->wqe, sizeof(struct recv_wqe_st));
//      }
	ib_cqe_p->count =
	    EX_FLD(cqe_p->good_cqe, tavorprm_completion_queue_entry_st,
		   byte_cnt);
}

static int ib_poll_cq(void *cqh, struct ib_cqe_st *ib_cqe_p, u8 * num_cqes)
{
	int rc;
	union cqe_st cqe;
	struct cq_st *cq = cqh;
	__u8 opcode;

	rc = poll_cq(cq, &cqe, num_cqes);
	if (rc || ((*num_cqes) == 0)) {
		return rc;
	}

	dev2ib_cqe(ib_cqe_p, &cqe);

	opcode =
	    EX_FLD(cqe.good_cqe, tavorprm_completion_queue_entry_st, opcode);
	if (opcode >= CQE_ERROR_OPCODE) {
		struct ud_send_wqe_st *wqe_p, wqe;
		__u32 *ptr;
		unsigned int i;

		wqe_p =
		    bus_to_virt(EX_FLD
				(cqe.error_cqe,
				 tavorprm_completion_with_error_st,
				 wqe_addr) << 6);
		eprintf("syndrome=0x%lx",
			EX_FLD(cqe.error_cqe, tavorprm_completion_with_error_st,
			       syndrome));
		eprintf("wqe_addr=0x%lx", wqe_p);
		eprintf("wqe_size=0x%lx",
			EX_FLD(cqe.error_cqe, tavorprm_completion_with_error_st,
			       wqe_size));
		eprintf("myqpn=0x%lx",
			EX_FLD(cqe.error_cqe, tavorprm_completion_with_error_st,
			       myqpn));
		eprintf("db_cnt=0x%lx",
			EX_FLD(cqe.error_cqe, tavorprm_completion_with_error_st,
			       db_cnt));
		memcpy(&wqe, wqe_p, sizeof wqe);
		be_to_cpu_buf(&wqe, sizeof wqe);

		eprintf("dumping wqe...");
		ptr = (__u32 *) (&wqe);
		for (i = 0; i < sizeof wqe; i += 4) {
			printf("%lx : ", ptr[i >> 2]);
		}

	}

	return rc;
}

/* always work on ipoib qp */
static int add_qp_to_mcast_group(union ib_gid_u mcast_gid, __u8 add)
{
	void *mg;
	__u8 *tmp;
	int rc;
	__u16 mgid_hash;
	void *mgmqp_p;

	tmp = dev_buffers_p->inprm_buf;
	memcpy(tmp, mcast_gid.raw, 16);
	be_to_cpu_buf(tmp, 16);
	rc = cmd_mgid_hash(tmp, &mgid_hash);
	if (!rc) {
		mg = (void *)dev_buffers_p->inprm_buf;
		memset(mg, 0, MT_STRUCT_SIZE(tavorprm_mgm_entry_st));
		INS_FLD(mcast_gid.as_u32.dw[0], mg, tavorprm_mgm_entry_st, mgid_128_96);	// memcpy(&mg->mgid_128_96, &mcast_gid.raw[0], 4);
		INS_FLD(mcast_gid.as_u32.dw[1], mg, tavorprm_mgm_entry_st, mgid_95_64);	// memcpy(&mg->mgid_95_64, &mcast_gid.raw[4], 4);
		INS_FLD(mcast_gid.as_u32.dw[2], mg, tavorprm_mgm_entry_st, mgid_63_32);	//memcpy(&mg->mgid_63_32, &mcast_gid.raw[8], 4);
		INS_FLD(mcast_gid.as_u32.dw[3], mg, tavorprm_mgm_entry_st, mgid_31_0);	//memcpy(&mg->mgid_31_0, &mcast_gid.raw[12], 4);
		be_to_cpu_buf(mg + MT_BYTE_OFFSET(tavorprm_mgm_entry_st, mgid_128_96), 16);	//be_to_cpu_buf(&mg->mgid_128_96, 16);
		mgmqp_p = mg + MT_BYTE_OFFSET(tavorprm_mgm_entry_st, mgmqp_0);
		INS_FLD(dev_ib_data.ipoib_qp.qpn, mgmqp_p, tavorprm_mgmqp_st, qpn_i);	//mg->mgmqp[0].qpn = dev_ib_data.ipoib_qp.qpn;
		INS_FLD(add, mgmqp_p, tavorprm_mgmqp_st, qi);	//mg->mgmqp[0].valid = add ? 1 : 0;
		rc = cmd_write_mgm(mg, mgid_hash);
	}
	return rc;
}

static int clear_interrupt(void)
{
	__u32 ecr;
	int ret = 0;

	if (gw_read_cr(0x80704, &ecr)) {
		eprintf("");
	} else {
		if (ecr) {
			ret = 1;
		}
	}
	gw_write_cr(0xf00d8, 0x80000000);	/* clear int */
	gw_write_cr(0x8070c, 0xffffffff);

	return ret;
}

static struct ud_send_wqe_st *alloc_send_wqe(udqp_t qph)
{
	struct udqp_st *qp = qph;
	__u8 new_entry;
	struct ud_send_wqe_st *wqe;

	if (qp->snd_wqe_cur_free == 0) {
		return NULL;
	}
	new_entry = qp->snd_wqe_alloc_idx;

	wqe = &qp->snd_wq[new_entry].wqe;
	qp->snd_wqe_cur_free--;
	qp->snd_wqe_alloc_idx = (qp->snd_wqe_alloc_idx + 1) % qp->max_snd_wqes;

	memset(wqe, 0, sizeof *wqe);

	wqe->mpointer[0].local_addr_l = virt_to_bus(qp->snd_bufs[new_entry]);

	return wqe;
}

/*
 *  alloc_rcv_wqe
 *
 *  Note: since we work directly on the work queue, wqes
 *        are left in big endian
 */
static struct recv_wqe_st *alloc_rcv_wqe(struct udqp_st *qp)
{
	__u8 new_entry;
	struct recv_wqe_st *wqe;

	if (qp->recv_wqe_cur_free == 0) {
		return NULL;
	}

	new_entry = qp->recv_wqe_alloc_idx;
	wqe = &qp->rcv_wq[new_entry].wqe;

	qp->recv_wqe_cur_free--;
	qp->recv_wqe_alloc_idx =
	    (qp->recv_wqe_alloc_idx + 1) % qp->max_recv_wqes;

	memset(wqe, 0, sizeof *wqe);

	/* GRH is always required */
	wqe->mpointer[0].local_addr_h = 0;
	wqe->mpointer[0].local_addr_l = virt_to_bus(qp->rcv_bufs[new_entry]);
	wqe->mpointer[0].lkey = dev_ib_data.mkey;
	wqe->mpointer[0].byte_count = GRH_SIZE;

	wqe->mpointer[1].local_addr_h = 0;
	wqe->mpointer[1].local_addr_l =
	    virt_to_bus(qp->rcv_bufs[new_entry] + GRH_SIZE);
	wqe->mpointer[1].lkey = dev_ib_data.mkey;
	wqe->mpointer[1].byte_count = qp->rcv_buf_sz;

	tprintf("rcv_buf=%lx\n", qp->rcv_bufs[new_entry]);

	/* we do it only on the data segment since the control
	   segment is always owned by HW */
	cpu_to_be_buf(wqe, sizeof *wqe);

//      tprintf("alloc wqe= 0x%x", wqe);
	return wqe;
}

static int free_send_wqe(struct ud_send_wqe_st *wqe)
{
	union ud_send_wqe_u *wqe_u;
	struct udqp_st *qp;

	wqe_u = (union ud_send_wqe_u *)wqe;
	qp = wqe_u->wqe_cont.qp;

	if (qp->snd_wqe_cur_free >= qp->max_snd_wqes) {
		return -1;
	}

	qp->snd_wqe_cur_free++;

	return 0;
}

static int free_rcv_wqe(struct recv_wqe_st *wqe)
{
	union recv_wqe_u *wqe_u;
	struct udqp_st *qp;

	wqe_u = (union recv_wqe_u *)wqe;
	qp = wqe_u->wqe_cont.qp;

	if (qp->recv_wqe_cur_free >= qp->max_recv_wqes) {
		return -1;
	}

	qp->recv_wqe_cur_free++;

	return 0;
}

static int free_wqe(void *wqe)
{
	int rc = 0;
	struct recv_wqe_st *rcv_wqe;

//      tprintf("free wqe= 0x%x", wqe);
	if ((wqe >= (void *)(dev_ib_data.ipoib_qp.rcv_wq)) &&
	    (wqe <
	     (void *)(&dev_ib_data.ipoib_qp.rcv_wq[NUM_IPOIB_RCV_WQES]))) {
		/* ipoib receive wqe */
		free_rcv_wqe(wqe);
		rcv_wqe = alloc_rcv_wqe(&dev_ib_data.ipoib_qp);
		if (rcv_wqe) {
			rc = post_rcv_buf(&dev_ib_data.ipoib_qp, rcv_wqe);
			if (rc) {
				eprintf("");
			}
		}
	} else if (wqe >= (void *)(dev_ib_data.ipoib_qp.snd_wq) &&
		   wqe <
		   (void *)(&dev_ib_data.ipoib_qp.snd_wq[NUM_IPOIB_SND_WQES])) {
		/* ipoib send wqe */
		free_send_wqe(wqe);
	} else if (wqe >= (void *)(dev_ib_data.mads_qp.rcv_wq) &&
		   wqe <
		   (void *)(&dev_ib_data.mads_qp.rcv_wq[NUM_MADS_RCV_WQES])) {
		/* mads receive wqe */
		free_rcv_wqe(wqe);
		rcv_wqe = alloc_rcv_wqe(&dev_ib_data.mads_qp);
		if (rcv_wqe) {
			rc = post_rcv_buf(&dev_ib_data.mads_qp, rcv_wqe);
			if (rc) {
				eprintf("");
			}
		}
	} else if (wqe >= (void *)(dev_ib_data.mads_qp.snd_wq) &&
		   wqe <
		   (void *)(&dev_ib_data.mads_qp.snd_wq[NUM_MADS_SND_WQES])) {
		/* mads send wqe */
		free_send_wqe(wqe);
	} else {
		rc = -1;
		eprintf("");
	}

	return rc;
}

static int update_eq_cons_idx(struct eq_st *eq)
{
	struct eq_dbell_st dbell;
	int rc;

	memset(&dbell, 0, sizeof dbell);
	INS_FLD(dev_ib_data.eq.eqn, &dbell, tavorprm_eq_cmd_doorbell_st, eqn);
	INS_FLD(EQ_DBELL_CMD_SET_CONS_IDX, &dbell, tavorprm_eq_cmd_doorbell_st,
		eq_cmd);
	INS_FLD(eq->cons_idx, &dbell, tavorprm_eq_cmd_doorbell_st, eq_param);
	rc = cmd_post_doorbell(&dbell, EQ_DBELL_OFFSET);

	return rc;
}

static void dev2ib_eqe(struct ib_eqe_st *ib_eqe_p, void *eqe_p)
{
	void *tmp;

	ib_eqe_p->event_type =
	    EX_FLD(eqe_p, tavorprm_event_queue_entry_st, event_type);

	tmp = eqe_p + MT_BYTE_OFFSET(tavorprm_event_queue_entry_st, event_data);
	ib_eqe_p->cqn = EX_FLD(tmp, tavorprm_completion_event_st, cqn);
}

static int poll_eq(struct ib_eqe_st *ib_eqe_p, __u8 * num_eqes)
{
	struct eqe_t eqe;
	__u8 owner;
	int rc;
	__u32 *ptr;
	struct eq_st *eq = &dev_ib_data.eq;

	ptr = (__u32 *) (&(eq->eq_buf[eq->cons_idx]));
	tprintf("cons)idx=%d, addr(eqe)=%x, val=0x%x", eq->cons_idx, virt_to_bus(ptr), ptr[7]);
	owner = (ptr[7] & 0x80000000) ? OWNER_HW : OWNER_SW;
	if (owner == OWNER_SW) {
        tprintf("got eqe");
		eqe = eq->eq_buf[eq->cons_idx];
		be_to_cpu_buf(&eqe, sizeof(eqe));
		dev2ib_eqe(ib_eqe_p, &eqe);
		ptr[7] |= 0x80000000;
		eq->eq_buf[eq->cons_idx] = eqe;
		eq->cons_idx = (eq->cons_idx + 1) % eq->eq_size;
		rc = update_eq_cons_idx(eq);
		if (rc) {
			return -1;
		}
		*num_eqes = 1;
	} else {
		*num_eqes = 0;
	}
	return 0;
}

static int ib_device_close(void)
{
	iounmap(tavor_pci_dev.uar);
	iounmap(tavor_pci_dev.cr_space);
	iounmap(dev_ib_data.error_buf_addr);
	return 0;
}

static __u32 dev_get_qpn(void *qph)
{
	struct udqp_st *qp = qph;

	return qp->qpn;
}

static void dev_post_dbell(void *dbell, __u32 offset)
{
	__u32 *ptr;
	unsigned long address;

	ptr = dbell;
	tprintf("ptr[0]= 0x%lx", ptr[0]);
	tprintf("ptr[1]= 0x%lx", ptr[1]);
	address = (unsigned long)(tavor_pci_dev.uar) + offset;
	tprintf("va=0x%lx pa=0x%lx", address,
		virt_to_bus((const void *)address));
	writel(htonl(ptr[0]), tavor_pci_dev.uar + offset);
	barrier();
	address += 4;
	tprintf("va=0x%lx pa=0x%lx", address,
		virt_to_bus((const void *)address));
	writel(htonl(ptr[1]), tavor_pci_dev.uar + offset + 4);
}


