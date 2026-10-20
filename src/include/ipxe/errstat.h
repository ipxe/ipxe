#ifndef _IPXE_ERRSTAT_H
#define _IPXE_ERRSTAT_H

/** @file
 *
 * Error statistics
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** An error statistic */
struct error_statistic {
	/** Error status code */
	int rc;
	/** Error count */
	unsigned int count;
};

/** Maximum number of unique errors that we will keep track of */
#define MAX_UNIQUE_ERRORS 4

/** Error statistics */
struct error_statistics {
	/** Count of successful completions */
	unsigned int good;
	/** Count of error completions */
	unsigned int bad;
	/** Error breakdowns */
	struct error_statistic errors[MAX_UNIQUE_ERRORS];
};

extern void record_stat ( struct error_statistics *stats, int rc );

#endif /* _IPXE_ERRSTAT_H */
