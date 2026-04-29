/**
 * éterOS - Intel PRO/1000 (82540EM) Driver Implementation
 * 
 * Controlador básico para tarjetas de red Intel E1000.
 * Soporta inicialización, lectura de MAC y envío/recepción básico (Polling).
 */

#include "../../../include/net/e1000.h"
#include "../../../include/pci.h"
#include "../../../include/vga.h"
#include "../../../include/serial.h"
#include "../../../include/mm.h"
#include "../../../include/string.h"
#include "../../../include/vmm.h"
#include "../../../include/pmm.h"
#include "../../../include/io.h"
#include "../../../include/timer.h"
#include "../../../include/pic.h"
#include "../../../include/sem.h"
#include "../../../include/task.h"
#include <net/nic.h>

extern sem_t net_sem;

#define E1000_VENDOR_ID     0x8086
#define E1000_DEVICE_ID     0x100E  

/* Configuración de Anillos */
#define NUM_RX_DESC         32
#define NUM_TX_DESC         8
#define RX_BUFFER_SIZE      2048

static uint8_t mac_address[6];
static volatile uint8_t* mmio_base;
static int e1000_active = 0;

/* Punteros a los anillos de descriptores */
static volatile struct e1000_rx_desc* rx_descs;
static volatile struct e1000_tx_desc* tx_descs;

/* Buffers de recepción */
static uint8_t* rx_buffers[NUM_RX_DESC];
static uint8_t* tx_buffers[NUM_TX_DESC];

/* Indices actuales (Head/Tail software tracking) */
static uint16_t rx_cur;
static uint16_t tx_cur;

/* ========================================================================= */
/* Helpers Internos                                                          */
/* ========================================================================= */

static void e1000_write_reg(uint16_t offset, uint32_t value) {
    if (!mmio_base) return;
    *(volatile uint32_t*)(mmio_base + offset) = value;
}

static uint32_t e1000_read_reg(uint16_t offset) {
    if (!mmio_base) return 0;
    return *(volatile uint32_t*)(mmio_base + offset);
}

/* ========================================================================= */
/* Inicialización RX/TX                                                      */
/* ========================================================================= */

static void e1000_init_rx(void) {
    /* 1. Asignar memoria para descriptores */
    rx_descs = (volatile struct e1000_rx_desc*)kmalloc(PAGE_SIZE);
    if (!rx_descs) {
        serial_write_string("[E1000] ERROR: Failed to allocate RX descriptors.\n");
        return;
    }
    memset((void*)rx_descs, 0, PAGE_SIZE);
    
    /* 2. Asignar buffers para cada descriptor */
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_buffers[i] = (uint8_t*)kmalloc(PAGE_SIZE);
        if (!rx_buffers[i]) {
            serial_write_string("[E1000] ERROR: Failed to allocate RX buffer.\n");
            return;
        }
        memset(rx_buffers[i], 0, PAGE_SIZE);
        rx_descs[i].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_descs[i].status = 0;
    }
    
    /* 3. Configurar registros RDBAL/RDBAH (Base Address) */
    uint64_t base_addr = (uint64_t)(uintptr_t)rx_descs;
    e1000_write_reg(E1000_RDBAL, (uint32_t)(base_addr & 0xFFFFFFFF));
    e1000_write_reg(E1000_RDBAH, (uint32_t)(base_addr >> 32));
    
    /* 4. Configurar longitud (RDLEN) - Total bytes of descriptors */
    e1000_write_reg(E1000_RDLEN, NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    
    /* 5. Configurar Head y Tail */
    e1000_write_reg(E1000_RDH, 0);
    e1000_write_reg(E1000_RDT, NUM_RX_DESC - 1);
    rx_cur = 0;
    
    /* 6. Configurar RCTL (Control) 
     * EN (Enable) | SBP (Store Bad Packets) | UPE (Unicast Promiscuous) | 
     * MPE (Multicast Promiscuous) | LBM_NONE | RDMTS_HALF | BAM (Broadcast Accept) | 
     * BSIZE_2048 | SECRC (Strip CRC)
     */
    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE | E1000_RCTL_MPE | 
                    E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC;
    e1000_write_reg(E1000_RCTL, rctl);
}

