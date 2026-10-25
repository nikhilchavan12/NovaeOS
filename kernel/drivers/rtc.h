/* =============================================================================
 * NovaeOS - CMOS Real-Time Clock (kernel/drivers/rtc.h)
 * ============================================================================= */

#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint8_t  second, minute, hour;
    uint8_t  day, month;
    uint16_t year;
} rtc_time_t;

void rtc_read(rtc_time_t *t);
int  rtc_day_of_week(int year, int month, int day);   /* 0 = Sunday */
int  rtc_days_in_month(int year, int month);

#endif /* RTC_H */
