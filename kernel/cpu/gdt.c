/* =============================================================================
 * NovaeOS - GDT Implementation (kernel/cpu/gdt.c)
 * ============================================================================= */

#include "gdt.h"
#include <stdint.h>

/* 6 GDT entries: null, kernel code, kernel data, user code, user data, TSS */
#define GDT_ENTRIES 6

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* External assembly function to flush/reload GDT */
extern void gdt_flush(uint32_t gdt_ptr_addr);

/* Set a GDT entry */
static void gdt_set_gate(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt_entries[idx].base_low    = (base & 0xFFFF);
    gdt_entries[idx].base_middle = (base >> 16) & 0xFF;
    gdt_entries[idx].base_high   = (base >> 24) & 0xFF;
    gdt_entries[idx].limit_low   = (limit & 0xFFFF);
    gdt_entries[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[idx].access      = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* Entry 0: Null descriptor (required) */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Entry 1: Kernel Code Segment
     * Base=0, Limit=4GB, Ring 0, Executable, Readable
     * Access: 0x9A = 1001 1010
     *   Present(1) | DPL=0(00) | S=1 | Type=1010 (code, readable)
     * Gran:  0xCF = 1100 1111
     *   G=1(4KB pages) | D/B=1(32-bit) | 0 | AVL=0 | limit[19:16]=1111 */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Entry 2: Kernel Data Segment
     * Base=0, Limit=4GB, Ring 0, Writable
     * Access: 0x92 = 1001 0010
     *   Present(1) | DPL=0(00) | S=1 | Type=0010 (data, writable) */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Entry 3: User Code Segment (Ring 3) */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* Entry 4: User Data Segment (Ring 3) */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* Entry 5: Null (TSS placeholder, we'll set up TSS later if needed) */
    gdt_set_gate(5, 0, 0, 0, 0);

    /* Load the GDT */
    gdt_flush((uint32_t)&gdt_ptr);
}
