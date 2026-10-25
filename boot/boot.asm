; =============================================================================
; NovaeOS - Bootloader (boot.asm)
; Multiboot-compliant bootloader for GRUB
; =============================================================================

bits 32

; Multiboot header constants
MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_FLAGS     equ 0x00000007   ; Align modules + memory map + video mode
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Stack size
STACK_SIZE equ 0x4000  ; 16 KB

; =============================================================================
; Multiboot Header (must be in first 8KB of kernel image)
; =============================================================================
section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM
    ; Address fields (unused: flag 16 is clear, GRUB loads us as ELF)
    dd 0, 0, 0, 0, 0
    ; Preferred video mode: linear framebuffer 1024x768x32
    dd 0            ; mode_type (0 = linear graphics)
    dd 1024         ; width
    dd 768          ; height
    dd 32           ; depth

; =============================================================================
; BSS Section - Uninitialized data (stack)
; =============================================================================
section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:

; =============================================================================
; Text Section - Entry point
; =============================================================================
section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Save multiboot info (passed in ebx by GRUB)
    push ebx        ; multiboot info pointer
    push eax        ; multiboot magic number

    ; Call the kernel's main function
    call kernel_main

    ; If kernel_main returns (it shouldn't), halt the CPU
.hang:
    cli             ; Disable interrupts
    hlt             ; Halt CPU
    jmp .hang       ; Loop forever just in case
