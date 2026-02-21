/**
 * éterOS - Driver de Mouse PS/2
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Implementación del driver de mouse PS/2 básico (3 bytes packet).
 * Maneja la inicialización del puerto auxiliar y la decodificación de paquetes.
 */

#include <mouse.h>
#include <io.h>
#include <serial.h>
#include <pic.h>
#include <vga.h> /* Para debug si es necesario */
#include <input/event.h>

/* Comandos del Mouse (enviados al puerto de datos 0x60) */
#define MOUSE_RESET         0xFF
#define MOUSE_RESEND        0xFE
#define MOUSE_SET_DEFAULTS  0xF6
#define MOUSE_DISABLE_DATA  0xF5
#define MOUSE_ENABLE_DATA   0xF4
#define MOUSE_SET_SAMPLE    0xF3
#define MOUSE_GET_ID        0xF2
#define MOUSE_SET_REMOTE    0xF0
#define MOUSE_SET_WRAP      0xEE
#define MOUSE_RESET_WRAP    0xEC
#define MOUSE_READ_DATA     0xEB
#define MOUSE_SET_STREAM    0xEA
#define MOUSE_STATUS_REQ    0xE9
#define MOUSE_SET_RES       0xE8
#define MOUSE_SET_SCALE2    0xE7
#define MOUSE_SET_SCALE1    0xE6

/* Comandos del Controlador PS/2 (enviados a 0x64) */
#define PS2_CMD_WRITE_AUX   0xD4    /* Escribir al dispositivo auxiliar (mouse) */
#define PS2_CMD_GET_CONFIG  0x20    /* Leer byte de configuración */
#define PS2_CMD_SET_CONFIG  0x60    /* Escribir byte de configuración */
#define PS2_CMD_ENABLE_AUX  0xA8    /* Habilitar puerto auxiliar */

/* Puertos */
#define PORT_DATA           0x60
#define PORT_STATUS         0x64

/* Estado interno del driver */
static uint8_t mouse_cycle = 0;     /* 0, 1, 2 para los 3 bytes del paquete */
static uint8_t mouse_bytes[3];      /* Buffer para los 3 bytes */
static mouse_callback_t active_callback = (void*)0;
static uint8_t prev_buttons = 0;

/* Espera a que el buffer de entrada (0x60) esté vacío para poder escribir */
static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        /* Esperar a que llegue data (Bit 0 de Status == 1) para LEER */
        while (timeout--) {
            if ((inb(PORT_STATUS) & 1) == 1) {
                return;
            }
        }
        /* Timeout, pero intentamos leer igual */
    } else {
        /* Esperar a que el buffer de entrada esté vacío (Bit 1 de Status == 0) para ESCRIBIR */
        while (timeout--) {
            if ((inb(PORT_STATUS) & 2) == 0) {
                return;
            }
        }
    }
}

/* Escribe un byte al mouse (a través del puerto auxiliar del PS/2) */
static void mouse_write(uint8_t command) {
    /* Avisar al controlador que el siguiente byte es para el dispositivo auxiliar */
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_WRITE_AUX);
    
    /* Enviar comando */
    mouse_wait(1);
    outb(PORT_DATA, command);
}

/* Lee un byte del mouse (esperando respuesta) */
static uint8_t mouse_read(void) {
    mouse_wait(0); /* Esperar a que haya dato */
    return inb(PORT_DATA);
}

