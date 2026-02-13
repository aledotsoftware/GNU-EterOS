/**
 * =============================================================================
 * éterOS - SantiTravel v1.1
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * Primera aplicación nativa del ecosistema éterOS.
 *
 * Registro de viajes y aventuras con Santi.
 * Interfaz basada en menú (80x25 VGA, sin scroll).
 *
 * v1.1 — Menú interactivo con pantallas separadas.
 *
 * =============================================================================
 */

#include "../../include/santitravel.h"
#include "../../include/vga.h"
#include "../../include/keyboard.h"
#include "../../include/string.h"
#include "../../include/io.h"
#include "../../include/timer.h"

/* ========================================================================= */
/* Datos de destinos                                                         */
/* ========================================================================= */

typedef struct {
    const char* city;
    const char* country;
    const char* code;
    uint8_t     visited;
} destination_t;

static const destination_t destinations[] = {
    /* ===== VISITADOS ===== */
    { "Posadas",          "Argentina",  "MIS", 1 },
    { "Cataratas Iguazu", "Argentina",  "IGU", 1 },
    { "Ciudad del Este",  "Paraguay",   "PY ", 1 },
    { "Foz de Iguazu",    "Brasil",     "BR ", 1 },
    { "Buenos Aires",     "Argentina",  "BUE", 1 },
    { "Ushuaia",          "Argentina",  "USH", 1 },
    { "Tolhuin",          "Argentina",  "TDF", 1 },
    { "Rio Grande",       "Argentina",  "TDF", 1 },
    { "Mendoza",          "Argentina",  "MDZ", 1 },
    { "Santiago",         "Chile",      "SCL", 1 },
    { "Vina del Mar",     "Chile",      "VNA", 1 },

    /* ===== POR VISITAR ===== */
    { "Bariloche",        "Argentina",  "BRC", 0 },
    { "El Calafate",      "Argentina",  "FTE", 0 },
    { "Salta",            "Argentina",  "SLA", 0 },
    { "Rio de Janeiro",   "Brasil",     "GIG", 0 },
    { "Cusco",            "Peru",       "CUZ", 0 },
    { "Cancun",           "Mexico",     "CUN", 0 },
    { "Bogota",           "Colombia",   "BOG", 0 },
    { "Cartagena",        "Colombia",   "CTG", 0 },
    { "Lima",             "Peru",       "LIM", 0 },
    { "Montevideo",       "Uruguay",    "MVD", 0 },
    { "Nueva York",       "EEUU",       "JFK", 0 },
    { "Tokyo",            "Japon",      "NRT", 0 },
    { "Barcelona",        "Espana",     "BCN", 0 },
    { "Roma",             "Italia",     "FCO", 0 },
};

#define NUM_DESTINATIONS  (sizeof(destinations) / sizeof(destinations[0]))

/* ========================================================================= */
/* Helpers de pantalla                                                       */
/* ========================================================================= */

