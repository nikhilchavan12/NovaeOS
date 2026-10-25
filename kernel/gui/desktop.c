/* =============================================================================
 * NovaeOS - Desktop shell: wallpaper, icons, taskbar, Start menu, flyouts
 * (kernel/gui/desktop.c)
 * ============================================================================= */

#include "gui.h"
#include "../drivers/rtc.h"
#include "../drivers/timer.h"
#include "../fs/vfs.h"
#include "../lib/kstring.h"
#include "../lib/io.h"

const char *wallpaper_names[WALLPAPER_COUNT] = { "Flow", "Aurora", "Sunset", "Midnight", "Solid color" };

static uint32_t wallpaper[1280 * 1024];
static int      scr_w, scr_h;
static rtc_time_t now_time;

/* ---- Wallpapers (procedural, resolution independent) ------------------------ */

/* sin(deg) * 1024, Bhaskara approximation */
static int isin(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    int sign = 1;
    if (deg >= 180) { deg -= 180; sign = -1; }
    int p = deg * (180 - deg);
    return sign * (4 * p * 1024) / (40500 - p);
}

static int isqrt_px(int n) {
    if (n <= 0) return 0;
    int x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}

typedef struct { int base, amp1, f1, ph1, amp2, f2, ph2, thick; uint32_t color; uint8_t alpha; } band_t;

static int band_center(const band_t *b, int u) {
    return b->base + b->amp1 * isin(u * b->f1 * 360 / 1024 + b->ph1) / 1024
                   + b->amp2 * isin(u * b->f2 * 360 / 1024 + b->ph2) / 1024;
}

static uint32_t three_stop(uint32_t a, uint32_t b, uint32_t c, int v) {
    if (v < 512) return gfx_blend(a, b, (uint8_t)(v * 255 / 512));
    int t = (v - 512) * 255 / 512;
    return gfx_blend(b, c, (uint8_t)(t > 255 ? 255 : t));
}

void wallpaper_render(int id, uint32_t *dst, int w, int h, int stride) {
    static const band_t flow[] = {
        { 600, 120, 1, 200, 40, 3,  30, 160, 0x1B4FD8, 150 },
        { 560, 140, 1, 230, 30, 2, 120, 110, 0x3D8BFF, 170 },
        { 640, 100, 1, 170, 50, 3, 300,  70, 0x0B2E9E, 170 },
        { 520, 160, 1, 250, 20, 4,  60,  40, 0xA9D1FF, 190 },
    };
    static const band_t aurora[] = {
        { 420, 140, 1,  40, 60, 3,  10, 260, 0x2BE39B, 150 },
        { 360, 100, 2, 130, 40, 5, 200, 200, 0x16B3D1, 120 },
        { 300, 120, 1, 260, 50, 4,  90, 220, 0x8A4DFF, 110 },
    };

    for (int x = 0; x < w; x++) {
        int u = x * 1024 / w;
        int centers[4];
        if (id == 0) for (int b = 0; b < 4; b++) centers[b] = band_center(&flow[b], u);
        if (id == 1) for (int b = 0; b < 3; b++) centers[b] = band_center(&aurora[b], u);
        int ridge1 = 0, ridge2 = 0;
        if (id == 2) {
            ridge1 = 770 + 50 * isin(u * 3 * 360 / 1024 + 20) / 1024 + 25 * isin(u * 8 * 360 / 1024) / 1024;
            ridge2 = 860 + 45 * isin(u * 2 * 360 / 1024 + 100) / 1024 + 20 * isin(u * 11 * 360 / 1024 + 45) / 1024;
        }

        for (int y = 0; y < h; y++) {
            int v = y * 1024 / h;
            uint32_t c;
            switch (id) {
            case 0: {
                c = three_stop(0xDCE9FA, 0xB5CFF3, 0x7FA9E8, v);
                for (int b = 0; b < 4; b++) {
                    int d = v - centers[b];
                    if (d < 0) d = -d;
                    if (d < flow[b].thick) {
                        int a = (flow[b].thick - d) * flow[b].alpha / flow[b].thick;
                        c = gfx_blend(c, flow[b].color, (uint8_t)a);
                    }
                }
                break;
            }
            case 1: {
                c = gfx_blend(0x040A1C, 0x0D2440, (uint8_t)(v / 4));
                for (int b = 0; b < 3; b++) {
                    int d = centers[b] - v;            /* curtains rise above the curve */
                    int a = 0;
                    if (d >= 0 && d < aurora[b].thick) a = (aurora[b].thick - d) * aurora[b].alpha / aurora[b].thick;
                    else if (d < 0 && d > -30)         a = (30 + d) * aurora[b].alpha / 30;
                    if (a > 0) c = gfx_blend(c, aurora[b].color, (uint8_t)a);
                }
                if (w >= 640 && (hash32((uint32_t)(y * 4099 + x)) & 0x3FF) == 0 && v < 700)
                    c = gfx_blend(c, 0xFFFFFF, (uint8_t)(120 + (hash32((uint32_t)x) & 0x7F)));
                break;
            }
            case 2: {
                c = three_stop(0x2A1858, 0xD9486A, 0xFFB46B, v * 1024 / 800 > 1023 ? 1023 : v * 1024 / 800);
                int du = (u - 512) * w / h, dv = v - 720;
                int dist = du * du + dv * dv;
                if (dist < 130 * 130) c = gfx_blend(c, 0xFFE7A3, 235);
                else if (dist < 330 * 330) {
                    int dd = dist / 330;   /* 51 .. 330 */
                    c = gfx_blend(c, 0xFFD08A, (uint8_t)(110 - (dd - 51) * 110 / 279));
                }
                if (v > ridge1) c = gfx_blend(0x4A2558, 0x2C1640, (uint8_t)((v - ridge1) > 255 ? 255 : (v - ridge1)));
                if (v > ridge2) c = 0x170B26;
                break;
            }
            case 3: {
                c = three_stop(0x0F0C29, 0x302B63, 0x24243E, v);
                int du = (u - 560) * w / h, dv = v - 380;
                int dist2 = du * du + dv * dv;
                if (dist2 < 480 * 480) {
                    int a = 150 - isqrt_px(dist2) * 150 / 480;
                    c = gfx_blend(c, 0x6A5CFF, (uint8_t)a);
                }
                if (w >= 640 && (hash32((uint32_t)(y * 7919 + x * 13)) & 0x1FF) == 0)
                    c = gfx_blend(c, 0xFFFFFF, (uint8_t)(90 + (hash32((uint32_t)y) & 0x9F)));
                break;
            }
            default:
                c = gfx_blend(gfx_darken(theme.accent, 60), theme.accent, (uint8_t)(255 - v / 6));
                break;
            }
            dst[y * stride + x] = c;
        }
    }
}

