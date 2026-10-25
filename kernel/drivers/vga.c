/* =============================================================================
 * NovaeOS - VGA Driver Implementation (kernel/drivers/vga.c)
 * Low-level VGA text mode 80x25 driver
 * ============================================================================= */

#include "vga.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* VGA text mode buffer starts at 0xB8000 */
static uint16_t *VGA_BUFFER = (uint16_t *)0xB8000;

/* Current terminal state */
static size_t   terminal_row    = 0;
static size_t   terminal_col    = 0;
static uint8_t  terminal_color  = 0;

/* Box drawing characters (CP437) */
#define BOX_TL  0xDA   /* ┌ */
#define BOX_TR  0xBF   /* ┐ */
#define BOX_BL  0xC0   /* └ */
#define BOX_BR  0xD9   /* ┘ */
#define BOX_H   0xC4   /* ─ */
#define BOX_V   0xB3   /* │ */
#define BOX_TL2 0xC9   /* ╔ (double) */
#define BOX_TR2 0xBB   /* ╗ (double) */
#define BOX_BL2 0xC8   /* ╚ (double) */
#define BOX_BR2 0xBC   /* ╝ (double) */
#define BOX_H2  0xCD   /* ═ (double) */
#define BOX_V2  0xBA   /* ║ (double) */

/* I/O port access */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Create a VGA color byte from fg + bg */
static inline uint8_t make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

/* Create a VGA character word (char + color byte) */
static inline uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

/* Update hardware cursor position */
static void update_cursor(void) {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Enable blinking cursor */
static void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/* Optional redirection of text output (used by the GUI terminal) */
static vga_output_hook_t output_hook = 0;

void vga_redirect(uint16_t *buffer) {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) buffer[i] = VGA_BUFFER[i];
    VGA_BUFFER = buffer;
}

void vga_set_output_hook(vga_output_hook_t hook) {
    output_hook = hook;
}

/* UTF-8 -> CP437 translation so box-drawing strings render correctly */
static const struct { uint16_t cp; uint8_t ch; } cp437_map[] = {
    {0x2550, 0xCD}, {0x2551, 0xBA}, {0x2554, 0xC9}, {0x2557, 0xBB}, {0x255A, 0xC8},
    {0x255D, 0xBC}, {0x2560, 0xCC}, {0x2563, 0xB9}, {0x2566, 0xCB}, {0x2569, 0xCA},
    {0x256C, 0xCE}, {0x2500, 0xC4}, {0x2502, 0xB3}, {0x250C, 0xDA}, {0x2510, 0xBF},
    {0x2514, 0xC0}, {0x2518, 0xD9}, {0x251C, 0xC3}, {0x2524, 0xB4}, {0x252C, 0xC2},
    {0x2534, 0xC1}, {0x253C, 0xC5}, {0x256D, 0xDA}, {0x256E, 0xBF}, {0x256F, 0xD9},
    {0x2570, 0xC0}, {0x2588, 0xDB}, {0x2593, 0xB2}, {0x2592, 0xB1}, {0x2591, 0xB0},
    {0x2580, 0xDF}, {0x2584, 0xDC}, {0x258C, 0xDD}, {0x2590, 0xDE}, {0x25A0, 0xFE},
    {0x2022, 0x07}, {0x00B7, 0xFA}, {0x00B0, 0xF8}, {0x2190, 0x1B}, {0x2192, 0x1A},
    {0x2191, 0x18}, {0x2193, 0x19}, {0x25BA, 0x10}, {0x25B6, 0x10}, {0x25C4, 0x11},
    {0x25C0, 0x11}, {0x25B2, 0x1E}, {0x25BC, 0x1F}, {0x2713, 0xFB}, {0x221A, 0xFB},
    {0x2665, 0x03}, {0x2666, 0x04}, {0x263A, 0x01}, {0x00D7, 'x'},  {0x00F7, 0xF6},
    {0x00B1, 0xF1}, {0x2264, 0xF3}, {0x2265, 0xF2}, {0x03C0, 0xE3}, {0x00B2, 0xFD},
    {0x2026, '.'},  {0x25CF, 0x07}, {0x25CB, 0x09}, {0x2605, '*'},  {0x2606, '*'},
};
static uint32_t utf8_cp   = 0;
static int      utf8_left = 0;

