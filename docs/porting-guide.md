# Guía de Porting de éterOS

Esta guía proporciona los pasos necesarios para portar éterOS a una nueva arquitectura de procesador.

## Checklist de Implementación

Para que éterOS funcione en una nueva arquitectura, se deben implementar los siguientes componentes:

### 1. Inicialización Temprana
- [ ] Script de enlazado (`linker.ld`) específico.
- [ ] Código de arranque en ensamblador (configuración de stack, paso a modo protegido/largo).
- [ ] Inicialización de la consola serial para debug.

### 2. Hardware Abstraction Layer (HAL)
Implementar todas las funciones en `kernel/arch/<arch>/hal_impl.c`:
- [ ] Control de interrupciones (Habilitar/Deshabilitar).
- [ ] Manejo de excepciones y traps.
- [ ] Timer del sistema para el scheduler.
- [ ] Funciones de I/O básicas.

### 3. Gestión de Memoria
- [ ] Configuración de la MMU/Paginación.
- [ ] Definición del mapa de memoria (Kernel High-Half).
- [ ] Funciones para invalidar TLB.

### 4. Multitarea
- [ ] Función de `context_switch` en ensamblador.
- [ ] Creación del contexto inicial para nuevos hilos.

## Pasos Detallados

1.  **Analizar el Bootloader**: Decidir si se usará un bootloader existente (como GRUB vía Multiboot) o uno propio.
2.  **Mapeo de Memoria**: Definir dónde residirá el kernel en memoria física y virtual.
3.  **Serial Debug**: Implementar `hal_serial_write` lo antes posible para ver logs.
4.  **Interrupciones**: Configurar el controlador de interrupciones (APIC, GIC, NVIC, etc.).
5.  **Scheduler**: Una vez que el timer funcione, implementar el cambio de contexto.

## Referencias de Implementación
- **x86_64**: Referencia principal para sistemas de alta performance.
- **AArch64**: (En progreso) Referencia para sistemas embebidos modernos.

## Referencias
- `include/hal.h`
- `kernel/arch/README.md`
