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

#include "ib_driver.h"

static const __u8 ipv4_bcast_gid[] = {
	0xff, 0x12, 0x40, 0x1b, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};

static int wait_logic_link_up(__u8 port)
{
	unsigned int relax_time, max_time;
	relax_time = 500;
	max_time = 30000;	/* 30 seconds */
	int rc;
	unsigned int i, error = 1;
	__u16 status;
	struct port_info_st pi_var;
	__u8 port_state;

	for (i = 0; i < max_time; i += relax_time) {
		rc = get_port_info(port, &pi_var, &status);
		if (rc) {
			eprintf("");
			return rc;
		} else {
			if (status == 0) {
				port_state = (pi_var.combined4 >> 24) & 0xf;
				//port_state= pi_var.port_state;
				if (port_state == 4) {
					error = 0;
					break;
				}
			}
		}
		printf("+");
		mdelay(relax_time);
	}

	if (i >= max_time)
		return -1;

	return 0;
}

static int ib_driver_init(struct pci_device *pci, udqp_t * ipoib_qph_p)
{
	int rc;
	__u8 port;
	__u16 status;
	__u32 qkey;
	__u16 mlid;
	ud_av_t av;
	struct ib_eqe_st ib_eqe;
	__u8 num_eqe;

	tprintf("");
	rc = ib_device_init(pci);
	if (rc)
		return rc;

	tprintf("");

	memcpy(ib_data.bcast_gid.raw, ipv4_bcast_gid, sizeof(ipv4_bcast_gid));

	port = PXE_IB_PORT;
	rc = setup_hca(port, &ib_data.eq);
	if (rc)
		return rc;
	tprintf("setup_hca() success");

	ib_data.port = port;

	if(print_info)
		printf("boot port = %d\n", ib_data.port);

	rc = wait_logic_link_up(port);
	if (rc)
		return rc;

	tprintf("wait_logic_link_up() success");

	rc = get_guid_info(&status);
	if (rc) {
		eprintf("");
		return rc;
	} else if (status) {
		eprintf("");
		return rc;
	}

	tprintf("get_guid_info() success");

	/* this to flush stdout that contains previous chars */
	printf("    \n");
	if(print_info) {
		__u8 *gid=ib_data.port_gid.raw;

		printf("\n");
		printf("port GID=%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:"
		       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n",
		       gid[0],gid[1],gid[2],gid[3],gid[4],gid[5],gid[6],gid[7],
		       gid[8],gid[9],gid[10],gid[11],gid[12],gid[13],gid[14],gid[15]);
	}

	rc = get_pkey_tbl(NULL, &status);
	if (rc) {
		eprintf("");
		return rc;
	} else if (status) {
		eprintf("");
		return rc;
	}
	rc = create_mads_qp(&ib_data.mads_qp,
			    &ib_data.mads_snd_cq, &ib_data.mads_rcv_cq);
	if (rc) {
		eprintf("");
		return rc;
	}

	tprintf("attempt to join mcast group ...");
	rc = join_mc_group(&qkey, &mlid, 1);
	if (rc) {
		eprintf("");
		return rc;
	} else {
		tprintf("join_mc_group() successfull qkey=0x%lx, mlid=0x%x",
			qkey, mlid);
	}

	rc = create_ipoib_qp(&ib_data.ipoib_qp,
			     &ib_data.ipoib_snd_cq,
			     &ib_data.ipoib_rcv_cq, qkey);
	if (rc) {
		eprintf("");
		return rc;
	}

	tprintf("create_ipoib_qp() success");
	*ipoib_qph_p = ib_data.ipoib_qp;

	tprintf("register qp to receive mcast...");
	rc = add_qp_to_mcast_group(ib_data.bcast_gid, 1);
	if (rc) {
		eprintf("");
		return rc;
	} else {
		tprintf("add_qp_to_mcast_group() success");
	}

	/* create a broadcast group ud AV */
	av = alloc_ud_av();
	if (!av) {
		eprintf("");
		return -1;
	}
	tprintf("alloc_ud_av() success");
	modify_av_params(av, mlid, 1, 0, 0, &ib_data.bcast_gid, BCAST_QPN);
	tprintf("modify_av_params() success");
	ib_data.bcast_av = av;

	do {
		rc = poll_eq(&ib_eqe, &num_eqe);
		if (rc) {
			eprintf("");
			return -1;
		}
		if (num_eqe) {
			tprintf("num_eqe=%d", num_eqe);
		}
		tprintf("num_eqe=%d", num_eqe);
	} while (num_eqe);
	tprintf("eq is drained");

	clear_interrupt();

	return rc;
}

static int ib_driver_close(int fw_fatal)
{
	int rc, ret = 0;
	__u32 qkey;
	__u16 mlid;

	rc = ib_device_close();
	if (rc) {
		eprintf("ib_device_close() failed");
		ret = 1;
	}

	tprintf("");
	if (!fw_fatal) {
		rc = join_mc_group(&qkey, &mlid, 0);
		if (rc) {
			eprintf("");
			ret = 1;
		}
		tprintf("join_mc_group(leave) success");

		rc = add_qp_to_mcast_group(ib_data.bcast_gid, 0);
		if (rc) {
			eprintf("");
			ret = 1;
		}
		tprintf("add_qp_to_mcast_group(remove) success");

		rc = cmd_close_ib(ib_data.port);
		if (rc) {
			eprintf("");
			ret = 1;
		}
		tprintf("cmd_close_ib(%d) success", ib_data.port);

		if (destroy_udqp(ib_data.mads_qp)) {
			eprintf("");
			ret = 1;
		}

		if (destroy_udqp(ib_data.ipoib_qp)) {
			eprintf("");
			ret = 1;
		}
	}

	rc = cmd_close_hca(fw_fatal);
	if (rc) {
		eprintf("");
		ret = 1;
	}

	rc = unset_hca();
	if (rc) {
		eprintf("");
		ret = 1;
	}

	return ret;
}

static int poll_cqe_tout(cq_t cqh, __u16 tout, void **wqe, int *good_p)
{
	int rc;
	struct ib_cqe_st ib_cqe;
	__u8 num_cqes;
	unsigned long end;

	end = currticks() + tout;
	do {
		rc = ib_poll_cq(cqh, &ib_cqe, &num_cqes);
		if (rc)
			return rc;

		if (num_cqes == 1) {
			if (good_p) {
				*good_p = ib_cqe.is_error ? 0 : 1;
			}
			if (wqe)
				*wqe = ib_cqe.wqe;
			return 0;
		}
	}
	while (currticks() < end);

	return -1;
}

static u8 *get_port_gid(void)
{
	return ib_data.port_gid.raw;
}

static __u32 ib_get_qpn(udqp_t qph)
{
	__u32 qpn;

	qpn = dev_get_qpn(qph);

	return qpn;
}

static int drain_eq(void)
{
	__u8 num_eqe = 0, tot_eqe = 0;
	int rc;

	do {
		tot_eqe += num_eqe;
		rc = poll_eq(ib_data.eq, &num_eqe);
		if (rc) {
			eprintf("");
			return -1;
		}

		tprintf("num_eqe=%d", num_eqe);
	} while (num_eqe);
	tprintf("eq is drained");
	if (tot_eqe) {
		tprintf("got %d eqes", tot_eqe);
		return -1;
	}

	return 0;
}


static int poll_error_buf(void)
{
	__u32 *ptr= dev_ib_data.error_buf_addr;
	__u32 i;

	for (i=0; i<dev_ib_data.error_buf_size; ++i, ptr++) {
		if ( readl(ptr) ) {
			return -1;
		}
	}

	return 0;
}


