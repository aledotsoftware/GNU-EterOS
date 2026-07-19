# eterOS Hardware Abstraction Layer (HAL) API

The HAL abstracts hardware-specific operations, providing a unified interface for the kernel to interact with the underlying hardware regardless of the target architecture (x86_64, ARM, etc.).

## Memory Management

```c
uint64_t hal_mem_get_phys(uint64_t virt);
```
Translates a virtual address to its corresponding physical address based on the current page tables.

```c
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags);
```
Maps a physical frame to a virtual page with the specified permissions (flags).

```c
void hal_mem_unmap(uint64_t virt);
```
Unmaps the virtual page and invalidates the corresponding TLB entry.

## Interrupts

```c
void hal_interrupts_disable(void);
```
Disables local CPU interrupts (e.g., `cli` on x86).

```c
void hal_interrupts_enable(void);
```
Enables local CPU interrupts (e.g., `sti` on x86).

```c
uint64_t hal_interrupts_save_and_disable(void);
```
Saves the current interrupt state and disables interrupts. Returns the saved state.

```c
void hal_interrupts_restore(uint64_t state);
```
Restores the interrupt state previously saved by `hal_interrupts_save_and_disable()`.

## Input/Output (I/O)

```c
uint8_t hal_inb(uint16_t port);
uint16_t hal_inw(uint16_t port);
uint32_t hal_inl(uint16_t port);
```
Reads a byte, word, or dword from the specified I/O port.

```c
void hal_outb(uint16_t port, uint8_t val);
void hal_outw(uint16_t port, uint16_t val);
void hal_outl(uint16_t port, uint32_t val);
```
Writes a byte, word, or dword to the specified I/O port.
