#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

// Mock IO functions
uint8_t cmos_registers[128];
uint8_t current_index = 0;

void outb(uint16_t port, uint8_t value) {
    if (port == 0x70) {
        current_index = value & 0x7F; // Ignore NMI bit
    }
}

uint8_t inb(uint16_t port) {
    if (port == 0x71) {
        return cmos_registers[current_index];
    }
    return 0;
}

// Stub other IO functions required by io.h/kernel link if any (none for rtc.c strictly)
void outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint16_t inw(uint16_t port) { (void)port; return 0; }
uint32_t inl(uint16_t port) { (void)port; return 0; }
void io_wait(void) {}
void cpu_halt(void) {}
int interrupts_enabled(void) { return 0; }
uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }


// Include source file to test
#include "../kernel/drivers/rtc/rtc.c"

void test_rtc_bcd_24h() {
    printf("Testing RTC BCD 24h...\n");
    memset(cmos_registers, 0, sizeof(cmos_registers));

    // Set time: 2023-12-31 23:59:59
    // BCD: 23 -> 0x23, 59 -> 0x59, 12 -> 0x12, 31 -> 0x31
    cmos_registers[0x00] = 0x59; // Sec
    cmos_registers[0x02] = 0x59; // Min
    cmos_registers[0x04] = 0x23; // Hour
    cmos_registers[0x07] = 0x31; // Day
    cmos_registers[0x08] = 0x12; // Month
    cmos_registers[0x09] = 0x23; // Year (23)

    cmos_registers[0x0A] = 0x00; // Not updating
    cmos_registers[0x0B] = 0x02; // 24h mode, BCD mode (bit 2=0)

    rtc_time_t t;
    rtc_get_time(&t);

    assert(t.seconds == 59);
    assert(t.minutes == 59);
    assert(t.hours == 23);
    assert(t.day == 31);
    assert(t.month == 12);
    assert(t.year == 2023);
    printf("PASS\n");
}

void test_rtc_bcd_12h_pm() {
    printf("Testing RTC BCD 12h PM...\n");
    memset(cmos_registers, 0, sizeof(cmos_registers));

    // Set time: 01:00 PM (13:00)
    // 1 PM is 0x01 | 0x80 = 0x81
    cmos_registers[0x04] = 0x81;
    cmos_registers[0x00] = 0x00;
    cmos_registers[0x02] = 0x00;
    cmos_registers[0x07] = 0x01;
    cmos_registers[0x08] = 0x01;
    cmos_registers[0x09] = 0x23;

    cmos_registers[0x0A] = 0x00;
    cmos_registers[0x0B] = 0x00; // 12h mode (bit 1=0), BCD (bit 2=0)

    rtc_time_t t;
    rtc_get_time(&t);

    assert(t.hours == 13);
    printf("PASS\n");
}

void test_rtc_bcd_12h_am() {
    printf("Testing RTC BCD 12h AM...\n");
    memset(cmos_registers, 0, sizeof(cmos_registers));

    // Set time: 12:00 AM (00:00)
    // 12 AM is 0x12 (bit 7 clear)
    cmos_registers[0x04] = 0x12;
    cmos_registers[0x00] = 0x00;
    cmos_registers[0x02] = 0x00;
    cmos_registers[0x07] = 0x01;
    cmos_registers[0x08] = 0x01;
    cmos_registers[0x09] = 0x23;

    cmos_registers[0x0A] = 0x00;
    cmos_registers[0x0B] = 0x00; // 12h mode, BCD

    rtc_time_t t;
    rtc_get_time(&t);

    assert(t.hours == 0);
    printf("PASS\n");
}

void test_argentina_conversion() {
    printf("Testing Argentina conversion...\n");
    rtc_time_t utc, local;

    // Init utc
    memset(&utc, 0, sizeof(utc));

    // Normal case
    utc.hours = 10; utc.day = 10; utc.month = 5; utc.year = 2023;
    rtc_to_argentina(&utc, &local);
    assert(local.hours == 7);
    assert(local.day == 10);

    // Day rollover
    utc.hours = 2; utc.day = 10; utc.month = 5; utc.year = 2023;
    rtc_to_argentina(&utc, &local);
    assert(local.hours == 23);
    assert(local.day == 9);

    // Month rollover
    utc.hours = 1; utc.day = 1; utc.month = 3; utc.year = 2023;
    rtc_to_argentina(&utc, &local);
    assert(local.hours == 22);
    assert(local.day == 28); // Feb 28 non-leap
    assert(local.month == 2);

    // Year rollover
    utc.hours = 0; utc.day = 1; utc.month = 1; utc.year = 2024;
    rtc_to_argentina(&utc, &local);
    assert(local.hours == 21);
    assert(local.day == 31);
    assert(local.month == 12);
    assert(local.year == 2023);

    // Leap year
    utc.hours = 1; utc.day = 1; utc.month = 3; utc.year = 2024; // 2024 is leap
    rtc_to_argentina(&utc, &local);
    assert(local.day == 29); // Feb 29
    assert(local.month == 2);

    printf("PASS\n");
}

int main() {
    test_rtc_bcd_24h();
    test_rtc_bcd_12h_pm();
    test_rtc_bcd_12h_am();
    test_argentina_conversion();
    return 0;
}
