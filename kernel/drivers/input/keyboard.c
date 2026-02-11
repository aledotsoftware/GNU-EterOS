/**
 * éterOS - Driver de Teclado PS/2
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Maneja scancodes Set 1 del teclado PS/2 estándar.
 * Usa un ring buffer para desacoplar el handler de IRQ
 * del consumidor (shell).
 *
 * Modular: Solo depende de types.h. No conoce al shell,
 * VGA, ni ningún otro subsistema.
 */

#include "../../../include/keyboard.h"
#include "../../../include/io.h"

/* ========================================================================= */
/* Ring Buffer                                                               */
/* ========================================================================= */
#define KB_BUFFER_SIZE  256

static volatile char    kb_buffer[KB_BUFFER_SIZE];
static volatile uint8_t kb_write_pos = 0;
static volatile uint8_t kb_read_pos  = 0;

static void kb_buffer_put(char c) {
    uint8_t next = (kb_write_pos + 1) % KB_BUFFER_SIZE;
    if (next != kb_read_pos) {   /* No sobreescribir si está lleno */
        kb_buffer[kb_write_pos] = c;
        kb_write_pos = next;
    }
}

/* ========================================================================= */
/* Estado del teclado                                                        */
/* ========================================================================= */
static volatile bool shift_pressed   = false;
static volatile bool caps_lock       = false;
static volatile bool ctrl_pressed    = false;

/* ========================================================================= */
/* Tabla de Scancodes (Set 1, US QWERTY)                                     */
/* ========================================================================= */

/* Scancodes normales (sin Shift) */
static const char scancode_to_ascii[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',    /* 0x00-0x07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',   /* 0x08-0x0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',    /* 0x10-0x17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',     /* 0x18-0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',    /* 0x20-0x27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',    /* 0x28-0x2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',     /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,        /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    0,        /* 0x40-0x47 */
    0,    0,    '-',  0,    0,    0,    '+',  0,        /* 0x48-0x4F */
    0,    0,    0,    0,    0,    0,    0,    0,        /* 0x50-0x57 */
    0,    0,    0,    0,    0,    0,    0,    0,        /* 0x58-0x5F */
};

/* Scancodes con Shift presionado */
static const char scancode_to_ascii_shift[128] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',    /* 0x00-0x07 */
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',   /* 0x08-0x0F */
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',    /* 0x10-0x17 */
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',     /* 0x18-0x1F */
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',    /* 0x20-0x27 */
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',    /* 0x28-0x2F */
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',     /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,        /* 0x38-0x3F */
};

/* ========================================================================= */
/* Procesamiento de Scancodes                                                */
/* ========================================================================= */

void keyboard_process_scancode(uint8_t scancode) {
    /* --- Key Release (bit 7 = 1) --- */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;

        switch (released) {
            case 0x2A: /* Left Shift  */
            case 0x36: /* Right Shift */
                shift_pressed = false;
                break;
            case 0x1D: /* Left Ctrl */
                ctrl_pressed = false;
                break;
        }
        return;
    }

    /* --- Key Press --- */
    switch (scancode) {
        case 0x2A: /* Left Shift */
        case 0x36: /* Right Shift */
            shift_pressed = true;
            return;

        case 0x1D: /* Left Ctrl */
            ctrl_pressed = true;
            return;

        case 0x3A: /* Caps Lock toggle */
            caps_lock = !caps_lock;
            return;
    }

    /* Ignorar scancodes fuera de rango */
    if (scancode >= 128) return;

    /* Obtener el carácter ASCII */
    char c;
    if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }

    /* Aplicar Caps Lock solo a letras */
    if (caps_lock && c >= 'a' && c <= 'z') {
        c -= 32;  /* Convertir a mayúscula */
    } else if (caps_lock && c >= 'A' && c <= 'Z' && !shift_pressed) {
        /* Caps + Shift = minúscula (comportamiento estándar) */
    }

    /* Ctrl+C → carácter especial */
    if (ctrl_pressed && (c == 'c' || c == 'C')) {
        c = 3;  /* ETX (End of Text) */
    }

    /* Ctrl+L → clear screen shortcut */
    if (ctrl_pressed && (c == 'l' || c == 'L')) {
        c = 12; /* Form Feed */
    }

    if (c != 0) {
        kb_buffer_put(c);
    }
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

void keyboard_init(void) {
    kb_write_pos = 0;
    kb_read_pos  = 0;
    shift_pressed = false;
    caps_lock = false;
    ctrl_pressed = false;

    /* Vaciar el buffer del controlador PS/2 */
    while (inb(KB_STATUS_PORT) & 0x01) {
        inb(KB_DATA_PORT);
    }
}

bool keyboard_has_input(void) {
    return kb_read_pos != kb_write_pos;
}

char keyboard_getchar(void) {
    /* Esperar hasta que haya un carácter disponible */
    while (!keyboard_has_input()) {
        __asm__ volatile ("hlt");  /* Bajo consumo hasta la próxima IRQ */
    }

    char c = kb_buffer[kb_read_pos];
    kb_read_pos = (kb_read_pos + 1) % KB_BUFFER_SIZE;
    return c;
}
