#ifndef _IPXE_PROFILE_H
#define _IPXE_PROFILE_H

/** @file
 *
 * Profiling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <bits/profile.h>
#include <ipxe/tables.h>

#ifdef NDEBUG
#define PROFILING 0
#else
#define PROFILING 1
#endif

/**
 * A data structure for storing profiling information
 */
struct profiler {
	/** Name */
	const char *name;
	/** Start timestamp */
	uint64_t started;
	/** Number of samples */
	unsigned int count;
	/** Mean sample value (scaled) */
	unsigned long mean;
	/** Mean sample value MSB
	 *
	 * This is the highest bit set in the raw (unscaled) value
	 * (i.e. one less than would be returned by flsl(raw_mean)).
	 */
	unsigned int mean_msb;
	/** Accumulated variance (scaled) */
	unsigned long long accvar;
	/** Accumulated variance MSB
	 *
	 * This is the highest bit set in the raw (unscaled) value
	 * (i.e. one less than would be returned by flsll(raw_accvar)).
	 */
	unsigned int accvar_msb;
};

/** Profiler table */
#define PROFILERS __table ( struct profiler, "profilers" )

/** Declare a profiler */
#if PROFILING
#define __profiler __table_entry ( PROFILERS, 01 )
#else
#define __profiler
#endif

extern void profile_update ( struct profiler *profiler, unsigned long sample );
extern unsigned long profile_mean ( struct profiler *profiler );
extern unsigned long profile_variance ( struct profiler *profiler );
extern unsigned long profile_stddev ( struct profiler *profiler );

/**
 * Start profiling
 *
 * @v profiler		Profiler
 */
static inline __attribute__ (( always_inline )) void
profile_start ( struct profiler *profiler ) {

	/* If profiling is active then record start timestamp */
	if ( PROFILING )
		profiler->started = profile_timestamp();
}

/**
 * Record profiling result
 *
 * @v profiler		Profiler
 */
static inline __attribute__ (( always_inline )) void
profile_stop ( struct profiler *profiler ) {
	uint64_t ended;

	/* If profiling is active then record end timestamp and update stats */
	if ( PROFILING ) {
		ended = profile_timestamp();
		profile_update ( profiler, ( ended - profiler->started ) );
	}
}

#endif /* _IPXE_PROFILE_H */
