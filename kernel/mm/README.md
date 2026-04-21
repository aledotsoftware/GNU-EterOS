# éterOS - Gestión de Memoria

Este directorio contiene los componentes fundamentales de la gestión de memoria en éterOS, abarcando desde la memoria física hasta el heap del kernel.

## Propósito

La gestión de memoria es responsable de:
- Administrar los frames de memoria física (PMM).
- Gestionar el espacio de direcciones virtuales (VMM).
- Proporcionar asignación dinámica de memoria para el kernel (Heap).
- Implementar protecciones de memoria y aislamiento entre procesos.

## Componentes Principales

### 1. Physical Memory Manager (PMM) (`pmm.c`)
- Gestiona la memoria RAM física en bloques de 4KB (frames).
- Utiliza un **Bitmap** para el seguimiento de frames libres y ocupados.
- Algoritmo de asignación: **Next-Fit** para eficiencia.
- Implementa conteo de referencias para soportar **Copy-on-Write (CoW)**.

### 2. Virtual Memory Manager (VMM) (`vmm.c`)
- Gestiona las tablas de paginación de x86_64 (PML4, PDPT, PD, PT).
- Mapea direcciones virtuales a físicas.
- Maneja los **Page Faults** para implementar carga bajo demanda y CoW.
- Gestiona el aislamiento entre el espacio de kernel y usuario.

### 3. Kernel Heap (`heap.c`)
- Proporciona las funciones `kmalloc` y `kfree`.
- Utiliza un algoritmo de **Listas Enlazadas con Coalescing** automático.
- Optimizado para diferentes tamaños de asignación.
- Incluye checks de seguridad (magic numbers) para detectar corrupciones.

## Estructura de Archivos

```
mm/
├── pmm.c       Physical Memory Manager
├── vmm.c       Virtual Memory Manager
├── heap.c      Kernel Dynamic Heap
└── README.md   Este documento
```

## Paginación y Memoria Virtual

éterOS utiliza un modelo de memoria "High-Half" donde el kernel reside en las direcciones superiores, mientras que el espacio de usuario ocupa las direcciones inferiores.

- **0x0000_0000_0000_0000 - 0x0000_7FFF_FFFF_FFFF**: Espacio de Usuario.
- **0xFFFF_8000_0000_0000 - 0xFFFF_FFFF_FFFF_FFFF**: Espacio de Kernel.

## Referencias

- Documentación de arquitectura: `README.md` (Sección 4)
- Headers de MM: `include/mm/`
- Tests de memoria: `tests/test_heap.c`, `tests/test_pmm_security.c`
