/* =============================================================================
 * NovaeOS - Desktop / Window Manager API (kernel/gui/gui.h)
 * ============================================================================= */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>
#include "../gfx/gfx.h"

#define TITLE_H     34
#define TASKBAR_H   48
#define WIN_RADIUS  8
#define MAX_WINDOWS 16

/* Icons */
enum {
    ICON_COMPUTER, ICON_FOLDER, ICON_NOTEPAD, ICON_CALC, ICON_TERMINAL,
    ICON_SETTINGS, ICON_FILE, ICON_INFO, ICON_POWER, ICON_RESTART,
    ICON_LOGO, ICON_SEARCH, ICON_PALETTE, ICON_MONITOR, ICON_COUNT
};

/* Mouse events delivered to windows (client-relative coordinates) */
enum { EV_DOWN, EV_UP, EV_MOVE, EV_DBLCLICK, EV_LEAVE };

/* Applications */
enum { APP_NONE, APP_FILES, APP_NOTEPAD, APP_CALC, APP_TERMINAL, APP_SETTINGS };

/* ---- Theme ---------------------------------------------------------------- */
typedef struct {
    bool     dark;
    uint32_t accent;
    int      wallpaper;
    bool     taskbar_left;
    /* derived by theme_apply() */
    uint32_t win_bg, win_fg, win_dim, panel, border, title_bg, title_inactive;
    uint32_t hover, input_bg, taskbar_bg, menu_bg;
} theme_t;

extern theme_t theme;
void theme_apply(void);

/* ---- Windows -------------------------------------------------------------- */
typedef struct window window_t;
struct window {
    int   x, y, w, h;
    int   min_w, min_h;
    int   rx, ry, rw, rh;            /* rectangle to restore after maximize */
    char  title[64];
    int   icon;
    int   app;
    bool  used, minimized, maximized, resizable;
    void *data;
    void (*paint)(window_t *win, int x, int y, int w, int h);   /* client rect, screen coords */
    void (*mouse)(window_t *win, int ev, int x, int y, uint8_t buttons);
    void (*key)(window_t *win, char c);
    void (*tick)(window_t *win);                                /* ~10 times per second */
    void (*close)(window_t *win);
};

window_t *wm_create(const char *title, int icon, int app, int w, int h);
void      wm_close(window_t *win);
void      wm_focus(window_t *win);
void      wm_minimize(window_t *win);
void      wm_toggle_maximize(window_t *win);
window_t *wm_focused(void);
window_t *wm_find_app(int app);
int       wm_list(window_t **out, int max);       /* in creation order */
void      wm_set_title(window_t *win, const char *title);
void      wm_invalidate(void);
uint32_t  wm_ticks(void);
void      gui_run(void);

/* ---- Desktop shell (desktop.c) -------------------------------------------- */
#define WALLPAPER_COUNT 5
extern const char *wallpaper_names[WALLPAPER_COUNT];

void desktop_init(void);
void desktop_set_wallpaper(int id);
void wallpaper_render(int id, uint32_t *dst, int w, int h, int stride);
void desktop_draw_background(void);
void desktop_draw_overlay(void);
bool desktop_overlay_mouse(int ev, int x, int y, uint8_t buttons);   /* taskbar / menus */
void desktop_background_mouse(int ev, int x, int y, uint8_t buttons);
bool desktop_key(char c);
void desktop_tick(void);
void desktop_close_popups(void);
void desktop_splash(void);
void desktop_power(int action);   /* 0 = shut down, 1 = restart */

/* ---- Icons (icons.c) ------------------------------------------------------ */
void icon_draw(int id, int x, int y, int size);
void logo_draw(int x, int y, int size);

/* ---- Widgets (widgets.c) -------------------------------------------------- */
bool ui_hit(int px, int py, int x, int y, int w, int h);
void ui_button(int x, int y, int w, int h, const char *label, bool hover, bool primary);
void ui_text_center(int x, int y, int w, int h, const char *s, uint32_t c);
void ui_textbox(int x, int y, int w, int h, const char *text, bool focused);
void ui_format_size(uint32_t bytes, char *out);
void ui_utoa(uint32_t v, char *out);

/* ---- Apps ----------------------------------------------------------------- */
void app_launch(int app);
const char *app_name(int app);
int  app_icon(int app);
void files_open(void);
void notepad_open(const char *filename);
void calc_open(void);
void terminal_open(void);
void settings_open(int page);
void files_refresh_all(void);

#endif /* GUI_H */
