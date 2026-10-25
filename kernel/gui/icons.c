/* =============================================================================
 * NovaeOS - Procedural icons and logo (kernel/gui/icons.c)
 * All artwork is drawn in a 48x48 design grid and scaled to the requested size
 * ============================================================================= */

#include "gui.h"

static int ix, iy, isz;
#define P(v)  ((v) * isz / 48)
#define X(v)  (ix + P(v))
#define Y(v)  (iy + P(v))

static void rr(int x, int y, int w, int h, int r, uint32_t c) {
    gfx_fill_round_rect(X(x), Y(y), P(w) ? P(w) : 1, P(h) ? P(h) : 1, P(r), c);
}
static void rect(int x, int y, int w, int h, uint32_t c) {
    gfx_fill_rect(X(x), Y(y), P(w) ? P(w) : 1, P(h) ? P(h) : 1, c);
}

static void grad_round_rect(int x, int y, int w, int h, int r, uint32_t top, uint32_t bottom) {
    for (int j = 0; j < h; j++) {
        int inset = 0;
        if (j < r) inset = gfx_round_inset(r, j);
        else if (j >= h - r) inset = gfx_round_inset(r, h - 1 - j);
        uint32_t c = gfx_blend(top, bottom, (uint8_t)(h > 1 ? j * 255 / (h - 1) : 0));
        gfx_fill_rect(x + inset, y + j, w - 2 * inset, 1, c);
    }
}

/* Four-pointed "nova" sparkle */
static void star4(int cx, int cy, int r, int t, uint32_t c) {
    if (r < 1) r = 1;
    for (int d = -r; d <= r; d++) {
        int a = d < 0 ? -d : d;
        int hw = (r - a) * t / r;
        gfx_fill_rect(cx - hw, cy + d, hw * 2 + 1, 1, c);
        gfx_fill_rect(cx + d, cy - hw, 1, hw * 2 + 1, c);
    }
}

void logo_draw(int x, int y, int size) {
    grad_round_rect(x, y, size, size, size / 5, 0x7B5CFF, 0x16B8F3);
    int r = size * 36 / 100;
    star4(x + size / 2, y + size / 2, r, r / 4 + 1, 0xFFFFFF);
    int r2 = size * 11 / 100;
    star4(x + size * 76 / 100, y + size * 25 / 100, r2, r2 / 3 + 1, 0xE8F4FF);
}

static void page(uint32_t band) {
    rr(10, 4, 28, 40, 3, 0x9AA3AD);
    rr(11, 5, 26, 38, 3, 0xFFFFFF);
    if (band) rect(11, 5, 26, 7, band);
    for (int i = 0; i < 4; i++) rect(15, 17 + i * 6, i == 3 ? 12 : 18, 2, 0xA8B4C2);
}

