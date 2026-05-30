/**
 * éterOS - PCI Bus Configuration Driver
 * 
 * Escanea el Bus PCI usando el método de puertos I/O (Legacy PCI).
 * Detecta tarjetas de red, video, almacenamiento y puentes.
 */

#include "../../../include/pci.h"
#include "../../../include/io.h"
#include "../../../include/types.h"
#include "../../../include/vga.h"
#include "../../../include/serial.h"
#include "../../../include/string.h"

static void scan_function(uint8_t bus, uint8_t slot, uint8_t func);

/* ========================================================================= */
/* Implementación PCI I/O (Legacy)                                          */
/* ========================================================================= */

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
    /* Formato de dirección PCI:
     * Bit 31: Enable (1)
     * Bits 30-24: Reserved (0)
     * Bits 23-16: Bus Number
     * Bits 15-11: Device Number (Slot)
     * Bits 10-8: Function Number
     * Bits 7-2: Register Number (offset >> 2)
     * Bits 1-0: 0
     */
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
 
    outl(PCI_CONFIG_ADDRESS, address);
    tmp = (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
    return tmp;
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address;
    
    address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outw(PCI_CONFIG_DATA + (offset & 2), value);
}

/* ========================================================================= */
/* Escaneo del Bus                                                           */
/* ========================================================================= */

static const char* get_class_name(uint8_t class_id) {
    switch (class_id) {
        case 0x00: return "Unclass";
        case 0x01: return "Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x0C: return "Serial";
        default:   return "Unknown";
    }
}

static const char* get_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel";
        case 0x1022: return "AMD";
        case 0x10DE: return "NVIDIA";
        case 0x10EC: return "Realtek";
        case 0x80EE: return "VirtualBox";
        case 0x1AF4: return "VirtIO";
        default:     return "???"; // Hex ID printed by caller
    }
}

static void check_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;
 
    uint16_t vendor_id = pci_read_word(bus, device, function, PCI_OFFSET_VENDOR_ID);
    if (vendor_id == 0xFFFF) return; /* Device doesn't exist */
 
    scan_function(bus, device, function);
    
    uint16_t header_type = pci_read_word(bus, device, function, PCI_OFFSET_HEADER_TYPE);
    if ((header_type & 0x80) != 0) {
        /* Multi-function device, check remaining functions */
        for (function = 1; function < 8; function++) {
            if (pci_read_word(bus, device, function, PCI_OFFSET_VENDOR_ID) != 0xFFFF) {
                scan_function(bus, device, function);
            }
        }
    }
}

static void scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_read_word(bus, slot, func, PCI_OFFSET_VENDOR_ID);
    uint16_t device_id = pci_read_word(bus, slot, func, PCI_OFFSET_DEVICE_ID);
    uint16_t class_code = pci_read_word(bus, slot, func, PCI_OFFSET_SUBCLASS); // Returns Class (high) & Subclass (low)
    
    uint8_t class_id = (class_code >> 8) & 0xFF;
    
    /* Imprimir en consola estilo lspci */
    char buffer[64];
    
    /* Bus:Slot.Func */
    itoa_s(bus, buffer, sizeof(buffer), 16);
    terminal_write_colored(buffer, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(":");
    itoa_s(slot, buffer, sizeof(buffer), 16);
    terminal_write_colored(buffer, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_string(".");
    itoa_s(func, buffer, sizeof(buffer), 16);
    terminal_write_colored(buffer, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    
    terminal_write_string(" [");
    terminal_write_colored(get_class_name(class_id), VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_string("] ");
    
    /* Vendor */
    const char* vendor_name = get_vendor_name(vendor_id);
    if (vendor_name[0] == '?') {
        terminal_write_string("Vendor:");
        utoa_hex_s(vendor_id, buffer, sizeof(buffer));
        terminal_write_string(buffer + 2); // Skip 0x
    } else {
        terminal_write_colored(vendor_name, VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    }
    
    terminal_write_string(" Dev:");
    utoa_hex_s(device_id, buffer, sizeof(buffer));
    terminal_write_string(buffer + 2); // Skip 0x
    
    terminal_write_string("\n"); 
    
    /* Also log to serial for debugging */
    serial_write_string("[PCI] Found: ");
    serial_write_string(get_class_name(class_id));
    serial_write_string(" (Vendor=");
    // ... serial logging would be more verbose
    serial_write_string(")\n");
}

void pci_scan_bus(void) {
    terminal_write_colored("\n[PCI] Escaneando bus PCI...\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    
    /* Brute-force scan of Bus 0 (usually sufficient for simple VMs) */
    /* Real systems require deeper bus enumeration */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            check_device((uint8_t)bus, slot);
        }
    }
}

void pci_init(void) {
    /* Por ahora solo escaneamos bajo demanda */
    // pci_scan_bus();
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t* dev) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_word((uint8_t)bus, slot, func, PCI_OFFSET_VENDOR_ID);
                if (vendor == 0xFFFF) continue;
                
                uint16_t device = pci_read_word((uint8_t)bus, slot, func, PCI_OFFSET_DEVICE_ID);
                if (vendor == vendor_id && device == device_id) {
                    /* ¡Encontrado! Llenar estructura */
                    if (dev) {
                        dev->bus = (uint8_t)bus;
                        dev->slot = slot;
                        dev->function = func;
                        dev->vendor_id = vendor;
                        dev->device_id = device;
                        
                        uint16_t class_code = pci_read_word((uint8_t)bus, slot, func, PCI_OFFSET_SUBCLASS);
                        dev->class_id = (class_code >> 8) & 0xFF;
                        dev->subclass_id = (uint8_t)(class_code & 0xFF);
                        dev->header_type = (uint8_t)(pci_read_word((uint8_t)bus, slot, func, PCI_OFFSET_HEADER_TYPE) & 0x7F);
                    }
                    return 1;
                }
                
                /* Si es función 0 y no es multi-function, saltar al siguiente device */
                if (func == 0) {
                    uint16_t header_type = pci_read_word((uint8_t)bus, slot, func, PCI_OFFSET_HEADER_TYPE);
                    if (!(header_type & 0x80)) break;
                }
            }
        }
    }
    return 0;
}
