/* =============================================================================
 * NovaeOS - Graphics Core (kernel/gfx/gfx.c)
 * ============================================================================= */

#include "gfx.h"
#include "../lib/io.h"
#include "../lib/kstring.h"

extern const uint8_t font8x16[256][16];

#define GFX_MAX_W 1280
#define GFX_MAX_H 1024

static uint32_t backbuf[GFX_MAX_W * GFX_MAX_H] __attribute__((aligned(16)));

static uint8_t   *fb       = 0;
static uint32_t   fb_pitch = 0;
static int        fb_bpp   = 0;
static int        scr_w    = 0, scr_h = 0;
static const char *drv_name = "none";

static int clip_x0, clip_y0, clip_x1, clip_y1;   /* x1/y1 exclusive */

/* ---- Bochs/QEMU BGA (VBE DISPI) ------------------------------------------ */

#define VBE_DISPI_INDEX 0x01CE
#define VBE_DISPI_DATA  0x01CF

static void bga_write(uint16_t idx, uint16_t val) { outw(VBE_DISPI_INDEX, idx); outw(VBE_DISPI_DATA, val); }
static uint16_t bga_read(uint16_t idx) { outw(VBE_DISPI_INDEX, idx); return inw(VBE_DISPI_DATA); }

static uint32_t pci_read(uint32_t bus, uint32_t slot, uint32_t func, uint32_t off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (slot << 11) | (func << 8) | (off & 0xFC));
    return inl(0xCFC);
}

static uint32_t bga_find_lfb(void) {
    for (uint32_t bus = 0; bus < 8; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read(bus, slot, 0, 0);
            if (id == 0x11111234u || id == 0xBEEF80EEu)   /* QEMU/Bochs std VGA, VirtualBox */
                return pci_read(bus, slot, 0, 0x10) & 0xFFFFFFF0u;
        }
    }
    return 0;
}

static bool bga_setup(int w, int h) {
    uint16_t id = bga_read(0);
    if (id < 0xB0C0 || id > 0xB0CF) return false;
    uint32_t lfb = bga_find_lfb();
    if (!lfb) return false;
    bga_write(4, 0x00);           /* disable */
    bga_write(1, (uint16_t)w);
    bga_write(2, (uint16_t)h);
    bga_write(3, 32);
    bga_write(4, 0x41);           /* enable + linear framebuffer */
    fb = (uint8_t *)lfb;
    fb_pitch = (uint32_t)w * 4;
    fb_bpp = 32;
    scr_w = w; scr_h = h;
    drv_name = "Bochs/QEMU BGA";
    return true;
}

bool gfx_init(const gfx_boot_fb_t *boot_fb) {
    if (boot_fb && boot_fb->addr && (boot_fb->bpp == 32 || boot_fb->bpp == 24) &&
        boot_fb->width >= 640 && boot_fb->height >= 480) {
        fb = (uint8_t *)boot_fb->addr;
        fb_pitch = boot_fb->pitch;
        fb_bpp = boot_fb->bpp;
        scr_w = (int)(boot_fb->width  > GFX_MAX_W ? GFX_MAX_W : boot_fb->width);
        scr_h = (int)(boot_fb->height > GFX_MAX_H ? GFX_MAX_H : boot_fb->height);
        drv_name = "VBE framebuffer";
    } else if (!bga_setup(1024, 768)) {
        return false;
    }
    gfx_reset_clip();
    return true;
}

int gfx_width(void)  { return scr_w; }
int gfx_height(void) { return scr_h; }
const char *gfx_driver_name(void) { return drv_name; }

/* ---- Clipping ------------------------------------------------------------- */

void gfx_set_clip(int x, int y, int w, int h) {
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = x + w > scr_w ? scr_w : x + w;
    clip_y1 = y + h > scr_h ? scr_h : y + h;
}

void gfx_reset_clip(void) { clip_x0 = 0; clip_y0 = 0; clip_x1 = scr_w; clip_y1 = scr_h; }

static bool clip_rect(int *x, int *y, int *w, int *h) {
    int x0 = *x, y0 = *y, x1 = *x + *w, y1 = *y + *h;
    if (x0 < clip_x0) x0 = clip_x0;
    if (y0 < clip_y0) y0 = clip_y0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (y1 > clip_y1) y1 = clip_y1;
    if (x1 <= x0 || y1 <= y0) return false;
    *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
    return true;
}

/* ---- Colors --------------------------------------------------------------- */

uint32_t gfx_blend(uint32_t bg, uint32_t fg, uint8_t a) {
    uint32_t ia = 255u - a;
    uint32_t r = (((bg >> 16) & 0xFF) * ia + ((fg >> 16) & 0xFF) * a) / 255u;
    uint32_t g = (((bg >> 8)  & 0xFF) * ia + ((fg >> 8)  & 0xFF) * a) / 255u;
    uint32_t b = (( bg        & 0xFF) * ia + ( fg        & 0xFF) * a) / 255u;
    return (r << 16) | (g << 8) | b;
}

