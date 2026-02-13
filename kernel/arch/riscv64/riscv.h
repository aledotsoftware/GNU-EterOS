#ifndef RISCV_H
#define RISCV_H

#include <types.h>

/* ========================================================================= */
/* Memory Map (QEMU virt)                                                    */
/* ========================================================================= */
#define RISCV_CLINT_BASE    0x02000000
#define RISCV_PLIC_BASE     0x0C000000
#define RISCV_UART0_BASE    0x10000000
#define RISCV_RAM_BASE      0x80000000

/* ========================================================================= */
/* SBI Constants                                                             */
/* ========================================================================= */
#define SBI_EID_SET_TIMER           0x54494D45 /* "TIME" */
#define SBI_EID_CONSOLE_PUTCHAR     0x01
#define SBI_EID_CONSOLE_GETCHAR     0x02
#define SBI_EID_SHUTDOWN            0x08
#define SBI_EID_TIMER               0x00

/* SBI Return Structure */
struct sbiret {
    long error;
    long value;
};

/* ========================================================================= */
/* CSR Definitions                                                           */
/* ========================================================================= */
/* Supervisor Status Register */
#define SSTATUS_SIE         (1L << 1)  /* Supervisor Interrupt Enable */
#define SSTATUS_SPIE        (1L << 5)  /* Supervisor Previous Interrupt Enable */
#define SSTATUS_SPP         (1L << 8)  /* Supervisor Previous Privilege */

/* Supervisor Interrupt Enable Register */
#define SIE_SSIE            (1L << 1)  /* Supervisor Software Interrupt Enable */
#define SIE_STIE            (1L << 5)  /* Supervisor Timer Interrupt Enable */
#define SIE_SEIE            (1L << 9)  /* Supervisor External Interrupt Enable */

/* Supervisor Cause Register */
#define SCAUSE_INTR         (1L << 63) /* Interrupt bit */
#define SCAUSE_CODE_MASK    (0xFF)     /* Exception code mask */

/* Interrupt Codes (scause & ~SCAUSE_INTR) */
#define IRQ_S_SOFT          1
#define IRQ_S_TIMER         5
#define IRQ_S_EXT           9

/* Supervisor Address Translation and Protection Register */
#define SATP_MODE_SV39      (8L << 60)
#define SATP_MODE_SV48      (9L << 60)
#define SATP_ASID_MASK      (0xFFFFL << 44)
#define SATP_PPN_MASK       (0xFFFFFFFFFFFL)

/* ========================================================================= */
/* Page Table Definitions (Sv39)                                             */
/* ========================================================================= */
#define PTE_V               (1L << 0) /* Valid */
#define PTE_R               (1L << 1) /* Read */
#define PTE_W               (1L << 2) /* Write */
#define PTE_X               (1L << 3) /* Execute */
#define PTE_U               (1L << 4) /* User */
#define PTE_G               (1L << 5) /* Global */
#define PTE_A               (1L << 6) /* Accessed */
#define PTE_D               (1L << 7) /* Dirty */

#define PAGE_SIZE           4096
#define PAGE_SHIFT          12

/* ========================================================================= */
/* PLIC Register Offsets (Context 1 = Hart 0 S-Mode)                         */
/* ========================================================================= */
#define PLIC_PRIORITY       0x000000
#define PLIC_PENDING        0x001000
#define PLIC_ENABLE         0x002080 /* Context 1 Enable */
#define PLIC_THRESHOLD      0x201000 /* Context 1 Threshold */
#define PLIC_CLAIM          0x201004 /* Context 1 Claim/Complete */

/* ========================================================================= */
/* Inline Assembly Macros                                                    */
/* ========================================================================= */

#define csrr(csr) ({ \
    unsigned long __v; \
    __asm__ volatile ("csrr %0, " #csr : "=r" (__v)); \
    __v; \
})

#define csrw(csr, val) ({ \
    __asm__ volatile ("csrw " #csr ", %0" :: "rK" (val)); \
})

#define csrs(csr, val) ({ \
    __asm__ volatile ("csrs " #csr ", %0" :: "rK" (val)); \
})

#define csrc(csr, val) ({ \
    __asm__ volatile ("csrc " #csr ", %0" :: "rK" (val)); \
})

#define r_tp() ({ \
    unsigned long __v; \
    __asm__ volatile ("mv %0, tp" : "=r" (__v)); \
    __v; \
})

#define w_tp(x) ({ \
    __asm__ volatile ("mv tp, %0" :: "r" (x)); \
})

/* ========================================================================= */
/* MMIO Accessors                                                            */
/* ========================================================================= */

static inline uint8_t mmio_read8(uint64_t addr) {
    return *(volatile uint8_t *)addr;
}

static inline void mmio_write8(uint64_t addr, uint8_t value) {
    *(volatile uint8_t *)addr = value;
}

static inline uint32_t mmio_read32(uint64_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write32(uint64_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
}

#endif /* RISCV_H */
