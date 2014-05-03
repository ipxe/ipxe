#ifndef _IPXE_PROFILE_H
#define _IPXE_PROFILE_H

/** @file
 *
 * Profiling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

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
	unsigned long started;
	/** Stop timestamp */
	unsigned long stopped;
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
 * @v started		Start timestamp
 */
static inline __attribute__ (( always_inline )) void
profile_start_at ( struct profiler *profiler, unsigned long started ) {

	/* If profiling is active then record start timestamp */
	if ( PROFILING )
		profiler->started = started;
}

/**
 * Start profiling
 *
 * @v profiler		Profiler
 */
static inline __attribute__ (( always_inline )) void
profile_start ( struct profiler *profiler ) {

	/* If profiling is active then record start timestamp */
	if ( PROFILING )
		profile_start_at ( profiler, profile_timestamp() );
}

/**
 * Record profiling result
 *
 * @v profiler		Profiler
 * @v stopped		Stop timestamp
 */
static inline __attribute__ (( always_inline )) void
profile_stop_at ( struct profiler *profiler, unsigned long stopped ) {

	/* If profiling is active then record end timestamp and update stats */
	if ( PROFILING ) {
		profiler->stopped = stopped;
		profile_update ( profiler, ( stopped - profiler->started ) );
	}
}

/**
 * Record profiling result
 *
 * @v profiler		Profiler
 */
static inline __attribute__ (( always_inline )) void
profile_stop ( struct profiler *profiler ) {

	/* If profiling is active then record end timestamp and update stats */
	if ( PROFILING )
		profile_stop_at ( profiler, profile_timestamp() );
}

#endif /* _IPXE_PROFILE_H */
