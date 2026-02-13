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

static window_t* win_counter;
static window_t* win_status;
static volatile bool gui_running = true;

/* Tarea 1: Incrementa un contador */
void task_gui_counter(void) {
    int counter = 0;
    char buffer[32];
    
    while (gui_running) {
        /* Renderizar contador */
        wm_fill_rect(win_counter, (rect_t){10, 10, 200, 30}, UI_COLOR_DARK);
        
        itoa_s(counter++, buffer, sizeof(buffer), 10);
        char msg[32] = "Count: ";
        size_t len = strlen(msg);
        strlcpy(msg + len, buffer, sizeof(msg) - len);
        
        wm_print_at(win_counter, 10, 10, msg);
        
        /* Simular trabajo */
        for(volatile int i=0; i<10000000; i++); 
        
        /* Redibujar ventana */
        wm_draw_window(win_counter);
        
        /* Yield cooperativo (aunque el scheduler es preemptivo, es buena práctica) */
        /* task_yield(); */
    }
    
    /* Auto-terminar */
    task_exit();
}

/* Tarea 2: Muestra una barra de estado animada */
void task_gui_status(void) {
    int pos = 0;
    int dir = 1;
    
    while (gui_running) {
        /* Borrar */
        wm_fill_rect(win_status, (rect_t){10, 30, 280, 20}, UI_COLOR_DARK);
        
        /* Dibujar "barra" */
        rect_t bar = {10 + pos, 30, 20, 10};
        wm_fill_rect(win_status, bar, UI_COLOR_CYAN);
        
        wm_print_at(win_status, 10, 10, "System Status: Running");
        
        pos += dir * 5;
        if (pos > 250 || pos < 0) dir *= -1;
        
        /* Delay */
        for(volatile int i=0; i<5000000; i++);
        
        wm_draw_window(win_status);
    }
    task_exit();
}

void gui_demo_run(void) {
    /* Inicializar WM */
    wm_init();
    
    /* Limpiar pantalla (fondo escritorio) */
    terminal_initialize(NULL); /* Asegura modo gráfico */
    
    /* Dibujar fondo plano */
    /* Accessing internal framebuffer clear via primitives would be cleaner, but terminal_initialize(NULL) does it */
    
    /* Crear ventanas */
    win_counter = wm_create_window(50, 50, 300, 200, "Flux Counter");
    win_status  = wm_create_window(400, 100, 300, 150, "System Monitor");
    
    gui_running = true;
    
    /* Lanzar tareas */
    int pid1 = task_create("gui_counter", task_gui_counter);
    int pid2 = task_create("gui_status", task_gui_status);
    
    if (pid1 < 0 || pid2 < 0) {
        terminal_write_string("Error creando tareas GUI.\n");
        return;
    }
    
    /* Loop principal (Input) */
    /* La tarea "shell" (actual) se queda gestionando el input o simplemente esperando */
    /* Flush de teclas residuales */
    while (keyboard_has_input()) keyboard_getchar();

    /* Mensaje (temporalmente sobre la UI) */
    wm_print_at(win_status, 10, 80, "Press ENTER to exit");

    /* Loop de espera principal */
    while (gui_running) {
        if (keyboard_has_input()) {
            /* Salir con cualquier tecla (ENTER = 10, ESC = 27) */
            /* Simplificamos para asegurar que funcione la salida */
            char c = keyboard_getchar();
            gui_running = false;
        }
        /* Yield para no comer 100% CPU en este loop de control */
        task_yield();
    }
    
    /* Esperar un poco a que terminen */
    for(volatile int i=0; i<1000000; i++);
    
    /* Limpieza */
    task_kill(pid1);
    task_kill(pid2);
    
    terminal_initialize(NULL); /* Restaurar consola limpia */
}
