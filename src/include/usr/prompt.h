#ifndef _USR_PROMPT_H
#define _USR_PROMPT_H

/** @file
 *
 * Prompt for keypress
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

extern int prompt ( const char *text, unsigned long timeout, int key );

#endif /* _USR_PROMPT_H */
