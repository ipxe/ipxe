#include "etherboot.h"
#include "timer.h"
#include "sal.h"
#include "pal.h"

static inline unsigned long get_cycles(void)
{
	unsigned long result;
	__asm__ __volatile__(";;mov %0=ar.itc;;" : "=r"(result));
	return result;
}

/* ------ Calibrate the TSC ------- 
 * Time how long it takes to excute a loop that runs in known time.
 * And find the convertion needed to get to CLOCK_TICK_RATE
 */

static unsigned long calibrate_cycles(void)
{
	unsigned long platform_ticks_per_second, drift_info;
	struct pal_freq_ratio itc_ratio;
	long result;
	result = sal_freq_base(SAL_FREQ_BASE_PLATFORM, &platform_ticks_per_second, &drift_info);
	if (result != 0) {
		printf("sal_freq_base failed: %lx\n",result);
		exit(1);
	} else {
		result = pal_freq_ratios(0,0,&itc_ratio);
		if (result != 0) {
			printf("pal_freq_ratios failed: %lx\n", result);
			exit(1);
		}
	}
	/* Avoid division by zero */
	if (itc_ratio.den == 0)
		itc_ratio.den = 1;

	return (platform_ticks_per_second *itc_ratio.num)/(itc_ratio.den*TICKS_PER_SEC);
}

static unsigned long clocks_per_tick;
void setup_timers(void)
{
	if (!clocks_per_tick) {
		clocks_per_tick = calibrate_cycles();
		/* Display the CPU Mhz to easily test if the calibration was bad */
		printf("ITC %ld Mhz\n", (clocks_per_tick/1000 * TICKS_PER_SEC)/1000);
	}
}

unsigned long currticks(void)
{
	return get_cycles()/clocks_per_tick;
}

static unsigned long timer_timeout;
static int __timer_running(void)
{
	return get_cycles() < timer_timeout;
}

void udelay(unsigned int usecs)
{
	unsigned long now;
	now = get_cycles();
	timer_timeout = now + usecs * ((clocks_per_tick * TICKS_PER_SEC)/(1000*1000));
	while(__timer_running());
}
void ndelay(unsigned int nsecs)
{
	unsigned long now;
	now = get_cycles();
	timer_timeout = now + nsecs * ((clocks_per_tick * TICKS_PER_SEC)/(1000*1000*1000));
	while(__timer_running());
}

void load_timer2(unsigned int timer2_ticks)
{
	unsigned long now;
	unsigned long clocks;
	now = get_cycles();
	clocks = timer2_ticks * ((clocks_per_tick * TICKS_PER_SEC)/CLOCK_TICK_RATE);
	timer_timeout = now + clocks;
}

int timer2_running(void)
{
	return __timer_running();
}