/* ---- Apps ------------------------------------------------------------------- */

static const int pinned_apps[] = { APP_FILES, APP_NOTEPAD, APP_CALC, APP_TERMINAL, APP_SETTINGS };
#define PINNED_COUNT 5

const char *app_name(int app) {
    switch (app) {
        case APP_FILES:    return "File Explorer";
        case APP_NOTEPAD:  return "Notepad";
        case APP_CALC:     return "Calculator";
        case APP_TERMINAL: return "Terminal";
        case APP_SETTINGS: return "Settings";
        default:           return "";
    }
}

int app_icon(int app) {
    switch (app) {
        case APP_FILES:    return ICON_FOLDER;
        case APP_NOTEPAD:  return ICON_NOTEPAD;
        case APP_CALC:     return ICON_CALC;
        case APP_TERMINAL: return ICON_TERMINAL;
        case APP_SETTINGS: return ICON_SETTINGS;
        default:           return ICON_LOGO;
    }
}

void app_launch(int app) {
    switch (app) {
        case APP_FILES:    files_open(); break;
        case APP_NOTEPAD:  notepad_open(0); break;
        case APP_CALC:     calc_open(); break;
        case APP_TERMINAL: terminal_open(); break;
        case APP_SETTINGS: settings_open(0); break;
    }
}

/* ---- Desktop icons ---------------------------------------------------------- */

typedef struct { const char *label; int icon; int action; } desk_icon_t;
enum { ACT_APP_BASE = 0, ACT_README = 100, ACT_ABOUT = 101 };

static const desk_icon_t desk_icons[] = {
    { "This PC",    ICON_COMPUTER, APP_FILES },
    { "Notepad",    ICON_NOTEPAD,  APP_NOTEPAD },
    { "Calculator", ICON_CALC,     APP_CALC },
    { "Terminal",   ICON_TERMINAL, APP_TERMINAL },
    { "Settings",   ICON_SETTINGS, APP_SETTINGS },
    { "README.txt", ICON_FILE,     ACT_README },
    { "About",      ICON_INFO,     ACT_ABOUT },
};
#define DESK_ICON_COUNT ((int)(sizeof(desk_icons) / sizeof(desk_icons[0])))
#define DESK_CELL_W 84
#define DESK_CELL_H 92

static int desk_selected = -1;

static void desk_icon_pos(int i, int *x, int *y) {
    int per_col = (scr_h - TASKBAR_H - 16) / DESK_CELL_H;
    *x = 10 + (i / per_col) * DESK_CELL_W;
    *y = 10 + (i % per_col) * DESK_CELL_H;
}

static void run_action(int action) {
    if (action == ACT_README)     notepad_open("README.txt");
    else if (action == ACT_ABOUT) settings_open(2);
    else                          app_launch(action);
}

static void shadow_text_center(int x, int w, int y, const char *s) {
    int tx = x + (w - gfx_text_width(s)) / 2;
    gfx_draw_text(tx + 1, y + 1, s, 0x000000);
    gfx_draw_text(tx, y, s, 0xFFFFFF);
}

