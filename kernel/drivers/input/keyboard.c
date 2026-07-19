/**
 * éterOS - Driver de Teclado PS/2
 * Copyright (c) 2025 Tudex Networks. All rights reserved.
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
#include <input/event.h>
#include <sem.h>

/* ========================================================================= */
/* Ring Buffer                                                               */
/* ========================================================================= */
#define KB_BUFFER_SIZE  256

static volatile char    kb_buffer[KB_BUFFER_SIZE];
static volatile uint8_t kb_write_pos = 0;
static volatile uint8_t kb_read_pos  = 0;
static sem_t kb_sem;

static void kb_buffer_put(char c) {
    uint8_t next = (kb_write_pos + 1) % KB_BUFFER_SIZE;
    if (next != kb_read_pos) {   /* No sobreescribir si está lleno */
        kb_buffer[kb_write_pos] = c;
        kb_write_pos = next;
        sem_signal(&kb_sem);
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

/* Scancodes normales (sin Shift) - ES (Spanish) */
static const char scancode_to_ascii_es[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',    /* 0x00-0x07 */
    '7',  '8',  '9',  '0',  '\'', 173,  '\b', '\t',   /* 0x08-0x0F  (173 = ¡) */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',    /* 0x10-0x17 */
    'o',  'p',  '`',  '+',  '\n', 0,    'a',  's',    /* 0x18-0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  164,    /* 0x20-0x27  (164 = ñ) */
    '{',  '|',  0,    '}',  'z',  'x',  'c',  'v',    /* 0x28-0x2F */
    'b',  'n',  'm',  ',',  '.',  '-',  0,    '*',    /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,      /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x40-0x47 */
    0,    0,    '-',  0,    0,    0,    '+',  0,      /* 0x48-0x4F */
    0,    0,    0,    0,    0,    0,    '<',  0,      /* 0x50-0x57 */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x58-0x5F */
};

/* Scancodes con Shift presionado - ES (Spanish) */
static const char scancode_to_ascii_shift_es[128] = {
    0,    27,   '!',  '"',  '#',  '$',  '%',  '&',    /* 0x00-0x07 */
    '/',  '(',  ')',  '=',  '?',  168,  '\b', '\t',   /* 0x08-0x0F  (168 = ¿) */
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',    /* 0x10-0x17 */
    'O',  'P',  '^',  '*',  '\n', 0,    'A',  'S',    /* 0x18-0x1F */
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  165,    /* 0x20-0x27  (165 = Ñ) */
    '[',  ']',  0,    '\\', 'Z',  'X',  'C',  'V',    /* 0x28-0x2F */
    'B',  'N',  'M',  ';',  ':',  '_',  0,    '*',    /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,      /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x40-0x47 */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x48-0x4F */
    0,    0,    0,    0,    0,    0,    '>',  0,      /* 0x50-0x57 */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x58-0x5F */
};

static const char* current_map = scancode_to_ascii;
static const char* current_map_shift = scancode_to_ascii_shift;

/* ========================================================================= */
/* Procesamiento de Scancodes                                                */
/* ========================================================================= */

#include <gfx/window.h>

/* Estado para scancodes extendidos (prefijo 0xE0) */
static volatile bool extended_scancode = false;

void compositor_wake(void);

void keyboard_process_scancode(uint8_t scancode) {
    /* Wake up the compositor on input */
    compositor_wake(); // Re-enabled to wake up compositor screen sleep

    /* Detectar prefijo de scancode extendido */
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }

    /* Report event to input subsystem with basic mapping */
    bool released = scancode & 0x80;
    uint8_t raw = scancode & 0x7F;
    uint16_t code = 0;

    if (extended_scancode) {
        /* Map extended keys */
        switch (raw) {
            case 0x48: code = KEY_UP; break;
            case 0x50: code = KEY_DOWN; break;
            case 0x4B: code = KEY_LEFT; break;
            case 0x4D: code = KEY_RIGHT; break;
            case 0x47: code = KEY_HOME; break;
            case 0x4F: code = KEY_END; break;
            case 0x49: code = KEY_PAGEUP; break;
            case 0x51: code = KEY_PAGEDOWN; break;
            case 0x53: code = KEY_DELETE; break;
            case 0x1C: code = KEY_KPENTER; break;
            default: code = 0; /* Unknown extended */
        }
    } else {
        /* Standard Set 1 Scancodes map well to Linux keycodes for main block */
        code = raw;
    }

    if (code != 0) {
        input_push(EV_KEY, code, released ? 0 : 1);
    }

    /* Procesar scancode extendido (teclas especiales) */
    if (extended_scancode) {
        extended_scancode = false;

        /* Solo press, no release (bit 7) */
        if (scancode & 0x80) return;

        switch (scancode) {
            case 0x48: kb_buffer_put((char)KB_KEY_UP);     return;
            case 0x50: kb_buffer_put((char)KB_KEY_DOWN);   return;
            case 0x4B: kb_buffer_put((char)KB_KEY_LEFT);   return;
            case 0x4D: kb_buffer_put((char)KB_KEY_RIGHT);  return;
            case 0x47: kb_buffer_put((char)KB_KEY_HOME);   return;
            case 0x4F: kb_buffer_put((char)KB_KEY_END);    return;
            case 0x53: kb_buffer_put((char)KB_KEY_DELETE); return;
            case 0x49: kb_buffer_put((char)KB_KEY_PGUP);   return;
            case 0x51: kb_buffer_put((char)KB_KEY_PGDOWN); return;
        }
        return;
    }

    /* --- Key Release (bit 7 = 1) --- */
    if (scancode & 0x80) {
        uint8_t released_code = scancode & 0x7F;

        switch (released_code) {
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
        c = current_map_shift[scancode];
    } else {
        c = current_map[scancode];
    }

    /* Aplicar Caps Lock solo a letras */
    if (caps_lock && c >= 'a' && c <= 'z') {
        c -= 32;
    } else if (caps_lock && c >= 'A' && c <= 'Z' && !shift_pressed) {
        /* Caps + Shift = minúscula */
    }

    /* Ctrl+C → carácter especial */
    if (ctrl_pressed && (c == 'c' || c == 'C')) {
        c = 3;
    }

    /* Ctrl+L → clear screen */
    if (ctrl_pressed && (c == 'l' || c == 'L')) {
        c = 12;
    }

    if (c != 0) {
        kb_buffer_put(c);
    }
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

static void kb_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        /* Wait for data (Bit 0 = 1) */
        while (timeout--) {
            if ((inb(KB_STATUS_PORT) & 1) == 1) return;
        }
    } else {
        /* Wait for buffer empty (Bit 1 = 0) */
        while (timeout--) {
            if ((inb(KB_STATUS_PORT) & 2) == 0) return;
        }
    }
}

static void kb_write(uint8_t data) {
    kb_wait(1);
    outb(KB_DATA_PORT, data);
}

static uint8_t kb_read(void) {
    kb_wait(0);
    return inb(KB_DATA_PORT);
}

void keyboard_init(void) {
    kb_write_pos = 0;
    kb_read_pos  = 0;
    shift_pressed = false;
    caps_lock = false;
    ctrl_pressed = false;
    sem_init(&kb_sem, 0);

    /* Vaciar el buffer del controlador PS/2 */
    while (inb(KB_STATUS_PORT) & 0x01) {
        inb(KB_DATA_PORT);
    }
}

bool keyboard_has_input(void) {
    return kb_read_pos != kb_write_pos;
}

char keyboard_getchar(void) {
    /* Esperar hasta que haya un carácter disponible usando semáforo */
    sem_wait(&kb_sem);

    char c = kb_buffer[kb_read_pos];
    kb_read_pos = (kb_read_pos + 1) % KB_BUFFER_SIZE;
    return c;
}

void keyboard_set_layout(int layout) {
    /* 0 = EN (US), 1 = ES */
    if (layout == 1) {
        current_map = scancode_to_ascii_es;
        current_map_shift = scancode_to_ascii_shift_es;
    } else {
        current_map = scancode_to_ascii;
        current_map_shift = scancode_to_ascii_shift;
    }
}

void keyboard_set_typematic(uint8_t delay, uint8_t rate) {
    /* delay: 0-3 (250ms - 1000ms), rate: 0-31 (30Hz - 2Hz) */
    uint8_t byte = (delay << 5) | (rate & 0x1F);

    kb_write(0xF3); // Set typematic rate and delay command
    kb_read();      // Wait for ACK (0xFA)

    kb_write(byte);
    kb_read();      // Wait for ACK
}
