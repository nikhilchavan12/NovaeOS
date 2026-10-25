/* =============================================================================
 * NovaeOS - Terminal (kernel/gui/apps/terminal.c)
 * Runs the existing shell commands inside a window
 * ============================================================================= */

#include "../gui.h"
#include "../../drivers/vga.h"
#include "../../drivers/keyboard.h"
#include "../../mm/heap.h"
#include "../../shell/shell.h"
#include "../../lib/kstring.h"

#define T_ROWS 300
#define T_COLS 128
#define T_HIST 16

typedef struct {
    char    ch[T_ROWS][T_COLS];
    uint8_t at[T_ROWS][T_COLS];
    int     row, col, cols;
    char    input[200];
    int     input_len;
    char    hist[T_HIST][200];
    int     hist_count, hist_pos;
    bool    cursor_on;
} term_t;

static const uint32_t palette[16] = {
    0x0C0C0C, 0x0037DA, 0x13A10E, 0x3A96DD, 0xC50F1F, 0x881798, 0xC19C00, 0xCCCCCC,
    0x767676, 0x3B78FF, 0x16C60C, 0x61D6D6, 0xE74856, 0xB4009E, 0xF9F1A5, 0xF2F2F2,
};

static term_t *active = 0;

static void clear_row(term_t *t, int r) {
    for (int i = 0; i < T_COLS; i++) { t->ch[r][i] = ' '; t->at[r][i] = 0x07; }
}

static void newline(term_t *t) {
    t->col = 0;
    if (++t->row >= T_ROWS) {
        int shift = 50;
        kmemcpy(t->ch, t->ch[shift], (size_t)(T_ROWS - shift) * T_COLS);
        kmemcpy(t->at, t->at[shift], (size_t)(T_ROWS - shift) * T_COLS);
        for (int r = T_ROWS - shift; r < T_ROWS; r++) clear_row(t, r);
        t->row -= shift;
    }
}

static void term_putc(term_t *t, char c, uint8_t attr) {
    switch (c) {
    case '\n': newline(t); break;
    case '\r': t->col = 0; break;
    case '\t': do { term_putc(t, ' ', attr); } while (t->col % 8); break;
    case '\b': if (t->col > 0) { t->col--; t->ch[t->row][t->col] = ' '; } break;
    case '\f':
        for (int r = 0; r < T_ROWS; r++) clear_row(t, r);
        t->row = t->col = 0;
        break;
    default:
        if (t->col >= t->cols) newline(t);
        t->ch[t->row][t->col] = c;
        t->at[t->row][t->col] = attr;
        t->col++;
    }
}

static void term_puts(term_t *t, const char *s, uint8_t attr) { while (*s) term_putc(t, *s++, attr); }

static void hook(char c, uint8_t attr) { if (active) term_putc(active, c, attr); }

static void prompt(term_t *t) {
    term_puts(t, "novae", 0x0A);
    term_puts(t, "@", 0x08);
    term_puts(t, "kernel", 0x0B);
    term_puts(t, ":/$ ", 0x08);
}

static bool launcher(const char *cmd, const char *arg) {
    if (!kstrcmp(cmd, "calc"))     { calc_open(); return true; }
    if (!kstrcmp(cmd, "settings")) { settings_open(0); return true; }
    if (!kstrcmp(cmd, "notepad") || !kstrcmp(cmd, "write")) { notepad_open(arg); return true; }
    return false;
}

static void set_input(term_t *t, const char *s) {
    while (t->input_len > 0) { term_putc(t, '\b', 0x0F); t->input_len--; }
    kstrncpy(t->input, s, sizeof(t->input) - 1);
    t->input[sizeof(t->input) - 1] = '\0';
    t->input_len = (int)kstrlen(t->input);
    term_puts(t, t->input, 0x0F);
}

