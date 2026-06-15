/**
 * éterOS - RTC Driver (Real Time Clock)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include "rtc.h"
#include "io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static int8_t current_timezone = -3; // Default UTC-3 (Argentina)

static int rtc_is_updating() {
#ifndef __ETEROS_HOST_TEST__
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags));
#endif
    outb(CMOS_ADDRESS, 0x0A | 0x80); // Disable NMI
    int ret = (inb(CMOS_DATA) & 0x80);
    outb(CMOS_ADDRESS, 0x0A);        // Re-enable NMI
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("push %0; popfq" :: "r"(flags));
#endif
    return ret;
}

static unsigned char rtc_read_register(int reg) {
#ifndef __ETEROS_HOST_TEST__
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags));
#endif
    outb(CMOS_ADDRESS, reg | 0x80); // Disable NMI
    unsigned char ret = inb(CMOS_DATA);
    outb(CMOS_ADDRESS, reg);        // Re-enable NMI
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("push %0; popfq" :: "r"(flags));
#endif
    return ret;
}

static void rtc_write_register(int reg, unsigned char val) {
#ifndef __ETEROS_HOST_TEST__
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags));
#endif
    outb(CMOS_ADDRESS, reg | 0x80); // Disable NMI
    outb(CMOS_DATA, val);
    outb(CMOS_ADDRESS, reg);        // Re-enable NMI
#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile("push %0; popfq" :: "r"(flags));
#endif
}

void rtc_init(void) {
    // No initialization needed for basic polling
}

void rtc_get_time(rtc_time_t* time) {
    unsigned char second, minute, hour, day, month, year;
    unsigned char last_second, last_minute, last_hour, last_day, last_month, last_year;
    unsigned char registerB;

    // Wait until update is not in progress
    while (rtc_is_updating());

    second = rtc_read_register(0x00);
    minute = rtc_read_register(0x02);
    hour   = rtc_read_register(0x04);
    day    = rtc_read_register(0x07);
    month  = rtc_read_register(0x08);
    year   = rtc_read_register(0x09);

    do {
        last_second = second;
        last_minute = minute;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;

        while (rtc_is_updating());
        second = rtc_read_register(0x00);
        minute = rtc_read_register(0x02);
        hour   = rtc_read_register(0x04);
        day    = rtc_read_register(0x07);
        month  = rtc_read_register(0x08);
        year   = rtc_read_register(0x09);
    } while ((last_second != second) || (last_minute != minute) ||
             (last_hour != hour) || (last_day != day) ||
             (last_month != month) || (last_year != year));

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

void rtc_set_time(rtc_time_t* time) {
    unsigned char registerB;
    unsigned char second, minute, hour, day, month, year;

    while (rtc_is_updating());

    registerB = rtc_read_register(0x0B);

    int is_bcd = !(registerB & 0x04);
    int is_12h = !(registerB & 0x02);

    second = time->seconds;
    minute = time->minutes;
    hour   = time->hours;
    day    = time->day;
    month  = time->month;
    year   = time->year % 100; // Assume 20xx

    int pm = 0;
    if (is_12h) {
        if (hour >= 12) {
            pm = 1;
            if (hour > 12) hour -= 12;
        } else if (hour == 0) {
            hour = 12;
        }
    }

    if (is_bcd) {
        second = ((second / 10) << 4) | (second % 10);
        minute = ((minute / 10) << 4) | (minute % 10);
        hour   = ((hour / 10) << 4) | (hour % 10);
        day    = ((day / 10) << 4) | (day % 10);
        month  = ((month / 10) << 4) | (month % 10);
        year   = ((year / 10) << 4) | (year % 10);
    }

    if (is_12h && pm) {
        hour |= 0x80;
    }

    // Disable interrupts/NMI while updating? For simplicity, we just wait and write.
    // In a real OS we should disable NMI by writing port 0x70 with 0x80 bit set, but
    // since we do a fast write it's usually fine. We must set register B SET bit
    // to prevent updates during our writes.

    // Set 'SET' bit (bit 7) of register B to pause RTC updates
    rtc_write_register(0x0B, registerB | 0x80);

    rtc_write_register(0x00, second);
    rtc_write_register(0x02, minute);
    rtc_write_register(0x04, hour);
    rtc_write_register(0x07, day);
    rtc_write_register(0x08, month);
    rtc_write_register(0x09, year);

    // Clear 'SET' bit of register B to resume updates
    rtc_write_register(0x0B, registerB);
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
    rtc_get_local_time(utc, local);
}

void rtc_get_local_time(const rtc_time_t* utc, rtc_time_t* local) {
    *local = *utc;

    if (current_timezone == 0) return;

    int new_hour = (int)local->hours + current_timezone;

    if (new_hour < 0) {
        // Go back one day
        local->hours = 24 + new_hour;

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
    } else if (new_hour >= 24) {
        // Go forward one day
        local->hours = new_hour - 24;

        uint8_t days_this_month = get_days_in_month(local->month, local->year);
        if (local->day < days_this_month) {
            local->day++;
        } else {
            local->day = 1;
            if (local->month < 12) {
                local->month++;
            } else {
                local->month = 1;
                local->year++;
            }
        }
    } else {
        local->hours = new_hour;
    }
}

void rtc_set_timezone(int8_t offset) {
    if (offset >= -12 && offset <= 14) {
        current_timezone = offset;
    }
}

int8_t rtc_get_timezone(void) {
    return current_timezone;
}
