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

#include "ib_mad.h"
#include "mad_attrib.h"
#include "cmdif.h"
#include "ib_driver.h"

#define TID_START 0x1234
#define TID_INC 117

static u32 next_tid = TID_START;

/*
 *  get_port_info
 *
 *	query the local device for the portinfo attribute
 *
 *	port(in) port number to query
 *  buf(out) buffer to hold the result
 */
static int get_port_info(__u8 port, struct port_info_st *buf, __u16 * status)
{
	union port_info_mad_u *inprm;
	union port_info_mad_u *outprm;
	int rc;

	inprm = get_inprm_buf();
	outprm = get_outprm_buf();
	memset(inprm, 0, sizeof *inprm);

	inprm->mad.mad_hdr.method = IB_MGMT_METHOD_GET;
	inprm->mad.mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	inprm->mad.mad_hdr.class_version = 1;
	inprm->mad.mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	inprm->mad.mad_hdr.attr_id = IB_SMP_ATTR_PORT_INFO;
	inprm->mad.mad_hdr.attr_mod = port;

	rc = cmd_mad_ifc(inprm, (struct ib_mad_st *)outprm, port);
	if (!rc) {
		memcpy(buf, &outprm->mad.port_info,
		       sizeof(outprm->mad.port_info));
		*status = inprm->mad.mad_hdr.status;
		if (!(*status)) {
			ib_data.sm_lid = outprm->mad.port_info.mastersm_lid;
			memcpy(&ib_data.port_gid.raw[0],
			       outprm->mad.port_info.gid_prefix, 8);
			cpu_to_be_buf(&ib_data.port_gid.raw[0], 8);
		}
	}
	return rc;
}

/*
 *  get_guid_info
 *
 *	query the local device for the guidinfo attribute
 *
 *  buf(out) buffer to hold the result
 */
static int get_guid_info(__u16 * status)
{
	union guid_info_mad_u *inprm;
	union guid_info_mad_u *outprm;
	int rc;

	inprm = get_inprm_buf();
	outprm = get_outprm_buf();
	memset(inprm, 0, sizeof *inprm);

	inprm->mad.mad_hdr.method = IB_MGMT_METHOD_GET;
	inprm->mad.mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	inprm->mad.mad_hdr.class_version = 1;
	inprm->mad.mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	inprm->mad.mad_hdr.attr_id = IB_SMP_ATTR_GUID_INFO;
	inprm->mad.mad_hdr.attr_mod = 0;

	rc = cmd_mad_ifc(inprm, (struct ib_mad_st *)outprm, ib_data.port);
	if (!rc) {
		*status = inprm->mad.mad_hdr.status;
		if (!(*status)) {
			memcpy(&ib_data.port_gid.raw[8],
			       &outprm->mad.guid_info.gid_tbl[0], 8);
			cpu_to_be_buf(&ib_data.port_gid.raw[8], 8);
		}
	}
	return rc;
}

static int get_pkey_tbl(struct pkey_tbl_st *pkey_tbl, __u16 * status)
{
	union pkey_tbl_mad_u *inprm;
	union pkey_tbl_mad_u *outprm;
	int rc;

	inprm = get_inprm_buf();
	outprm = get_outprm_buf();
	memset(inprm, 0, sizeof *inprm);
	memset(outprm, 0, sizeof *outprm);

	inprm->mad.mad_hdr.method = IB_MGMT_METHOD_GET;
	inprm->mad.mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	inprm->mad.mad_hdr.class_version = 1;
	inprm->mad.mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	inprm->mad.mad_hdr.attr_id = IB_SMP_ATTR_PKEY_TABLE;
	inprm->mad.mad_hdr.attr_mod = 0;

	rc = cmd_mad_ifc(inprm, (struct ib_mad_st *)outprm, 1);
	if (!rc) {
		if (pkey_tbl)
			memcpy(pkey_tbl, &outprm->mad.pkey_tbl, 2);
		*status = inprm->mad.mad_hdr.status;
		if (!(*status)) {
			ib_data.pkey = outprm->mad.pkey_tbl.pkey_tbl[0][1];
			ib_data.bcast_gid.raw[4] =
			    outprm->mad.pkey_tbl.pkey_tbl[0][1] >> 8;
			ib_data.bcast_gid.raw[5] =
			    outprm->mad.pkey_tbl.pkey_tbl[0][1] & 0xff;
		}
	}
	return rc;
}

