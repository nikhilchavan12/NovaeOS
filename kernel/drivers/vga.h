/* =============================================================================
 * NovaeOS - VGA Driver (kernel/drivers/vga.h)
 * Low-level VGA text mode 80x25 driver
 * ============================================================================= */

#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* VGA text mode dimensions */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA text mode color constants */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,  /* Often appears as yellow */
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* Function prototypes */
void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_printf(const char *fmt, ...);
void vga_set_cursor(size_t col, size_t row);
void vga_get_cursor(size_t *col, size_t *row);
void vga_scroll(void);
void vga_putchar_at(char c, vga_color_t fg, vga_color_t bg, size_t x, size_t y);
void vga_draw_box(size_t x, size_t y, size_t w, size_t h, vga_color_t fg, vga_color_t bg);
void vga_draw_hline(size_t x, size_t y, size_t len, char ch, vga_color_t fg, vga_color_t bg);
void vga_fill_row(size_t row, char ch, vga_color_t fg, vga_color_t bg);
void vga_puts_at(const char *str, size_t x, size_t y, vga_color_t fg, vga_color_t bg);

#endif /* VGA_H */
