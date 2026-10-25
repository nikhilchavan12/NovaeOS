/* =============================================================================
 * NovaeOS - CMOS Real-Time Clock (kernel/drivers/rtc.c)
 * ============================================================================= */

#include "rtc.h"
#include "../lib/io.h"

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t v) { return (uint8_t)((v & 0x0F) + (v >> 4) * 10); }

static void read_raw(rtc_time_t *t) {
    while (cmos_read(0x0A) & 0x80) { }   /* wait for update-in-progress to clear */
    t->second = cmos_read(0x00);
    t->minute = cmos_read(0x02);
    t->hour   = cmos_read(0x04);
    t->day    = cmos_read(0x07);
    t->month  = cmos_read(0x08);
    t->year   = cmos_read(0x09);
}

void rtc_read(rtc_time_t *t) {
    rtc_time_t a, b;
    read_raw(&a);
    do { b = a; read_raw(&a); } while (a.second != b.second || a.minute != b.minute);

    uint8_t regb = cmos_read(0x0B);
    uint8_t pm = a.hour & 0x80;
    a.hour &= 0x7F;
    if (!(regb & 0x04)) {
        a.second = bcd_to_bin(a.second);
        a.minute = bcd_to_bin(a.minute);
        a.hour   = bcd_to_bin(a.hour);
        a.day    = bcd_to_bin(a.day);
        a.month  = bcd_to_bin(a.month);
        a.year   = bcd_to_bin((uint8_t)a.year);
    }
    if (!(regb & 0x02) && pm) a.hour = (uint8_t)((a.hour + 12) % 24);
    a.year = (uint16_t)(a.year + 2000);
    *t = a;
}

int rtc_day_of_week(int y, int m, int d) {
    static const int offs[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + offs[m - 1] + d) % 7;
}

int rtc_days_in_month(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return days[m - 1];
}