uint32_t gfx_lighten(uint32_t c, int amount) { return gfx_blend(c, 0xFFFFFF, (uint8_t)amount); }
uint32_t gfx_darken(uint32_t c, int amount)  { return gfx_blend(c, 0x000000, (uint8_t)amount); }

/* ---- Primitives ----------------------------------------------------------- */

static int isqrt(int n) {
    if (n <= 0) return 0;
    int x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

void gfx_put_pixel(int x, int y, uint32_t c) {
    if (x < clip_x0 || y < clip_y0 || x >= clip_x1 || y >= clip_y1) return;
    backbuf[y * GFX_MAX_W + x] = c;
}

static void blend_pixel(int x, int y, uint32_t c, uint8_t a) {
    if (x < clip_x0 || y < clip_y0 || x >= clip_x1 || y >= clip_y1) return;
    uint32_t *p = &backbuf[y * GFX_MAX_W + x];
    *p = gfx_blend(*p, c, a);
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (!clip_rect(&x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t *row = &backbuf[(y + j) * GFX_MAX_W + x];
        for (int i = 0; i < w; i++) row[i] = c;
    }
}

void gfx_blend_rect(int x, int y, int w, int h, uint32_t c, uint8_t a) {
    if (!clip_rect(&x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t *row = &backbuf[(y + j) * GFX_MAX_W + x];
        for (int i = 0; i < w; i++) row[i] = gfx_blend(row[i], c, a);
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t c) {
    gfx_fill_rect(x, y, w, 1, c);
    gfx_fill_rect(x, y + h - 1, w, 1, c);
    gfx_fill_rect(x, y, 1, h, c);
    gfx_fill_rect(x + w - 1, y, 1, h, c);
}

uint32_t gfx_get_pixel(int x, int y) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) return 0;
    return backbuf[y * GFX_MAX_W + x];
}

/* Horizontal inset of a rounded corner on row 'j' (0 = outermost row) */
int gfx_round_inset(int r, int j) {
    if (j >= r) return 0;
    int dy = r - j;
    return r - isqrt(r * r - dy * dy);
}
#define corner_inset gfx_round_inset

void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (int j = 0; j < h; j++) {
        int inset = 0;
        if (j < r) inset = corner_inset(r, j);
        else if (j >= h - r) inset = corner_inset(r, h - 1 - j);
        gfx_fill_rect(x + inset, y + j, w - inset * 2, 1, c);
        if (inset > 0) {   /* cheap anti-aliasing on the curve */
            blend_pixel(x + inset - 1, y + j, c, 110);
            blend_pixel(x + w - inset, y + j, c, 110);
        }
    }
}

void gfx_draw_round_rect(int x, int y, int w, int h, int r, uint32_t c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    gfx_fill_rect(x + r, y, w - 2 * r, 1, c);
    gfx_fill_rect(x + r, y + h - 1, w - 2 * r, 1, c);
    gfx_fill_rect(x, y + r, 1, h - 2 * r, c);
    gfx_fill_rect(x + w - 1, y + r, 1, h - 2 * r, c);
    int prev = r;
    for (int j = 0; j < r; j++) {
        int inset = corner_inset(r, j);
        int from = inset, to = (j == 0) ? r : prev;
        if (to < from + 1) to = from + 1;
        for (int i = from; i < to && i < r; i++) {
            gfx_put_pixel(x + i, y + j, c);
            gfx_put_pixel(x + w - 1 - i, y + j, c);
            gfx_put_pixel(x + i, y + h - 1 - j, c);
            gfx_put_pixel(x + w - 1 - i, y + h - 1 - j, c);
        }
        prev = inset;
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint32_t c) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = isqrt(r * r - dy * dy);
        gfx_fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, c);
    }
}

void gfx_draw_circle(int cx, int cy, int r, uint32_t c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_put_pixel(cx + x, cy + y, c); gfx_put_pixel(cx + y, cy + x, c);
        gfx_put_pixel(cx - y, cy + x, c); gfx_put_pixel(cx - x, cy + y, c);
        gfx_put_pixel(cx - x, cy - y, c); gfx_put_pixel(cx - y, cy - x, c);
        gfx_put_pixel(cx + y, cy - x, c); gfx_put_pixel(cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_put_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int j = 0; j < h; j++)
        gfx_fill_rect(x, y + j, w, 1, gfx_blend(top, bottom, (uint8_t)(h > 1 ? j * 255 / (h - 1) : 0)));
}

void gfx_hgradient(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    for (int i = 0; i < w; i++)
        gfx_fill_rect(x + i, y, 1, h, gfx_blend(left, right, (uint8_t)(w > 1 ? i * 255 / (w - 1) : 0)));
}

void gfx_drop_shadow(int x, int y, int w, int h, int size) {
    for (int i = 0; i < size; i++) {
        int g = size - i;
        gfx_blend_rect(x - g + 2, y - g + 4, w + 2 * g - 4, h + 2 * g - 4, 0x000000, 4);
    }
}

