/* =============================================================================
 * NovaeOS - Settings (kernel/gui/apps/settings.c)
 * Personalization (wallpaper, accent, light/dark, taskbar), System info, About
 * ============================================================================= */

#include "../gui.h"
#include "../../drivers/timer.h"
#include "../../fs/vfs.h"
#include "../../mm/pmm.h"
#include "../../lib/kstring.h"

#define NAV_W   230
#define THUMB_W 112
#define THUMB_H 70

typedef struct { int page, hover; uint32_t last_sec; } settings_t;
static settings_t st;

static uint32_t thumbs[WALLPAPER_COUNT][THUMB_W * THUMB_H];
static bool     thumbs_ready = false;
static uint32_t thumbs_accent = 0;

static const uint32_t accents[8] = {
    0x0067C0, 0x0099BC, 0x107C10, 0x8764B8, 0xC239B3, 0xE81123, 0xF7630C, 0x4C4A48
};
static const char *pages[3] = { "Personalization", "System", "About" };
static const int page_icons[3] = { ICON_PALETTE, ICON_MONITOR, ICON_INFO };

enum { H_NAV = 0, H_WALL = 10, H_ACCENT = 20, H_LIGHT = 30, H_DARK = 31, H_CENTER = 40, H_LEFT = 41 };

static void thumb_rect(int i, int *x, int *y) { *x = NAV_W + 24 + i * (THUMB_W + 12); *y = 96; }
static void accent_center(int i, int *x, int *y) { *x = NAV_W + 44 + i * 44; *y = 250; }

static int hit_test(int x, int y) {
    for (int i = 0; i < 3; i++)
        if (ui_hit(x, y, 10, 90 + i * 42, NAV_W - 20, 38)) return H_NAV + i;
    if (st.page != 0) return -1;
    for (int i = 0; i < WALLPAPER_COUNT; i++) {
        int tx, ty;
        thumb_rect(i, &tx, &ty);
        if (ui_hit(x, y, tx, ty, THUMB_W, THUMB_H)) return H_WALL + i;
    }
    for (int i = 0; i < 8; i++) {
        int cx, cy;
        accent_center(i, &cx, &cy);
        if (ui_hit(x, y, cx - 17, cy - 17, 34, 34)) return H_ACCENT + i;
    }
    if (ui_hit(x, y, NAV_W + 24, 316, 110, 34))  return H_LIGHT;
    if (ui_hit(x, y, NAV_W + 144, 316, 110, 34)) return H_DARK;
    if (ui_hit(x, y, NAV_W + 24, 404, 110, 34))  return H_CENTER;
    if (ui_hit(x, y, NAV_W + 144, 404, 110, 34)) return H_LEFT;
    return -1;
}

static void card(int x, int y, int w, int h) {
    gfx_fill_round_rect(x, y, w, h, 6, theme.border);
    gfx_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 5, theme.panel);
}

static void cpu_name(char *out) {
    uint32_t a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000000));
    if (a >= 0x80000004) {
        uint32_t *o = (uint32_t *)out;
        for (uint32_t leaf = 0; leaf < 3; leaf++) {
            __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000002 + leaf));
            o[leaf * 4] = a; o[leaf * 4 + 1] = b; o[leaf * 4 + 2] = c; o[leaf * 4 + 3] = d;
        }
        out[48] = '\0';
        char *s = out;
        while (*s == ' ') s++;
        if (s != out) kstrcpy(out, s);
    } else {
        __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
        uint32_t *o = (uint32_t *)out;
        o[0] = b; o[1] = d; o[2] = c;
        out[12] = '\0';
    }
}