void desktop_draw_background(void) {
    gfx_blit(wallpaper, 0, 0, scr_w, scr_h, 1280);
    for (int i = 0; i < DESK_ICON_COUNT; i++) {
        int x, y;
        desk_icon_pos(i, &x, &y);
        if (i == desk_selected) {
            gfx_blend_rect(x, y, DESK_CELL_W - 4, DESK_CELL_H - 6, 0xFFFFFF, 60);
            gfx_draw_rect(x, y, DESK_CELL_W - 4, DESK_CELL_H - 6, gfx_blend(0xFFFFFF, theme.accent, 120));
        }
        icon_draw(desk_icons[i].icon, x + (DESK_CELL_W - 4 - 48) / 2, y + 8, 48);
        shadow_text_center(x, DESK_CELL_W - 4, y + 62, desk_icons[i].label);
    }
}

/* ---- Popups ----------------------------------------------------------------- */

enum { POP_NONE, POP_START, POP_CONTEXT, POP_CALENDAR };
static int  popup = POP_NONE;
static bool power_menu = false;
static int  hover_item = -1;          /* generic hover id inside the open popup/taskbar */
static char search[32];
static int  search_len = 0;
static int  ctx_x, ctx_y;
static int  cal_month, cal_year;

void desktop_close_popups(void) {
    if (popup != POP_NONE || power_menu) wm_invalidate();
    popup = POP_NONE;
    power_menu = false;
    hover_item = -1;
}

static void open_popup(int p) {
    popup = p;
    power_menu = false;
    hover_item = -1;
    search_len = 0; search[0] = '\0';
    if (p == POP_CALENDAR) { cal_month = now_time.month; cal_year = now_time.year; }
    wm_invalidate();
}

/* ---- Taskbar ---------------------------------------------------------------- */

#define TB_ITEM 44
#define TB_GAP  4
#define TB_CLOCK_W 96
enum { HOV_TB_START = 1000, HOV_TB_APP = 1001, HOV_TB_CLOCK = 1100, HOV_TB_DESKTOP = 1101 };

static int taskbar_x0(void) {
    int count = 1 + PINNED_COUNT;
    int total = count * TB_ITEM + (count - 1) * TB_GAP;
    return theme.taskbar_left ? 8 : (scr_w - total) / 2;
}

static void tb_item_rect(int i, int *x, int *y) {
    *x = taskbar_x0() + i * (TB_ITEM + TB_GAP);
    *y = scr_h - TASKBAR_H + (TASKBAR_H - TB_ITEM) / 2;
}

static void two_digits(char *out, int v) { out[0] = (char)('0' + v / 10 % 10); out[1] = (char)('0' + v % 10); }

static void draw_taskbar(void) {
    int y0 = scr_h - TASKBAR_H;
    gfx_blend_rect(0, y0, scr_w, TASKBAR_H, theme.taskbar_bg, 222);
    gfx_fill_rect(0, y0, scr_w, 1, theme.dark ? 0x3A3A3A : 0xD0D4DA);

    window_t *focused = wm_focused();
    for (int i = 0; i <= PINNED_COUNT; i++) {
        int x, y;
        tb_item_rect(i, &x, &y);
        bool hov = (hover_item == (i == 0 ? HOV_TB_START : HOV_TB_APP + i - 1));
        bool active = (i == 0 && popup == POP_START);
        if (hov || active)
            gfx_fill_round_rect(x, y, TB_ITEM, TB_ITEM, 5, theme.dark ? 0x3A3A3A : 0xFFFFFF);
        if (i == 0) {
            logo_draw(x + 10, y + 10, 24);
            continue;
        }
        int app = pinned_apps[i - 1];
        icon_draw(app_icon(app), x + 10, y + 9, 24);
        window_t *w = wm_find_app(app);
        if (w) {
            bool foc = focused && focused->app == app;
            int pw = foc ? 16 : 6;
            gfx_fill_round_rect(x + (TB_ITEM - pw) / 2, y + TB_ITEM - 4, pw, 3, 1, foc ? theme.accent : theme.win_dim);
        }
    }

    /* Clock */
    int cx = scr_w - TB_CLOCK_W - 10;
    if (hover_item == HOV_TB_CLOCK || popup == POP_CALENDAR)
        gfx_fill_round_rect(cx, y0 + 4, TB_CLOCK_W, TASKBAR_H - 8, 5, theme.dark ? 0x3A3A3A : 0xFFFFFF);
    char t[6] = "00:00", d[11] = "00/00/0000";
    two_digits(t, now_time.hour); two_digits(t + 3, now_time.minute);
    two_digits(d, now_time.day); two_digits(d + 3, now_time.month);
    two_digits(d + 6, now_time.year / 100); two_digits(d + 8, now_time.year % 100);
    gfx_draw_text(cx + TB_CLOCK_W - 10 - gfx_text_width(t), y0 + 6, t, theme.win_fg);
    gfx_draw_text(cx + TB_CLOCK_W - 10 - gfx_text_width(d), y0 + 24, d, theme.win_fg);

    /* Tray: language indicator */
    gfx_draw_text(cx - 40, y0 + 6, "ENG", theme.win_fg);
    gfx_draw_text(cx - 36, y0 + 24, "IN", theme.win_dim);

    /* Show-desktop sliver */
    gfx_fill_rect(scr_w - 5, y0 + 10, 1, TASKBAR_H - 20, theme.win_dim);
}

