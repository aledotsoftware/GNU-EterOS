# éterOS - Stack de Red TCP/IP

Este directorio contiene la implementación completa del stack de red de éterOS, incluyendo la integración con lwIP y drivers de red.

## Propósito

El módulo de red de éterOS proporciona:
- Stack TCP/IP completo (Ethernet, IP, TCP, UDP, ICMP)
- Integración con lwIP 2.2.0 (stack industrial)
- Soporte para sockets BSD
- DHCP automático para configuración de red
- DNS para resolución de nombres
- Interfaz con drivers de red (e1000)

## Arquitectura del Stack

```
┌─────────────────────────────────────────────────┐
│         Userspace Applications                  │
│         (wget, ping, ntp)                       │
└────────────────┬────────────────────────────────┘
                 │ Syscalls (socket, send, recv)
┌────────────────┴────────────────────────────────┐
│         Socket Layer (socket.c)                 │
│         BSD Sockets API                         │
└────────────────┬────────────────────────────────┘
                 │
┌────────────────┴────────────────────────────────┐
│         lwIP Stack (lwip_port/)                 │
│  ┌──────────────────────────────────────────┐   │
│  │  TCP/UDP/ICMP (Transport Layer)         │   │
│  ├──────────────────────────────────────────┤   │
│  │  IP (Network Layer)                      │   │
│  ├──────────────────────────────────────────┤   │
│  │  Ethernet (Link Layer)                   │   │
│  └──────────────────────────────────────────┘   │
└────────────────┬────────────────────────────────┘
                 │ ethernetif
┌────────────────┴────────────────────────────────┐
│         NIC Driver (e1000)                      │
│         Hardware Abstraction                    │
└─────────────────────────────────────────────────┘
```

## Estructura de Archivos

```
net/
├── core/               Stack de red nativo (legacy/experimental)
│   ├── stack.c          Inicialización del stack
│   ├── nic.c            Abstracción de NIC
│   ├── dhcp.c           Cliente DHCP
│   ├── dhcp_parser.c    Parser de paquetes DHCP
│   ├── tcp.c            Implementación TCP básica
│   ├── raw_tcp.c        TCP sin estado
│   └── ip_utils.c       Utilidades IP
├── lwip_port/          Port de lwIP para éterOS
│   ├── ethernetif.c     Interfaz Ethernet para lwIP
│   ├── ethernetif.h     Headers de interfaz
│   ├── lwipopts.h       Configuración de lwIP
│   ├── sys_arch.c       Capa de sistema operativo
│   └── arch/            Headers específicos de arquitectura
├── socket.c            Implementación de sockets BSD
├── compat.c            Capa de compatibilidad
└── mock.c              Mocks para testing
```

## Componentes Principales

### lwIP Stack (lwip_port/)

éterOS integra lwIP 2.2.0, un stack TCP/IP industrial ligero y robusto.

**Características de lwIP:**
- TCP completo con control de flujo y retransmisión
- UDP para comunicación sin conexión
- ICMP para ping y diagnóstico
- DHCP para configuración automática de red
- DNS para resolución de nombres de dominio
- ARP para resolución de direcciones MAC

**Archivos de Port:**
- `ethernetif.c`: Conecta lwIP con el driver e1000. Implementa funciones de transmisión/recepción de paquetes.
- `sys_arch.c`: Proporciona primitivas de sistema operativo (semáforos, mailboxes, threads) requeridas por lwIP.
- `lwipopts.h`: Configuración de lwIP (tamaños de buffer, opciones de protocolo, debugging).

### Socket Layer (socket.c)

Implementa la API de sockets BSD para userspace:

```c
// Crear socket
int socket(int domain, int type, int protocol);

// Conectar a servidor
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// Enviar/recibir datos
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);

// Cerrar socket
int close(int sockfd);
```

Mapea syscalls de usuario a funciones de lwIP.

### DHCP Client (core/dhcp.c)

Cliente DHCP para configuración automática de red:
- Envía DHCP DISCOVER al arrancar
- Procesa DHCP OFFER del servidor
- Envía DHCP REQUEST para aceptar oferta
- Recibe DHCP ACK con configuración (IP, gateway, DNS)
- Configura interfaz de red automáticamente

