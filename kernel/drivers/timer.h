/* =============================================================================
 * NovaeOS - PIT Timer Driver (kernel/drivers/timer.h)
 * ============================================================================= */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
uint32_t timer_get_ticks(void);
uint32_t timer_get_hz(void);
void     timer_set_hook(void (*hook)(uint32_t ticks));   /* runs in IRQ0 context */
void timer_sleep(uint32_t ms);

#endif /* TIMER_H */
