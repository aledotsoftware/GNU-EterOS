1. **Auditar el Layout de Memoria:**
   - La disposición está mayormente definida en `boot/x86_64/boot.asm` y `kernel/mm/pmm.c`. Ya confirmamos que se movió a `FINAL_KERNEL_ADDR` (`0x100000`).
   - El PMM Bitmap está en `0x400000`. Initrd se carga en `0x4000000`. Todo bien aquí.

2. **Orden de Inicialización y `kmalloc` Seguro (`kernel/main.c`):**
   - Asegurar el orden `hal_init` -> `pmm_init` -> `hal_mmu_init` -> `mm_init` con `ASSERT`.
   - Modificar `kernel/main.c` para incluir `ASSERT(1)` entre llamadas si no están, o verificar que no haya llamadas a heap (como en `bcache_init`) antes de `mm_init()`.

3. **PMM Hardening (`kernel/mm/pmm.c`):**
   - Verificar `pmm_init()` y asegurar que reserva (marcar ocupado) las regiones de kernel, initrd, PMM y framebuffer. `boot_info` debería pasarse si se puede o se debe acceder.
   - RefCounts se deben usar correctamente para double-free. Modificar `pmm_free_page()` o `pmm_alloc_page()` si se necesita double-free checks en el bitmap.

4. **VMM Hardening (`kernel/mm/vmm.c`):**
   - Asegurarnos de que `PAGE_USER` se aplica apropiadamente en el nivel `PML4`.

5. **Excepciones y Panics (`kernel/arch/x86_64/idt.c`):**
   - Añadir volcado de stack trace, registros RAX-R15, CR2, CR3, etc. Esto ya se hace parcialmente pero verificar si necesita el walk de `RBP chain`. En `handle_exception` ya vimos el walk, vamos a asegurar que imprima la tarea y PID si no lo hace, y si fue read/write.

6. **TSS y RSP0 (`kernel/task.c` / `kernel/arch/x86_64/gdt.c`):**
   - En `tss_init()` o `gdt_init()` asegurar `tss.rsp0 = kernel_stack_top`.
   - En `schedule()`, antes de `context_switch`, poner `tss_set_rsp0(next_task->kernel_stack)`. Ya está en el código.

7. **Context Switch Mismatch (`kernel/task.c` y `kernel/arch/x86_64/context_switch.asm`):**
   - Verificar convención de punteros.
   - C manda `&current->rsp` y `next_task->rsp`. (Opción B: puntero + valor).
   - ASM hace `mov [rdi], rsp` y `mov rsp, rsi`.
   - En `context_switch.asm`: `mov [rdi], rsp` y `mov rsp, rsi`. Esto parece estar correcto según la Opción B.

8. **Script de Test:**
   - Crear el script `tests/test_boot.sh` para correr QEMU `-serial stdio -no-reboot -no-shutdown` y verificar.

9. **Pre-commit:**
   - "Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done."
