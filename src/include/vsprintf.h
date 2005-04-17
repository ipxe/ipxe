#ifndef VSPRINTF_H
#define VSPRINTF_H

/*
 * Note that we cannot use __attribute__ (( format ( printf, ... ) ))
 * to get automatic type checking on arguments, because we use
 * non-standard format characters such as "%!" and "%@".
 *
 */

extern int sprintf ( char *buf, const char *fmt, ... );
extern void printf ( const char *fmt, ... );

#endif /* VSPRINTF_H */
