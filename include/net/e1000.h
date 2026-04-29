/**
 * éterOS - Intel PRO/1000 (82540EM) Driver Header
 * 
 * Definiciones de registros y estructuras para la tarjeta de red.
 */

#ifndef ETEROS_NET_E1000_H
#define ETEROS_NET_E1000_H

#include "../types.h"
#include "../pci.h"

/* ========================================================================= */
/* Registros MMIO (Offsets desde la Base Address)                            */
/* ========================================================================= */

#define E1000_CTRL      0x00000  /* Device Control - RW */
#define E1000_STATUS    0x00008  /* Device Status - RO */
#define E1000_EERD      0x00014  /* EEPROM Read - RW */
#define E1000_ICR       0x000C0  /* Interrupt Cause Read - RC */
#define E1000_ICS       0x000C8  /* Interrupt Cause Set - WO */
#define E1000_IMS       0x000D0  /* Interrupt Mask Set - RW */
#define E1000_IMC       0x000D8  /* Interrupt Mask Clear - WO */

/* Receive Registers */
#define E1000_RCTL      0x00100  /* RX Control - RW */
#define E1000_RDBAL     0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH     0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN     0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH       0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT       0x02818  /* RX Descriptor Tail - RW */

/* Transmit Registers */
#define E1000_TCTL      0x00400  /* TX Control - RW */
#define E1000_TDBAL     0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH     0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN     0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH       0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT       0x03818  /* TX Descriptor Tail - RW */

/* MAC Address (RAL/RAH) - Receive Address Array */
#define E1000_RAL       0x05400  /* Receive Address Low - RW */
#define E1000_RAH       0x05404  /* Receive Address High - RW */

/* ========================================================================= */
/* Flags de Control                                                          */
/* ========================================================================= */

/* CTRL Register */
#define E1000_CTRL_SLU   (1 << 6)   /* Set Link Up */
#define E1000_CTRL_RST   (1 << 26)  /* Device Reset */

/* RCTL Register */
#define E1000_RCTL_EN    (1 << 1)   /* Receiver Enable */
#define E1000_RCTL_SBP   (1 << 2)   /* Store Bad Packets */
#define E1000_RCTL_UPE   (1 << 3)   /* Unicast Promiscuous Enabled */
#define E1000_RCTL_MPE   (1 << 4)   /* Multicast Promiscuous Enabled */
#define E1000_RCTL_LPE   (1 << 5)   /* Long Packet Enable */
#define E1000_RCTL_LBM_NONE (0 << 6) /* Loopback Mode: None */
#define E1000_RCTL_BAM   (1 << 15)  /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048 (0 << 16)
#define E1000_RCTL_BSIZE_4096 (1 << 16)
#define E1000_RCTL_SECRC (1 << 26)  /* Strip Ethernet CRC */

/* TCTL Register */
#define E1000_TCTL_EN    (1 << 1)   /* Transmit Enable */
#define E1000_TCTL_PSP   (1 << 3)   /* Pad Short Packets */
#define E1000_TCTL_CT    (0x0F << 4) /* Collision Threshold */
#define E1000_TCTL_COLD  (0x40 << 12) /* Collision Distance */
#define E1000_TCTL_RTLC  (1 << 24)  /* Re-transmit on Late Collision */

/* Descriptor Commands */
#define E1000_CMD_EOP    (1 << 0)   /* End of Packet */
#define E1000_CMD_IFCS   (1 << 1)   /* Insert FCS */
#define E1000_CMD_IC     (1 << 2)   /* Insert Checksum */
#define E1000_CMD_RS     (1 << 3)   /* Report Status */

#define E1000_STA_DD     (1 << 0)   /* Descriptor Done */

/* ========================================================================= */
/* Estructuras de Descriptores                                               */
/* ========================================================================= */

/* Descriptor de Recepción (Legacy) */
struct e1000_rx_desc {
    uint64_t addr;      /* Dirección física del buffer (64-bit) */
    uint16_t length;    /* Longitud del paquete recibido */
    uint16_t checksum;
    uint8_t  status;    /* Estado del descriptor */
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

/* Descriptor de Transmisión (Legacy) */
struct e1000_tx_desc {
    uint64_t addr;      /* Dirección física del buffer */
    uint16_t length;
    uint8_t  cso;       /* Checksum Offset */
    uint8_t  cmd;       /* Comando */
    uint8_t  status;
    uint8_t  css;       /* Checksum Start */
    uint16_t special;
} __attribute__((packed));

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

/* Inicializa el controlador E1000 */
int e1000_init(pci_device_t* pci_dev);

/* Envía un paquete (Raw Ethernet Frame). Retorna 0 en éxito, -1 en error. */
int e1000_send_packet(const void* data, uint16_t len);

/* Recibe un paquete (Polling). Retorna longitud o 0 si no hay paq. */
int e1000_receive(void* buffer, uint16_t max_len);

/* Obtener dirección MAC */
uint8_t* e1000_get_mac(void);

/* Retorna 1 si el driver está activo, 0 si no */
int e1000_is_active(void);

#endif /* ETEROS_NET_E1000_H */
