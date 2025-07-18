#ifndef _USR_AWS_DATE_H
#define _USR_AWS_DATE_H

#include <time.h>

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} DateTime;

extern void epoch_to_datetime(time_t epochs, DateTime *dt);
extern int format_amz_date(DateTime *dt, char **amz_date);
extern int format_date_stamp(DateTime *dt, char **date_stamp);


#endif /* _USR_AWS_DATE_H */