void icon_draw(int id, int x, int y, int size) {
    ix = x; iy = y; isz = size;
    switch (id) {
    case ICON_LOGO:
        logo_draw(x, y, size);
        break;
    case ICON_COMPUTER:
    case ICON_MONITOR:
        rr(3, 7, 42, 29, 3, 0x2C3440);
        grad_round_rect(X(6), Y(10), P(36), P(23), P(2), 0x58B4FF, 0x1A5FD0);
        star4(X(24), Y(21), P(7), P(2) + 1, 0xFFFFFF);
        rect(20, 36, 8, 5, 0x6C7580);
        rr(13, 40, 22, 4, 2, 0x4A525C);
        break;
    case ICON_FOLDER:
        rr(4, 9, 18, 10, 3, 0xD99A1E);
        rr(4, 13, 40, 29, 4, 0xE9A92A);
        grad_round_rect(X(4), Y(17), P(40), P(25), P(4), 0xFFD466, 0xF5B53B);
        rect(4, 17, 40, 1, 0xFFE59A);
        break;
    case ICON_NOTEPAD:
        page(0x2F7BD8);
        rect(28, 26, 3, 14, 0xF2B233);
        rect(28, 40, 3, 3, 0x40464E);
        break;
    case ICON_FILE:
        rr(10, 4, 28, 40, 3, 0x9AA3AD);
        rr(11, 5, 26, 38, 3, 0xFFFFFF);
        rect(28, 5, 9, 9, 0xD9DEE4);
        for (int i = 0; i < 4; i++) rect(15, 19 + i * 6, i == 3 ? 10 : 18, 2, 0xA8B4C2);
        break;
    case ICON_CALC:
        rr(9, 3, 30, 42, 5, 0x343A42);
        rr(13, 7, 22, 10, 2, 0xBDE7C9);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                rr(13 + c * 8, 21 + r * 8, 6, 6, 1, (r == 2 && c == 2) ? 0xFF8A3D : 0xD6DCE3);
        break;
    case ICON_TERMINAL:
        rr(3, 7, 42, 34, 4, 0x15181D);
        rr(3, 7, 42, 7, 3, 0x3A414B);
        rect(3, 11, 42, 3, 0x3A414B);
        for (int i = 0; i < 6; i++) {           /* ">" chevron */
            rect(10 + i, 18 + i, 3, 2, 0x3BD16F);
            rect(10 + i, 29 - i, 3, 2, 0x3BD16F);
        }
        rect(21, 30, 12, 3, 0xE6E6E6);
        break;
    case ICON_SETTINGS: {
        static const int dirs[8][2] = {{1000,0},{707,707},{0,1000},{-707,707},{-1000,0},{-707,-707},{0,-1000},{707,-707}};
        int cx = X(24), cy = Y(24);
        for (int i = 0; i < 8; i++)
            gfx_fill_circle(cx + dirs[i][0] * P(16) / 1000, cy + dirs[i][1] * P(16) / 1000, P(5) + 1, 0x5F6B7A);
        gfx_fill_circle(cx, cy, P(16), 0x6F7C8C);
        gfx_fill_circle(cx, cy, P(11), 0x8996A6);
        gfx_fill_circle(cx, cy, P(6), 0xE4E9EF);
        break;
    }
    case ICON_INFO:
        gfx_fill_circle(X(24), Y(24), P(20), 0x2F7BD8);
        rect(22, 21, 5, 15, 0xFFFFFF);
        rect(22, 13, 5, 5, 0xFFFFFF);
        break;
    case ICON_PALETTE:
        gfx_fill_circle(X(24), Y(24), P(20), 0xF0E6D8);
        gfx_fill_circle(X(16), Y(17), P(4) + 1, 0xE64A4A);
        gfx_fill_circle(X(27), Y(13), P(4) + 1, 0xF2B233);
        gfx_fill_circle(X(35), Y(21), P(4) + 1, 0x3BB273);
        gfx_fill_circle(X(14), Y(29), P(4) + 1, 0x2F7BD8);
        gfx_fill_circle(X(28), Y(33), P(5) + 1, 0x8C5CE6);
        break;
    case ICON_SEARCH:
        for (int t = 0; t <= P(2); t++) gfx_draw_circle(X(20), Y(20), P(11) - t, theme.win_fg);
        for (int t = 0; t < P(4) + 1; t++)
            gfx_draw_line(X(28) + t / 2, Y(28) + t - t / 2, X(40), Y(40), theme.win_fg);
        break;
    case ICON_POWER:
        for (int t = 0; t <= P(3); t++) gfx_draw_circle(X(24), Y(26), P(15) - t, theme.win_fg);
        rect(19, 4, 10, 16, theme.menu_bg);
        rect(22, 5, 4, 20, theme.win_fg);
        break;
    case ICON_RESTART:
        for (int t = 0; t <= P(3); t++) gfx_draw_circle(X(24), Y(25), P(15) - t, theme.win_fg);
        rect(24, 4, 12, 14, theme.menu_bg);
        for (int i = 0; i < P(9); i++)
            gfx_fill_rect(X(24) + i / 2, Y(5) + i, P(9) - i, 1, theme.win_fg);
        break;
    default:
        rr(8, 8, 32, 32, 6, 0x888888);
        break;
    }
}