static void paint_personalization(int x0, int y0, int cw) {
    (void)cw;
    if (!thumbs_ready || thumbs_accent != theme.accent) {
        for (int i = 0; i < WALLPAPER_COUNT; i++) wallpaper_render(i, thumbs[i], THUMB_W, THUMB_H, THUMB_W);
        thumbs_ready = true;
        thumbs_accent = theme.accent;
    }
    gfx_draw_text_bold(x0 + NAV_W + 24, y0 + 70, "Background", theme.win_fg);
    for (int i = 0; i < WALLPAPER_COUNT; i++) {
        int tx, ty;
        thumb_rect(i, &tx, &ty);
        bool sel = theme.wallpaper == i;
        if (sel || st.hover == H_WALL + i)
            gfx_fill_round_rect(x0 + tx - 3, y0 + ty - 3, THUMB_W + 6, THUMB_H + 6, 6, sel ? theme.accent : theme.border);
        gfx_blit(thumbs[i], x0 + tx, y0 + ty, THUMB_W, THUMB_H, THUMB_W);
        ui_text_center(x0 + tx, y0 + ty + THUMB_H + 6, THUMB_W, FONT_H, wallpaper_names[i], sel ? theme.win_fg : theme.win_dim);
    }

    gfx_draw_text_bold(x0 + NAV_W + 24, y0 + 206, "Accent color", theme.win_fg);
    for (int i = 0; i < 8; i++) {
        int cx, cy;
        accent_center(i, &cx, &cy);
        if (theme.accent == accents[i]) gfx_fill_circle(x0 + cx, y0 + cy, 19, theme.win_fg);
        else if (st.hover == H_ACCENT + i) gfx_fill_circle(x0 + cx, y0 + cy, 18, theme.border);
        gfx_fill_circle(x0 + cx, y0 + cy, 16, accents[i]);
        if (theme.accent == accents[i]) gfx_draw_char(x0 + cx - 4, y0 + cy - 8, 0xFB, 0xFFFFFF);
    }

    gfx_draw_text_bold(x0 + NAV_W + 24, y0 + 292, "Choose your mode", theme.win_fg);
    ui_button(x0 + NAV_W + 24, y0 + 316, 110, 34, "Light", st.hover == H_LIGHT, !theme.dark);
    ui_button(x0 + NAV_W + 144, y0 + 316, 110, 34, "Dark", st.hover == H_DARK, theme.dark);

    gfx_draw_text_bold(x0 + NAV_W + 24, y0 + 380, "Taskbar alignment", theme.win_fg);
    ui_button(x0 + NAV_W + 24, y0 + 404, 110, 34, "Center", st.hover == H_CENTER, !theme.taskbar_left);
    ui_button(x0 + NAV_W + 144, y0 + 404, 110, 34, "Left", st.hover == H_LEFT, theme.taskbar_left);

    gfx_draw_text(x0 + NAV_W + 24, y0 + 466, "Changes apply instantly.", theme.win_dim);
}

static void info_card(int x, int y, int w, int icon, const char *title, const char *value) {
    card(x, y, w, 62);
    icon_draw(icon, x + 16, y + 15, 32);
    gfx_draw_text_bold(x + 64, y + 12, title, theme.win_fg);
    gfx_draw_text(x + 64, y + 34, value, theme.win_dim);
}

static void paint_system(int x0, int y0, int cw) {
    int x = x0 + NAV_W + 24, w = cw - NAV_W - 48, y = y0 + 70;
    char buf[96], num[16];

    ui_utoa((uint32_t)gfx_width(), num); kstrcpy(buf, num); kstrcat(buf, " x ");
    ui_utoa((uint32_t)gfx_height(), num); kstrcat(buf, num); kstrcat(buf, ", 32-bit color  (");
    kstrcat(buf, gfx_driver_name()); kstrcat(buf, ")");
    info_card(x, y, w, ICON_MONITOR, "Display", buf);

    char cpu[64];
    cpu_name(cpu);
    info_card(x, y + 74, w, ICON_SETTINGS, "Processor", cpu);

    uint32_t total = (uint32_t)(pmm_get_total_blocks() * 4 / 1024);
    uint32_t freem = (uint32_t)(pmm_get_free_blocks() * 4 / 1024);
    ui_utoa(freem, num); kstrcpy(buf, num); kstrcat(buf, " MB free of ");
    ui_utoa(total, num); kstrcat(buf, num); kstrcat(buf, " MB");
    info_card(x, y + 148, w, ICON_COMPUTER, "Memory", buf);
    int bw = w - 64 - 220;
    if (bw > 40 && total) {
        gfx_fill_round_rect(x + w - bw - 20, y + 148 + 27, bw, 8, 4, theme.border);
        gfx_fill_round_rect(x + w - bw - 20, y + 148 + 27, (int)((uint32_t)bw * (total - freem) / total) + 8, 8, 4, theme.accent);
    }

    uint32_t secs = timer_get_ticks() / timer_get_hz();
    ui_utoa(secs / 3600, num); kstrcpy(buf, num); kstrcat(buf, "h ");
    ui_utoa((secs / 60) % 60, num); kstrcat(buf, num); kstrcat(buf, "m ");
    ui_utoa(secs % 60, num); kstrcat(buf, num); kstrcat(buf, "s");
    info_card(x, y + 222, w, ICON_INFO, "Uptime", buf);

    char names[VFS_MAX_FILES][VFS_MAX_FILENAME];
    vfs_type_t types[VFS_MAX_FILES];
    int count = 0;
    vfs_list(names, types, &count);
    ui_utoa((uint32_t)count, num); kstrcpy(buf, "NovaeVFS RAM disk, "); kstrcat(buf, num); kstrcat(buf, " files");
    info_card(x, y + 296, w, ICON_FOLDER, "Storage", buf);
}

