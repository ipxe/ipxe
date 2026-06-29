#ifndef _IPXE_TOD_H
#define _IPXE_TOD_H

/** @file
 *
 * Time-of-Day (TOD) Clock
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** An extended time of day (as read by "stcke") */
union tod_extended {
	/** Time-of-day clock value */
	struct {
		/** Epoch */
		uint8_t epoch;
		/** Time-of-day clock within epoch */
		uint8_t tod[13];
		/** Programmable field */
		uint8_t program[2];
	} __attribute__ (( packed ));
	/** Tick count */
	uint64_t ticks;
};

/**
 * Tick rate per microsecond
 *
 * The time-of-day clock is architecturally defined such that bit
 * number 51 (using big-endian bit numbering) increments once per
 * microsecond.
 *
 * When reading the "ticks" field, which effectively shifts this value
 * right by eight bits to create space for the epoch value, we end up
 * with a value that increments by 16 every microsecond.
 */
#define TOD_TICKS_PER_US 16

/**
 * Tick rate per millisecond
 *
 * For short interval timing, we accept a 2.4% error for the sake of
 * being able to use a shift rather than a division.
 */
#define TOD_TICKS_PER_MS ( 1024 * TOD_TICKS_PER_US )

/**
 * Tick offset from the Unix Epoch
 *
 * The time-of-day clock epoch starts at 1900-01-01 00:00 +0000.
 */
#define TOD_EPOCH 0x7d91048bca0000UL

/**
 * Get current time-of-day clock value (in ticks)
 *
 * @ret ticks		Current time, in ticks
 */
static inline unsigned long tod_ticks ( void ) {
	union tod_extended tod;

	/* Read clock */
	__asm__ ( "stcke %0" : "=R" ( tod ) );
	return tod.ticks;
}

/** Time-of-day clock states */
enum tod_state {
	/** Clock is running and set */
	TOD_STATE_SET,
	/** Clock is running but not set (i.e. relative timing only) */
	TOD_STATE_NOT_SET,
	/** Clock is in an error state */
	TOD_STATE_ERROR,
	/** Clock is not running */
	TOD_STATE_STOPPED,
};

/**
 * Get current time-of-day clock state
 *
 * @ret state		Clock state
 */
static inline enum tod_state tod_state ( void ) {
	union tod_extended tod;
	enum tod_state state;

	/* Read clock */
	__asm__ ( "stcke %0" : "=R" ( tod ), "=@cc" ( state ) );
	return state;
}

/**
 * Check if clock is running
 *
 * @v state		Clock state
 * @ret is_running	Clock is running
 */
static inline int tod_is_running ( enum tod_state state ) {

	/* Clock is running in either the "set" or "not-set" states */
	return ( state <= TOD_STATE_NOT_SET );
}

/**
 * Get clock state name (for debugging)
 *
 * @v state		Clock state
 * @ret name		Clock state name
 */
static inline const char * tod_state_name ( enum tod_state state ) {
	static const char *states[] = {
		[TOD_STATE_SET]		= "set",
		[TOD_STATE_NOT_SET]	= "not-set",
		[TOD_STATE_ERROR]	= "error",
		[TOD_STATE_STOPPED]	= "stopped",
	};
	return states[state];
}

#endif /* _IPXE_TOD_H */
