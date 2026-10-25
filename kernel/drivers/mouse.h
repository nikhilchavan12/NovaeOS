/* =============================================================================
 * NovaeOS - PS/2 Mouse Driver (kernel/drivers/mouse.h)
 * ============================================================================= */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

typedef struct {
    int     x, y;
    uint8_t buttons;
} mouse_event_t;

void mouse_init(int screen_w, int screen_h);
bool mouse_poll(mouse_event_t *ev);

#endif /* MOUSE_H */
