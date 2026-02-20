#include <io.h>
#include <cpu.h>
#include <klog.h>

/* IA32_PAT MSR */
#define MSR_IA32_PAT 0x277

/* PAT Values */
#define PAT_WB  0x06
#define PAT_WC  0x01
#define PAT_UC_ 0x07
#define PAT_UC  0x00

/**
 * Initialize Page Attribute Table (PAT)
 *
 * Configures the PAT MSR to define memory caching types for page tables.
 * Standard configuration used:
 * PA0 (PWT=0, PCD=0): WB  (Write Back)        - 0x06
 * PA1 (PWT=1, PCD=0): WC  (Write Combining)   - 0x01  <-- CHANGED from WT (0x04)
 * PA2 (PWT=0, PCD=1): UC- (Uncacheable Minus) - 0x07
 * PA3 (PWT=1, PCD=1): UC  (Uncacheable)       - 0x00
 *
 * Upper 4 entries mirror the lower 4.
 */
void pat_init(void) {
    uint64_t pat = 0;

    pat |= (uint64_t)PAT_WB  << 0;
    pat |= (uint64_t)PAT_WC  << 8;
    pat |= (uint64_t)PAT_UC_ << 16;
    pat |= (uint64_t)PAT_UC  << 24;

    pat |= (uint64_t)PAT_WB  << 32;
    pat |= (uint64_t)PAT_WC  << 40;
    pat |= (uint64_t)PAT_UC_ << 48;
    pat |= (uint64_t)PAT_UC  << 56;

    wrmsr(MSR_IA32_PAT, pat);

    /* Verify? Reading back MSR is possible but we trust wrmsr */
    // uint64_t read = rdmsr(MSR_IA32_PAT);
}
