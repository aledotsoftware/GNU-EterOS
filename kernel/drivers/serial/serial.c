/**
 * éterOS - Implementación del Driver de Puerto Serie
 * 
 * Driver UART 16550 para depuración a través de COM1.
 * Permite enviar mensajes de log al puerto serie, 
 * capturables con QEMU usando -serial stdio.
 */

#include "../include/serial.h"
#include "../include/io.h"

/* ========================================================================= */
/* Registros del UART 16550 (offsets desde la base del puerto)               */
/* ========================================================================= */
#define UART_DATA           0   /* Datos TX/RX */
#define UART_INT_ENABLE     1   /* Habilitación de interrupciones */
#define UART_FIFO_CTRL      2   /* Control de FIFO */
#define UART_IIR            2   /* Identificación de interrupción */
#define UART_LINE_CTRL      3   /* Control de línea */
#define UART_MODEM_CTRL     4   /* Control de modem */
#define UART_LINE_STATUS    5   /* Estado de línea */
#define UART_MODEM_STATUS   6   /* Estado de modem */

/* Bits de estado de línea */
#define LSR_DATA_READY      0x01
#define LSR_TX_EMPTY        0x20

/* Bits de habilitación de interrupciones */
#define IER_RX_AVAIL        0x01
#define IER_TX_EMPTY        0x02

/* ========================================================================= */
/* Variables Globales (Buffer Circular)                                      */
/* ========================================================================= */
#define SERIAL_BUFFER_SIZE 1024

static uint8_t tx_buffer[SERIAL_BUFFER_SIZE];
static volatile uint16_t tx_head = 0;
static volatile uint16_t tx_tail = 0;

/* ========================================================================= */
/* Implementación                                                            */
/* ========================================================================= */

int serial_init(void) {
    uint16_t port = COM1_PORT;

    /* Deshabilitar todas las interrupciones inicialmente */
    outb(port + UART_INT_ENABLE, 0x00);

    /* Habilitar DLAB (Divisor Latch Access Bit) para configurar baud rate */
    outb(port + UART_LINE_CTRL, 0x80);

    /* Configurar divisor para 38400 baud (divisor = 3) */
    outb(port + UART_DATA, 0x03);       /* Byte bajo del divisor */
    outb(port + UART_INT_ENABLE, 0x00); /* Byte alto del divisor */

    /* 8 bits, sin paridad, 1 bit de stop (8N1) */
    outb(port + UART_LINE_CTRL, 0x03);

    /* Habilitar FIFO, limpiar buffers, umbral de 14 bytes */
    outb(port + UART_FIFO_CTRL, 0xC7);

    /* Habilitar DTR, RTS, OUT2 (interrupciones habilitadas por hardware) */
    outb(port + UART_MODEM_CTRL, 0x0B);

    /* Modo loopback para test */
    outb(port + UART_MODEM_CTRL, 0x1E);

    /* Enviar byte de prueba */
    outb(port + UART_DATA, 0xAE);

    /* Verificar que recibimos el mismo byte */
    if (inb(port + UART_DATA) != 0xAE) {
        return -1;  /* Puerto serie defectuoso */
    }

    /* Configurar modo normal (sin loopback) */
    outb(port + UART_MODEM_CTRL, 0x0F);

    /* Habilitar interrupciones de RX inicialmente (TX se habilita bajo demanda) */
    // Nota: Por ahora solo RX si fuera necesario, pero la lógica de TX lo habilita.
    // outb(port + UART_INT_ENABLE, IER_RX_AVAIL);

    return 0;
}

static int serial_is_transmit_empty(void) {
    return inb(COM1_PORT + UART_LINE_STATUS) & LSR_TX_EMPTY;
}

void serial_irq_handler(void) {
    /* Leer registro de identificación de interrupción */
    uint8_t iir = inb(COM1_PORT + UART_IIR);
    if (iir & 1) return; /* No hay interrupción pendiente */

    /* Verificar si el transmisor está vacío */
    if (inb(COM1_PORT + UART_LINE_STATUS) & LSR_TX_EMPTY) {
        if (tx_head != tx_tail) {
            /* Enviar siguiente byte del buffer */
            outb(COM1_PORT + UART_DATA, tx_buffer[tx_tail]);
            tx_tail = (tx_tail + 1) % SERIAL_BUFFER_SIZE;
        } else {
            /* Buffer vacío, deshabilitar interrupción TX para evitar bucle */
            uint8_t ier = inb(COM1_PORT + UART_INT_ENABLE);
            outb(COM1_PORT + UART_INT_ENABLE, ier & ~IER_TX_EMPTY);
        }
    }
}

void serial_putchar(char c) {
    uint16_t next_head = (tx_head + 1) % SERIAL_BUFFER_SIZE;

    /* Si el buffer está lleno, esperar a que el ISR libere espacio */
    while (next_head == tx_tail) {
        __asm__ volatile("pause");
    }

    /* Agregar al buffer */
    tx_buffer[tx_head] = c;
    tx_head = next_head;

    /* Habilitar interrupción TX para asegurar transmisión */
    uint8_t ier = inb(COM1_PORT + UART_INT_ENABLE);
    if (!(ier & IER_TX_EMPTY)) {
        outb(COM1_PORT + UART_INT_ENABLE, ier | IER_TX_EMPTY);
    }
}

void serial_write_string(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');   /* Agregar CR antes de LF */
        }
        serial_putchar(*str++);
    }
}

int serial_received(void) {
    return inb(COM1_PORT + UART_LINE_STATUS) & LSR_DATA_READY;
}

char serial_read(void) {
    while (!serial_received());
    return (char)inb(COM1_PORT + UART_DATA);
}