void gfx_blit(const uint32_t *src, int x, int y, int w, int h, int stride) {
    int cx = x, cy = y, cw = w, ch = h;
    if (!clip_rect(&cx, &cy, &cw, &ch)) return;
    for (int j = 0; j < ch; j++) {
        const uint32_t *s = src + (cy - y + j) * stride + (cx - x);
        uint32_t *d = &backbuf[(cy + j) * GFX_MAX_W + cx];
        for (int i = 0; i < cw; i++) d[i] = s[i];
    }
}

/* ---- Text ----------------------------------------------------------------- */

void gfx_draw_char(int x, int y, uint8_t ch, uint32_t c) {
    if (x >= clip_x1 || y >= clip_y1 || x + FONT_W <= clip_x0 || y + FONT_H <= clip_y0) return;
    const uint8_t *g = font8x16[ch];
    for (int j = 0; j < FONT_H; j++) {
        uint8_t bits = g[j];
        if (!bits) continue;
        for (int i = 0; i < FONT_W; i++)
            if (bits & (0x80 >> i)) gfx_put_pixel(x + i, y + j, c);
    }
}

void gfx_draw_text(int x, int y, const char *s, uint32_t c) {
    for (; *s; s++, x += FONT_W) gfx_draw_char(x, y, (uint8_t)*s, c);
}

void gfx_draw_text_bold(int x, int y, const char *s, uint32_t c) {
    gfx_draw_text(x, y, s, c);
    gfx_draw_text(x + 1, y, s, c);
}

void gfx_draw_text_scaled(int x, int y, const char *s, uint32_t c, int scale) {
    for (; *s; s++, x += FONT_W * scale) {
        const uint8_t *g = font8x16[(uint8_t)*s];
        for (int j = 0; j < FONT_H; j++)
            for (int i = 0; i < FONT_W; i++)
                if (g[j] & (0x80 >> i))
                    gfx_fill_rect(x + i * scale, y + j * scale, scale, scale, c);
    }
}

int gfx_text_width(const char *s) { return (int)kstrlen(s) * FONT_W; }

/* ---- Mouse cursor (composited at present time, never stored) -------------- */

#define CUR_W 12
#define CUR_H 19
static const char *cursor_img[CUR_H] = {
    "X           ", "XX          ", "X.X         ", "X..X        ",
    "X...X       ", "X....X      ", "X.....X     ", "X......X    ",
    "X.......X   ", "X........X  ", "X.........X ", "X......XXXXX",
    "X...X..X    ", "X..XX..X    ", "X.X  X..X   ", "XX   X..X   ",
    "X     X..X  ", "      X..X  ", "       XX   ",
};
static int cur_x = -100, cur_y = -100;
static uint32_t cursor_save[CUR_W * CUR_H];

static void cursor_composite(bool draw) {
    for (int j = 0; j < CUR_H; j++) {
        int py = cur_y + j;
        if (py < 0 || py >= scr_h) continue;
        for (int i = 0; i < CUR_W; i++) {
            int px = cur_x + i;
            char m = cursor_img[j][i];
            if (px < 0 || px >= scr_w || m == ' ') continue;
            uint32_t *p = &backbuf[py * GFX_MAX_W + px];
            if (draw) { cursor_save[j * CUR_W + i] = *p; *p = (m == 'X') ? 0x000000 : 0xFFFFFF; }
            else      { *p = cursor_save[j * CUR_W + i]; }
        }
    }
}

static void copy_to_fb(int x, int y, int w, int h) {
    int sx = 0, sy = 0, sw = scr_w, sh = scr_h;
    (void)sx; (void)sy;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        const uint32_t *src = &backbuf[(y + j) * GFX_MAX_W + x];
        uint8_t *dst = fb + (uint32_t)(y + j) * fb_pitch;
        if (fb_bpp == 32) {
            uint32_t *d = (uint32_t *)dst + x;
            int n = w;
            __asm__ volatile ("rep movsl" : "+D"(d), "+S"(src), "+c"(n) : : "memory");
        } else {
            uint8_t *d = dst + x * 3;
            for (int i = 0; i < w; i++) {
                uint32_t c = src[i];
                d[0] = (uint8_t)c; d[1] = (uint8_t)(c >> 8); d[2] = (uint8_t)(c >> 16);
                d += 3;
            }
        }
    }
}

void gfx_present_rect(int x, int y, int w, int h) {
    cursor_composite(true);
    copy_to_fb(x, y, w, h);
    cursor_composite(false);
}

void gfx_present(void) { gfx_present_rect(0, 0, scr_w, scr_h); }

void gfx_set_cursor_pos(int x, int y) { cur_x = x; cur_y = y; }

void gfx_move_cursor(int x, int y) {
    if (x == cur_x && y == cur_y) return;
    int ox = cur_x, oy = cur_y;
    cur_x = x; cur_y = y;
    gfx_present_rect(ox, oy, CUR_W, CUR_H);
    gfx_present_rect(cur_x, cur_y, CUR_W, CUR_H);
}
