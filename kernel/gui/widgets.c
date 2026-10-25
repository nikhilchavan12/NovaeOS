/* =============================================================================
 * NovaeOS - Common widgets (kernel/gui/widgets.c)
 * ============================================================================= */

#include "gui.h"
#include "../lib/kstring.h"

bool ui_hit(int px, int py, int x, int y, int w, int h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}

void ui_text_center(int x, int y, int w, int h, const char *s, uint32_t c) {
    gfx_draw_text(x + (w - gfx_text_width(s)) / 2, y + (h - FONT_H) / 2, s, c);
}

void ui_button(int x, int y, int w, int h, const char *label, bool hover, bool primary) {
    uint32_t bg = primary ? theme.accent : theme.panel;
    if (hover) bg = primary ? gfx_lighten(bg, 30) : theme.hover;
    gfx_fill_round_rect(x, y, w, h, 5, primary ? bg : theme.border);
    gfx_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 4, bg);
    ui_text_center(x, y, w, h, label, primary ? 0xFFFFFF : theme.win_fg);
}

void ui_textbox(int x, int y, int w, int h, const char *text, bool focused) {
    gfx_fill_round_rect(x, y, w, h, 4, focused ? theme.accent : theme.border);
    gfx_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 3, theme.input_bg);
    if (focused) gfx_fill_rect(x + 3, y + h - 2, w - 6, 2, theme.accent);
    gfx_set_clip(x + 4, y, w - 8, h);
    int tx = x + 8, ty = y + (h - FONT_H) / 2;
    gfx_draw_text(tx, ty, text, theme.win_fg);
    if (focused && (wm_ticks() / 50) % 2 == 0)
        gfx_fill_rect(tx + gfx_text_width(text), ty, 2, FONT_H, theme.win_fg);
    gfx_reset_clip();
}

void ui_utoa(uint32_t v, char *out) {
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
}

void ui_format_size(uint32_t bytes, char *out) {
    if (bytes < 1024) {
        ui_utoa(bytes, out);
        kstrcat(out, " bytes");
    } else if (bytes < 1024 * 1024) {
        ui_utoa(bytes / 1024, out);
        kstrcat(out, " KB");
    } else {
        ui_utoa(bytes / (1024 * 1024), out);
        kstrcat(out, " MB");
    }
}
