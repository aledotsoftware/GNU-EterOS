# eterOS - Subsistema Criptográfico

Este directorio contiene implementaciones ligeras de algoritmos criptográficos para el núcleo de eterOS.

## Propósito

El subsistema criptográfico provee rutinas esenciales para la seguridad del sistema operativo, como:
- Verificación de firmas digitales durante procesos de actualización OTA (Over-The-Air)
- Hashing de contraseñas de usuario (`/etc/shadow`)
- Generación y manejo de firmas para binarios seguros

## Algoritmos Soportados

Actualmente, eterOS incluye los siguientes algoritmos base:

- **SHA-256 (`sha256.c`)**: Función de hash criptográfico estándar, utilizada en el sistema de contraseñas y como parte de Ed25519.
- **SHA-512 (`sha512.c`)**: Función de hash extendida, necesaria internamente para las operaciones de curva elíptica Ed25519.
- **Ed25519 (`ed25519.c`)**: Esquema de firmas de curva elíptica (EdDSA) optimizado, utilizado para validar la autenticidad de los paquetes de actualización del sistema y componentes críticos del kernel.

## Arquitectura

El diseño de estos módulos está orientado a ser extremadamente ligero y libre de dependencias externas o de asignación dinámica de memoria (`kmalloc`), lo que permite que puedan ser invocados de forma segura durante fases tempranas del bootloader o en entornos restringidos de memoria durante pánicos del kernel o rutinas críticas de interrupción.

Las funciones principales se exponen de forma directa mediante la cabecera `include/crypto.h` para que cualquier subsistema del kernel pueda utilizarlas.
