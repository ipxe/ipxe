#ifndef _GPXE_IB_MAD_H
#define _GPXE_IB_MAD_H

/** @file
 *
 * Infiniband management datagrams
 *
 */

#include <stdint.h>
#include <gpxe/ib_packet.h>

/*****************************************************************************
 *
 * Subnet management MADs
 *
 *****************************************************************************
 */

/** A subnet management header
 *
 * Defined in sections 14.2.1.1 and 14.2.1.2 of the IBA.
 */
struct ib_smp_hdr {
	uint64_t mkey;
	uint16_t slid;
	uint16_t dlid;
	uint8_t reserved[28];
} __attribute__ (( packed ));

/** Subnet management class version */
#define IB_SMP_CLASS_VERSION			1

/** Subnet management direction bit
 *
 * This bit resides in the "status" field in the MAD header.
 */
#define IB_SMP_STATUS_D_INBOUND			0x8000

/* Subnet management attributes */
#define IB_SMP_ATTR_NOTICE			0x0002
#define IB_SMP_ATTR_NODE_DESC			0x0010
#define IB_SMP_ATTR_NODE_INFO			0x0011
#define IB_SMP_ATTR_SWITCH_INFO			0x0012
#define IB_SMP_ATTR_GUID_INFO			0x0014
#define IB_SMP_ATTR_PORT_INFO			0x0015
#define IB_SMP_ATTR_PKEY_TABLE			0x0016
#define IB_SMP_ATTR_SL_TO_VL_TABLE		0x0017
#define IB_SMP_ATTR_VL_ARB_TABLE		0x0018
#define IB_SMP_ATTR_LINEAR_FORWARD_TABLE	0x0019
#define IB_SMP_ATTR_RANDOM_FORWARD_TABLE	0x001A
#define IB_SMP_ATTR_MCAST_FORWARD_TABLE		0x001B
#define IB_SMP_ATTR_SM_INFO			0x0020
#define IB_SMP_ATTR_VENDOR_DIAG			0x0030
#define IB_SMP_ATTR_LED_INFO			0x0031
#define IB_SMP_ATTR_VENDOR_MASK			0xFF00

/**
 * A Node Description attribute
 *
 * Defined in section 14.2.5.2 of the IBA
 */
struct ib_node_desc {
	char node_string[64];
} __attribute__ (( packed ));

/** A Node Information attribute
 *
 * Defined in section 14.2.5.3 of the IBA.
 */
struct ib_node_info {
	uint8_t base_version;
	uint8_t class_version;
	uint8_t node_type;
	uint8_t num_ports;
	uint8_t sys_guid[8];
	uint8_t node_guid[8];
	uint8_t port_guid[8];
	uint16_t partition_cap;
	uint16_t device_id;
	uint32_t revision;
	uint8_t local_port_num;
	uint8_t vendor_id[3];
} __attribute__ ((packed));

#define IB_NODE_TYPE_HCA		0x01
#define IB_NODE_TYPE_SWITCH		0x02
#define IB_NODE_TYPE_ROUTER		0x03

/** A GUID Information attribute
 *
 * Defined in section 14.2.5.5 of the IBA.
 */
struct ib_guid_info {
	uint8_t guid[8][8];
} __attribute__ (( packed ));

/** A Port Information attribute
 *
 * Defined in section 14.2.5.6 of the IBA.
 */
struct ib_port_info {
	uint64_t mkey;
	uint8_t gid_prefix[8];
	uint16_t lid;
	uint16_t mastersm_lid;
	uint32_t cap_mask;
	uint16_t diag_code;
	uint16_t mkey_lease_period;
	uint8_t local_port_num;
	uint8_t link_width_enabled;
	uint8_t link_width_supported;
	uint8_t link_width_active;
	uint8_t link_speed_supported__port_state;
	uint8_t port_phys_state__link_down_def_state;
	uint8_t mkey_prot_bits__lmc;
	uint8_t link_speed_active__link_speed_enabled;
	uint8_t neighbour_mtu__mastersm_sl;
	uint8_t vl_cap__init_type;
	uint8_t vl_high_limit;
	uint8_t vl_arbitration_high_cap;
	uint8_t vl_arbitration_low_cap;
	uint8_t init_type_reply__mtu_cap;
	uint8_t vl_stall_count__hoq_life;
	uint8_t operational_vls__enforcement;
	uint16_t mkey_violations;
	uint16_t pkey_violations;
	uint16_t qkey_violations;
	uint8_t guid_cap;
	uint8_t client_reregister__subnet_timeout;
	uint8_t resp_time_value;
	uint8_t local_phy_errors__overrun_errors;
	uint16_t max_credit_hint;
	uint32_t link_round_trip_latency;
} __attribute__ (( packed ));

