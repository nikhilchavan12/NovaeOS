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
static uint32_t ticks_per_ms = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;
}

void timer_init(uint32_t frequency_hz) {
    ticks_per_ms = frequency_hz / 1000;
    if (ticks_per_ms == 0) ticks_per_ms = 1;

    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    /* Channel 0, lobyte/hibyte, rate generator (mode 2) */
    outb(PIT_CMD,   0x36);
    outb(PIT_DATA0, (uint8_t)(divisor & 0xFF));
    outb(PIT_DATA0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register our IRQ0 handler */
    idt_register_handler(32, timer_callback);
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

void timer_sleep(uint32_t ms) {
    uint32_t end = tick_count + (ms * ticks_per_ms);
    while (tick_count < end)
        __asm__ volatile ("hlt");
}
