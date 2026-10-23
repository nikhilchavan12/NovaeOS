/* =============================================================================
 * NovaeOS - IDT Implementation (kernel/cpu/idt.c)
 * ============================================================================= */

#include "idt.h"
#include <stdint.h>

static idt_entry_t  idt_entries[IDT_ENTRIES];
static idt_ptr_t    idt_ptr;
static isr_handler_t interrupt_handlers[IDT_ENTRIES];

extern void idt_flush(uint32_t idt_ptr_addr);

/* ============================================================================
 * PIC (8259) remapping
 * By default, PIC interrupts overlap CPU exceptions (IRQ0 = INT 8).
 * We remap PIC1 to INT 32-39 and PIC2 to INT 40-47.
 * ============================================================================ */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20   /* End-of-interrupt command */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void io_wait(void) { outb(0x80, 0); }

static void pic_remap(void) {
    uint8_t a1 = inb(PIC1_DATA);  /* Save masks */
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_CMD,  0x11); io_wait(); /* ICW1: cascade, ICW4 needed */
    outb(PIC2_CMD,  0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait(); /* ICW2: PIC1 vector offset = 32 */
    outb(PIC2_DATA, 0x28); io_wait(); /* ICW2: PIC2 vector offset = 40 */
    outb(PIC1_DATA, 0x04); io_wait(); /* ICW3: slave PIC at IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait(); /* ICW3: cascade identity */
    outb(PIC1_DATA, 0x01); io_wait(); /* ICW4: 8086 mode */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, a1);  /* Restore masks */
    outb(PIC2_DATA, a2);
}

/* Set one IDT gate */
static void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[n].base_low  = base & 0xFFFF;
    idt_entries[n].base_high = (base >> 16) & 0xFFFF;
    idt_entries[n].selector  = sel;
    idt_entries[n].zero      = 0;
    idt_entries[n].flags     = flags | 0x60; /* DPL=3 so user can trigger */
}

void idt_init(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    /* Clear the IDT and handler table */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_entries[i].base_low  = 0;
        idt_entries[i].base_high = 0;
        idt_entries[i].selector  = 0;
        idt_entries[i].zero      = 0;
        idt_entries[i].flags     = 0;
        interrupt_handlers[i] = 0;
    }

    pic_remap();

    /* CPU Exceptions (ISR 0-31) */
    idt_set_gate( 0, (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate( 1, (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate( 2, (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate( 3, (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate( 4, (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate( 5, (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate( 6, (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate( 7, (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate( 8, (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate( 9, (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* Hardware Interrupts (IRQ 0-15 remapped to INT 32-47) */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    idt_flush((uint32_t)&idt_ptr);
}

void idt_register_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

/* ============================================================================
 * Common ISR/IRQ dispatch (called from assembly stubs)
 * ============================================================================ */

static const char *exception_messages[] = {
    "Division By Zero",          "Debug",
    "Non Maskable Interrupt",    "Breakpoint",
    "Into Detected Overflow",    "Out of Bounds",
    "Invalid Opcode",            "No Coprocessor",
    "Double Fault",              "Coprocessor Segment Overrun",
    "Bad TSS",                   "Segment Not Present",
    "Stack Fault",               "General Protection Fault",
    "Page Fault",                "Unknown Interrupt",
    "Coprocessor Fault",         "Alignment Check",
    "Machine Check",             "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved"
};

void isr_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no]) {
        interrupt_handlers[regs->int_no](regs);
    }
    /* Default: if no handler and it's a CPU exception, we hang */
    /* (In a full OS you'd display a BSOD / kernel panic here) */
}

void irq_handler(registers_t *regs) {
    /* Send EOI to PICs */
    if (regs->int_no >= 40)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);

    if (interrupt_handlers[regs->int_no])
        interrupt_handlers[regs->int_no](regs);
}
