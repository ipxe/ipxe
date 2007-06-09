#ifndef _STDARG_H
#define _STDARG_H

typedef void * va_list;

#define va_start( ap, last ) do {	\
		ap = ( &last + 1 );	\
	} while ( 0 )

#define va_arg( ap, type ) ({		\
		type *_this = ap;	\
		ap = ( _this + 1 );	\
		*(_this);		\
	})

#define va_end( ap ) do { } while ( 0 )

#define va_copy( dest, src ) do {	\
		dest = src;		\
	} while ( 0 )

#endif /* _STDARG_H */