void mouse_init(void) {
    uint8_t status;

    serial_write_string("[MOUSE] Inicializando PS/2 Mouse...\n");

    /* 1. Habilitar puerto auxiliar */
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_ENABLE_AUX);

    /* 2. Habilitar interrupciones para IRQ 12 (Auxiliary Device) */
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_GET_CONFIG);
    mouse_wait(0);
    status = inb(PORT_DATA);
    status |= 2; /* Bit 1: Enable IRQ 12 */
    status &= ~0x20; /* Clear bit 5 (disable mouse clock -> enable it) Wait no, bit 5 is disable mouse usually? In config byte: bit 5 is disable mouse port. 0 = enable. */
    /* Config byte:
       Bit 0: Enable IRQ 1 (Keyboard)
       Bit 1: Enable IRQ 12 (Mouse)
       Bit 4: Disable Keyboard (1=disable)
       Bit 5: Disable Mouse (1=disable)
    */
    /* Queremos Bit 1 (IRQ12) activado y Bit 5 (Disable Mouse) desactivado */
    
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_SET_CONFIG);
    mouse_wait(1);
    outb(PORT_DATA, status);

    /* 3. Resetear defaults del mouse */
    mouse_write(MOUSE_SET_DEFAULTS);
    mouse_read(); /* Acknowledge (0xFA) */

    /* 4. Habilitar reporte de datos */
    mouse_write(MOUSE_ENABLE_DATA);
    mouse_read(); /* Acknowledge (0xFA) */
    
    /* Desenmascarar IRQ12 en el PIC también */
    pic_unmask_irq(12);

    /* Habilitar IRQ2 (Cascade) just in case (Master PIC IRQ2 connected to Slave) */
    pic_unmask_irq(2);

    serial_write_string("[MOUSE] Driver inicializado y IRQ12 desenmascarada.\n");
}

/* Registra un callback para recibir eventos */
void mouse_set_callback(mouse_callback_t callback) {
    active_callback = callback;
}

/* Procesa un byte recibido de la interrupción */
void mouse_process_byte(uint8_t byte) {
    /* 
     * El paquete estándar PS/2 tiene 3 bytes:
     * Byte 0: Y_OVERFLOW X_OVERFLOW Y_SIGN X_SIGN 1 MIDDLE RIGHT LEFT
     * Byte 1: Delta X
     * Byte 2: Delta Y
     */

    /* Sincronización: El bit 3 del primer byte siempre debe ser 1 */
    if (mouse_cycle == 0 && !(byte & 0x08)) {
        /* Desincronizado, ignorar byte y esperar inicio de paquete */
        return;
    }

    mouse_bytes[mouse_cycle++] = byte;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        /* Decodificar paquete */
        uint8_t status = mouse_bytes[0];
        int8_t dx = (int8_t)mouse_bytes[1];
        int8_t dy = (int8_t)mouse_bytes[2];

        /* Ajustar signos si el bit de signo está activado en status (aunque el cast a int8_t suele bastar si es complemento a dos) */
        /* Nota: En PS/2 byte 0, bit 4 es X sign, bit 5 es Y sign.
           Pero normalmente los device envían delta como signed byte directamente.
           Si dx/dy son > 127 o < -128 overflow bit se activa.
        */
        
        /* En algunos mouses viejos o protocolos es necesario verificar los bits de overflow */
        bool x_overflow = status & 0x40;
        bool y_overflow = status & 0x80;
        
        if (x_overflow || y_overflow) {
            /* Ignorar paquetes con overflow para evitar saltos locos */
            dx = 0;
            dy = 0;
        }

        /* Report to Input System */
        if (dx != 0) input_mouse_push(EV_REL, REL_X, dx);
        if (dy != 0) input_mouse_push(EV_REL, REL_Y, -dy); /* Invert Y */

        uint8_t buttons = status & 0x07;

        if ((buttons & 1) != (prev_buttons & 1))
            input_mouse_push(EV_KEY, BTN_LEFT, (buttons & 1));

        if ((buttons & 2) != (prev_buttons & 2))
            input_mouse_push(EV_KEY, BTN_RIGHT, (buttons & 2) ? 1 : 0);

        if ((buttons & 4) != (prev_buttons & 4))
            input_mouse_push(EV_KEY, BTN_MIDDLE, (buttons & 4) ? 1 : 0);

        prev_buttons = buttons;
        input_mouse_push(EV_SYN, 0, 0);

        /* Notificar callback */
        if (active_callback) {
            /* PS/2 Y axis is bottom-to-top usually, but screen is top-to-bottom */
            /* Invertimos DY para que coincida con coordenadas de pantalla */
            active_callback(dx, -dy, status & 0x07);
        }
    }
}
