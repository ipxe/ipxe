#ifndef DEBUG_H
#define DEBUG_H

//#include <lib.h>
extern int last_putchar;

/* Defining DEBUG_THIS before including this file enables debug() macro
 * for the file. DEBUG_ALL is for global control. */

#if DEBUG_THIS || DEBUG_ALL
#define DEBUG 1
#else
#undef DEBUG
#endif

#if DEBUG
# define debug(...) \
    ((last_putchar=='\n' ? printf("%s: ", __FUNCTION__) : 0), \
    printf(__VA_ARGS__))
# define debug_hexdump hexdump
#else
# define debug(...) /* nothing */
# define debug_hexdump(...) /* nothing */
#endif

#define debugx debug

#endif /* DEBUG_H */
