#ifndef _SYSLOG_H
#define _SYSLOG_H

/** @file
 *
 * System logger
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdarg.h>
#include <config/console.h>

/**
 * @defgroup syslogpri Syslog priorities
 *
 * These values are chosen to match those used in the syslog network
 * protocol (RFC 5424).
 *
 * @{
 */

/** Emergency: system is unusable */
#define LOG_EMERG 0

/** Alert: action must be taken immediately */
#define LOG_ALERT 1

/** Critical: critical conditions */
#define LOG_CRIT 2

/** Error: error conditions */
#define LOG_ERR 3

/** Warning: warning conditions */
#define LOG_WARNING 4

/** Notice: normal but significant conditions */
#define LOG_NOTICE 5

/** Informational: informational messages */
#define LOG_INFO 6

/** Debug: debug-level messages */
#define LOG_DEBUG 7

/** @} */

/** Do not log any messages */
#define LOG_NONE -1

extern void log_vprintf ( const char *fmt, va_list args );

extern void __attribute__ (( format ( printf, 1, 2 ) ))
log_printf ( const char *fmt, ... );

/**
 * Write message to system log
 *
 * @v priority		Message priority
 * @v fmt		Format string
 * @v ...		Arguments
 */
#define vsyslog( priority, fmt, args ) do {		\
	if ( (priority) <= LOG_LEVEL ) {		\
		log_vprintf ( fmt, (args) );		\
	}						\
	} while ( 0 )

/**
 * Write message to system log
 *
 * @v priority		Message priority
 * @v fmt		Format string
 * @v ...		Arguments
 */
#define syslog( priority, fmt, ... ) do {		\
	if ( (priority) <= LOG_LEVEL ) {		\
		log_printf ( fmt, ##__VA_ARGS__ );	\
	}						\
	} while ( 0 )

#endif /* _SYSLOG_H */
