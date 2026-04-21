# Eterland - Especificación Técnica

Eterland es el sistema de ventanas y compositor de éterOS.

## Protocolo IPC

La comunicación entre los clientes (aplicaciones) y el servidor (Eterland) se basa en un protocolo de paso de mensajes sobre memoria compartida.

### Formato de Mensajes

Los mensajes tienen una cabecera fija seguida de datos variables:

```c
struct eterland_msg {
    uint32_t type;
    uint32_t window_id;
    uint32_t length;
    uint8_t  data[];
};
```

### Tipos de Mensajes
- `MSG_CREATE_WINDOW`: El cliente solicita una nueva ventana.
- `MSG_DESTROY_WINDOW`: El cliente cierra una ventana.
- `MSG_FLUSH`: El cliente notifica que ha terminado de dibujar en su buffer.
- `MSG_MOUSE_EVENT`: El servidor notifica un movimiento o click de mouse.
- `MSG_KEY_EVENT`: El servidor notifica una pulsación de tecla.

## Gestión de Ventanas

Cada ventana tiene:
- Un ID único.
- Un buffer de memoria compartida (SHM).
- Coordenadas (x, y) y dimensiones (w, h).
- Atributos de transparencia (alpha).

## Ejemplo de Cliente Gráfico

```c
// 1. Conectar con Eterland
int fd = socket(AF_UNIX, SOCK_STREAM, 0);
connect(fd, "/tmp/eterland.sock", ...);

// 2. Pedir ventana
eterland_msg msg = { .type = MSG_CREATE_WINDOW, .w = 800, .h = 600 };
send(fd, &msg, sizeof(msg), 0);

// 3. Mapear buffer
void* buffer = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

// 4. Dibujar y hacer Flush
draw_ui(buffer);
msg.type = MSG_FLUSH;
send(fd, &msg, sizeof(msg), 0);
```

## Referencias
- `kernel/gfx/window.c`
- `kernel/gfx/gfx.c`
