/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "etherboot.h"
#include "timer.h"
#include "latch.h"
#include "hardware.h"


/* get timer returns the contents of the timer */
static unsigned long get_timer(void)
{
	return P2001_TIMER->Freerun_Timer;
}

/* ------ Calibrate the TSC ------- 
 * Time how long it takes to excute a loop that runs in known time.
 * And find the convertion needed to get to CLOCK_TICK_RATE
 */

static unsigned long configure_timer(void)
{
	return (1);
}

static unsigned long clocks_per_tick = 1;

void setup_timers(void)
{
	if (!clocks_per_tick) {
		clocks_per_tick = configure_timer();
	}
}

unsigned long currticks(void)
{
	return get_timer(); /* /clocks_per_tick */
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
