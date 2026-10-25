/* =============================================================================
 * NovaeOS - Framebuffer text console (kernel/gfx/fbcon.c)
 * ============================================================================= */

#include "fbcon.h"
#include "gfx.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"

static uint16_t text_buf[VGA_WIDTH * VGA_HEIGHT];
static uint16_t shown_buf[VGA_WIDTH * VGA_HEIGHT];
static size_t   shown_col = 999, shown_row = 999;
static bool     shown_blink;

static const uint32_t vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static void draw_cell(int ox, int oy, size_t i) {
    uint16_t e = text_buf[i];
    int x = ox + (int)(i % VGA_WIDTH) * FONT_W, y = oy + (int)(i / VGA_WIDTH) * FONT_H;
    gfx_fill_rect(x, y, FONT_W, FONT_H, vga_palette[(e >> 12) & 0x0F]);
    gfx_draw_char(x, y, (uint8_t)(e & 0xFF), vga_palette[(e >> 8) & 0x0F]);
}

static void refresh(uint32_t ticks) {
    if (ticks % 4) return;
    int ox = (gfx_width() - VGA_WIDTH * FONT_W) / 2, oy = (gfx_height() - VGA_HEIGHT * FONT_H) / 2;
    size_t col, row;
    vga_get_cursor(&col, &row);
    bool blink = (ticks / 50) % 2 == 0;
    bool changed = false;

    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        if (text_buf[i] != shown_buf[i] || i == shown_row * VGA_WIDTH + shown_col) {
            shown_buf[i] = text_buf[i];
            draw_cell(ox, oy, i);
            changed = true;
        }
    }
    if (col != shown_col || row != shown_row || blink != shown_blink) {
        changed = true;
        if (shown_row < VGA_HEIGHT && shown_col < VGA_WIDTH) draw_cell(ox, oy, shown_row * VGA_WIDTH + shown_col);
    }
    if (!changed) return;
    if (blink && row < VGA_HEIGHT && col < VGA_WIDTH)
        gfx_fill_rect(ox + (int)col * FONT_W, oy + (int)row * FONT_H + 13, FONT_W, 2,
                      vga_palette[(text_buf[row * VGA_WIDTH + col] >> 8) & 0x0F]);
    shown_col = col; shown_row = row; shown_blink = blink;
    gfx_present_rect(ox, oy, VGA_WIDTH * FONT_W, VGA_HEIGHT * FONT_H);
}

void fbcon_init(void) {
    gfx_set_cursor_pos(-100, -100);
    gfx_fill_rect(0, 0, gfx_width(), gfx_height(), 0x000000);
    gfx_present();
    vga_redirect(text_buf);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) shown_buf[i] = (uint16_t)~text_buf[i];
    timer_set_hook(refresh);
}