#define IB_LINK_WIDTH_1X		0x01
#define IB_LINK_WIDTH_4X		0x02
#define IB_LINK_WIDTH_8X		0x04
#define IB_LINK_WIDTH_12X		0x08

#define IB_LINK_SPEED_SDR		0x01
#define IB_LINK_SPEED_DDR		0x02
#define IB_LINK_SPEED_QDR		0x04

#define IB_PORT_STATE_DOWN		0x01
#define IB_PORT_STATE_INIT		0x02
#define IB_PORT_STATE_ARMED		0x03
#define IB_PORT_STATE_ACTIVE		0x04

#define IB_PORT_PHYS_STATE_SLEEP	0x01
#define IB_PORT_PHYS_STATE_POLLING	0x02

#define IB_MTU_256			0x01
#define IB_MTU_512			0x02
#define IB_MTU_1024			0x03
#define IB_MTU_2048			0x04
#define IB_MTU_4096			0x05

#define IB_VL_0				0x01
#define IB_VL_0_1			0x02
#define IB_VL_0_3			0x03
#define IB_VL_0_7			0x04
#define IB_VL_0_14			0x05

/** A Partition Key Table attribute
 *
 * Defined in section 14.2.5.7 of the IBA.
 */
struct ib_pkey_table {
	uint16_t pkey[32];
} __attribute__ (( packed ));

/** A subnet management attribute */
union ib_smp_data {
	struct ib_node_desc node_desc;
	struct ib_node_info node_info;
	struct ib_guid_info guid_info;
	struct ib_port_info port_info;
	struct ib_pkey_table pkey_table;
	uint8_t bytes[64];
} __attribute__ (( packed ));

/** A subnet management directed route path */
struct ib_smp_dr_path {
	uint8_t hops[64];
} __attribute__ (( packed ));

/** Subnet management MAD class-specific data */
struct ib_smp_class_specific {
	uint8_t hop_pointer;
	uint8_t hop_count;
} __attribute__ (( packed ));

/*****************************************************************************
 *
 * Subnet administration MADs
 *
 *****************************************************************************
 */

struct ib_rmpp_hdr {
	uint32_t raw[3];
} __attribute__ (( packed ));

struct ib_sa_hdr {
	uint32_t sm_key[2];
	uint16_t reserved;
	uint16_t attrib_offset;
	uint32_t comp_mask[2];
} __attribute__ (( packed ));

#define IB_SA_ATTR_MC_MEMBER_REC		0x38
#define IB_SA_ATTR_PATH_REC			0x35

struct ib_path_record {
	uint32_t reserved0[2];
	struct ib_gid dgid;
	struct ib_gid sgid;
	uint16_t dlid;
	uint16_t slid;
	uint32_t hop_limit__flow_label__raw_traffic;
	uint32_t pkey__numb_path__reversible__tclass;
	uint8_t reserved1;
	uint8_t reserved__sl;
	uint8_t mtu_selector__mtu;
	uint8_t rate_selector__rate;
	uint32_t preference__packet_lifetime__packet_lifetime_selector;
	uint32_t reserved2[35];
} __attribute__ (( packed ));

#define IB_SA_PATH_REC_DGID			(1<<2)
#define IB_SA_PATH_REC_SGID			(1<<3)

struct ib_mc_member_record {
	struct ib_gid mgid;
	struct ib_gid port_gid;
	uint32_t qkey;
	uint16_t mlid;
	uint8_t mtu_selector__mtu;
	uint8_t tclass;
	uint16_t pkey;
	uint8_t rate_selector__rate;
	uint8_t packet_lifetime_selector__packet_lifetime;
	uint32_t sl__flow_label__hop_limit;
	uint8_t scope__join_state;
	uint8_t proxy_join__reserved;
	uint16_t reserved0;
	uint32_t reserved1[37];
} __attribute__ (( packed ));