static void paint_about(int x0, int y0, int cw) {
    int x = x0 + NAV_W + 24, w = cw - NAV_W - 48;
    logo_draw(x, y0 + 70, 96);
    gfx_draw_text_scaled(x + 120, y0 + 76, "NovaeOS", theme.win_fg, 3);
    gfx_draw_text(x + 122, y0 + 132, "Version 1.0.0  -  Desktop edition", theme.win_dim);

    static const char *rows[][2] = {
        { "Architecture", "x86 (i686), 32-bit protected mode" },
        { "Bootloader",   "GRUB (Multiboot)" },
        { "Kernel",       "Monolithic, freestanding C + NASM" },
        { "Graphics",     "Linear framebuffer, double buffered" },
        { "Input",        "PS/2 keyboard and mouse" },
        { "Filesystem",   "NovaeVFS (in-memory)" },
    };
    int y = y0 + 190;
    card(x, y, w, 6 * 36 + 12);
    for (int i = 0; i < 6; i++) {
        gfx_draw_text(x + 20, y + 16 + i * 36, rows[i][0], theme.win_dim);
        gfx_draw_text(x + 170, y + 16 + i * 36, rows[i][1], theme.win_fg);
        if (i < 5) gfx_fill_rect(x + 16, y + 42 + i * 36, w - 32, 1, theme.border);
    }
    gfx_draw_text(x, y0 + 440, "Made with care by the NovaeOS project.", theme.win_dim);
}

static void settings_paint(window_t *w, int x0, int y0, int cw, int ch) {
    (void)w;
    gfx_fill_rect(x0, y0, cw, ch, theme.win_bg);
    gfx_fill_rect(x0, y0, NAV_W, ch, theme.dark ? 0x1C1C1C : 0xF0F3F7);

    logo_draw(x0 + 18, y0 + 18, 44);
    gfx_draw_text_bold(x0 + 74, y0 + 22, "NovaeOS", theme.win_fg);
    gfx_draw_text(x0 + 74, y0 + 42, "Local account", theme.win_dim);

    for (int i = 0; i < 3; i++) {
        int iy = y0 + 90 + i * 42;
        if (st.page == i) {
            gfx_fill_round_rect(x0 + 10, iy, NAV_W - 20, 38, 5, theme.dark ? 0x2D2D2D : 0xFFFFFF);
            gfx_fill_round_rect(x0 + 10, iy + 10, 3, 18, 1, theme.accent);
        } else if (st.hover == H_NAV + i) {
            gfx_fill_round_rect(x0 + 10, iy, NAV_W - 20, 38, 5, theme.hover);
        }
        icon_draw(page_icons[i], x0 + 24, iy + 11, 16);
        gfx_draw_text(x0 + 52, iy + 11, pages[i], theme.win_fg);
    }

    gfx_draw_text_scaled(x0 + NAV_W + 24, y0 + 16, pages[st.page], theme.win_fg, 2);
    if (st.page == 0)      paint_personalization(x0, y0, cw);
    else if (st.page == 1) paint_system(x0, y0, cw);
    else                   paint_about(x0, y0, cw);
}

static void settings_mouse(window_t *w, int ev, int x, int y, uint8_t buttons) {
    (void)w;
    int h = ev == EV_LEAVE ? -1 : hit_test(x, y);
    if (h != st.hover) { st.hover = h; wm_invalidate(); }
    if (ev != EV_DOWN || buttons != 1 || h < 0) return;

    if (h < 3)                         st.page = h;
    else if (h >= H_WALL && h < H_WALL + WALLPAPER_COUNT) desktop_set_wallpaper(h - H_WALL);
    else if (h >= H_ACCENT && h < H_ACCENT + 8) {
        theme.accent = accents[h - H_ACCENT];
        if (theme.wallpaper == WALLPAPER_COUNT - 1) desktop_set_wallpaper(theme.wallpaper);
    }
    else if (h == H_LIGHT || h == H_DARK) { theme.dark = (h == H_DARK); theme_apply(); }
    else if (h == H_CENTER || h == H_LEFT) theme.taskbar_left = (h == H_LEFT);
    st.hover = hit_test(x, y);
    wm_invalidate();
}

static void settings_tick(window_t *w) {
    (void)w;
    uint32_t sec = timer_get_ticks() / timer_get_hz();
    if (st.page == 1 && sec != st.last_sec) { st.last_sec = sec; wm_invalidate(); }
}

void settings_open(int page) {
    window_t *w = wm_find_app(APP_SETTINGS);
    st.page = page;
    st.hover = -1;
    if (w) { wm_focus(w); return; }
    w = wm_create("Settings", ICON_SETTINGS, APP_SETTINGS, 880, 580);
    if (!w) return;
    w->paint = settings_paint;
    w->mouse = settings_mouse;
    w->tick  = settings_tick;
}