/* Returns the CP437 byte to print, or -1 while a UTF-8 sequence is incomplete */
static int translate_utf8(uint8_t b) {
    if (b < 0x80) { utf8_left = 0; return b; }
    if ((b & 0xC0) == 0x80) {
        if (utf8_left == 0) return '?';
        utf8_cp = (utf8_cp << 6) | (b & 0x3F);
        if (--utf8_left > 0) return -1;
        for (size_t i = 0; i < sizeof(cp437_map) / sizeof(cp437_map[0]); i++)
            if (cp437_map[i].cp == utf8_cp) return cp437_map[i].ch;
        return '?';
    }
    if ((b & 0xE0) == 0xC0)      { utf8_cp = b & 0x1F; utf8_left = 1; }
    else if ((b & 0xF0) == 0xE0) { utf8_cp = b & 0x0F; utf8_left = 2; }
    else                         { utf8_cp = b & 0x07; utf8_left = 3; }
    return -1;
}

void vga_init(void) {
    terminal_row   = 0;
    terminal_col   = 0;
    terminal_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    enable_cursor(14, 15);
    vga_clear();
}

void vga_clear(void) {
    if (output_hook) { output_hook('\f', terminal_color); return; }
    uint16_t blank = make_vga_entry(' ', terminal_color);
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_BUFFER[y * VGA_WIDTH + x] = blank;
    terminal_row = 0;
    terminal_col = 0;
    update_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    terminal_color = make_color(fg, bg);
}

void vga_set_cursor(size_t col, size_t row) {
    terminal_col = col;
    terminal_row = row;
    update_cursor();
}

void vga_get_cursor(size_t *col, size_t *row) {
    *col = terminal_col;
    *row = terminal_row;
}

void vga_scroll(void) {
    /* Move all rows up by one */
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    /* Clear the last row */
    uint16_t blank = make_vga_entry(' ', terminal_color);
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    if (terminal_row > 0)
        terminal_row--;
}

void vga_putchar(char raw) {
    int t = translate_utf8((uint8_t)raw);
    if (t < 0) return;
    char c = (char)t;

    if (output_hook) { output_hook(c, terminal_color); return; }

    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
    } else if (c == '\r') {
        terminal_col = 0;
    } else if (c == '\t') {
        terminal_col = (terminal_col + 8) & ~7;
        if (terminal_col >= VGA_WIDTH) {
            terminal_col = 0;
            terminal_row++;
        }
    } else if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
            VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_col] =
                make_vga_entry(' ', terminal_color);
        }
    } else {
        VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_col] =
            make_vga_entry(c, terminal_color);
        terminal_col++;
        if (terminal_col >= VGA_WIDTH) {
            terminal_col = 0;
            terminal_row++;
        }
    }

    if (terminal_row >= VGA_HEIGHT)
        vga_scroll();

    update_cursor();
}

void vga_puts(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++)
        vga_putchar(str[i]);
}

/* Formats an unsigned number into buf (reversed digits are handled here) */
static int format_uint(char *buf, uint32_t n, uint32_t base, bool upper) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[32];
    int i = 0, len = 0;
    do { tmp[i++] = digits[n % base]; n /= base; } while (n);
    while (i) buf[len++] = tmp[--i];
    buf[len] = '\0';
    return len;
}

static void print_padded(const char *s, int len, int width, bool left, char pad) {
    if (!left) for (int i = len; i < width; i++) vga_putchar(pad);
    for (int i = 0; i < len; i++) vga_putchar(s[i]);
    if (left) for (int i = len; i < width; i++) vga_putchar(' ');
}

/* ============================================================================
 * vga_printf - Minimal printf implementation
 * Supports: %s %c %d %u %x %X %p %% with optional '-', '0' and width
 * ============================================================================ */