/* ---- Start menu ------------------------------------------------------------- */

#define SM_W 580
#define SM_H 600
enum { HOV_SM_PIN = 0, HOV_SM_REC = 100, HOV_SM_POWER = 200, HOV_SM_PWR_ITEM = 210, HOV_SM_USER = 220 };

typedef struct { const char *name; int icon; int action; } start_entry_t;
static const start_entry_t start_entries[] = {
    { "Files",      ICON_FOLDER,   APP_FILES },
    { "Notepad",    ICON_NOTEPAD,  APP_NOTEPAD },
    { "Calculator", ICON_CALC,     APP_CALC },
    { "Terminal",   ICON_TERMINAL, APP_TERMINAL },
    { "Settings",   ICON_SETTINGS, APP_SETTINGS },
    { "About",      ICON_INFO,     ACT_ABOUT },
    { "README",     ICON_FILE,     ACT_README },
};
#define START_COUNT ((int)(sizeof(start_entries) / sizeof(start_entries[0])))

static void start_rect(int *x, int *y) {
    *x = theme.taskbar_left ? 8 : (scr_w - SM_W) / 2;
    *y = scr_h - TASKBAR_H - 10 - SM_H;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool matches(const char *name) {
    if (search_len == 0) return true;
    for (int i = 0; name[i]; i++) {
        int j = 0;
        while (j < search_len && name[i + j] && lower(name[i + j]) == lower(search[j])) j++;
        if (j == search_len) return true;
    }
    return false;
}

/* Visible (filtered) pinned entries */
static int start_visible(int *idx) {
    int n = 0;
    for (int i = 0; i < START_COUNT; i++)
        if (matches(start_entries[i].name)) idx[n++] = i;
    return n;
}

static void pin_cell(int sx, int sy, int k, int *x, int *y) {
    int cw = (SM_W - 48) / 6;
    *x = sx + 24 + (k % 6) * cw;
    *y = sy + 112 + (k / 6) * 84;
}

static char rec_names[VFS_MAX_FILES][VFS_MAX_FILENAME];
static int  rec_count = 0;

static void refresh_recommended(void) {
    vfs_type_t types[VFS_MAX_FILES];
    int count = 0;
    vfs_list(rec_names, types, &count);
    rec_count = count > 6 ? 6 : count;
}

static void rec_cell(int sx, int sy, int k, int *x, int *y) {
    *x = sx + 24 + (k % 2) * ((SM_W - 48) / 2);
    *y = sy + 330 + (k / 2) * 56;
}

static void power_rect(int sx, int sy, int *x, int *y) { *x = sx + SM_W - 64; *y = sy + SM_H - 56; }

static void draw_start_menu(void) {
    int sx, sy;
    start_rect(&sx, &sy);
    gfx_drop_shadow(sx, sy, SM_W, SM_H, 16);
    gfx_fill_round_rect(sx, sy, SM_W, SM_H, 8, theme.menu_bg);
    gfx_draw_round_rect(sx, sy, SM_W, SM_H, 8, theme.border);

    /* Search box */
    gfx_fill_round_rect(sx + 24, sy + 22, SM_W - 48, 38, 19, theme.dark ? 0x1F1F1F : 0xFFFFFF);
    gfx_draw_round_rect(sx + 24, sy + 22, SM_W - 48, 38, 19, search_len ? theme.accent : theme.border);
    icon_draw(ICON_SEARCH, sx + 40, sy + 33, 16);
    if (search_len) {
        gfx_draw_text(sx + 66, sy + 33, search, theme.win_fg);
        if ((wm_ticks() / 50) % 2 == 0)
            gfx_fill_rect(sx + 66 + gfx_text_width(search), sy + 33, 2, FONT_H, theme.win_fg);
    } else {
        gfx_draw_text(sx + 66, sy + 33, "Type here to search", theme.win_dim);
    }

    gfx_draw_text_bold(sx + 40, sy + 80, search_len ? "Best match" : "Pinned", theme.win_fg);

    int idx[START_COUNT];
    int n = start_visible(idx);
    for (int k = 0; k < n; k++) {
        int x, y;
        pin_cell(sx, sy, k, &x, &y);
        int cw = (SM_W - 48) / 6;
        if (hover_item == HOV_SM_PIN + k || (search_len && k == 0))
            gfx_fill_round_rect(x + 2, y, cw - 4, 78, 5, theme.dark ? 0x383838 : 0xFFFFFF);
        icon_draw(start_entries[idx[k]].icon, x + (cw - 32) / 2, y + 10, 32);
        ui_text_center(x, y + 50, cw, FONT_H, start_entries[idx[k]].name, theme.win_fg);
    }
    if (n == 0) gfx_draw_text(sx + 40, sy + 120, "No results", theme.win_dim);

    gfx_draw_text_bold(sx + 40, sy + 300, "Recommended", theme.win_fg);
    for (int k = 0; k < rec_count; k++) {
        int x, y;
        rec_cell(sx, sy, k, &x, &y);
        int cw = (SM_W - 48) / 2;
        if (hover_item == HOV_SM_REC + k)
            gfx_fill_round_rect(x, y, cw - 8, 50, 5, theme.dark ? 0x383838 : 0xFFFFFF);
        icon_draw(ICON_FILE, x + 10, y + 9, 32);
        gfx_draw_text(x + 52, y + 8, rec_names[k], theme.win_fg);
        char sz[24];
        ui_format_size((uint32_t)vfs_get_size(rec_names[k]), sz);
        gfx_draw_text(x + 52, y + 26, sz, theme.win_dim);
    }

    /* Bottom bar */
    int by = sy + SM_H - 68;
    gfx_fill_rect(sx + 1, by, SM_W - 2, 1, theme.border);
    gfx_fill_round_rect(sx + 1, by + 1, SM_W - 2, 67, 8, theme.dark ? 0x232323 : 0xECEEF1);
    gfx_fill_rect(sx + 1, by + 1, SM_W - 2, 10, theme.dark ? 0x232323 : 0xECEEF1);
    gfx_fill_circle(sx + 56, by + 34, 16, theme.accent);
    ui_text_center(sx + 40, by + 18, 32, 32, "N", 0xFFFFFF);
    gfx_draw_text(sx + 84, by + 26, "Novae User", theme.win_fg);

    int px, py;
    power_rect(sx, sy, &px, &py);
    if (hover_item == HOV_SM_POWER || power_menu)
        gfx_fill_round_rect(px, py, 40, 40, 5, theme.dark ? 0x3A3A3A : 0xFFFFFF);
    icon_draw(ICON_POWER, px + 10, py + 10, 20);

    if (power_menu) {
        int mx0 = px - 120, my0 = py - 96;
        gfx_drop_shadow(mx0, my0, 160, 88, 8);
        gfx_fill_round_rect(mx0, my0, 160, 88, 6, theme.menu_bg);
        gfx_draw_round_rect(mx0, my0, 160, 88, 6, theme.border);
        static const char *labels[2] = { "Restart", "Shut down" };
        for (int i = 0; i < 2; i++) {
            int iy = my0 + 6 + i * 38;
            if (hover_item == HOV_SM_PWR_ITEM + i)
                gfx_fill_round_rect(mx0 + 4, iy, 152, 36, 4, theme.hover);
            icon_draw(i == 0 ? ICON_RESTART : ICON_POWER, mx0 + 14, iy + 10, 16);
            gfx_draw_text(mx0 + 40, iy + 10, labels[i], theme.win_fg);
        }
    }
}

/* Returns hover id at point inside the start menu (or -1) */
static int start_hit(int x, int y) {
    int sx, sy;
    start_rect(&sx, &sy);
    int px, py;
    power_rect(sx, sy, &px, &py);
    if (power_menu) {
        int mx0 = px - 120, my0 = py - 96;
        for (int i = 0; i < 2; i++)
            if (ui_hit(x, y, mx0 + 4, my0 + 6 + i * 38, 152, 36)) return HOV_SM_PWR_ITEM + i;
    }
    if (ui_hit(x, y, px, py, 40, 40)) return HOV_SM_POWER;
    int idx[START_COUNT];
    int n = start_visible(idx);
    int cw = (SM_W - 48) / 6;
    for (int k = 0; k < n; k++) {
        int cx, cy;
        pin_cell(sx, sy, k, &cx, &cy);
        if (ui_hit(x, y, cx, cy, cw, 78)) return HOV_SM_PIN + k;
    }
    for (int k = 0; k < rec_count; k++) {
        int cx, cy;
        rec_cell(sx, sy, k, &cx, &cy);
        if (ui_hit(x, y, cx, cy, (SM_W - 48) / 2 - 8, 50)) return HOV_SM_REC + k;
    }
    return -1;
}

static void start_activate(int id) {
    if (id >= HOV_SM_PWR_ITEM && id < HOV_SM_PWR_ITEM + 2) {
        desktop_power(id == HOV_SM_PWR_ITEM ? 1 : 0);
        return;
    }
    if (id == HOV_SM_POWER) { power_menu = !power_menu; wm_invalidate(); return; }
    if (id >= HOV_SM_REC && id < HOV_SM_REC + rec_count) {
        char name[VFS_MAX_FILENAME];
        kstrcpy(name, rec_names[id - HOV_SM_REC]);
        desktop_close_popups();
        notepad_open(name);
        return;
    }
    if (id >= HOV_SM_PIN && id < HOV_SM_PIN + START_COUNT) {
        int idx[START_COUNT];
        int n = start_visible(idx);
        if (id - HOV_SM_PIN < n) {
            int action = start_entries[idx[id - HOV_SM_PIN]].action;
            desktop_close_popups();
            run_action(action);
        }
    }
}

/* ---- Calendar flyout -------------------------------------------------------- */

#define CAL_W 320
#define CAL_H 360
static const char *month_names[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char *day_names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

static void cal_rect(int *x, int *y) { *x = scr_w - CAL_W - 10; *y = scr_h - TASKBAR_H - 10 - CAL_H; }

static void draw_calendar(void) {
    int x, y;
    cal_rect(&x, &y);
    gfx_drop_shadow(x, y, CAL_W, CAL_H, 14);
    gfx_fill_round_rect(x, y, CAL_W, CAL_H, 8, theme.menu_bg);
    gfx_draw_round_rect(x, y, CAL_W, CAL_H, 8, theme.border);

    char t[9] = "00:00:00";
    two_digits(t, now_time.hour); two_digits(t + 3, now_time.minute); two_digits(t + 6, now_time.second);
    gfx_draw_text_scaled(x + 20, y + 16, t, theme.win_fg, 2);

    char line[48];
    kstrcpy(line, day_names[rtc_day_of_week(now_time.year, now_time.month, now_time.day)]);
    kstrcat(line, ", ");
    kstrcat(line, month_names[now_time.month - 1]);
    kstrcat(line, " ");
    char num[12];
    ui_utoa(now_time.day, num);
    kstrcat(line, num);
    gfx_draw_text(x + 20, y + 56, line, theme.accent);
    gfx_fill_rect(x + 1, y + 84, CAL_W - 2, 1, theme.border);

    kstrcpy(line, month_names[cal_month - 1]);
    kstrcat(line, " ");
    ui_utoa((uint32_t)cal_year, num);
    kstrcat(line, num);
    gfx_draw_text_bold(x + 20, y + 98, line, theme.win_fg);
    if (hover_item == 1) gfx_fill_round_rect(x + CAL_W - 76, y + 92, 28, 28, 4, theme.hover);
    if (hover_item == 2) gfx_fill_round_rect(x + CAL_W - 44, y + 92, 28, 28, 4, theme.hover);
    gfx_draw_char(x + CAL_W - 66, y + 98, 0x1E, theme.win_fg);
    gfx_draw_char(x + CAL_W - 34, y + 98, 0x1F, theme.win_fg);

    static const char *wd[7] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    int cell = (CAL_W - 32) / 7;
    for (int i = 0; i < 7; i++)
        ui_text_center(x + 16 + i * cell, y + 128, cell, 20, wd[i], theme.win_dim);

    int first = rtc_day_of_week(cal_year, cal_month, 1);
    int days = rtc_days_in_month(cal_year, cal_month);
    for (int d = 1; d <= days; d++) {
        int slot = first + d - 1;
        int cx = x + 16 + (slot % 7) * cell, cy = y + 152 + (slot / 7) * 32;
        bool today = d == now_time.day && cal_month == now_time.month && cal_year == now_time.year;
        if (today) gfx_fill_circle(cx + cell / 2, cy + 14, 14, theme.accent);
        ui_utoa((uint32_t)d, num);
        ui_text_center(cx, cy + 6, cell, 16, num, today ? 0xFFFFFF : theme.win_fg);
    }
}

static int cal_hit(int px, int py) {
    int x, y;
    cal_rect(&x, &y);
    if (ui_hit(px, py, x + CAL_W - 76, y + 92, 28, 28)) return 1;
    if (ui_hit(px, py, x + CAL_W - 44, y + 92, 28, 28)) return 2;
    return -1;
}

/* ---- Desktop context menu --------------------------------------------------- */

#define CTX_W 220
#define CTX_ITEM 34
static const char *ctx_labels[] = { "New text document", "Open in Terminal", "Next wallpaper", "Personalize", "About NovaeOS" };
static const int   ctx_icons[]  = { ICON_FILE, ICON_TERMINAL, ICON_PALETTE, ICON_SETTINGS, ICON_INFO };
#define CTX_COUNT 5

static void draw_context(void) {
    int h = CTX_COUNT * CTX_ITEM + 8;
    gfx_drop_shadow(ctx_x, ctx_y, CTX_W, h, 10);
    gfx_fill_round_rect(ctx_x, ctx_y, CTX_W, h, 6, theme.menu_bg);
    gfx_draw_round_rect(ctx_x, ctx_y, CTX_W, h, 6, theme.border);
    for (int i = 0; i < CTX_COUNT; i++) {
        int iy = ctx_y + 4 + i * CTX_ITEM;
        if (hover_item == i) gfx_fill_round_rect(ctx_x + 4, iy, CTX_W - 8, CTX_ITEM, 4, theme.hover);
        icon_draw(ctx_icons[i], ctx_x + 14, iy + 9, 16);
        gfx_draw_text(ctx_x + 42, iy + 9, ctx_labels[i], theme.win_fg);
    }
}

static int ctx_hit(int x, int y) {
    if (!ui_hit(x, y, ctx_x + 4, ctx_y + 4, CTX_W - 8, CTX_COUNT * CTX_ITEM)) return -1;
    return (y - ctx_y - 4) / CTX_ITEM;
}

static void ctx_activate(int i) {
    desktop_close_popups();
    switch (i) {
    case 0: {
        char name[VFS_MAX_FILENAME];
        for (int n = 1; n < 100; n++) {
            kstrcpy(name, "New file ");
            char num[12];
            ui_utoa((uint32_t)n, num);
            kstrcat(name, num);
            kstrcat(name, ".txt");
            if (!vfs_exists(name)) break;
        }
        if (vfs_create(name, VFS_TYPE_FILE) >= 0) {
            files_refresh_all();
            notepad_open(name);
        }
        break;
    }
    case 1: terminal_open(); break;
    case 2: desktop_set_wallpaper((theme.wallpaper + 1) % WALLPAPER_COUNT); break;
    case 3: settings_open(0); break;
    case 4: settings_open(2); break;
    }
}

/* ---- Public hooks ----------------------------------------------------------- */

void desktop_draw_overlay(void) {
    draw_taskbar();
    if (popup == POP_START)    draw_start_menu();
    if (popup == POP_CALENDAR) draw_calendar();
    if (popup == POP_CONTEXT)  draw_context();
}

static void set_hover(int id) {
    if (hover_item != id) { hover_item = id; wm_invalidate(); }
}

static int taskbar_hit(int x, int y) {
    if (y < scr_h - TASKBAR_H) return -1;
    for (int i = 0; i <= PINNED_COUNT; i++) {
        int ix, iy;
        tb_item_rect(i, &ix, &iy);
        if (ui_hit(x, y, ix, iy, TB_ITEM, TB_ITEM)) return i == 0 ? HOV_TB_START : HOV_TB_APP + i - 1;
    }
    if (x >= scr_w - 8) return HOV_TB_DESKTOP;
    if (x >= scr_w - TB_CLOCK_W - 10) return HOV_TB_CLOCK;
    return -1;
}

static void taskbar_click(int id) {
    if (id == HOV_TB_START) {
        if (popup == POP_START) desktop_close_popups();
        else { refresh_recommended(); open_popup(POP_START); }
        return;
    }
    if (id == HOV_TB_CLOCK) {
        if (popup == POP_CALENDAR) desktop_close_popups();
        else open_popup(POP_CALENDAR);
        return;
    }
    desktop_close_popups();
    if (id == HOV_TB_DESKTOP) {
        window_t *list[MAX_WINDOWS];
        int n = wm_list(list, MAX_WINDOWS);
        for (int i = 0; i < n; i++) if (!list[i]->minimized) wm_minimize(list[i]);
        return;
    }
    if (id >= HOV_TB_APP && id < HOV_TB_APP + PINNED_COUNT) {
        int app = pinned_apps[id - HOV_TB_APP];
        window_t *w = wm_find_app(app);
        window_t *f = wm_focused();
        if (!w) app_launch(app);
        else if (f && f->app == app) wm_minimize(f);
        else wm_focus(w);
    }
}

bool desktop_overlay_mouse(int ev, int x, int y, uint8_t buttons) {
    bool down = (ev == EV_DOWN || ev == EV_DBLCLICK);

    if (popup == POP_CONTEXT) {
        int h = ctx_hit(x, y);
        if (h >= 0 || ui_hit(x, y, ctx_x, ctx_y, CTX_W, CTX_COUNT * CTX_ITEM + 8)) {
            if (ev == EV_MOVE) set_hover(h);
            else if (down && buttons == 1 && h >= 0) ctx_activate(h);
            return true;
        }
        if (down) desktop_close_popups();
    }

    if (popup == POP_START) {
        int sx, sy;
        start_rect(&sx, &sy);
        int h = start_hit(x, y);
        if (h >= 0 || ui_hit(x, y, sx, sy, SM_W, SM_H)) {
            if (ev == EV_MOVE) set_hover(h);
            else if (down && buttons == 1) {
                if (power_menu && h != HOV_SM_POWER && (h < HOV_SM_PWR_ITEM || h > HOV_SM_PWR_ITEM + 1)) {
                    power_menu = false; wm_invalidate();
                } else if (h >= 0) start_activate(h);
            }
            return true;
        }
        if (down && taskbar_hit(x, y) != HOV_TB_START) desktop_close_popups();
    }

    if (popup == POP_CALENDAR) {
        int cx, cy;
        cal_rect(&cx, &cy);
        if (ui_hit(x, y, cx, cy, CAL_W, CAL_H)) {
            int h = cal_hit(x, y);
            if (ev == EV_MOVE) set_hover(h);
            else if (down && h > 0) {
                cal_month += (h == 1) ? -1 : 1;
                if (cal_month < 1)  { cal_month = 12; cal_year--; }
                if (cal_month > 12) { cal_month = 1;  cal_year++; }
                wm_invalidate();
            }
            return true;
        }
        if (down && taskbar_hit(x, y) != HOV_TB_CLOCK) desktop_close_popups();
    }

    int t = taskbar_hit(x, y);
    if (y >= scr_h - TASKBAR_H) {
        if (ev == EV_MOVE) set_hover(t);
        else if (down && buttons == 1 && t >= 0) taskbar_click(t);
        return true;
    }
    if (ev == EV_MOVE && hover_item >= HOV_TB_START && popup == POP_NONE) set_hover(-1);
    if (ev == EV_MOVE && popup != POP_NONE) set_hover(-1);
    return false;
}

void desktop_background_mouse(int ev, int x, int y, uint8_t buttons) {
    int hit = -1;
    for (int i = 0; i < DESK_ICON_COUNT; i++) {
        int ix, iy;
        desk_icon_pos(i, &ix, &iy);
        if (ui_hit(x, y, ix, iy, DESK_CELL_W - 4, DESK_CELL_H - 6)) hit = i;
    }
    if (buttons == 2) {
        desk_selected = hit;
        ctx_x = x; ctx_y = y;
        int h = CTX_COUNT * CTX_ITEM + 8;
        if (ctx_x + CTX_W > scr_w) ctx_x = scr_w - CTX_W - 4;
        if (ctx_y + h > scr_h - TASKBAR_H) ctx_y = scr_h - TASKBAR_H - h - 4;
        open_popup(POP_CONTEXT);
        return;
    }
    if (desk_selected != hit) { desk_selected = hit; wm_invalidate(); }
    if (ev == EV_DBLCLICK && hit >= 0) run_action(desk_icons[hit].action);
}

bool desktop_key(char c) {
    if (c == 0x10) {   /* Windows key */
        if (popup == POP_START) desktop_close_popups();
        else { refresh_recommended(); open_popup(POP_START); }
        return true;
    }
    if (popup == POP_NONE) return false;
    if (c == 0x1B) { desktop_close_popups(); return true; }
    if (popup == POP_START) {
        if (c == '\b') {
            if (search_len > 0) search[--search_len] = '\0';
        } else if (c == '\n') {
            int idx[START_COUNT];
            if (start_visible(idx) > 0) start_activate(HOV_SM_PIN);
            return true;
        } else if ((unsigned char)c >= 0x20 && c < 0x7F && search_len < (int)sizeof(search) - 1) {
            search[search_len++] = c;
            search[search_len] = '\0';
        }
        hover_item = -1;
        wm_invalidate();
        return true;
    }
    return false;
}

void desktop_tick(void) {
    rtc_time_t t;
    rtc_read(&t);
    if (t.minute != now_time.minute || t.hour != now_time.hour || t.day != now_time.day ||
        (popup == POP_CALENDAR && t.second != now_time.second))
        wm_invalidate();
    if (popup == POP_START && search_len) wm_invalidate();   /* caret blink */
    now_time = t;
}

void desktop_set_wallpaper(int id) {
    theme.wallpaper = id;
    wallpaper_render(id, wallpaper, scr_w, scr_h, 1280);
    wm_invalidate();
}

void desktop_init(void) {
    scr_w = gfx_width();
    scr_h = gfx_height();
    rtc_read(&now_time);
    wallpaper_render(theme.wallpaper, wallpaper, scr_w, scr_h, 1280);
}

/* ---- Boot splash and power ---------------------------------------------------- */

static void spinner_frame(int frame, const char *caption) {
    gfx_fill_rect(0, 0, scr_w, scr_h, 0x000000);
    int ls = 128;
    logo_draw((scr_w - ls) / 2, scr_h / 2 - 170, ls);
    gfx_draw_text_scaled((scr_w - gfx_text_width("NovaeOS") * 3) / 2, scr_h / 2 - 20, "NovaeOS", 0xFFFFFF, 3);
    for (int i = 0; i < 6; i++) {
        int ang = frame * 12 - i * 22;
        int px = scr_w / 2 + 22 * isin(ang + 90) / 1024;
        int py = scr_h / 2 + 110 + 22 * isin(ang) / 1024;
        gfx_fill_circle(px, py, 3, gfx_blend(0x000000, 0xFFFFFF, (uint8_t)(255 - i * 36)));
    }
    if (caption)
        gfx_draw_text((scr_w - gfx_text_width(caption)) / 2, scr_h / 2 + 160, caption, 0xC8C8C8);
}

void desktop_splash(void) {
    uint32_t start = timer_get_ticks();
    int frame = 0;
    while (timer_get_ticks() - start < 150) {
        spinner_frame(frame++, 0);
        gfx_present();
        uint32_t t = timer_get_ticks();
        while (timer_get_ticks() - t < 3) __asm__ volatile ("hlt");
    }
}

void desktop_power(int action) {
    const char *caption = action ? "Restarting" : "Shutting down";
    for (int f = 0; f < 40; f++) {
        spinner_frame(f, caption);
        gfx_set_cursor_pos(-100, -100);
        gfx_present();
        uint32_t t = timer_get_ticks();
        while (timer_get_ticks() - t < 3) __asm__ volatile ("hlt");
    }
    if (action) {
        while (inb(0x64) & 0x02) { }
        outb(0x64, 0xFE);
    } else {
        outw(0x604, 0x2000);    /* QEMU (new) */
        outw(0xB004, 0x2000);   /* Bochs / old QEMU */
        outw(0x4004, 0x3400);   /* VirtualBox */
    }
    gfx_fill_rect(0, 0, scr_w, scr_h, 0x000000);
    const char *msg = "It is now safe to turn off your computer.";
    gfx_draw_text((scr_w - gfx_text_width(msg)) / 2, scr_h / 2, msg, 0xFFA040);
    gfx_present();
    for (;;) __asm__ volatile ("cli; hlt");
}
