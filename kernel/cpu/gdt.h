/* =============================================================================
 * NovaeOS - GDT (Global Descriptor Table) (kernel/cpu/gdt.h)
 * Sets up memory segmentation for protected mode
 * ============================================================================= */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* GDT segment selectors */
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18
#define GDT_USER_DATA    0x20
#define GDT_TSS          0x28

/* GDT entry (8 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;    /* Lower 16 bits of segment limit */
    uint16_t base_low;     /* Lower 16 bits of base address */
    uint8_t  base_middle;  /* Bits 16-23 of base address */
    uint8_t  access;       /* Access byte (ring, type, etc.) */
    uint8_t  granularity;  /* Flags + upper 4 bits of limit */
    uint8_t  base_high;    /* Bits 24-31 of base address */
} gdt_entry_t;

/* GDT pointer loaded with lgdt */
typedef struct __attribute__((packed)) {
    uint16_t limit;   /* Size of GDT - 1 */
    uint32_t base;    /* Linear address of GDT */
} gdt_ptr_t;

void gdt_init(void);

#endif /* GDT_H */
