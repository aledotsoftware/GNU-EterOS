## 1. Visión y Estrategia de Transición
EterStore es la infraestructura que permite la instalación, actualización y gestión de aplicaciones en éterOS. La estrategia se divide en dos fases críticas:
1.  **Fase de Adopción (Actual):** Compatibilidad nativa con el ecosistema Linux (`.deb`, `AppImage`).
2.  **Fase Nativa (Futura):** Transición hacia el formato optimizado `.epk`.

## 2. Compatibilidad con Ecosistema Linux
Para garantizar que éterOS sea útil desde el primer día, EterStore debe actuar como una capa de compatibilidad para:

### A. Paquetes Debian (.deb)
*   **Mecanismo de Instalación:** `epm` debe ser capaz de descomprimir la estructura de datos (`data.tar.xz`) de un `.deb` y mapearla en el VFS de éterOS.
*   **Gestión de Dependencias:** Implementar un mapeador que traduzca dependencias de Linux (ej: `libc6`, `libx11-6`) a los componentes equivalentes en éterOS.
*   **Scripts de Post-Instalación:** Soporte para ejecutar scripts de configuración de paquetes en un entorno de shell compatible con POSIX.

### B. AppImages
*   **Ejecución Directa:** Soporte para montar imágenes `FUSE` o `SquashFS` en modo usuario.
*   **Runtime:** Proporcionar un entorno de ejecución que incluya las librerías base de Linux mínimas para que un AppImage se sienta "en casa".

## 3. El Formato Nativo EterPackage (.epk)
Una vez establecida la base de usuarios, se introducirá el formato `.epk` para aplicaciones diseñadas específicamente para aprovechar las ventajas de éterOS (como la memoria compartida de Eterland).
*   **Compresión:** `Zstd` para máxima velocidad de descompresión.
*   **Contenedor:** Un archivo de imagen de solo lectura comprimido que se monta al vuelo (sistema tipo SquashFS).
*   **Metadatos:** JSON firmado digitalmente que contiene dependencias, permisos requeridos y scripts de ciclo de vida.

## 3. Infraestructura de Repositorios (EterNet)
*   **Protocolo:** HTTP/HTTPS con soporte para reanudación de descargas (Resume).
*   **Seguridad:** Verificación por hashes SHA-256 obligatoria para prevenir inyecciones de código.
*   **EterStore CLI (`epm`):** El cliente de línea de comandos capaz de resolver árboles de dependencias y gestionar la base de datos local de aplicaciones instaladas.

## 4. Dependencias del Kernel y Core
Para que EterStore sea viable, se deben implementar:
1.  **Syscalls de FileSystem:** Soporte para `link`, `symlink` y atributos extendidos en JFS.
2.  **Librerías Dinámicas:** Soporte para el cargador de ELF (`ld.so`) y búsqueda de librerías en `/usr/lib`.
3.  **Seguridad (Sandbox):** Un sistema de permisos que limite a qué archivos puede acceder una app instalada vía EterStore.

## 5. Modelo de Distribución
*   **Base:** Paquetes esenciales del sistema firmados por el equipo oficial.
*   **Comunidad:** Espacio transparente para repositorios de terceras partes auditados por la comunidad.
