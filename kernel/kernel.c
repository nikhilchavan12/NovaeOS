/* =============================================================================
 * NovaeOS - Kernel Main (kernel/kernel.c)
 * Entry point called by the multiboot bootloader
 * ============================================================================= */

#include "drivers/vga.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include "shell/shell.h"

/* Multiboot magic number */
#define MULTIBOOT_MAGIC 0x2BADB002

/* Multiboot info structure (we use a minimal subset) */
typedef struct __attribute__((packed)) {
    unsigned int flags;
    unsigned int mem_lower;   /* in KB */
    unsigned int mem_upper;   /* in KB */
    /* ... more fields we don't use */
} multiboot_info_t;

/* Kernel heap - 1 MB statically allocated in BSS */
#define HEAP_SIZE (1024 * 1024)  /* 1 MB */
static unsigned char kernel_heap[HEAP_SIZE];

/* PMM bitmap - supports up to 256 MB RAM */
#define PMM_BITMAP_SIZE (256 * 1024 / 32)   /* 256MB / 4KB / 32 bits */
static unsigned int pmm_bitmap[PMM_BITMAP_SIZE];

/* =============================================================================
 * kernel_main - Called by boot.asm after CPU setup
 *   magic  - should be MULTIBOOT_MAGIC (0x2BADB002)
 *   mbinfo - pointer to multiboot_info_t (from GRUB)
 * ============================================================================= */
void kernel_main(unsigned int magic, multiboot_info_t *mbinfo) {

    /* Step 1: Initialize VGA (so we can print errors during init) */
    vga_init();
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("[BOOT] VGA initialized\n");

    /* Step 2: Verify multiboot magic */
    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("[FATAL] Invalid multiboot magic! Halting.\n");
        __asm__ volatile ("cli; hlt");
    }
    vga_puts("[BOOT] Multiboot verified\n");

    /* Step 3: Set up GDT */
    gdt_init();
    vga_puts("[BOOT] GDT loaded\n");

    /* Step 4: Set up IDT + PIC remapping */
    idt_init();
    vga_puts("[BOOT] IDT + PIC initialized\n");

    /* Step 5: Physical Memory Manager
     * Use mem_upper from multiboot (RAM above 1MB in KB)
     * If GRUB didn't provide it, default to 32 MB */
    unsigned int mem_kb = 0;
    if (mbinfo && (mbinfo->flags & 0x1))
        mem_kb = mbinfo->mem_upper + 1024;  /* mem_upper is above 1MB */
    if (mem_kb < 4096)
        mem_kb = 32 * 1024;  /* Fallback: 32 MB */

    pmm_init(mem_kb, pmm_bitmap);
    /* Mark first 4 MB as used (kernel + reserved) */
    vga_puts("[BOOT] PMM initialized\n");

    /* Step 6: Kernel heap */
    heap_init(kernel_heap, HEAP_SIZE);
    vga_puts("[BOOT] Heap initialized (1 MB)\n");

    /* Step 7: PIT Timer at 100 Hz */
    timer_init(100);
    vga_puts("[BOOT] PIT Timer @ 100 Hz\n");

    /* Step 8: PS/2 Keyboard */
    keyboard_init();
    vga_puts("[BOOT] Keyboard driver ready\n");

    /* Step 9: Virtual Filesystem */
    vfs_init();
    vga_puts("[BOOT] VFS initialized\n");

    /* Step 10: Enable interrupts */
    __asm__ volatile ("sti");
    vga_puts("[BOOT] Interrupts enabled\n");

    /* Brief boot delay so user can read messages */
    timer_sleep(1500);

    /* Step 11: Launch the shell! */
    shell_run();

    /* Should never reach here */
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("\n[FATAL] Shell exited! System halted.\n");
    __asm__ volatile ("cli; hlt");
}
