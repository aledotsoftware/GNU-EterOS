/*
 * Demo de interfaz gráfica multitarea (Flux UI Prototype)
 */
#include <ui/window.h>
#include <task.h>
#include <types.h>
#include <string.h>
#include <vga.h>
#include <timer.h>
#include <keyboard.h> /* Para detectar ESC */
#include <framebuffer.h>

static window_t* win_counter;
static window_t* win_status;
static volatile bool gui_running = true;

/* Estado compartido */
static volatile int shared_counter = 0;
static volatile int shared_status_pos = 0;
static volatile int shared_status_dir = 1;

/* Tarea 1: Incrementa contador (Lógica pura, sin dibujado) */
void task_gui_counter(void) {
    while (gui_running) {
        shared_counter++;
        /* Delay simulado */
        for(volatile int i=0; i<5000000; i++); 
        task_yield();
    }
    task_exit();
}

/* Tarea 2: Actualiza posición barra (Lógica pura) */
void task_gui_status(void) {
    while (gui_running) {
        shared_status_pos += shared_status_dir * 5;
        if (shared_status_pos > 250 || shared_status_pos < 0) shared_status_dir *= -1;
        
        /* Delay simulado */
        for(volatile int i=0; i<2000000; i++);
        task_yield();
    }
    task_exit();
}

void gui_demo_run(void) {
    /* Inicializar WM y Video */
    wm_init();
    framebuffer_enable_double_buffer();
    wm_redraw_desktop(); /* Dibuja fondo en backbuffer */
    
    /* Crear ventanas */
    win_counter = wm_create_window(50, 50, 300, 200, "Flux Counter");
    win_status  = wm_create_window(400, 100, 300, 150, "System Monitor");
    
    gui_running = true;
    
    /* Tareas worker */
    int pid1 = task_create("gui_counter", task_gui_counter);
    int pid2 = task_create("gui_status", task_gui_status);
    
    /* Flush inputs iniciales */
    while (keyboard_has_input()) keyboard_getchar();

    /* Bucle Principal: Input + Render + Flush */
    while (gui_running) {
        
        /* 1. INPUT */
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            unsigned char uc = (unsigned char)c;
            if (c == 27) gui_running = false; /* ESC Exit */
            
            /* Movimiento ventana status con teclado */
            if (uc == KEY_UP) wm_move_window(win_status, 0, -10);
            if (uc == KEY_DOWN) wm_move_window(win_status, 0, 10);
            if (uc == KEY_LEFT) wm_move_window(win_status, -10, 0);
            if (uc == KEY_RIGHT) wm_move_window(win_status, 10, 0);
        }

        /* 2. RENDER (Solo el main thread dibuja) */
        /* Limpiar ventanas (rellenar fondo) */
        wm_fill_rect(win_counter, (rect_t){0, 0, 300, 200}, UI_COLOR_DARK); /* Hack: clear content area */
        wm_fill_rect(win_status, (rect_t){0, 0, 300, 150}, UI_COLOR_DARK);

        /* 2a. Dibujar Contenido Counter */
        char buffer[32];
        itoa_s(shared_counter, buffer, sizeof(buffer), 10);
        char msg[32] = "Count: ";
        size_t len = strlen(msg);
        strlcpy(msg + len, buffer, sizeof(msg) - len);
        wm_print_at(win_counter, 10, 10, msg);

        /* 2b. Dibujar Contenido Status */
        wm_print_at(win_status, 10, 10, "System Status: Running");
        rect_t bar = {10 + shared_status_pos, 30, 20, 10};
        wm_fill_rect(win_status, bar, UI_COLOR_CYAN);
        
        wm_print_at(win_status, 10, 80, "Arrows: Move Win | ESC: Exit");

        /* 3. COMPOSITE & FLUSH */
        /* Redibujar fondo y ventanas en orden */
        /* Optimización: wm_redraw_desktop() limpia y redibuja todo. 
           Idealmente solo limpiaríamos regiones sucias, pero esto elimina flickering. */
        wm_redraw_desktop(); 
        
        /* Copiar a VRAM */
        framebuffer_flush();
        
        /* 4. Yield */
        task_yield();
    }
    
    task_kill(pid1);
    task_kill(pid2);
    terminal_initialize(NULL);
}
