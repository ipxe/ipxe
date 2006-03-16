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

#include "cmdif.h"
#include "cmdif_comm.h"
#include "cmdif_priv.h"

static int cmdif_is_free(int *is_free)
{
	int rc;
	__u32 result;

	rc = gw_read_cr(HCR_OFFSET_GO, &result);
	if (rc) {
		eprintf("");
		return rc;
	}
	*is_free = (result & 0x800000) == 0;

	return 0;
}

static void edit_hcr(command_fields_t * cmd_prms, __u32 * buf)
{
	unsigned int i;

	switch (cmd_prms->in_trans) {
	case TRANS_NA:
		/* note! since these are zeroes I do not bother to deal with endianess */
		buf[0] = 0;
		buf[1] = 0;
		break;

	case TRANS_IMMEDIATE:
		buf[0] = cmd_prms->in_param[0];
		buf[1] = cmd_prms->in_param[1];
		break;

	case TRANS_MAILBOX:
		buf[0] = 0;
		buf[1] = virt_to_bus(cmd_prms->in_param);

		for (i = 0; i < cmd_prms->in_param_size; i += 4)
			cmd_prms->in_param[i >> 2] =
			    cpu_to_be32(cmd_prms->in_param[i >> 2]);
		break;
	}

	buf[2] = cmd_prms->input_modifier;

	switch (cmd_prms->out_trans) {
	case TRANS_NA:
		/* note! since these are zeroes I do not bother to deal with endianess */
		buf[3] = 0;
		buf[4] = 0;
		break;

	case TRANS_IMMEDIATE:
		break;
	case TRANS_MAILBOX:
		buf[3] = 0;
		buf[4] = virt_to_bus(cmd_prms->out_param);
		break;
	}

	buf[5] = 0;		/* token is always 0 */
	buf[6] = cmd_prms->opcode |	/* opcode */
	    0x800000 |		/* go bit */
	    ((cmd_prms->opcode_modifier & 0xf) << 12);	/* opcode modifier 
*/ }

static int wait_cmdif_free(void)
{
	int ret, is_free;
	unsigned int i, relax_time = 1, max_time = 5000;

	/* wait until go bit is free */
	for (i = 0; i < max_time; i += relax_time) {
		ret = cmdif_is_free(&is_free);
		if (ret)
			return ret;
		if (is_free)
			break;
		mdelay(relax_time);
	}
	if (i >= max_time)
		return -1;
	return 0;
}

static XHH_cmd_status_t cmd_invoke(command_fields_t * cmd_prms)
{
	int ret, is_free, i;
	__u32 hcr[7], data;
	__u8 status;

	/* check if go bit is free */
	ret = cmdif_is_free(&is_free);
	if (ret) {
		eprintf("");
		return -1;
	}

	__asm__ __volatile__("":::"memory");
	/* it must be free */
	if (!is_free) {
		eprintf("");
		return -1;
	}
	__asm__ __volatile__("":::"memory");
	edit_hcr(cmd_prms, hcr);
	__asm__ __volatile__("":::"memory");

	for (i = 0; i < 7; ++i) {
		ret = gw_write_cr(HCR_BASE + i * 4, hcr[i]);
		if (ret) {
			eprintf("");
			return -1;
		}
	}

	__asm__ __volatile__("":::"memory");
	/* wait for completion */
	ret = wait_cmdif_free();
	if (ret) {
		eprintf("");
		return -1;
	}

	__asm__ __volatile__("":::"memory");
	ret = gw_read_cr(HCR_OFFSET_STATUS, &data);
	if (ret) {
		eprintf("");
		return -1;
	}

	status = data >> 24;

	if (status) {
		tprintf("status=0x%x", status);
		return status;
	}

	if (cmd_prms->out_trans == TRANS_MAILBOX)
		be_to_cpu_buf(cmd_prms->out_param, cmd_prms->out_param_size);
	else if (cmd_prms->out_trans == TRANS_IMMEDIATE) {
		if (gw_read_cr(HCR_OFFSET_OUTPRM_H, &cmd_prms->out_param[0]))
			return -1;
		if (gw_read_cr(HCR_OFFSET_OUTPRM_L, &cmd_prms->out_param[1]))
			return -1;
	}

	return 0;
}

/*************************************************
					commands
*************************************************/

/*
 *  cmd_close_hca
 */
static int cmd_close_hca(int panic)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_CLOSE_HCA;
	cmd_desc.opcode_modifier= panic;
	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_init_hca
 */
