/* =============================================================================
 * NovaeOS - Linker Script (linker.ld)
 * Controls the memory layout of the kernel binary
 *
 * Memory map:
 *   0x00100000  (1 MB) - Kernel load address (above BIOS region)
 *   .multiboot  - Multiboot header (must come first)
 *   .text       - Executable code
 *   .rodata     - Read-only data (strings, constants)
 *   .data       - Initialized global variables
 *   .bss        - Uninitialized global variables (stack, bitmap, heap)
 * ============================================================================= */

ENTRY(_start)

SECTIONS {
    /* Load kernel at 1 MB physical = 0x00100000 */
    . = 0x00100000;

    /* ---- Multiboot header (must be in first 8KB of image) ---- */
    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    /* ---- Code segment ---- */
    .text ALIGN(4096) : {
        *(.text)
        *(.text.*)
    }

    /* ---- Read-only data ---- */
    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* ---- Initialized data ---- */
    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    /* ---- BSS (zero-initialized) ---- */
    .bss ALIGN(4096) : {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    /* Record end of kernel */
    _kernel_end = .;

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
