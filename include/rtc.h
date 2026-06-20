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
 * Escribe una hora en el RTC (en UTC).
 */
void rtc_set_time(rtc_time_t* time);

/**
 * Convierte una hora UTC a la hora local configurada en el sistema.
 */
void rtc_get_local_time(const rtc_time_t* utc, rtc_time_t* local);

/**
 * Configura la zona horaria del sistema.
 * @param offset Desplazamiento respecto a UTC (ej. -3 para Argentina).
 */
void rtc_set_timezone(int8_t offset);

/**
 * Obtiene la zona horaria actual.
 */
int8_t rtc_get_timezone(void);

#endif /* ETEROS_RTC_H */
