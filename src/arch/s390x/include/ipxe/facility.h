#ifndef _IPXE_FACILITY_H
#define _IPXE_FACILITY_H

/** @file
 *
 * Installed facilities
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Message-security-assist extension 5 facility ID */
#define FACILITY_MSA5 57

/** Multiple-epoch facility ID */
#define FACILITY_MULTIPLE_EPOCH 139

/** Installed facilities */
struct s390x_facilities {
	/** Bit mask of installed facilities */
	uint64_t mask[4];
};

extern int __attribute__ (( pure ))
facility_is_installed ( unsigned int facility );

#endif /* _IPXE_FACILITY_H */