#define IB_SA_MCMEMBER_REC_MGID			(1<<0)
#define IB_SA_MCMEMBER_REC_PORT_GID		(1<<1)
#define IB_SA_MCMEMBER_REC_QKEY			(1<<2)
#define IB_SA_MCMEMBER_REC_MLID			(1<<3)
#define IB_SA_MCMEMBER_REC_MTU_SELECTOR		(1<<4)
#define IB_SA_MCMEMBER_REC_MTU			(1<<5)
#define IB_SA_MCMEMBER_REC_TRAFFIC_CLASS	(1<<6)
#define IB_SA_MCMEMBER_REC_PKEY			(1<<7)
#define IB_SA_MCMEMBER_REC_RATE_SELECTOR	(1<<8)
#define IB_SA_MCMEMBER_REC_RATE			(1<<9)
#define IB_SA_MCMEMBER_REC_PACKET_LIFE_TIME_SELECTOR	(1<<10)
#define IB_SA_MCMEMBER_REC_PACKET_LIFE_TIME	(1<<11)
#define IB_SA_MCMEMBER_REC_SL			(1<<12)
#define IB_SA_MCMEMBER_REC_FLOW_LABEL		(1<<13)
#define IB_SA_MCMEMBER_REC_HOP_LIMIT		(1<<14)
#define IB_SA_MCMEMBER_REC_SCOPE		(1<<15)
#define IB_SA_MCMEMBER_REC_JOIN_STATE		(1<<16)
#define IB_SA_MCMEMBER_REC_PROXY_JOIN		(1<<17)

union ib_sa_data {
	struct ib_path_record path_record;
	struct ib_mc_member_record mc_member_record;
} __attribute__ (( packed ));

/*****************************************************************************
 *
 * MADs
 *
 *****************************************************************************
 */

/** Management datagram class_specific data */
union ib_mad_class_specific {
	uint16_t raw;
	struct ib_smp_class_specific smp;
} __attribute__ (( packed ));

/** A management datagram common header
 *
 * Defined in section 13.4.2 of the IBA.
 */
struct ib_mad_hdr {
	uint8_t base_version;
	uint8_t mgmt_class;
	uint8_t class_version;
	uint8_t method;
	uint16_t status;
	union ib_mad_class_specific class_specific;
	uint32_t tid[2];
	uint16_t attr_id;
	uint8_t reserved[2];
	uint32_t attr_mod;
} __attribute__ (( packed ));

/* Management base version */
#define IB_MGMT_BASE_VERSION			1

/* Management classes */
#define IB_MGMT_CLASS_SUBN_LID_ROUTED		0x01
#define IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE	0x81
#define IB_MGMT_CLASS_SUBN_ADM			0x03
#define IB_MGMT_CLASS_PERF_MGMT			0x04
#define IB_MGMT_CLASS_BM			0x05
#define IB_MGMT_CLASS_DEVICE_MGMT		0x06
#define IB_MGMT_CLASS_CM			0x07
#define IB_MGMT_CLASS_SNMP			0x08
#define IB_MGMT_CLASS_VENDOR_RANGE2_START	0x30
#define IB_MGMT_CLASS_VENDOR_RANGE2_END		0x4F

/* Management methods */
#define IB_MGMT_METHOD_GET			0x01
#define IB_MGMT_METHOD_SET			0x02
#define IB_MGMT_METHOD_GET_RESP			0x81
#define IB_MGMT_METHOD_SEND			0x03
#define IB_MGMT_METHOD_TRAP			0x05
#define IB_MGMT_METHOD_REPORT			0x06
#define IB_MGMT_METHOD_REPORT_RESP		0x86
#define IB_MGMT_METHOD_TRAP_REPRESS		0x07
#define IB_MGMT_METHOD_DELETE			0x15

/* Status codes */
#define IB_MGMT_STATUS_OK			0x0000
#define IB_MGMT_STATUS_BAD_VERSION		0x0001
#define IB_MGMT_STATUS_UNSUPPORTED_METHOD	0x0002
#define IB_MGMT_STATUS_UNSUPPORTED_METHOD_ATTR	0x0003
#define IB_MGMT_STATUS_INVALID_VALUE		0x0004

/** A subnet management MAD */
struct ib_mad_smp {
	struct ib_mad_hdr mad_hdr;
	struct ib_smp_hdr smp_hdr;
	union ib_smp_data smp_data;
	struct ib_smp_dr_path initial_path;
	struct ib_smp_dr_path return_path;
} __attribute__ (( packed ));

/** A subnet administration MAD */
struct ib_mad_sa {
	struct ib_mad_hdr mad_hdr;
	struct ib_rmpp_hdr rmpp_hdr;
	struct ib_sa_hdr sa_hdr;
	union ib_sa_data sa_data;
} __attribute__ (( packed ));

/** A management datagram */
union ib_mad {
	struct ib_mad_hdr hdr;
	struct ib_mad_smp smp;
	struct ib_mad_sa sa;
	uint8_t bytes[256];
} __attribute__ (( packed ));

#endif /* _GPXE_IB_MAD_H */
