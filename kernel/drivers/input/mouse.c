/**
 * éterOS - Driver de Mouse PS/2 (Versión de Máxima Compatibilidad)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include <mouse.h>
#include <io.h>
#include <serial.h>
#include <pic.h>
#include <input/event.h>

#define PORT_DATA           0x60
#define PORT_STATUS         0x64
#define PS2_CMD_WRITE_AUX   0xD4
#define PS2_CMD_GET_CONFIG  0x20
#define PS2_CMD_SET_CONFIG  0x60
#define PS2_CMD_ENABLE_AUX  0xA8

#define MOUSE_SET_DEFAULTS  0xF6
#define MOUSE_ENABLE_DATA   0xF4
#define MOUSE_RESET         0xFF

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];
static uint8_t prev_buttons = 0;
static mouse_callback_t active_callback = (void*)0;
static uint8_t mouse_sensitivity = 5; // Default middle
static bool mouse_left_handed = false;
#define MOUSE_DEBUG_LOG_PACKETS 0
#if MOUSE_DEBUG_LOG_PACKETS
static uint32_t mouse_debug_packets = 0;

static void mouse_debug_write_i32(int32_t value) {
    char buf[16];
    int i = 0;
    uint32_t magnitude;

    if (value == 0) {
        serial_write_string("0");
        return;
    }

    if (value < 0) {
        serial_write_string("-");
        magnitude = (uint32_t)(-value);
    } else {
        magnitude = (uint32_t)value;
    }

    while (magnitude > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }

    while (i-- > 0) {
        char out[2] = { buf[i], '\0' };
        serial_write_string(out);
    }
}
#endif

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(PORT_STATUS) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(PORT_STATUS) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t command) {
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_WRITE_AUX);
    mouse_wait(1);
    outb(PORT_DATA, command);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(PORT_DATA);
}

void mouse_init(void) {
    uint8_t status;

    serial_write_string("[MOUSE] Inicializando modo compatible...\n");

    // 1. Habilitar puerto auxiliar
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_ENABLE_AUX);

    // 2. Configurar interrupciones (IRQ12)
    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_GET_CONFIG);
    status = mouse_read();
    status |= 0x02;  // Enable IRQ12
    status &= ~0x20; // Enable Mouse Clock

    mouse_wait(1);
    outb(PORT_STATUS, PS2_CMD_SET_CONFIG);
    mouse_wait(1);
    outb(PORT_DATA, status);

    // 3. Comandos básicos (sin comprobación estricta para evitar cuelgues)
    mouse_write(MOUSE_SET_DEFAULTS);
    mouse_read(); // ACK
    
    mouse_write(MOUSE_ENABLE_DATA);
    mouse_read(); // ACK

    // 4. Activar IRQs
    pic_unmask_irq(12);
    pic_unmask_irq(2);

    serial_write_string("[MOUSE] Inicializacion completada.\n");
}

void mouse_process_byte(uint8_t byte) {
    if (mouse_cycle == 0 && !(byte & 0x08)) return;

    mouse_bytes[mouse_cycle++] = byte;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        
        int32_t dx = (int8_t)mouse_bytes[1];
        int32_t dy = (int8_t)mouse_bytes[2];
        uint8_t buttons = mouse_bytes[0] & 0x07;

        static int32_t mouse_acc_x = 0;
        static int32_t mouse_acc_y = 0;

        // Apply sensitivity (default 5, lower is slower, higher is faster)
        int32_t multiplier = 256;
        if (mouse_sensitivity < 5) {
            multiplier = (mouse_sensitivity * 256) / 5;
        } else if (mouse_sensitivity > 5) {
            int32_t factor = mouse_sensitivity - 3;
            multiplier = (factor * 256) / 2;
        }

        mouse_acc_x += dx * multiplier;
        mouse_acc_y += dy * multiplier;

        dx = mouse_acc_x / 256;
        dy = mouse_acc_y / 256;

        mouse_acc_x -= dx * 256;
        mouse_acc_y -= dy * 256;

        // Apply handedness swap
        if (mouse_left_handed) {
            uint8_t new_buttons = buttons & 0x04; // Keep middle button
            if (buttons & 0x01) new_buttons |= 0x02; // Left becomes Right
            if (buttons & 0x02) new_buttons |= 0x01; // Right becomes Left
            buttons = new_buttons;
        }

        #if MOUSE_DEBUG_LOG_PACKETS
        if (mouse_debug_packets < 12 && (dx != 0 || dy != 0 || buttons != prev_buttons)) {
            serial_write_string("[MOUSE] Packet dx=");
            mouse_debug_write_i32(dx);
            serial_write_string(" dy=");
            mouse_debug_write_i32(dy);
            serial_write_string(" buttons=");
            mouse_debug_write_i32(buttons);
            serial_write_string("\n");
            mouse_debug_packets++;
        }
        #endif

        if (dx != 0) input_mouse_push(EV_REL, REL_X, dx);
        if (dy != 0) input_mouse_push(EV_REL, REL_Y, -dy);

        if ((buttons & 1) != (prev_buttons & 1)) input_mouse_push(EV_KEY, BTN_LEFT, (buttons & 1));
        if ((buttons & 2) != (prev_buttons & 2)) input_mouse_push(EV_KEY, BTN_RIGHT, (buttons & 2) >> 1);
        if ((buttons & 4) != (prev_buttons & 4)) input_mouse_push(EV_KEY, BTN_MIDDLE, (buttons & 4) >> 2);

        prev_buttons = buttons;
        input_mouse_push(EV_SYN, 0, 0);

        if (active_callback) {
            active_callback((int8_t)dx, (int8_t)-dy, buttons);
        }
    }
}

void mouse_set_callback(mouse_callback_t callback) {
    active_callback = callback;
}

void mouse_set_sensitivity(uint8_t sens) {
    if (sens < 1) sens = 1;
    if (sens > 10) sens = 10;
    mouse_sensitivity = sens;
}

void mouse_set_handedness(bool left_handed) {
    mouse_left_handed = left_handed;
}
