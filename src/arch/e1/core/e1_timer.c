/*
 * Copyright 2003 Yannis Mitsos and George Thanos 
 * {gmitsos@gthanos}@telecom.ntua.gr
 * Released under GPL2, see the file COPYING in the top directory
 *
 */
#include "etherboot.h"
#include "timer.h"
#include "e132_xs_board.h"

/* get timer returns the contents of the timer */
static inline unsigned long get_timer(void)
{
	unsigned long result;
	__asm__ __volatile__("
					ORI	SR, 0x20
					mov	%0, TR" 
					: "=l"(result));
	return result;
}

/* ------ Calibrate the TSC ------- 
 * Time how long it takes to excute a loop that runs in known time.
 * And find the convertion needed to get to CLOCK_TICK_RATE
 */

static unsigned long configure_timer(void)
{
	unsigned long TPR_value; /* Timer Prescalar Value */

	TPR_value = 0x000C00000;
	
	asm volatile (" 
				FETCH   4	
				ORI		SR, 0x20
				MOV		TPR, %0
				ORI		SR, 0x20
				MOVI	TR, 0x0"
				: /* no outputs */
				: "l" (TPR_value)
				); 

	printf("The time prescaler register is set to: <%#x>\n",TPR_value);
	return (1);
}

static unsigned long clocks_per_tick;

void setup_timers(void)
{
	if (!clocks_per_tick) {
		clocks_per_tick = configure_timer();
	}
}

unsigned long currticks(void)
{
	return get_timer()/clocks_per_tick;
}

static unsigned long timer_timeout;
static int __timer_running(void)
{
	return get_timer() < timer_timeout;
}

void udelay(unsigned int usecs)
{
	unsigned long now;
	now = get_timer();
	timer_timeout = now + usecs * ((clocks_per_tick * TICKS_PER_SEC)/(1000*1000));
	while(__timer_running());
}
void ndelay(unsigned int nsecs)
{
	unsigned long now;
	now = get_timer();
	timer_timeout = now + nsecs * ((clocks_per_tick * TICKS_PER_SEC)/(1000*1000*1000));
	while(__timer_running());
}

void load_timer2(unsigned int timer2_ticks)
{
	unsigned long now;
	unsigned long clocks;
	now = get_timer();
	clocks = timer2_ticks * ((clocks_per_tick * TICKS_PER_SEC)/CLOCK_TICK_RATE);
	timer_timeout = now + clocks;
}

int timer2_running(void)
{
	return __timer_running();
}
