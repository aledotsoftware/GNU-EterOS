# éterOS - Módulo de Criptografía

Este módulo proporciona primitivas criptográficas esenciales para el kernel y el espacio de usuario de éterOS.

## Propósito

Garantizar la integridad, autenticidad y confidencialidad de los datos dentro del sistema, proporcionando implementaciones eficientes de algoritmos estándar.

## Algoritmos Soportados

### Hashes
- **SHA-256**: Algoritmo de hash de 256 bits, utilizado para verificar la integridad de archivos y en protocolos de red.
- **SHA-512**: Algoritmo de hash de 512 bits, para aplicaciones que requieren mayor seguridad o procesadores de 64 bits.

### Firmas Digitales
- **Ed25519**: Esquema de firma EdDSA utilizando Curve25519. Utilizado para autenticación y verificación de firmas digitales de forma rápida y segura.

## Uso Seguro de Primitivas

- Las funciones de hash deben utilizarse para verificar la integridad de los datos (`sha256_update`, `sha256_final`).
- Ed25519 debe usarse para firmar mensajes y verificar la identidad de los componentes del sistema.
- El kernel utiliza estas primitivas para el subsistema OTA (Over-The-Air updates) y validación de paquetes EterPackage.

## Estructura de Archivos

```
crypto/
├── sha256.c    Implementación de SHA-256
├── sha512.c    Implementación de SHA-512
├── ed25519.c   Implementación de Ed25519 (firmas)
└── README.md   Este documento
```

## API de Ejemplo (C)

```c
// Ejemplo de SHA-256
sha256_ctx ctx;
uint8_t hash[32];
sha256_init(&ctx);
sha256_update(&ctx, data, len);
sha256_final(&ctx, hash);
```

## Referencias

- Headers de crypto: `include/crypto/`
- Tests de crypto: `tests/test_crypto.c`