static int join_mc_group(__u32 * qkey_p, __u16 * mlid_p, __u8 join)
{
	struct mc_member_mad_st *mad, *rcv_mad;
	void *snd_wqe;
	void *tmp_wqe;
	udqp_t qp;
	void *av;
	int rc;
	u32 tid;
	void *rcv_wqe;
	int is_good;

	qp = ib_data.mads_qp;

	snd_wqe = alloc_send_wqe(qp);
	if (!snd_wqe) {
		eprintf("");
		return -1;
	}
	tprintf("allocated snd_wqe=0x%lx", snd_wqe);

	mad = get_send_wqe_buf(snd_wqe, 0);
	memset(mad, 0, 256);

	av = alloc_ud_av();
	if (!av) {
		eprintf("");
		free_wqe(snd_wqe);
		return -1;
	}
	modify_av_params(av, ib_data.sm_lid, 0, 0, 0, NULL, SA_QPN);

	prep_send_wqe_buf(qp, av, snd_wqe, NULL, 0, 256, 0);

	mad->mad_hdr.method = join ? IB_MGMT_METHOD_SET : IB_MGMT_METHOD_DELETE;
	mad->mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_ADM;
	mad->mad_hdr.class_version = 2;
	mad->mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	mad->mad_hdr.attr_id = IB_SA_ATTR_MC_MEMBER_REC;
	tid = next_tid;
	next_tid += TID_INC;
	mad->mad_hdr.tid[1] = tid;

	mad->sa_hdr.comp_mask[1] = IB_SA_MCMEMBER_REC_MGID |
	    IB_SA_MCMEMBER_REC_PORT_GID | IB_SA_MCMEMBER_REC_JOIN_STATE;

	mad->mc_member.combined4 |= (1 << 24);	/*mad->mc_member.join_state = 1; */

	be_to_cpu_buf(mad, sizeof *mad);
	memcpy(mad->mc_member.mgid, ib_data.bcast_gid.raw, 16);
	memcpy(mad->mc_member.port_gid, ib_data.port_gid.raw, 16);

	rc = post_send_req(qp, snd_wqe, 1);
	if (rc) {
		eprintf("");
		free_ud_av(av);
		free_wqe(snd_wqe);
		return -1;
	}

	tprintf("");
	/* poll the CQ to get the completions
	   on the send and the expected receive */

	/* send completion */
	rc = poll_cqe_tout(ib_data.mads_snd_cq, SEND_CQE_POLL_TOUT, &tmp_wqe,
			   &is_good);
	if (rc) {
		eprintf("");
		return -1;
	}

	if (tmp_wqe != snd_wqe) {
		eprintf("");
		return -1;
	}

	if (free_wqe(snd_wqe)) {
		eprintf("");
		return -1;
	}
	free_ud_av(av);

	if (!is_good) {
		eprintf("");
		return -1;
	}

	/* receive completion */
	rc = poll_cqe_tout(ib_data.mads_rcv_cq, SA_RESP_POLL_TOUT, &rcv_wqe,
			   &is_good);
	if (rc) {
		eprintf("");
		return -1;
	}

	if (is_good) {
		rcv_mad = get_rcv_wqe_buf(rcv_wqe, 1);
		be_to_cpu_buf(rcv_mad, sizeof *rcv_mad);
		if (rcv_mad->mad_hdr.tid[1] == tid) {
			/* that's our response */
			if (mad->mad_hdr.status == 0) {
				/* good response - save results */
				*qkey_p = rcv_mad->mc_member.q_key;
				*mlid_p = rcv_mad->mc_member.combined1 >> 16;	// rcv_mad->mc_member.mlid;
			} else {
				/* join failed */
				eprintf("");
				return -1;
			}
		} else {
			/* not our response */
			eprintf("");
			return -1;
		}
	}

	if (free_wqe(rcv_wqe)) {
		eprintf("");
		return -1;
	}

	return is_good ? 0 : -1;
}

