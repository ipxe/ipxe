#ifndef LATCH_H
#define LATCH_H

#define TICKS_PER_SEC		(1000UL)

/* Fixed timer interval used for calibrating a more precise timer */
#define LATCHES_PER_SEC		10

void sleep_latch(void);

#endif /* LATCH_H */
