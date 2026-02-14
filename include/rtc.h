#ifndef ETEROS_RTC_H
#define ETEROS_RTC_H

#include "types.h"

/**
 * Representa una fecha y hora del sistema.
 */
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

/**
 * Inicializa el driver de RTC.
 */
void rtc_init(void);

/**
 * Obtiene la hora actual del RTC en UTC.
 */
void rtc_get_time(rtc_time_t* time);

/**
 * Convierte una hora UTC a hora local de Argentina (UTC-3).
 * Maneja correctamente el cambio de día, mes y año.
 */
void rtc_to_argentina(const rtc_time_t* utc, rtc_time_t* local);

#endif /* ETEROS_RTC_H */
