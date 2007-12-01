
#include <gpxe/init.h>
#include <gpxe/timer.h>
#include <stdio.h>
#include <bits/cpu.h>
#include <bits/timer2.h>
#include <io.h>


#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscll(val) \
     __asm__ __volatile__ ("rdtsc" : "=A" (val))

static unsigned long long calibrate_tsc(void)
{
	uint32_t startlow, starthigh;
	uint32_t endlow, endhigh;

	rdtsc(startlow,starthigh);
	i386_timer2_udelay(USECS_IN_MSEC/2);
	rdtsc(endlow,endhigh);

	/* 64-bit subtract - gcc just messes up with long longs */
	/* XXX ORLY? Check it. */
	__asm__("subl %2,%0\n\t"
		"sbbl %3,%1"
		:"=a" (endlow), "=d" (endhigh)
		:"g" (startlow), "g" (starthigh),
		"0" (endlow), "1" (endhigh));

	/* Error: ECPUTOOFAST */
	if (endhigh)
		goto bad_ctc;

	endlow *= MSECS_IN_SEC*2;
	return endlow;

	/*
	 * The CTC wasn't reliable: we got a hit on the very first read,
	 * or the CPU was so fast/slow that the quotient wouldn't fit in
	 * 32 bits..
	 */
bad_ctc:
	return 0;
}
static uint32_t clocks_per_second = 0;

static tick_t rtdsc_currticks(void)
{
	uint32_t clocks_high, clocks_low;
	uint32_t currticks;

	/* Read the Time Stamp Counter */
	rdtsc(clocks_low, clocks_high);

	/* currticks = clocks / clocks_per_tick; */
	__asm__("divl %1"
		:"=a" (currticks)
		:"r" (clocks_per_second/USECS_IN_SEC), "0" (clocks_low), "d" (clocks_high));

	return currticks;
}

static int rtdsc_ts_init(void)
{

	struct cpuinfo_x86 cpu_info;

	get_cpuinfo(&cpu_info);
	if (cpu_info.features & X86_FEATURE_TSC) {
		clocks_per_second = calibrate_tsc();
		if (clocks_per_second) {
			DBG("RTDSC Ticksource installed. CPU running at %ld Mhz\n",
				clocks_per_second/(1000*1000));
			return 0;
		}
	}

	printf("RTDSC timer not available on this machine.\n");
	return 1;
}

struct timer rtdsc_ts __timer (01) = {
	.init = rtdsc_ts_init,
	.udelay = generic_currticks_udelay,
	.currticks = rtdsc_currticks,
};

