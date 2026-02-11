/**
 * éterOS - PCI Bus Driver Header
 * 
 * Implementación básica para enumeración de dispositivos PCI.
 * Usa el mecanismo de configuración Legacy (I/O Ports 0xCF8/0xCFC).
 */

#ifndef ETEROS_PCI_H
#define ETEROS_PCI_H

#include "types.h"

/* ========================================================================= */
/* Constantes PCI                                                            */
/* ========================================================================= */

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* Offsets en el espacio de configuración PCI */
#define PCI_OFFSET_VENDOR_ID    0x00
#define PCI_OFFSET_DEVICE_ID    0x02
#define PCI_OFFSET_COMMAND      0x04
#define PCI_OFFSET_STATUS       0x06
#define PCI_OFFSET_REVISION_ID  0x08
#define PCI_OFFSET_PROG_IF      0x09
#define PCI_OFFSET_SUBCLASS     0x0A
#define PCI_OFFSET_CLASS        0x0B
#define PCI_OFFSET_HEADER_TYPE  0x0E
#define PCI_OFFSET_BAR0         0x10

/* Códigos de Clase Comunes */
#define PCI_CLASS_UNCLASSIFIED  0x00
#define PCI_CLASS_STORAGE       0x01
#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_MULTIMEDIA    0x04
#define PCI_CLASS_MEMORY        0x05
#define PCI_CLASS_BRIDGE        0x06

/* Vendors Comunes */
#define PCI_VENDOR_INTEL        0x8086
#define PCI_VENDOR_AMD          0x1022
#define PCI_VENDOR_REALTEK      0x10EC
#define PCI_VENDOR_VIRTUALBOX   0x80EE

/**
 * Estructura que representa un dispositivo PCI detectado.
 */
typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t header_type;
} pci_device_t;

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

/**
 * Inicializa el subsistema PCI (escaneo inicial).
 */
void pci_init(void);

/**
 * Lee una palabra de 16 bits del espacio de configuración PCI.
 */
uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/**
 * Escribe una palabra de 16 bits en el espacio de configuración PCI.
 */
void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

/**
 * Escanea el bus PCI e imprime los dispositivos detectados (estilo lspci).
 */
void pci_scan_bus(void);

#endif /* ETEROS_PCI_H */
