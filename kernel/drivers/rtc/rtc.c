/**
 * éterOS - RTC Driver (Real Time Clock)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include "rtc.h"
#include "io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static int rtc_is_updating() {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static unsigned char rtc_read_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void rtc_init(void) {
    // No initialization needed for basic polling
}

void rtc_get_time(rtc_time_t* time) {
    unsigned char second, minute, hour, day, month, year;
    unsigned char registerB;

    // Wait until update is not in progress
    // To be safe, we should read until we get the same values twice
    // or just wait for update flag to clear.
    // Waiting for update flag to clear is usually enough if we are fast.
    while (rtc_is_updating());

    second = rtc_read_register(0x00);
    minute = rtc_read_register(0x02);
    hour   = rtc_read_register(0x04);
    day    = rtc_read_register(0x07);
    month  = rtc_read_register(0x08);
    year   = rtc_read_register(0x09);

    registerB = rtc_read_register(0x0B);

    // Check modes
    int is_bcd = !(registerB & 0x04);
    int is_12h = !(registerB & 0x02);

    // Convert BCD to binary if necessary
    if (is_bcd) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        day    = (day & 0x0F) + ((day / 16) * 10);
        month  = (month & 0x0F) + ((month / 16) * 10);
        year   = (year & 0x0F) + ((year / 16) * 10);

        if (is_12h) {
             int pm = (hour & 0x80) != 0;
             hour &= 0x7F;
             hour = (hour & 0x0F) + ((hour / 16) * 10);
             if (pm) {
                 if (hour != 12) hour += 12;
             } else {
                 if (hour == 12) hour = 0;
             }
        } else {
             hour = (hour & 0x0F) + ((hour / 16) * 10);
        }
    } else {
        // Binary mode
        if (is_12h) {
             int pm = (hour & 0x80) != 0;
             hour &= 0x7F;
             if (pm) {
                 if (hour != 12) hour += 12;
             } else {
                 if (hour == 12) hour = 0;
             }
        }
    }

    time->seconds = second;
    time->minutes = minute;
    time->hours   = hour;
    time->day     = day;
    time->month   = month;
    time->year    = 2000 + year; // Assume 20xx
}

static int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static uint8_t get_days_in_month(uint8_t month, uint16_t year) {
    // Days in month: 0, 31, 28, 31, 30, ...
    static const uint8_t days[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month == 2 && is_leap_year(year)) {
        return 29;
    }

    if (month >= 1 && month <= 12) {
        return days[month];
    }
    return 30; // Fallback
}

void rtc_to_argentina(const rtc_time_t* utc, rtc_time_t* local) {
    *local = *utc;

    // Argentina is UTC-3
    if (local->hours >= 3) {
        local->hours -= 3;
    } else {
        // Go back one day
        local->hours = 24 - (3 - local->hours);

        if (local->day > 1) {
            local->day--;
        } else {
            // Go back one month
            if (local->month > 1) {
                local->month--;
            } else {
                // Go back one year
                local->month = 12;
                local->year--;
            }
            local->day = get_days_in_month(local->month, local->year);
        }
    }
}
