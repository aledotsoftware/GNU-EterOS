# Eterland Technical Manifest
**Evolución del Servidor Gráfico y Protocolo de Composición de éterOS**

## 1. Visión
Eterland es el sucesor del compositor monolítico actual. Su objetivo es proporcionar un entorno de ventanas de alto rendimiento basado en el principio de **"Cero Copia"** (Zero-Copy), donde las aplicaciones dibujan de forma independiente y el servidor solo compone el resultado final.

## 2. Arquitectura de Composición
A diferencia de los sistemas legacy, Eterland separa la lógica de renderizado de la lógica del kernel.

### A. Buffers Compartidos (Shared Buffers)
*   **Mecanismo:** Basado en descriptores de memoria compartida (SHM).
*   **Flujo:**
    1. El cliente pide un búfer de píxeles al servidor.
    2. El servidor asigna memoria y la mapea en ambos procesos mediante `sys_mmap(MAP_SHARED)`.
    3. El cliente dibuja (vía CPU o GPU).
    4. El cliente envía un mensaje de "frame listo" a través de un socket IPC.

### B. Protocolo de Comunicación
*   **Transporte:** Unix Domain Sockets (`AF_UNIX`).
*   **Serialización:** Binaria asíncrona.
*   **Eventos:** El servidor distribuye eventos de teclado y mouse solo a la ventana con el foco activo, garantizando seguridad y aislamiento.

## 3. Requerimientos del Kernel (Hoja de Ruta)
Para habilitar Eterland, el equipo de desarrollo debe priorizar:
1.  **Syscalls de SHM:** `shm_open`, `ftruncate` y soporte real para `MAP_SHARED`.
2.  **Aislamiento de Memoria:** Garantizar que una ventana no pueda leer el búfer de otra sin permiso.
3.  **Sincronización:** Implementar primitivas de sincronización (fences) para evitar desgarros de pantalla (screen tearing).

## 4. Filosofía de Diseño
*   **Minimalismo:** Solo las funciones básicas de composición están en el servidor core.
*   **Extensibilidad:** Los paneles, barras de tareas y fondos de pantalla son clientes externos, no parte del servidor gráfico.
