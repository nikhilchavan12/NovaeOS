/* =============================================================================
 * NovaeOS - Graphics Core (kernel/gfx/gfx.h)
 * Linear framebuffer (GRUB/VBE or Bochs/QEMU BGA), double-buffered 32-bit drawing
 * ============================================================================= */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>

#define RGB(r, g, b) ((uint32_t)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

#define FONT_W 8
#define FONT_H 16

typedef struct {
    uint32_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
} gfx_boot_fb_t;

bool gfx_init(const gfx_boot_fb_t *boot_fb);   /* boot_fb may be NULL */
int  gfx_width(void);
int  gfx_height(void);
const char *gfx_driver_name(void);

/* Clipping (applies to every drawing call) */
void gfx_set_clip(int x, int y, int w, int h);
void gfx_reset_clip(void);

/* Colors */
uint32_t gfx_blend(uint32_t bg, uint32_t fg, uint8_t alpha);
uint32_t gfx_lighten(uint32_t c, int amount);
uint32_t gfx_darken(uint32_t c, int amount);

/* Primitives (draw into the back buffer) */
void gfx_put_pixel(int x, int y, uint32_t c);
uint32_t gfx_get_pixel(int x, int y);
int  gfx_round_inset(int r, int row);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t c);
void gfx_blend_rect(int x, int y, int w, int h, uint32_t c, uint8_t alpha);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t c);
void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t c);
void gfx_draw_round_rect(int x, int y, int w, int h, int r, uint32_t c);
void gfx_fill_circle(int cx, int cy, int r, uint32_t c);
void gfx_draw_circle(int cx, int cy, int r, uint32_t c);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t c);
void gfx_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void gfx_hgradient(int x, int y, int w, int h, uint32_t left, uint32_t right);
void gfx_drop_shadow(int x, int y, int w, int h, int size);
void gfx_blit(const uint32_t *src, int x, int y, int w, int h, int src_stride);

/* Text (8x16 bitmap font, CP437) */
void gfx_draw_char(int x, int y, uint8_t ch, uint32_t c);
void gfx_draw_text(int x, int y, const char *s, uint32_t c);
void gfx_draw_text_bold(int x, int y, const char *s, uint32_t c);
void gfx_draw_text_scaled(int x, int y, const char *s, uint32_t c, int scale);
int  gfx_text_width(const char *s);

/* Output */
void gfx_present(void);
void gfx_present_rect(int x, int y, int w, int h);
void gfx_move_cursor(int x, int y);      /* move + repaint only the cursor area */
void gfx_set_cursor_pos(int x, int y);   /* takes effect at the next present */

#endif /* GFX_H */
