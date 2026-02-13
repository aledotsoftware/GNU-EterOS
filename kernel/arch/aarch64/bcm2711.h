#ifndef BCM2711_H
#define BCM2711_H

#include <types.h>

/*
 * BCM2711 (Raspberry Pi 4) Memory Map
 *
 * MMIO Base: 0xFE000000 (Legacy peripherals)
 * GIC Base:  0xFF840000 (ARM Local peripherals)
 */

#define MMIO_BASE       0xFE000000

/* ---- UART (PL011) ---- */
#define UART0_OFFSET    0x201000
#define UART0_BASE      (MMIO_BASE + UART0_OFFSET)

/* Registers */
#define UART0_DR        (UART0_BASE + 0x00)
#define UART0_FR        (UART0_BASE + 0x18)
#define UART0_IBRD      (UART0_BASE + 0x24)
#define UART0_FBRD      (UART0_BASE + 0x28)
#define UART0_LCRH      (UART0_BASE + 0x2C)
#define UART0_CR        (UART0_BASE + 0x30)
#define UART0_IMSC      (UART0_BASE + 0x38)
#define UART0_ICR       (UART0_BASE + 0x44)

/* ---- GIC-400 (Generic Interrupt Controller) ---- */
/* RPi4 uses GICv2 on the ARM Local Bus */
#define GIC_BASE        0xFF840000
#define GICD_OFFSET     0x1000
#define GICC_OFFSET     0x2000

#define GICD_BASE       (GIC_BASE + GICD_OFFSET)
#define GICC_BASE       (GIC_BASE + GICC_OFFSET)

/* Distributor Registers */
#define GICD_CTLR       (GICD_BASE + 0x000)
#define GICD_TYPER      (GICD_BASE + 0x004)
#define GICD_ISENABLER  (GICD_BASE + 0x100)
#define GICD_ICENABLER  (GICD_BASE + 0x180)
#define GICD_IPRIORITYR (GICD_BASE + 0x400)
#define GICD_ITARGETSR  (GICD_BASE + 0x800)
#define GICD_ICFGR      (GICD_BASE + 0xC00)

/* CPU Interface Registers */
#define GICC_CTLR       (GICC_BASE + 0x0000)
#define GICC_PMR        (GICC_BASE + 0x0004)
#define GICC_IAR        (GICC_BASE + 0x000C)
#define GICC_EOIR       (GICC_BASE + 0x0010)

/* ---- GPIO ---- */
#define GPIO_BASE       (MMIO_BASE + 0x200000)
#define GPIO_GPFSEL1    (GPIO_BASE + 0x04)
#define GPIO_PUP_PDN0   (GPIO_BASE + 0xE4)

/* ---- Mailbox ---- */
#define MBOX_BASE       (MMIO_BASE + 0xB880)
#define MBOX_READ       (MBOX_BASE + 0x00)
#define MBOX_POLL       (MBOX_BASE + 0x10)
#define MBOX_SENDER     (MBOX_BASE + 0x14)
#define MBOX_STATUS     (MBOX_BASE + 0x18)
#define MBOX_CONFIG     (MBOX_BASE + 0x1C)
#define MBOX_WRITE      (MBOX_BASE + 0x20)

#endif /* BCM2711_H */
