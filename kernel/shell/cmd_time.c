#include "shell_internal.h"
#include "../../include/rtc.h"
#include "../../include/string.h"
#include "../../include/net/socket.h"
#include "../../include/net/lwip_socket.h"
#include "../../include/net/dhcp.h" /* For ntohl/htonl if needed, but we can implement basic byte swaps */
#include "../../include/net/nic.h"

// Simple byte swap for 32-bit (network to host)
static uint32_t bswap_32(uint32_t x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8)  |
           ((x & 0x00FF0000) >> 8)  |
           ((x & 0xFF000000) >> 24);
}

void cmd_timezone(const char* args) {
    if (!args || !*args) {
        terminal_write_string("Uso: timezone <offset>\n");
        terminal_write_string("Ejemplo: timezone -3 (Argentina), timezone 0 (UTC)\n");
        return;
    }

    int32_t offset;
    if (atoi_s(args, &offset) == 0 && offset >= -12 && offset <= 14) {
        rtc_set_timezone((int8_t)offset);
        terminal_write_string("Zona horaria actualizada.\n");
    } else {
        terminal_write_string("Offset invalido. Debe estar entre -12 y +14.\n");
    }
}

// NTP Time is seconds since Jan 1, 1900.
// UNIX Time is seconds since Jan 1, 1970.
#define NTP_TIMESTAMP_DELTA 2208988800ull

// Simple UNIX timestamp to RTC time conversion
static void unix_to_rtc(time_t timestamp, rtc_time_t* t) {
    int64_t z = timestamp / 86400;
    int64_t time_of_day = timestamp % 86400;

    if (time_of_day < 0) {
        z--;
        time_of_day += 86400;
    }

    t->hours = time_of_day / 3600;
    t->minutes = (time_of_day % 3600) / 60;
    t->seconds = time_of_day % 60;

    z += 719468; // Days between 0000-03-01 and 1970-01-01
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365*yoe + yoe/4 - yoe/100);
    int64_t mp = (5*doy + 2)/153;
    int64_t d = doy - (153*mp+2)/5 + 1;
    int64_t m = mp + (mp < 10 ? 3 : -9);

    t->year = y + (m <= 2 ? 1 : 0);
    t->month = m;
    t->day = d;
}

void cmd_ntp(const char* args) {
    const char* server = "pool.ntp.org";
    if (args && *args) {
        server = args;
    }

    if (!current_nic || !network_ready) {
        terminal_write_string("  [NTP] Error: Adaptador de red no activo o DHCP no asignado.\n");
        return;
    }

    terminal_write_string("  [NTP] Sincronizando con ");
    terminal_write_string(server);
    terminal_write_string("...\n");

    int sock = sys_lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        terminal_write_string("  [NTP] Error al crear socket UDP.\n");
        return;
    }

    struct sockaddr_in_old addr;
    addr.sin_family = AF_INET;
    addr.sin_port = bswap_32(123) >> 16; // Port 123 (Network byte order)

    // Resolve IP dynamically
    uint32_t resolved_ip = 0;
    if (net_gethostbyname(server, &resolved_ip) == 0) {
        // net_gethostbyname returns host byte order (IP4_ADDR_GET_U32)
        // lwIP stack in our wrapper expects host byte order too for sin_addr
        addr.sin_addr = bswap_32(resolved_ip);
    } else {
        terminal_write_string("  [NTP] Resolucion DNS fallo. Abortando.\n");
        sys_lwip_close(sock);
        return;
    }

    if (sys_lwip_connect(sock, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        terminal_write_string("  [NTP] Error al conectar.\n");
        sys_lwip_close(sock);
        return;
    }

    // NTP packet is 48 bytes
    uint8_t packet[48];
    memset(packet, 0, sizeof(packet));

    // Set LI=0, VN=3, Mode=3 (Client)
    packet[0] = 0x1B;

    terminal_write_string("  [NTP] Enviando request...\n");
    if (sys_lwip_send(sock, packet, sizeof(packet), 0) < 0) {
        terminal_write_string("  [NTP] Error al enviar.\n");
        sys_lwip_close(sock);
        return;
    }

    terminal_write_string("  [NTP] Esperando respuesta...\n");

    int len = sys_lwip_recv(sock, packet, sizeof(packet), 0);
    if (len < 48) {
        terminal_write_string("  [NTP] Respuesta invalida o timeout.\n");
        sys_lwip_close(sock);
        return;
    }

    sys_lwip_close(sock);

    // Transmit Timestamp is at offset 40 (seconds) and 44 (fraction)
    uint32_t ntp_secs = ((uint32_t)packet[40] << 24) |
                        ((uint32_t)packet[41] << 16) |
                        ((uint32_t)packet[42] << 8)  |
                         (uint32_t)packet[43];

    if (ntp_secs == 0) {
        terminal_write_string("  [NTP] Timestamp nulo recibido.\n");
        return;
    }

    time_t unix_time;
    if ((ntp_secs & 0x80000000) == 0) {
        // MSB is 0, NTP timestamp has rolled over to era 1 (starts in 2036)
        unix_time = (time_t)ntp_secs + 0x100000000ull - NTP_TIMESTAMP_DELTA;
    } else {
        // Era 0
        unix_time = (time_t)ntp_secs - NTP_TIMESTAMP_DELTA;
    }

    rtc_time_t new_time;
    unix_to_rtc(unix_time, &new_time);

    rtc_set_time(&new_time);

    terminal_write_string("  [NTP] Reloj sincronizado exitosamente (UTC).\n");
}

void cmd_time(const char* args) {
    (void)args;
    rtc_time_t utc, local;

    rtc_get_time(&utc);
    rtc_get_local_time(&utc, &local);

    char buf[16];

    terminal_write_string("\n  Hora Local:  ");
    itoa_s(local.hours, buf, sizeof(buf), 10);
    if(local.hours < 10) terminal_write_string("0");
    terminal_write_string(buf);
    terminal_write_string(":");
    itoa_s(local.minutes, buf, sizeof(buf), 10);
    if(local.minutes < 10) terminal_write_string("0");
    terminal_write_string(buf);
    terminal_write_string(":");
    itoa_s(local.seconds, buf, sizeof(buf), 10);
    if(local.seconds < 10) terminal_write_string("0");
    terminal_write_string(buf);

    terminal_write_string("  (Offset: ");
    int8_t tz = rtc_get_timezone();
    itoa_s(tz, buf, sizeof(buf), 10);
    if (tz > 0) terminal_write_string("+");
    terminal_write_string(buf);
    terminal_write_string(")\n");

    terminal_write_string("  Hora UTC:    ");
    itoa_s(utc.hours, buf, sizeof(buf), 10);
    if(utc.hours < 10) terminal_write_string("0");
    terminal_write_string(buf);
    terminal_write_string(":");
    itoa_s(utc.minutes, buf, sizeof(buf), 10);
    if(utc.minutes < 10) terminal_write_string("0");
    terminal_write_string(buf);
    terminal_write_string(":");
    itoa_s(utc.seconds, buf, sizeof(buf), 10);
    if(utc.seconds < 10) terminal_write_string("0");
    terminal_write_string(buf);
    terminal_write_string("\n\n\n");
}
