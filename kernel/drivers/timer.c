/* =============================================================================
 * NovaeOS - PIT Timer Driver (kernel/drivers/timer.c)
 * Programmable Interval Timer (8253/8254) at ~100Hz
 * ============================================================================= */

#include "timer.h"
#include "../cpu/idt.h"

#define PIT_BASE_FREQ  1193180   /* PIT oscillator frequency in Hz */
#define PIT_CMD        0x43
#define PIT_DATA0      0x40

static volatile uint32_t tick_count = 0;
static uint32_t timer_hz = 100;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void (*tick_hook)(uint32_t ticks) = 0;

void timer_set_hook(void (*hook)(uint32_t ticks)) {
    tick_hook = hook;
}

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;
    if (tick_hook) tick_hook(tick_count);
}

void timer_init(uint32_t frequency_hz) {
    timer_hz = frequency_hz;

    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    /* Channel 0, lobyte/hibyte, rate generator (mode 2) */
    outb(PIT_CMD,   0x36);
    outb(PIT_DATA0, (uint8_t)(divisor & 0xFF));
    outb(PIT_DATA0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register our IRQ0 handler */
    idt_register_handler(32, timer_callback);
}

uint32_t timer_get_hz(void) {
    return timer_hz;
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

void timer_sleep(uint32_t ms) {
    uint32_t end = tick_count + (ms * timer_hz + 999) / 1000;
    while (tick_count < end)
        __asm__ volatile ("hlt");
}
