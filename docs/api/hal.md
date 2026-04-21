# Hardware Abstraction Layer (HAL) API

La HAL es la interfaz que permite a éterOS ser portable entre diferentes arquitecturas de hardware.

## Concepto

Cualquier arquitectura que desee soportar éterOS debe implementar las funciones definidas en `include/hal.h`. La implementación específica se encuentra en `kernel/arch/<arch>/hal_impl.c`.

## Funciones Requeridas

### Gestión de Interrupciones
- `void hal_interrupts_enable(void)`: Habilita las interrupciones de hardware.
- `void hal_interrupts_disable(void)`: Deshabilita las interrupciones de hardware.
- `void hal_set_interrupt_handler(int irq, void (*handler)(void*))`: Registra un manejador para una interrupción específica.

### Control de CPU
- `void hal_cpu_halt(void)`: Detiene la CPU hasta la próxima interrupción.
- `void hal_cpu_reboot(void)`: Reinicia el sistema.
- `void hal_cpu_get_info(cpu_info_t *info)`: Obtiene información sobre la CPU actual.

### I/O y Comunicación
- `void hal_serial_write(char c)`: Escribe un carácter al puerto serial de debug.
- `uint8_t hal_io_inb(uint16_t port)`: Lee un byte de un puerto de I/O (x86).
- `void hal_io_outb(uint16_t port, uint8_t value)`: Escribe un byte a un puerto de I/O (x86).

## Implementación para Nuevas Arquitecturas

Para portar éterOS a una nueva arquitectura:

1.  **Directorio**: Crear `kernel/arch/<nueva_arch>/`.
2.  **Headers**: Definir tipos específicos en `kernel/arch/<nueva_arch>/types.h`.
3.  **hal_impl.c**: Implementar todas las funciones declaradas en `include/hal.h`.
4.  **Boot**: Implementar la secuencia de arranque que salte a `kmain()`.

## Ejemplo de Implementación (Ficticio)

```c
void hal_interrupts_enable() {
    asm volatile("sti"); // x86 example
}
```

## Referencias
- `include/hal.h`
- `kernel/arch/x86_64/hal_impl.c`
