# apt-get y .deb reales en eterOS

`apt-get` ya no usa paquetes ficticios del `initrd`. La idea actual es:

- descargar `.deb` reales desde Internet usando HTTP
- cachearlos en `/var/cache/apt/archives`
- instalar solo lo que tenga sentido para el estado actual de eterOS

## Criterio de seleccion

Hoy un paquete es buen candidato si cumple al menos una de estas dos condiciones:

- es un paquete de datos: fuentes, iconos, tablas, assets
- no exige ejecutar un binario Linux dinamico ni maintainer scripts complejos

Hoy un paquete no es buen candidato si:

- trae binarios enlazados contra `libc6`, `libncurses`, `libstdc++`, `systemd`, etc.
- depende de `postinst`, `triggers` o integracion fuerte con Debian
- requiere `https` obligatorio o mirrors con redirecciones complejas

## Catalogo inicial curado

### Aptos para probar transporte/caché y futuros installs de datos

- `fonts-dejavu-core`
  - utilidad: fuentes para UI y render
  - razon: paquete de datos
- `hicolor-icon-theme`
  - utilidad: iconos base Freedesktop
  - razon: paquete de datos
- `iso-codes`
  - utilidad: tablas de paises, idiomas y monedas
  - razon: paquete de datos

### Bloqueados por runtime todavia incompleto

- `nano`
  - razon: el binario Debian normal depende de runtime Linux compartido y `ncurses`

## Limitacion actual importante

Aunque el `.deb` sea real y se descargue bien, Debian estable suele publicar `data.tar.xz` o `data.tar.gz`.

eterOS hoy:

- ya descarga `.deb` reales
- ya parsea `ar`
- ya extrae `data.tar` sin compresion
- todavia no extrae `data.tar.xz`, `data.tar.gz` ni `data.tar.zst`

## Siguiente paso tecnico

La siguiente iteracion recomendable es agregar soporte de descompresion para al menos:

1. `gzip`
2. `xz`

Recien despues tiene sentido ampliar el catalogo a mas paquetes descargados de Debian.