static int get_path_record(union ib_gid_u *dgid, __u16 * dlid_p, u8 * sl_p,
			   u8 * rate_p)
{
	struct path_record_mad_st *mad, *rcv_mad;
	void *snd_wqe;
	udqp_t qp;
	ud_av_t av;
	void *tmp_wqe;
	void *rcv_wqe;
	u32 tid;
	int rc;
	int is_good;

	tprintf("gid=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
		"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		dgid->raw[0], dgid->raw[1], dgid->raw[2], dgid->raw[3],
		dgid->raw[4], dgid->raw[5], dgid->raw[6], dgid->raw[7],
		dgid->raw[8], dgid->raw[9], dgid->raw[10], dgid->raw[11],
		dgid->raw[12], dgid->raw[13], dgid->raw[14], dgid->raw[15]);
	qp = ib_data.mads_qp;

	snd_wqe = alloc_send_wqe(qp);
	if (!snd_wqe) {
		eprintf("");
		return -1;
	}

	mad = get_send_wqe_buf(snd_wqe, 0);
	memset(mad, 0, 256);

	av = alloc_ud_av();
	if (!av) {
		eprintf("");
		free_wqe(snd_wqe);
		return -1;
	}
	modify_av_params(av, ib_data.sm_lid, 0, 0, 0, NULL, SA_QPN);

	prep_send_wqe_buf(qp, av, snd_wqe, NULL, 0, 256, 0);

	mad->mad_hdr.method = IB_MGMT_METHOD_GET;
	mad->mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_ADM;
	mad->mad_hdr.class_version = 2;
	mad->mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	mad->mad_hdr.attr_id = IB_SA_ATTR_PATH_REC;
	tid = next_tid;
	next_tid += TID_INC;
	mad->mad_hdr.tid[1] = tid;

	memcpy(mad->path_record.dgid.raw, dgid->raw, 16);
	cpu_to_be_buf(mad->path_record.dgid.raw, 16);

	mad->sa_hdr.comp_mask[1] = IB_SA_PATH_REC_DGID | IB_SA_PATH_REC_SGID;

	cpu_to_be_buf(mad, sizeof *mad);
	memcpy(mad->path_record.sgid.raw, ib_data.port_gid.raw, 16);

	rc = post_send_req(qp, snd_wqe, 1);
	if (rc) {
		eprintf("");
		free_ud_av(av);
		free_wqe(snd_wqe);
		return rc;
	}

	/* poll the CQ to get the completions
	   on the send and the expected receive */

	/* send completion */
	rc = poll_cqe_tout(ib_data.mads_snd_cq, SEND_CQE_POLL_TOUT, &tmp_wqe,
			   &is_good);
	if (rc) {
		eprintf("");
		return -1;
	}

	if (tmp_wqe != snd_wqe) {
		eprintf("");
		return -1;
	}

	if (free_wqe(snd_wqe)) {
		eprintf("");
		return -1;
	}
	free_ud_av(av);

	if (!is_good) {
		eprintf("");
		return -1;
	}

	/* receive completion */
	rc = poll_cqe_tout(ib_data.mads_rcv_cq, SA_RESP_POLL_TOUT, &rcv_wqe,
			   &is_good);
	if (rc) {
		eprintf("");
		return -1;
	}

	if (is_good) {
		rcv_mad = get_rcv_wqe_buf(rcv_wqe, 1);
		be_to_cpu_buf(rcv_mad, sizeof *rcv_mad);
		if (rcv_mad->mad_hdr.tid[1] == tid) {
			/* that's our response */
			if (mad->mad_hdr.status == 0) {
				/* good response - save results */
				*dlid_p = rcv_mad->path_record.dlid;
				*sl_p = (rcv_mad->path_record.combined3 >> 16) & 0xf;	//  rcv_mad->path_record.sl;
				*rate_p = rcv_mad->path_record.combined3 & 0x3f;	//rcv_mad->path_record.rate;
			} else {
				/* join failed */
				eprintf("");
				return -1;
			}
		} else {
			/* not our response */
			eprintf("");
			return -1;
		}
	}

	if (free_wqe(rcv_wqe)) {
		eprintf("");
		return -1;
	}

	tprintf("");
	return is_good ? 0 : -1;
}
