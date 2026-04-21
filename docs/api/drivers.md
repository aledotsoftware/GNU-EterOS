# Guía de Escritura de Drivers

Esta guía describe cómo desarrollar e integrar nuevos controladores de hardware en éterOS.

## Estructura de un Driver

Un driver típico en éterOS consta de:
1.  **Inicialización**: Función que detecta y configura el hardware.
2.  **Manejador de Interrupciones**: Función que responde a eventos del hardware.
3.  **Interfaz de Operaciones**: (Opcional) Registro en DevFS para permitir acceso desde userspace.

## Registro de Drivers

Los drivers suelen inicializarse en `kmain()` o mediante la enumeración del bus PCI.

```c
void my_driver_init() {
    // 1. Detectar hardware
    // 2. Configurar registros
    // 3. Registrar interrupción
    hal_set_interrupt_handler(MY_IRQ, &my_interrupt_handler);
}
```

## Manejo de Interrupciones

Los manejadores de interrupciones deben ser breves y eficientes.

```c
void my_interrupt_handler(void* stack) {
    // Leer estado del hardware
    // Procesar evento
    // Avisar al hardware que la interrupción fue servida (EOI)
}
```

## Plantilla de Driver Básico

```c
#include <hal.h>
#include <klog.h>

void sample_driver_init() {
    klog("SAMPLE", "Inicializando driver de ejemplo...");
    // Configuración inicial aquí
}

void sample_interrupt_handler() {
    // Manejo de eventos
}
```

## Referencias
- `kernel/drivers/README.md`
- `include/pci.h`
- `kernel/drivers/pci/pci.c`