### NIC Abstraction (core/nic.c)

Capa de abstracción para Network Interface Cards:
- Registra drivers de red (e1000)
- Proporciona API unificada para enviar/recibir paquetes
- Maneja múltiples interfaces de red (futuro)

### Stack Nativo (core/)

Implementación experimental de stack TCP/IP nativo (no usa lwIP):
- `tcp.c`: Implementación básica de TCP
- `raw_tcp.c`: TCP sin estado para casos simples
- `ip_utils.c`: Utilidades para manejo de paquetes IP

**Estado**: Experimental, lwIP es el stack principal en producción.

## Capas del Protocolo

### Capa de Enlace (Ethernet)
- Formato de frame: [MAC dest][MAC src][EtherType][Payload][CRC]
- EtherType: 0x0800 (IPv4), 0x0806 (ARP)
- MTU: 1500 bytes

### Capa de Red (IP)
- IPv4 con fragmentación
- Routing básico (gateway default)
- ICMP para ping y error reporting

### Capa de Transporte
- **TCP**: Conexión confiable, control de flujo, retransmisión
- **UDP**: Sin conexión, bajo overhead, para DNS y DHCP

### Capa de Aplicación
- **HTTP**: Cliente HTTP para wget
- **DNS**: Resolución de nombres de dominio
- **DHCP**: Configuración automática de red

## Integración con Drivers

El stack de red se integra con el driver e1000 (Intel PRO/1000):

1. **Inicialización**: `network_init()` detecta NIC vía PCI
2. **Configuración**: Configura buffers de TX/RX, habilita interrupciones
3. **DHCP**: Negocia IP automáticamente
4. **Transmisión**: `ethernetif_output()` envía paquetes al driver
5. **Recepción**: IRQ del e1000 → `ethernetif_input()` → lwIP procesa paquete

## Puntos de Entrada Principales

### Inicialización
```c
void network_init(void);           // Inicializa stack de red
void network_task(void);           // Task de red (DHCP, procesamiento)
```

### Syscalls de Red
- `sys_socket()` - Crear socket
- `sys_connect()` - Conectar a servidor
- `sys_send()` - Enviar datos
- `sys_recv()` - Recibir datos
- `sys_close()` - Cerrar socket

### Interfaz con lwIP
```c
err_t ethernetif_init(struct netif *netif);
err_t ethernetif_output(struct netif *netif, struct pbuf *p);
void ethernetif_input(struct netif *netif);
```

## Configuración de Red

### Configuración Estática (manual)
```c
IP4_ADDR(&ipaddr, 192, 168, 1, 100);
IP4_ADDR(&netmask, 255, 255, 255, 0);
IP4_ADDR(&gw, 192, 168, 1, 1);
netif_add(&netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, ethernet_input);
```

### Configuración Dinámica (DHCP)
```c
dhcp_start(&netif);  // Inicia negociación DHCP
// lwIP maneja DISCOVER, OFFER, REQUEST, ACK automáticamente
```

## Debugging

lwIP proporciona opciones de debugging en `lwipopts.h`:
```c
#define LWIP_DEBUG 1
#define ETHARP_DEBUG LWIP_DBG_ON
#define NETIF_DEBUG LWIP_DBG_ON
#define IP_DEBUG LWIP_DBG_ON
#define TCP_DEBUG LWIP_DBG_ON
```

## Limitaciones Conocidas

- Solo soporta IPv4 (IPv6 no implementado)
- Una sola interfaz de red activa
- No soporta routing avanzado
- DNS funcional pero no expuesto completamente a VFS

## Aplicaciones de Red

### wget (kernel/apps/wget.c)
Cliente HTTP simple para descargar archivos:
```bash
wget http://example.com/file.txt
```

### ping (futuro)
Utilidad ICMP echo para diagnóstico de red.

### ntp (futuro)
Cliente NTP para sincronización de tiempo.

## Referencias

- Documentación de lwIP: https://www.nongnu.org/lwip/
- Driver e1000: `kernel/drivers/net/e1000.c`
- Sockets API: `include/net/socket.h`
- Configuración de red: `include/net/defs.h`
