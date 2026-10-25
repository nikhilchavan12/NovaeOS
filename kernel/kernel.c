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
#include "gfx/gfx.h"
#include "gfx/fbcon.h"
#include "gui/gui.h"
#include "lib/kstring.h"

/* Multiboot magic number */
#define MULTIBOOT_MAGIC 0x2BADB002

/* Multiboot info structure */
typedef struct __attribute__((packed)) {
    unsigned int flags;
    unsigned int mem_lower;   /* in KB */
    unsigned int mem_upper;   /* in KB */
    unsigned int boot_device;
    unsigned int cmdline;     /* flag bit 2 */
    unsigned int mods_count, mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length, mmap_addr;
    unsigned int drives_length, drives_addr;
    unsigned int config_table;
    unsigned int boot_loader_name;
    unsigned int apm_table;
    unsigned int vbe_control_info, vbe_mode_info;
    unsigned short vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    unsigned long long framebuffer_addr;   /* flag bit 12 */
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;        /* 1 = direct RGB */
} multiboot_info_t;

/* True if the kernel command line contains 'word' as a separate token */
static bool cmdline_has(multiboot_info_t *mb, const char *word) {
    if (!mb || !(mb->flags & 0x4) || !mb->cmdline) return false;
    const char *s = (const char *)mb->cmdline;
    size_t n = kstrlen(word);
    for (; *s; s++) {
        if (kstrncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ') &&
            (s == (const char *)mb->cmdline || s[-1] == ' '))
            return true;
    }
    return false;
}

static void fpu_init(void) {
    unsigned int cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1u << 2);           /* EM: no emulation */
    cr0 |=  (1u << 1);           /* MP: monitor coprocessor */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    __asm__ volatile ("fninit");
}

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
    if (mem_kb > 256 * 1024)
        mem_kb = 256 * 1024; /* PMM bitmap covers 256 MB */

    pmm_init(mem_kb, pmm_bitmap);   /* everything below _kernel_end stays reserved */
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

    fpu_init();

    /* Step 11: Desktop GUI, unless the text-mode shell was requested */
    gfx_boot_fb_t boot_fb = {0};
    bool have_fb = mbinfo && (mbinfo->flags & (1u << 12)) && mbinfo->framebuffer_type == 1 &&
                   mbinfo->framebuffer_addr < 0x100000000ULL;
    if (have_fb) {
        boot_fb.addr   = (uint32_t)mbinfo->framebuffer_addr;
        boot_fb.pitch  = mbinfo->framebuffer_pitch;
        boot_fb.width  = mbinfo->framebuffer_width;
        boot_fb.height = mbinfo->framebuffer_height;
        boot_fb.bpp    = mbinfo->framebuffer_bpp;
    }

    if (cmdline_has(mbinfo, "text")) {
        /* The bootloader may still hand us a graphics mode: draw the text console into it */
        if (have_fb && gfx_init(&boot_fb))
            fbcon_init();
    } else {
        if (gfx_init(have_fb ? &boot_fb : NULL))
            gui_run();
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        vga_puts("[BOOT] No linear framebuffer available, starting text shell\n");
    }

    /* Brief boot delay so user can read messages */
    timer_sleep(1500);

    /* Step 12: Launch the shell! */
    shell_run();

    /* Should never reach here */
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("\n[FATAL] Shell exited! System halted.\n");
    __asm__ volatile ("cli; hlt");
}
