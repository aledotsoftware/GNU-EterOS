; =============================================================================
; éterOS - Context Switch (x86_64)
; =============================================================================
; Realiza el cambio de contexto entre dos tareas.
; Guarda los registros callee-saved y RSP de la tarea actual.
; Restaura los registros callee-saved y RSP de la nueva tarea.
;
; Los registros caller-saved (RAX, RCX, RDX, RSI, RDI, R8-R11) ya fueron
; guardados por el llamador (convención System V AMD64 ABI), o por el
; ISR stub si venimos de una interrupción.
; =============================================================================

[bits 64]

global context_switch

section .text

; -----------------------------------------------------------------------------
; context_switch(uint64_t* old_rsp, uint64_t* new_rsp, void* old_fpu, void* new_fpu)
;   RDI = puntero donde guardar el RSP actual (de la tarea saliente)
;   RSI = puntero al RSP de la tarea entrante (a restaurar)
;   RDX = puntero al buffer FPU (fpu_state) de la tarea saliente
;   RCX = puntero al buffer FPU (fpu_state) de la tarea entrante
; -----------------------------------------------------------------------------
context_switch:
    ; Guardar FPU state si old_fpu no es NULL
    test rdx, rdx
    jz .skip_fpu_save
    fxsave [rdx]
.skip_fpu_save:

    ; Guardar registros callee-saved en el stack actual
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Guardar el RSP actual en *old_rsp
    mov [rdi], rsp

    ; Cargar el RSP de la nueva tarea (*new_rsp)
    mov rsp, [rsi]

    ; Restaurar registros callee-saved del stack de la nueva tarea
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Restaurar FPU state si new_fpu no es NULL
    test rcx, rcx
    jz .skip_fpu_restore
    fxrstor [rcx]
.skip_fpu_restore:

    ; RET salta a la dirección de retorno que está en el stack de la nueva tarea.
    ; Si la tarea ya corría antes, vuelve a schedule() -> timer ISR -> iretq.
    ; Si es la primera vez, salta a task_entry_wrapper().
    ret

; -----------------------------------------------------------------------------
; fork_return()
;   Punto de retorno para procesos hijos creados con fork().
;   Restaura el estado de usuario y retorna via sysret.
;   La tarea hija "despierta" aquí con RSP apuntando a los registros guardados.
; -----------------------------------------------------------------------------
global fork_return
fork_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Restaurar User Stack Pointer desde Per-CPU Data (offset 72 - user_stack_scratch)
    ; schedule() se aseguró de que gs:72 tenga el user_rsp del hijo.
    mov rsp, [gs:72]

    ; Restaurar User GS Base
    swapgs

    ; Retornar a Ring 3
    o64 sysret

%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
