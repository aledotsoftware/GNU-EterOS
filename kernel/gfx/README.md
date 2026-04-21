# éterOS - Servidor Gráfico Eterland

Eterland es el compositor gráfico nativo de éterOS, diseñado para ofrecer una experiencia visual fluida y moderna con efectos de transparencia (Glassmorphism).

## Arquitectura de Composición

Eterland funciona como un servidor de composición que gestiona múltiples ventanas y las mezcla en el framebuffer lineal (LFB).

### Componentes Principales

1.  **Shared Buffers (SHM)**: Las aplicaciones solicitan buffers de memoria compartida al kernel via `sys_mmap(MAP_SHARED)`. Estos buffers son accesibles tanto por la aplicación para dibujar como por Eterland para componer.
2.  **Motor Omni**: El motor de renderizado que realiza el blending de las ventanas.
3.  **Gestión de Ventanas**: Maneja la posición, orden Z (z-index), y decoraciones de las ventanas.

## Protocolo IPC

Las aplicaciones se comunican con Eterland a través de sockets de dominio Unix (o memoria compartida) para:
- Crear/Destruir ventanas.
- Notificar regiones sucias (dirty regions) para redibujado.
- Recibir eventos de entrada (mouse, teclado).

## Características

- **Alpha Blending**: Soporte nativo para 256 niveles de transparencia.
- **Glassmorphism**: Efectos de desenfoque y transparencia para una UI moderna.
- **Dirty Region Tracking**: Optimización que solo redibuja las áreas de la pantalla que han cambiado, reduciendo drásticamente el uso de ancho de banda de memoria.
- **Zero-Copy**: El contenido de las ventanas se dibuja directamente en buffers compartidos, evitando copias innecesarias de memoria entre procesos.

## Estructura de Archivos

```
gfx/
├── gfx.c       Primitivas de dibujo (puntos, líneas, rectángulos)
├── window.c    Lógica del compositor y gestión de ventanas
├── png.c       Decodificador de imágenes PNG
└── README.md   Este documento
```

## Referencias

- Especificación de protocolo: `docs/eterland.md`
- API de dibujo: `kernel/gfx/gfx.c`