void vga_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            vga_putchar(fmt[i]);
            continue;
        }
        i++;
        bool left = false;
        char pad = ' ';
        int width = 0;
        for (; fmt[i] == '-' || fmt[i] == '0'; i++) {
            if (fmt[i] == '-') left = true;
            else pad = '0';
        }
        for (; fmt[i] >= '0' && fmt[i] <= '9'; i++)
            width = width * 10 + (fmt[i] - '0');
        if (left) pad = ' ';

        char buf[40];
        switch (fmt[i]) {
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                int len = 0;
                while (s[len]) len++;
                print_padded(s, len, width, left, ' ');
                break;
            }
            case 'c':
                buf[0] = (char)va_arg(args, int);
                print_padded(buf, 1, width, left, ' ');
                break;
            case 'd': {
                int32_t v = va_arg(args, int32_t);
                int len = 0;
                if (v < 0) {
                    buf[0] = '-';
                    len = 1 + format_uint(buf + 1, (uint32_t)(-(v + 1)) + 1u, 10, false);
                } else {
                    len = format_uint(buf, (uint32_t)v, 10, false);
                }
                if (pad == '0' && buf[0] == '-' && !left) {
                    vga_putchar('-');
                    print_padded(buf + 1, len - 1, width - 1, false, '0');
                } else {
                    print_padded(buf, len, width, left, pad);
                }
                break;
            }
            case 'u':
                print_padded(buf, format_uint(buf, va_arg(args, uint32_t), 10, false), width, left, pad);
                break;
            case 'x':
                print_padded(buf, format_uint(buf, va_arg(args, uint32_t), 16, false), width, left, pad);
                break;
            case 'X':
                print_padded(buf, format_uint(buf, va_arg(args, uint32_t), 16, true), width, left, pad);
                break;
            case 'p':
                vga_puts("0x");
                print_padded(buf, format_uint(buf, (uint32_t)va_arg(args, void *), 16, false), width, left, pad);
                break;
            case '%':
                vga_putchar('%');
                break;
            default:
                vga_putchar('%');
                vga_putchar(fmt[i]);
                break;
        }
    }

    va_end(args);
}

/* ============================================================================
 * Drawing helpers
 * ============================================================================ */

void vga_putchar_at(char c, vga_color_t fg, vga_color_t bg, size_t x, size_t y) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) return;
    VGA_BUFFER[y * VGA_WIDTH + x] = make_vga_entry(c, make_color(fg, bg));
}

void vga_puts_at(const char *str, size_t x, size_t y, vga_color_t fg, vga_color_t bg) {
    for (size_t i = 0; str[i] != '\0' && x + i < VGA_WIDTH; i++)
        vga_putchar_at(str[i], fg, bg, x + i, y);
}

void vga_fill_row(size_t row, char ch, vga_color_t fg, vga_color_t bg) {
    if (row >= VGA_HEIGHT) return;
    uint16_t entry = make_vga_entry(ch, make_color(fg, bg));
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_BUFFER[row * VGA_WIDTH + x] = entry;
}

void vga_draw_hline(size_t x, size_t y, size_t len, char ch, vga_color_t fg, vga_color_t bg) {
    for (size_t i = 0; i < len && x + i < VGA_WIDTH; i++)
        vga_putchar_at(ch, fg, bg, x + i, y);
}

void vga_draw_box(size_t x, size_t y, size_t w, size_t h, vga_color_t fg, vga_color_t bg) {
    if (w < 2 || h < 2) return;

    /* Corners */
    vga_putchar_at(BOX_TL2, fg, bg, x,         y);
    vga_putchar_at(BOX_TR2, fg, bg, x + w - 1, y);
    vga_putchar_at(BOX_BL2, fg, bg, x,         y + h - 1);
    vga_putchar_at(BOX_BR2, fg, bg, x + w - 1, y + h - 1);

    /* Top and bottom horizontal lines */
    for (size_t i = 1; i < w - 1; i++) {
        vga_putchar_at(BOX_H2, fg, bg, x + i, y);
        vga_putchar_at(BOX_H2, fg, bg, x + i, y + h - 1);
    }

    /* Left and right vertical lines + fill interior */
    for (size_t j = 1; j < h - 1; j++) {
        vga_putchar_at(BOX_V2, fg, bg, x,         y + j);
        vga_putchar_at(BOX_V2, fg, bg, x + w - 1, y + j);
        for (size_t i = 1; i < w - 1; i++)
            vga_putchar_at(' ', fg, bg, x + i, y + j);
    }
}