/** Dibuja una línea horizontal decorativa */
static void draw_separator(void) {
    terminal_write_colored(
        "  --------------------------------------------------------\n",
        VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

/** Dibuja el mini-título para pantallas internas */
static void draw_header(const char* title) {
    terminal_initialize(NULL);
    terminal_write_string("\n");
    terminal_write_colored("  SantiTravel", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(" > ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored(title, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    draw_separator();
}

/** Dibuja el footer universal */
static void draw_footer(const char* hint) {
    terminal_write_string("\n");
    draw_separator();
    terminal_write_colored("  ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored(hint, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_string("\n");
}

/* ========================================================================= */
/* Animación del avión                                                       */
/* ========================================================================= */

static void anim_frame(const char* line1, const char* line2,
                       const char* line3, const char* line4,
                       const char* line5, uint32_t wait) {
    terminal_initialize(NULL);
    /* Centrado vertical: empezar en fila ~8 */
    terminal_write_string("\n\n\n\n\n\n\n\n");
    terminal_write_colored(line1, VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored(line2, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored(line3, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored(line4, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored(line5, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    timer_wait(wait);
}

static void run_animation(void) {
    /* Frame 1: En tierra */
    anim_frame(
        "                    __|__\n",
        "             --o--o--(  )--o--o--\n",
        "               SANTI TRAVEL\n",
        "             __|____________|__\n",
        "   ========================================\n",
        500
    );

    /* Frame 2: Motores encendidos */
    anim_frame(
        "                    __|__\n",
        "          >>>--o--o--(  )--o--o-->>>\n",
        "               SANTI TRAVEL\n",
        "             __|____________|__\n",
        "   ====*=====*======*======*=====*====\n",
        400
    );

    /* Frame 3: Despegando */
    terminal_initialize(NULL);
    terminal_write_string("\n\n\n\n\n\n");
    terminal_write_colored("                    __|__\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("          >>>--o--o--(  )--o--o-->>>\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("               SANTI TRAVEL\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("                  |    |\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    terminal_write_colored("   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", VGA_COLOR_BLUE, VGA_COLOR_BLACK);
    timer_wait(400);

    /* Frame 4: En el aire */
    terminal_initialize(NULL);
    terminal_write_string("\n\n\n");
    terminal_write_colored("          .    *         .       *\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("     *            .                  .\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("                    __|__\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("          >>>--o--o--(  )--o--o-->>>\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("               SANTI TRAVEL", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("        VAMOS!\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_string("\n");
    terminal_write_colored("      .         *              .       *\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\n\n\n\n\n\n");
    terminal_write_colored("   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", VGA_COLOR_BLUE, VGA_COLOR_BLACK);
    timer_wait(600);

    /* Frame 5: Volando alto */
    terminal_initialize(NULL);
    terminal_write_colored("\n", VGA_COLOR_BLACK, VGA_COLOR_BLACK);
    terminal_write_colored("          .    *         .       *\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("                    __|__\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("          >>>--o--o--(  )--o--o-->>>\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("               SANTI TRAVEL\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("      .         *              .       *\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    terminal_write_colored("   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", VGA_COLOR_BLUE, VGA_COLOR_BLACK);
    timer_wait(800);
}

/* ========================================================================= */
/* Pantalla: Menú principal                                                  */
/* ========================================================================= */

static void count_stats(uint32_t* visited, uint32_t* pending) {
    *visited = 0;
    *pending = 0;
    for (size_t i = 0; i < NUM_DESTINATIONS; i++) {
        if (destinations[i].visited) (*visited)++;
        else (*pending)++;
    }
}

static void show_menu(void) {
    uint32_t visited, pending;
    count_stats(&visited, &pending);

    char buf[8];

    terminal_initialize(NULL);
    terminal_write_string("\n");

    /* ---- Título ASCII ---- */
    terminal_write_colored(
        "     ___           _   _ _____                    _ \n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "    / __| __ _ _ _| |_(_)_   _| _ __ ___ _  _____| |\n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "    \\__ \\/ _` | ' \\  _| | | || '_/ _` \\ V / -_) |\n",
        VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        "    |___/\\__,_|_||_\\__|_| |_||_| \\__,_|\\_/\\___|_|\n",
        VGA_COLOR_BLUE, VGA_COLOR_BLACK);

    terminal_write_string("\n");
    draw_separator();

    /* ---- Estadísticas ---- */
    terminal_write_colored("  Visitados: ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    itoa_s(visited, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_colored("   Por visitar: ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    itoa_s(pending, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_colored("   Total: ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    itoa_s(visited + pending, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n");

    draw_separator();
    terminal_write_string("\n");

    /* ---- Opciones del menú ---- */
    terminal_write_colored("   [1] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Lugares conocidos\n\n");

    terminal_write_colored("   [2] ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    terminal_write_string("Lugares por conocer\n\n");

    terminal_write_colored("   [0] ", VGA_COLOR_RED, VGA_COLOR_BLACK);
    terminal_write_string("Volver al shell\n");

    terminal_write_string("\n\n");
    draw_separator();
    terminal_write_colored("  Presiona 1, 2 o 0: ",
                          VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

/* ========================================================================= */
/* Pantalla: Listado de destinos (Genérica)                                  */
/* ========================================================================= */

static void show_destinations_list(bool show_visited) {
    const char* header = show_visited ? "Lugares Conocidos" : "Lugares por Conocer";
    draw_header(header);
    terminal_write_string("\n");

    uint32_t n = 0;

    /* Configuración visual según el modo */
    vga_color_t main_color = show_visited ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_LIGHT_MAGENTA;
    const char* bullet     = show_visited ? "   * " : "   o ";
    vga_color_t city_color = show_visited ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_GREY;
    vga_color_t country_color = show_visited ? VGA_COLOR_LIGHT_GREY : VGA_COLOR_DARK_GREY;

    for (size_t i = 0; i < NUM_DESTINATIONS; i++) {
        bool is_visited = (destinations[i].visited != 0);
        if (is_visited != show_visited) continue;

        n++;

        terminal_write_colored(bullet, main_color, VGA_COLOR_BLACK);
        terminal_write_colored("[", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(destinations[i].code, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        terminal_write_colored("] ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

        terminal_write_colored(destinations[i].city, city_color, VGA_COLOR_BLACK);

        terminal_write_colored(", ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(destinations[i].country, country_color, VGA_COLOR_BLACK);
        terminal_write_string("\n");
    }

    char buf[8];
    terminal_write_string("\n");

    const char* label  = show_visited ? "  Total: " : "  Faltan: ";
    const char* suffix = show_visited ? " destinos visitados\n" : " destinos por descubrir!\n";

    terminal_write_colored(label, main_color, VGA_COLOR_BLACK);
    itoa_s(n, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored(suffix, main_color, VGA_COLOR_BLACK);

    draw_footer("Presiona cualquier tecla para volver al menu...");
    keyboard_getchar();
}

/* ========================================================================= */
/* Entry Point                                                               */
/* ========================================================================= */

void santitravel_run(void) {
    /* ---- Animación de despegue ---- */
    run_animation();

    /* ---- Loop del menú ---- */
    for (;;) {
        show_menu();

        char c = keyboard_getchar();

        switch (c) {
            case '1':
                show_destinations_list(true);
                break;
            case '2':
                show_destinations_list(false);
                break;
            case '0':
            case 27:   /* ESC */
                return;  /* Volver al shell */
            default:
                break;
        }
    }
}
