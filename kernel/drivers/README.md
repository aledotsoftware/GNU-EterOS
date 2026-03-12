# éterOS - Módulo de Drivers

Este directorio contiene todos los controladores de hardware (drivers) del kernel de éterOS. Los drivers proporcionan la interfaz entre el hardware físico y las capas superiores del sistema operativo.

## Propósito

Los drivers en éterOS siguen una arquitectura modular que permite:
- Abstracción del hardware específico
- Soporte para múltiples dispositivos del mismo tipo
- Inicialización y configuración automática vía PCI/ACPI
- Manejo de interrupciones y DMA

## Estructura de Archivos

```
drivers/
├── disk/           Controladores de almacenamiento
│   └── partition.c  Manejo de particiones MBR/GPT
├── input/          Dispositivos de entrada
│   ├── input.c      Sistema de eventos de entrada
│   ├── keyboard.c   Driver de teclado PS/2
│   └── mouse.c      Driver de mouse PS/2
├── net/            Controladores de red
│   └── e1000.c      Intel PRO/1000 Gigabit Ethernet
├── pci/            Bus PCI
│   └── pci.c        Enumeración y configuración PCI
├── rtc/            Reloj de tiempo real
│   └── rtc.c        CMOS RTC driver
├── serial/         Puerto serial
│   └── serial.c     UART 16550 (COM1-COM4)
├── timer/          Temporizadores
│   └── pit.c        Programmable Interval Timer (8253/8254)
├── video/          Controladores de video
│   ├── font.c       Fuente bitmap para consola
│   ├── framebuffer.c Framebuffer lineal (VBE/GOP)
│   └── vga.c        Modo texto VGA
└── tty.c           Terminal virtual (TTY)
```

## Drivers Disponibles

### Almacenamiento (disk/)
- **partition.c**: Detecta y parsea tablas de particiones MBR. Soporta hasta 4 particiones primarias. Expone offsets para que los drivers de filesystem puedan montar particiones individuales.

### Entrada (input/)
- **keyboard.c**: Driver para teclado PS/2. Maneja IRQ1, traduce scancodes a keycodes, soporta modificadores (Shift, Ctrl, Alt). Genera eventos de entrada para el sistema.
- **mouse.c**: Driver para mouse PS/2. Maneja IRQ12, reporta movimiento relativo y estado de botones. Genera eventos de entrada para el compositor gráfico.
- **input.c**: Sistema unificado de eventos de entrada. Ring buffer compartido accesible vía `/dev/input`.

### Red (net/)
- **e1000.c**: Driver nativo para Intel PRO/1000 (82540EM/82545EM). Descubierto vía PCI, soporta transmisión/recepción de paquetes Ethernet, integrado con lwIP stack. Soporta DHCP automático.

### PCI (pci/)
- **pci.c**: Escanea el bus PCI, enumera dispositivos, lee configuración (Vendor ID, Device ID, BARs). Permite a otros drivers descubrir y configurar hardware PCI.

### RTC (rtc/)
- **rtc.c**: Lee fecha/hora del CMOS RTC. Proporciona tiempo del sistema para timestamps de archivos y logs.

### Serial (serial/)
- **serial.c**: Driver UART 16550 para puertos COM1-COM4. Usado para debug output y comunicación serial. Soporta baudrates configurables.

### Timer (timer/)
- **pit.c**: Programmable Interval Timer. Configurado a 100Hz (10ms tick). Genera IRQ0 para preempción del scheduler y timekeeping.

### Video (video/)
- **framebuffer.c**: Driver de framebuffer lineal. Obtiene LFB (Linear Frame Buffer) vía VBE (BIOS) o GOP (UEFI). Proporciona acceso directo a memoria de video para el compositor gráfico.
- **vga.c**: Driver de modo texto VGA 80x25. Usado para consola temprana antes de inicializar el framebuffer gráfico.
- **font.c**: Fuente bitmap 8x16 para renderizado de texto en framebuffer.

### TTY (tty.c)
- **tty.c**: Terminal virtual. Implementa `/dev/tty`, maneja input/output de consola, line discipline, y control de terminal.

## Puntos de Entrada Principales

### Inicialización
Los drivers se inicializan en secuencia durante el boot:
1. `serial_init()` - Debug output temprano
2. `pci_init()` - Descubrimiento de hardware
3. `pit_init()` - Timer para scheduler
4. `keyboard_init()`, `mouse_init()` - Entrada de usuario
5. `e1000_init()` - Networking (si se detecta hardware)
6. `framebuffer_init()` - Video gráfico

### Manejo de Interrupciones
Cada driver registra su handler de interrupción:
- IRQ0: PIT timer
- IRQ1: Keyboard
- IRQ12: Mouse
- IRQ variable: e1000 (asignado por PCI)

### Interfaz con VFS
Algunos drivers exponen dispositivos vía DevFS:
- `/dev/tty` - Terminal
- `/dev/input` - Eventos de teclado/mouse
- `/dev/null`, `/dev/zero` - Dispositivos especiales

## Referencias

- Documentación de APIs: `docs/api/drivers.md`
- Hardware Abstraction Layer: `include/hal.h`
- Configuración PCI: `include/pci.h`
