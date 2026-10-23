; =============================================================================
; NovaeOS - GDT Flush (kernel/cpu/gdt_flush.asm)
; Reloads segment registers after loading the GDT
; =============================================================================

bits 32

global gdt_flush

; void gdt_flush(uint32_t gdt_ptr_addr)
gdt_flush:
    mov eax, [esp + 4]   ; Get pointer to gdt_ptr struct
    lgdt [eax]           ; Load the GDT

    ; Reload all segment registers with kernel data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with kernel code selector (0x08)
    jmp 0x08:.flush

.flush:
    ret