static void term_key(window_t *win, char c) {
    term_t *t = win->data;
    t->cursor_on = true;
    if (c == '\n') {
        term_putc(t, '\n', 0x0F);
        t->input[t->input_len] = '\0';
        if (!kstrcmp(t->input, "exit")) { wm_close(win); return; }
        if (t->input_len) {
            kstrcpy(t->hist[t->hist_count % T_HIST], t->input);
            t->hist_count++;
        }
        t->hist_pos = t->hist_count;
        active = t;
        vga_set_output_hook(hook);
        shell_set_gui_launcher(launcher);
        shell_exec(t->input);
        vga_set_output_hook(0);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        active = 0;
        t->input_len = 0;
        t->input[0] = '\0';
        if (win->used) prompt(t);
    } else if (c == '\b') {
        if (t->input_len > 0) { t->input_len--; term_putc(t, '\b', 0x0F); }
    } else if (c == KEY_UP) {
        if (t->hist_pos > 0 && t->hist_pos > t->hist_count - T_HIST) {
            t->hist_pos--;
            set_input(t, t->hist[t->hist_pos % T_HIST]);
        }
    } else if (c == KEY_DOWN) {
        if (t->hist_pos < t->hist_count) {
            t->hist_pos++;
            set_input(t, t->hist_pos == t->hist_count ? "" : t->hist[t->hist_pos % T_HIST]);
        }
    } else if ((unsigned char)c >= 0x20 && c != KEY_DELETE && t->input_len < (int)sizeof(t->input) - 1) {
        t->input[t->input_len++] = c;
        term_putc(t, c, 0x0F);
    }
    wm_invalidate();
}

static void term_paint(window_t *win, int x0, int y0, int cw, int ch) {
    term_t *t = win->data;
    gfx_fill_rect(x0, y0, cw, ch, 0x0C0C0C);
    int cols = (cw - 16) / FONT_W;
    if (cols > T_COLS) cols = T_COLS;
    if (cols < 20) cols = 20;
    t->cols = cols;
    int vis = (ch - 12) / FONT_H;
    int first = t->row + 1 - vis;
    if (first < 0) first = 0;
    for (int r = first; r <= t->row; r++) {
        int py = y0 + 6 + (r - first) * FONT_H;
        for (int c = 0; c < T_COLS; c++) {
            uint8_t a = t->at[r][c];
            int px = x0 + 8 + c * FONT_W;
            if (a >> 4) gfx_fill_rect(px, py, FONT_W, FONT_H, palette[(a >> 4) & 7]);
            if (t->ch[r][c] != ' ') gfx_draw_char(px, py, (uint8_t)t->ch[r][c], palette[a & 0x0F]);
        }
    }
    if (win == wm_focused() && t->cursor_on)
        gfx_fill_rect(x0 + 8 + t->col * FONT_W, y0 + 6 + (t->row - first) * FONT_H + 13, FONT_W, 3, 0xF2F2F2);
}

static void term_tick(window_t *win) {
    term_t *t = win->data;
    static uint32_t last;
    if (win == wm_focused() && wm_ticks() - last >= 50) {
        last = wm_ticks();
        t->cursor_on = !t->cursor_on;
        wm_invalidate();
    }
}

static void term_close(window_t *win) { kfree(win->data); }

void terminal_open(void) {
    term_t *t = kmalloc(sizeof(term_t));
    if (!t) return;
    window_t *w = wm_create("Terminal", ICON_TERMINAL, APP_TERMINAL, 720, 460);
    if (!w) { kfree(t); return; }
    kmemset(t, 0, sizeof(*t));
    for (int r = 0; r < T_ROWS; r++) clear_row(t, r);
    t->cols = 88;
    t->cursor_on = true;
    w->resizable = true;
    w->min_w = 360; w->min_h = 200;
    w->data  = t;
    w->paint = term_paint;
    w->key   = term_key;
    w->tick  = term_tick;
    w->close = term_close;
    term_puts(t, "NovaeOS Terminal [Version 1.0.0]\n", 0x0F);
    term_puts(t, "Type 'help' for commands, 'exit' to close.\n\n", 0x08);
    prompt(t);
}
