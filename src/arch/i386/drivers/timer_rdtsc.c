
#include <gpxe/init.h>
#include <gpxe/timer.h>
#include <errno.h>
#include <stdio.h>
#include <bits/cpu.h>
#include <bits/timer2.h>
#include <io.h>


#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscll(val) \
     __asm__ __volatile__ ("rdtsc" : "=A" (val))


/* Measure how many clocks we get in one microsecond */
static inline uint64_t calibrate_tsc(void)
{

	uint64_t rdtsc_start;
	uint64_t rdtsc_end;

	rdtscll(rdtsc_start);
	i386_timer2_udelay(USECS_IN_MSEC);
	rdtscll(rdtsc_end);
	
	return (rdtsc_end - rdtsc_start) / USECS_IN_MSEC;
}

static uint32_t clocks_per_usec = 0;

/* We measure time in microseconds. */
static tick_t rdtsc_currticks(void)
{
	uint64_t clocks;

	/* Read the Time Stamp Counter */
	rdtscll(clocks);

	return clocks / clocks_per_usec;
}

static int rdtsc_ts_init(void)
{

	struct cpuinfo_x86 cpu_info;

	get_cpuinfo(&cpu_info);
	if (cpu_info.features & X86_FEATURE_TSC) {
		clocks_per_usec= calibrate_tsc();
		if (clocks_per_usec) {
			DBG("RDTSC ticksource installed. CPU running at %ld Mhz\n",
				clocks_per_usec);
			return 0;
		}
	}

	DBG("RDTSC ticksource not available on this machine.\n");
	return -ENODEV;
}

struct timer rdtsc_ts __timer (01) = {
	.init = rdtsc_ts_init,
	.udelay = generic_currticks_udelay,
	.currticks = rdtsc_currticks,
};

