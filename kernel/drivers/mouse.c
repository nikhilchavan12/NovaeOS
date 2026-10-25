/* =============================================================================
 * NovaeOS - PS/2 Mouse Driver (kernel/drivers/mouse.c)
 * IRQ12, standard 3-byte packets, absolute position clamped to the screen
 * ============================================================================= */

#include "mouse.h"
#include "../cpu/idt.h"
#include "../lib/io.h"

#define MOUSE_QUEUE 128

static int max_x = 1023, max_y = 767;
static volatile int cur_x = 512, cur_y = 384;
static uint8_t packet[3];
static int     packet_idx = 0;

static mouse_event_t queue[MOUSE_QUEUE];
static volatile unsigned q_head = 0, q_tail = 0;

static void ctrl_wait_write(void) {
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++) { }
}
static void ctrl_wait_read(void) {
    for (int i = 0; i < 100000 && !(inb(0x64) & 0x01); i++) { }
}
static void mouse_write(uint8_t v) {
    ctrl_wait_write(); outb(0x64, 0xD4);
    ctrl_wait_write(); outb(0x60, v);
    ctrl_wait_read();  (void)inb(0x60);   /* ACK */
}

static void mouse_callback(registers_t *regs) {
    (void)regs;
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return;
    uint8_t data = inb(0x60);
    if (!(status & 0x20)) return;           /* not from the aux device */

    if (packet_idx == 0 && !(data & 0x08)) return;   /* resync */
    packet[packet_idx++] = data;
    if (packet_idx < 3) return;
    packet_idx = 0;

    if (packet[0] & 0xC0) return;           /* overflow, drop */
    int dx = packet[1] - ((packet[0] & 0x10) ? 256 : 0);
    int dy = packet[2] - ((packet[0] & 0x20) ? 256 : 0);

    int nx = cur_x + dx, ny = cur_y - dy;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > max_x) nx = max_x;
    if (ny > max_y) ny = max_y;
    cur_x = nx; cur_y = ny;

    unsigned next = (q_head + 1) % MOUSE_QUEUE;
    if (next != q_tail) {
        queue[q_head].x = nx;
        queue[q_head].y = ny;
        queue[q_head].buttons = packet[0] & 0x07;
        q_head = next;
    }
}

void mouse_init(int screen_w, int screen_h) {
    max_x = screen_w - 1;
    max_y = screen_h - 1;
    cur_x = screen_w / 2;
    cur_y = screen_h / 2;

    ctrl_wait_write(); outb(0x64, 0xA8);             /* enable aux port */
    ctrl_wait_write(); outb(0x64, 0x20);             /* read config byte */
    ctrl_wait_read();
    uint8_t cfg = inb(0x60);
    cfg |= 0x02;                                     /* IRQ12 on */
    cfg &= (uint8_t)~0x20;                           /* aux clock on */
    ctrl_wait_write(); outb(0x64, 0x60);
    ctrl_wait_write(); outb(0x60, cfg);

    mouse_write(0xF6);                               /* defaults */
    mouse_write(0xF4);                               /* enable streaming */

    idt_register_handler(44, mouse_callback);        /* IRQ12 = INT 44 */

    /* Unmask IRQ2 (cascade) on the master PIC and IRQ12 on the slave */
    outb(0x21, inb(0x21) & (uint8_t)~0x04);
    outb(0xA1, inb(0xA1) & (uint8_t)~0x10);
}

bool mouse_poll(mouse_event_t *ev) {
    if (q_tail == q_head) return false;
    *ev = queue[q_tail];
    q_tail = (q_tail + 1) % MOUSE_QUEUE;
    return true;
}
