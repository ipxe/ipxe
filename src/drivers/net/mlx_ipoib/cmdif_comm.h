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

#ifndef __cmdif_comm_h__
#define __cmdif_comm_h__

  /* initialization and general commands */
#define XDEV_CMD_INIT_HCA			0x7
#define XDEV_CMD_CLOSE_HCA			0x8
#define XDEV_CMD_INIT_IB				0x9
#define XDEV_CMD_CLOSE_IB			0xa

  /* TPT commands */
#define XDEV_CMD_SW2HW_MPT			0xd
#define	XDEV_CMD_HW2SW_MPT			0xf

  /* EQ commands */
#define XDEV_CMD_MAP_EQ				0x12
#define XDEV_CMD_SW2HW_EQ			0x13
#define XDEV_CMD_HW2SW_EQ			0x14

  /* CQ commands */
#define XDEV_CMD_SW2HW_CQ			0x16
#define	XDEV_CMD_HW2SW_CQ			0x17

  /* QP/EE commands */
#define	XDEV_CMD_RST2INIT_QPEE		0x19
#define XDEV_CMD_INIT2RTR_QPEE		0x1a
#define XDEV_CMD_RTR2RTS_QPEE		0x1b
#define XDEV_CMD_2ERR_QPEE			0x1e
#define XDEV_CMD_ERR2RST_QPEE		0x21

  /* special QPs and management commands */
#define XDEV_CMD_MAD_IFC				0x24

  /* multicast commands */
#define XDEV_CMD_READ_MGM			0x25
#define XDEV_CMD_MGID_HASH			0x27

#define XDEV_CMD_POST_DOORBELL		0x41

#endif				/* __cmdif_comm_h__ */