static int cmd_init_hca(__u32 * inprm, __u32 in_prm_size)
{
	int rc;

	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.opcode = XDEV_CMD_INIT_HCA;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = in_prm_size;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_sw2hw_eq
 */
static int cmd_sw2hw_eq(__u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;
	void *inprm;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	inprm = get_inprm_buf();
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.opcode = XDEV_CMD_SW2HW_EQ;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_hw2sw_eq
 */
static int cmd_hw2sw_eq(__u8 eqn)
{
	int rc;
	command_fields_t cmd_desc;
	void *outprm;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	outprm = get_outprm_buf();
	cmd_desc.opcode = XDEV_CMD_HW2SW_EQ;
	cmd_desc.input_modifier = eqn;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = outprm;
	cmd_desc.out_param_size = 0x40;
	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_map_eq
 */
static int cmd_map_eq(__u8 eqn, __u32 mask, int map)
{
	int rc;
	command_fields_t cmd_desc;
	__u32 *inprm;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	inprm = get_inprm_buf();

	inprm[1] = mask;
	inprm[0] = 0;

	cmd_desc.opcode = XDEV_CMD_MAP_EQ;
	cmd_desc.in_trans = TRANS_IMMEDIATE;
	cmd_desc.in_param = inprm;
	cmd_desc.input_modifier = ((map ? 0 : 1) << 31) | eqn;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_sw2hw_mpt
 */
static int cmd_sw2hw_mpt(__u32 * lkey, __u32 in_key, __u32 * inprm,
			 __u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.opcode = XDEV_CMD_SW2HW_MPT;
	cmd_desc.input_modifier = in_key & MKEY_IDX_MASK;	/* only one MR for the whole driver */
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;

	rc = cmd_invoke(&cmd_desc);
	if (!rc)
		*lkey = in_key;

	return rc;
}

/*
 *  cmd_hw2sw_mpt
 */
static int cmd_hw2sw_mpt(__u32 key)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_HW2SW_MPT;
	cmd_desc.input_modifier = key & MKEY_IDX_MASK;
	cmd_desc.opcode_modifier = 1;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_init_ib
 */
static int cmd_init_ib(__u32 port, __u32 * inprm, __u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_INIT_IB;
	cmd_desc.input_modifier = port;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_close_ib
 */
static int cmd_close_ib(__u32 port)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_CLOSE_IB;
	cmd_desc.input_modifier = port;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_sw2hw_cq
 */
static int cmd_sw2hw_cq(__u32 cqn, __u32 * inprm, __u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_SW2HW_CQ;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;
	cmd_desc.input_modifier = cqn;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_hw2sw_cq
 */
static int cmd_hw2sw_cq(__u32 cqn)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_HW2SW_CQ;
	cmd_desc.input_modifier = cqn;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_rst2init_qpee
 */
static int cmd_rst2init_qpee(__u32 qpn, __u32 * inprm, __u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_RST2INIT_QPEE;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;
	cmd_desc.input_modifier = qpn;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_init2rtr_qpee
 */
static int cmd_init2rtr_qpee(__u32 qpn, __u32 * inprm, __u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_INIT2RTR_QPEE;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;
	cmd_desc.input_modifier = qpn;;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_rtr2rts_qpee
 */
static int cmd_rtr2rts_qpee(__u32 qpn, __u32 * inprm, __u32 inprm_sz)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_RTR2RTS_QPEE;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = inprm;
	cmd_desc.in_param_size = inprm_sz;
	cmd_desc.input_modifier = qpn;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_2rst_qpee
 */
static int cmd_2rst_qpee(__u32 qpn)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_ERR2RST_QPEE;
	cmd_desc.opcode_modifier = 0;
	cmd_desc.input_modifier = qpn;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_2err_qpee
 */
static int cmd_2err_qpee(__u32 qpn)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_2ERR_QPEE;
	cmd_desc.input_modifier = qpn;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_post_doorbell
 */
static int cmd_post_doorbell(void *inprm, __u32 offset)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_POST_DOORBELL;
	cmd_desc.in_trans = TRANS_IMMEDIATE;
	cmd_desc.in_param = inprm;
	cmd_desc.input_modifier = offset;
	if (0) {
		rc = cmd_invoke(&cmd_desc);
	} else {
		dev_post_dbell(inprm, offset);
		rc = 0;
	}

	return rc;
}

static int cmd_mad_ifc(void *inprm, struct ib_mad_st *mad, __u8 port)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = XDEV_CMD_MAD_IFC;
	cmd_desc.opcode_modifier = 1;	/* no mkey/bkey validation */
	cmd_desc.input_modifier = port;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param_size = 256;
	cmd_desc.in_param = (__u32 *) inprm;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = (__u32 *) mad;
	cmd_desc.out_param_size = 256;
	rc = cmd_invoke(&cmd_desc);

	return rc;
}

static int cmd_mgid_hash(__u8 * gid, __u16 * mgid_hash_p)
{
	int rc;
	command_fields_t cmd_desc;
	__u16 result[2];

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = XDEV_CMD_MGID_HASH;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = (__u32 *) gid;
	cmd_desc.in_param_size = 16;
	cmd_desc.out_trans = TRANS_IMMEDIATE;

	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		rc = gw_read_cr(HCR_BASE + 16, (__u32 *) result);
		if (!rc) {
			*mgid_hash_p = result[0];
		}
	}

	return rc;
}
