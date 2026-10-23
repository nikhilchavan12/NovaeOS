/* =============================================================================
 * NovaeOS - VGA Driver Implementation (kernel/drivers/vga.c)
 * Low-level VGA text mode 80x25 driver
 * ============================================================================= */

#include "vga.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* VGA text mode buffer starts at 0xB8000 */
static uint16_t * const VGA_BUFFER = (uint16_t *)0xB8000;

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
    return (uint16_t)c | ((uint16_t)color << 8);
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

void vga_init(void) {
    terminal_row   = 0;
    terminal_col   = 0;
    terminal_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    enable_cursor(14, 15);
    vga_clear();
}

void vga_clear(void) {
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

void vga_putchar(char c) {
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

/* ============================================================================
 * vga_printf - Minimal printf implementation
 * Supports: %s %c %d %u %x %X %p %%
 * ============================================================================ */

static void print_uint(uint32_t n, uint32_t base, bool upper) {
    const char *digits_lower = "0123456789abcdef";
    const char *digits_upper = "0123456789ABCDEF";
    const char *digits = upper ? digits_upper : digits_lower;
    char buf[32];
    int i = 0;
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    for (int j = i - 1; j >= 0; j--)
        vga_putchar(buf[j]);
}

static void print_int(int32_t n) {
    if (n < 0) {
        vga_putchar('-');
        print_uint((uint32_t)(-n), 10, false);
    } else {
        print_uint((uint32_t)n, 10, false);
    }
}

void vga_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            vga_putchar(fmt[i]);
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 's': {
                const char *s = va_arg(args, const char *);
                vga_puts(s ? s : "(null)");
                break;
            }
            case 'c':
                vga_putchar((char)va_arg(args, int));
                break;
            case 'd':
                print_int(va_arg(args, int32_t));
                break;
            case 'u':
                print_uint(va_arg(args, uint32_t), 10, false);
                break;
            case 'x':
                print_uint(va_arg(args, uint32_t), 16, false);
                break;
            case 'X':
                print_uint(va_arg(args, uint32_t), 16, true);
                break;
            case 'p':
                vga_puts("0x");
                print_uint((uint32_t)va_arg(args, void *), 16, false);
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
