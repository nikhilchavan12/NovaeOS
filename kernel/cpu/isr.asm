; =============================================================================
; NovaeOS - ISR/IRQ Assembly Stubs (kernel/cpu/isr.asm)
; Low-level interrupt entry points that save/restore registers
; and call C handlers isr_handler() and irq_handler()
; =============================================================================

bits 32

extern isr_handler
extern irq_handler

; ============================================================
; Macro: ISR without error code (CPU doesn't push one)
; ============================================================
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0        ; Dummy error code
    push dword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; ============================================================
; Macro: ISR with error code (CPU already pushed one)
; ============================================================
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; ============================================================
; Macro: IRQ stubs (remapped to INT 32+)
; ============================================================
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0        ; Dummy error code
    push dword %2       ; Interrupt number (32 + IRQ#)
    jmp irq_common_stub
%endmacro

; ============================================================
; CPU Exceptions (0-31)
; ============================================================
ISR_NOERRCODE  0   ; Division by zero
ISR_NOERRCODE  1   ; Debug
ISR_NOERRCODE  2   ; Non-maskable interrupt
ISR_NOERRCODE  3   ; Breakpoint
ISR_NOERRCODE  4   ; Overflow
ISR_NOERRCODE  5   ; Bound range exceeded
ISR_NOERRCODE  6   ; Invalid opcode
ISR_NOERRCODE  7   ; Device not available
ISR_ERRCODE    8   ; Double fault (has error code)
ISR_NOERRCODE  9   ; Coprocessor segment overrun
ISR_ERRCODE   10   ; Invalid TSS (has error code)
ISR_ERRCODE   11   ; Segment not present (has error code)
ISR_ERRCODE   12   ; Stack-segment fault (has error code)
ISR_ERRCODE   13   ; General protection fault (has error code)
ISR_ERRCODE   14   ; Page fault (has error code)
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; x87 FPU exception
ISR_ERRCODE   17   ; Alignment check (has error code)
ISR_NOERRCODE 18   ; Machine check
ISR_NOERRCODE 19   ; SIMD exception
ISR_NOERRCODE 20   ; Virtualization exception
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; ============================================================
; Hardware IRQs (remapped to interrupts 32-47)
; ============================================================
IRQ  0, 32   ; PIT Timer
IRQ  1, 33   ; PS/2 Keyboard
IRQ  2, 34   ; Cascade
IRQ  3, 35   ; COM2
IRQ  4, 36   ; COM1
IRQ  5, 37   ; LPT2
IRQ  6, 38   ; Floppy
IRQ  7, 39   ; LPT1 / Spurious
IRQ  8, 40   ; CMOS RTC
IRQ  9, 41   ; Free/Legacy SCSI
IRQ 10, 42   ; Free/Legacy SCSI
IRQ 11, 43   ; Free
IRQ 12, 44   ; PS/2 Mouse
IRQ 13, 45   ; FPU / Coprocessor
IRQ 14, 46   ; Primary ATA
IRQ 15, 47   ; Secondary ATA

; ============================================================
; Common ISR stub: save all registers, call C handler, restore
; ============================================================
isr_common_stub:
    pusha               ; Push edi,esi,ebp,esp,ebx,edx,ecx,eax
    mov ax, ds
    push eax            ; Save data segment

    mov ax, 0x10        ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Pass pointer to saved registers as argument
    call isr_handler
    add esp, 4          ; Clean up argument

    pop eax             ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8          ; Remove int_no and err_code
    sti
    iret                ; Return from interrupt

; ============================================================
; Common IRQ stub
; ============================================================
irq_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    sti
    iret

; ============================================================
; IDT flush (called from idt.c)
; ============================================================
global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret
