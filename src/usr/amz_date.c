#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usr/amz_date.h>

/**
 * This function checks if a year is a leap year by the following rules:
 * - A year is a leap year if it is divisible by 4.
 * - However, if it is divisible by 100, it is not a leap year unless it is also divisible by 400.
 *
 * @v year     The year to be checked.
 * @ret        Returns 1 if the year is a leap year, 0 otherwise.
 */
int is_leap ( int year ) {
	return ( year % 4 == 0 && ( year % 100 != 0 || year % 400 == 0 ) );
};

/**
 * Converts a Unix epoch timestamp to a DateTime structure.
 *
 * This function takes a Unix epoch timestamp (seconds since January 1, 1970, 00:00:00 UTC)
 * and populates a DateTime structure with the corresponding year, month, day, hour,
 * minute, and second in UTC.
 *
 * @v epochs The Unix epoch timestamp (seconds since January 1, 1970, 00:00:00 UTC).
 * @v dt     Pointer to a DateTime structure to be populated with the date and time.
 */
void epoch_to_datetime ( time_t epochs, DateTime *dt ) {
	const int SECONDS_PER_MINUTE = 60;
	const int SECONDS_PER_HOUR = 3600;
	const int SECONDS_PER_DAY = 86400;
	int month_lengths[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	uint64_t seconds = ( uint64_t ) epochs;
	int days = seconds / SECONDS_PER_DAY;
	int rem_secs = seconds % SECONDS_PER_DAY;

	dt->hour = rem_secs / SECONDS_PER_HOUR;
	dt->minute = ( rem_secs % SECONDS_PER_HOUR ) / SECONDS_PER_MINUTE;
	dt->second = rem_secs % SECONDS_PER_MINUTE;

	dt->year = 1970;
	dt->month = 1;

	/* Determine the current year by subtracting the number of days in each year from total days */
	while ( 1 ) {
		int days_in_year = is_leap ( dt->year ) ? 366 : 365;
		if ( days >= days_in_year ) {
			days -= days_in_year;
			dt->year++;
		} else {
			break;
		}
	}

	/* Set February to 29 days if the current year is a leap year */
	if ( is_leap ( dt->year ) )
		month_lengths[1] = 29;

	/* Determine the month by subtracting month lengths from remaining days */
	for ( int i = 0; i < 12; ++i ) {
		if ( days >= month_lengths[i] ) {
			days -= month_lengths[i];
			dt->month++;
		} else {
			break;
		}
	}

	/* Fix the zero indexing */
	dt->day = days + 1;
}

/* *
 * Formats a date and time structure into an Amazon-style date string (ISO 8601).
 * https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_sigv-signing-elements.html#date
 *
 * This function takes a pointer to a DateTime structure and formats it
 * into a string representing the date and time in the format MMDDTHHMMSSZ,
 * which is required for certain AWS API operations.
 *
 * @v dt        Pointer to a DateTime structure containing the date and time.
 * @v amz_date  Pointer to a char pointer where the allocated Amazon date string will be stored.
 * Unmodified on error.
 * @ret rc      Return status code (0 on success, -ENOMEM on memory allocation failure).
 *
 * The caller is responsible for freeing the memory allocated for the returned amz_date string.
 */
int format_amz_date ( DateTime *dt, char **amz_date ) {
	/* YYYYMMDDTHHMMSSZ + null terminator*/
	const int AMZ_DATE_LEN = 17;

	char *result = malloc ( AMZ_DATE_LEN );
	if ( result == NULL ) {
		return -ENOMEM;
	}

	snprintf ( result, AMZ_DATE_LEN, "%04d%02d%02dT%02d%02d%02dZ",
			   dt->year, dt->month, dt->day, dt->hour, dt->minute, dt->second );

	*amz_date = result;

	return 0;
}

/* *
 * Formats a date and time structure into a YYYYMMDD date string.
 *
 * This function takes a pointer to a DateTime structure and formats it
 * into a string representing the date in the format YYYYMMDD, which is
 * required for certain AWS API operations.
 *
 * @v dt          Pointer to a DateTime structure containing the date and time.
 * @v date_stamp  Pointer to a char pointer where the allocated YYYYMMDD date string will be stored.
 * Unmodified on error.
 * @ret rc        Return status code (0 on success, -ENOMEM on memory allocation failure).
 *
 * The caller is responsible for freeing the memory allocated for the returned date_stamp string.
 */
int format_date_stamp ( DateTime *dt, char **date_stamp ) {
	/* "YYYYMMDD" + null terminator */
	const int DATE_STAMP_LEN = 9;

	char *result = malloc ( DATE_STAMP_LEN );
	if ( result == NULL ) {
		return -ENOMEM;
	}

	snprintf ( result, DATE_STAMP_LEN, "%04d%02d%02d", dt->year, dt->month, dt->day );

	*date_stamp = result;

	return 0;
}