static void e1000_init_tx(void) {
    /* 1. Asignar memoria para descriptores TX */
    tx_descs = (volatile struct e1000_tx_desc*)kmalloc(PAGE_SIZE);
    if (!tx_descs) {
        serial_write_string("[E1000] ERROR: Failed to allocate TX descriptors.\n");
        return;
    }
    memset((void*)tx_descs, 0, PAGE_SIZE);

    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_buffers[i] = (uint8_t*)kmalloc(PAGE_SIZE);
        if (!tx_buffers[i]) {
            serial_write_string("[E1000] ERROR: Failed to allocate TX buffer.\n");
            return;
        }
        memset(tx_buffers[i], 0, PAGE_SIZE);
        tx_descs[i].addr = (uint64_t)(uintptr_t)tx_buffers[i];
        tx_descs[i].status = E1000_STA_DD;
    }
    
    /* 2. Configurar registros TDBAL/TDBAH */
    uint64_t base_addr = (uint64_t)(uintptr_t)tx_descs;
    e1000_write_reg(E1000_TDBAL, (uint32_t)(base_addr & 0xFFFFFFFF));
    e1000_write_reg(E1000_TDBAH, (uint32_t)(base_addr >> 32));
    
    /* 3. Configurar longitud (TDLEN) */
    e1000_write_reg(E1000_TDLEN, NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    
    /* 4. Configurar Head y Tail */
    e1000_write_reg(E1000_TDH, 0);
    e1000_write_reg(E1000_TDT, 0);
    tx_cur = 0;
    
    /* 5. Configurar TCTL 
     * EN (Enable) | PSP (Pad Short Packets) | COLD (Collision Distance)
     */
    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP | (15 << 4) /* CT */ | (0x40 << 12) /* COLD */ | E1000_TCTL_RTLC;
    e1000_write_reg(E1000_TCTL, tctl);
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

int e1000_send_packet(const void* data, uint16_t len);
int e1000_receive(void* buffer, uint16_t max_len);
uint8_t* e1000_get_mac(void);

static int e1000_init_wrapper(void* dev) {
    return e1000_init((pci_device_t*)dev);
}

static nic_driver_t e1000_driver = {
    .name = "Intel PRO/1000",
    .init = e1000_init_wrapper,
    .send = e1000_send_packet,
    .receive = e1000_receive,
    .get_mac = e1000_get_mac
};

int e1000_init(pci_device_t* pci_dev_ptr) {
    pci_device_t dev;
    pci_device_t* pci_dev = pci_dev_ptr;
    
    if (!pci_dev) {
        if (pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID, &dev)) {
            pci_dev = &dev;
        } else {
            /* Silent return during probe - allows scanning for other cards */
            return -1;
        }
    }
    
    terminal_write_colored("  [NET]  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("Intel E1000 detectado en PCI ");
    char buf[32];
    itoa_s(pci_dev->bus, buf, sizeof(buf), 10); terminal_write_string(buf); terminal_write_string(":");
    itoa_s(pci_dev->slot, buf, sizeof(buf), 10); terminal_write_string(buf); terminal_write_string(".");
    itoa_s(pci_dev->function, buf, sizeof(buf), 10); terminal_write_string(buf); terminal_write_string("\n");

    /* Habilitar Bus Mastering y Memory Space */
    uint16_t command = pci_read_word(pci_dev->bus, pci_dev->slot, pci_dev->function, PCI_OFFSET_COMMAND);
    if (!(command & (1 << 2)) || !(command & (1 << 1))) {
        command |= (1 << 2) | (1 << 1); /* Bit 2: Bus Master, Bit 1: Memory Space */
        pci_write_word(pci_dev->bus, pci_dev->slot, pci_dev->function, PCI_OFFSET_COMMAND, command);
    }

    /* Obtener MMIO Base Address */
    uint16_t bar0_low = pci_read_word(pci_dev->bus, pci_dev->slot, pci_dev->function, PCI_OFFSET_BAR0);
    uint16_t bar0_high = pci_read_word(pci_dev->bus, pci_dev->slot, pci_dev->function, PCI_OFFSET_BAR0 + 2);
    uint32_t bar0 = ((uint32_t)bar0_high << 16) | bar0_low;
    
    if (bar0 & 1) return -2; // I/O Space not supported
    bar0 &= 0xFFFFFFF0;
    
    mmio_base = (uint8_t*)(uintptr_t)bar0;
    
    /* Map 128KB of MMIO Space (32 pages x 4KB) */
    for (int i = 0; i < 32; i++) {
        vmm_map_page(bar0 + i * 4096, bar0 + i * 4096, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
    }
    
    e1000_active = 1;
    
    serial_write_string("[E1000] MMIO Base: 0x");
    utoa_hex_s((uint64_t)bar0, buf, sizeof(buf));
    serial_write_string(buf);
    serial_write_string("\n");
    
    /* Leer MAC Address */
    uint32_t ral = e1000_read_reg(E1000_RAL);
    uint32_t rah = e1000_read_reg(E1000_RAH);
    
    mac_address[0] = (uint8_t)(ral);
    mac_address[1] = (uint8_t)(ral >> 8);
    mac_address[2] = (uint8_t)(ral >> 16);
    mac_address[3] = (uint8_t)(ral >> 24);
    mac_address[4] = (uint8_t)(rah);
    mac_address[5] = (uint8_t)(rah >> 8);
    
    terminal_write_colored("  [NET]  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_string("MAC Address: ");
    for (int i = 0; i < 6; i++) {
        uint8_t val = mac_address[i];
        char hex[3];
        char h = (val >> 4) & 0xF; char l = val & 0xF;
        hex[0] = (h < 10) ? '0' + h : 'A' + (h - 10);
        hex[1] = (l < 10) ? '0' + l : 'A' + (l - 10);
        hex[2] = 0;
        terminal_write_string(hex);
        if (i < 5) terminal_write_string(":");
    }
    terminal_write_string("\n");

    /* Inicializar RX y TX */
    e1000_init_rx();
    e1000_init_tx();
    
    /* Activar Link (SLU - Set Link Up) */
    uint32_t ctrl = e1000_read_reg(E1000_CTRL);
    e1000_write_reg(E1000_CTRL, ctrl | E1000_CTRL_SLU);
    
    /* VirtualBox resulta mas estable en polling puro que con IRQ legacy. */
    e1000_write_reg(E1000_IMC, 0xFFFFFFFF);

    nic_register_driver(&e1000_driver);

    return 0;
}

void irq_network_handler(void) {
    if (!e1000_active) {
        pic_send_eoi(11);
        return;
    }

    /* Read ICR (Interrupt Cause Register) to clear interrupt */
    volatile uint32_t icr = e1000_read_reg(E1000_ICR);
    (void)icr; /* Suppress unused variable warning */

    /* If Packet Received (or Link Change), signal network task */
    /* Checks: RXT0 (Bit 7), RXO (Bit 6), RXDMT0 (Bit 4), LSC (Bit 2) */
    if (icr & ((1<<7) | (1<<6) | (1<<4) | (1<<2))) {
        sem_signal(&net_sem);
    }

    pic_send_eoi(11);
}

uint8_t* e1000_get_mac(void) {
    return mac_address;
}

int e1000_send_packet(const void* data, uint16_t len) {
    if (!e1000_active) return -1;
    if (len > PAGE_SIZE) return -1;

    while (!(tx_descs[tx_cur].status & E1000_STA_DD)) {
        task_yield();
    }
    
    memcpy(tx_buffers[tx_cur], data, len);
    tx_descs[tx_cur].addr = (uint64_t)(uintptr_t)tx_buffers[tx_cur];
    tx_descs[tx_cur].length = len;
    tx_descs[tx_cur].cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS; // End of Packet, Insert FCS, Report Status
    tx_descs[tx_cur].status = 0;
    
    uint8_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
    
    e1000_write_reg(E1000_TDT, tx_cur);
    
    /* Esperar a que se envíe (bloqueante por ahora, polling status DD) */
    /* Timeout de 2 segundos aprox */
    uint64_t start_ticks = timer_get_ticks();
    uint64_t timeout_ticks = TIMER_HZ * 2;

    while (!(tx_descs[old_cur].status & 0xFF)) {
        if (timer_get_ticks() - start_ticks > timeout_ticks) {
            serial_write_string("[NET] Error: Timeout enviando paquete.\n");
            return -1;
        }
    }
    
    serial_write_string("[NET] Paquete enviado.\n");
    return 0;
}

/* Polling de recepción (deberá ser llamado periódicamente o tras IRQ) */
int e1000_receive(void* buffer, uint16_t max_len) {
    if (!e1000_active) return 0;
    
    if (rx_descs[rx_cur].status & E1000_STA_DD) {
        /* Tenemos un paquete */
        uint16_t len = rx_descs[rx_cur].length;
        if (len > max_len) len = max_len;
        
        memcpy(buffer, rx_buffers[rx_cur], len);
        
        /* Reset Descriptor Status */
        rx_descs[rx_cur].status = 0;
        
        /* Avanzar anillo */
        uint16_t old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % NUM_RX_DESC;
        
        /* Notificar al hardware que el descriptor está libre de nuevo */
        e1000_write_reg(E1000_RDT, old_cur);
        
        return len;
    }
    return 0;
}

int e1000_is_active(void) {
    return e1000_active;
}
