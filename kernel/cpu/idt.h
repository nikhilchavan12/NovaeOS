/* =============================================================================
 * NovaeOS - IDT (Interrupt Descriptor Table) (kernel/cpu/idt.h)
 * ============================================================================= */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

/* IDT entry (8 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t base_low;   /* Lower 16 bits of handler address */
    uint16_t selector;   /* Kernel code segment selector */
    uint8_t  zero;       /* Always 0 */
    uint8_t  flags;      /* Gate type, DPL, Present */
    uint16_t base_high;  /* Upper 16 bits of handler address */
} idt_entry_t;

/* IDT pointer */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

/* Saved registers for interrupt handlers */
typedef struct __attribute__((packed)) {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;                 /* CPU auto-push */
} registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

void idt_init(void);
void idt_register_handler(uint8_t n, isr_handler_t handler);

/* Exported ISR stubs (defined in isr.asm) */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* IRQ stubs */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

#endif /* IDT_H */
