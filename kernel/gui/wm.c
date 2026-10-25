/* =============================================================================
 * NovaeOS - Window Manager and GUI event loop (kernel/gui/wm.c)
 * ============================================================================= */

#include "gui.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../lib/kstring.h"

theme_t theme = { .dark = false, .accent = 0x0067C0, .wallpaper = 0, .taskbar_left = false };

void theme_apply(void) {
    if (theme.dark) {
        theme.win_bg = 0x202020;  theme.win_fg = 0xF3F3F3;  theme.win_dim = 0xA8A8A8;
        theme.panel  = 0x2D2D2D;  theme.border = 0x454545;  theme.title_bg = 0x2B2B2B;
        theme.title_inactive = 0x232323; theme.hover = 0x3D3D3D; theme.input_bg = 0x1B1B1B;
        theme.taskbar_bg = 0x1C1C1C; theme.menu_bg = 0x2A2A2A;
    } else {
        theme.win_bg = 0xF9F9F9;  theme.win_fg = 0x1B1B1B;  theme.win_dim = 0x5F5F5F;
        theme.panel  = 0xFDFDFD;  theme.border = 0xD3D3D3;  theme.title_bg = 0xEBEEF3;
        theme.title_inactive = 0xF4F4F4; theme.hover = 0xE3E6EA; theme.input_bg = 0xFFFFFF;
        theme.taskbar_bg = 0xF0F2F5; theme.menu_bg = 0xF5F6F8;
    }
}

/* ---- Window bookkeeping ----------------------------------------------------- */

static window_t  windows[MAX_WINDOWS];
static uint32_t  win_seq[MAX_WINDOWS];
static uint32_t  next_seq = 1;
static window_t *zorder[MAX_WINDOWS];     /* bottom .. top */
static int       zcount = 0;
static bool      dirty = true;

enum { DRAG_NONE, DRAG_MOVE, DRAG_RESIZE, DRAG_CLIENT };
static int       drag_mode = DRAG_NONE;
static window_t *drag_win  = 0;
static int       drag_dx, drag_dy;

static int       mx, my;
static uint8_t   mbtn;
static window_t *hover_win = 0;           /* window whose client area is under the cursor */
static window_t *hover_title_win = 0;
static int       hover_title_btn = -1;    /* 0 = minimize, 1 = maximize, 2 = close */
static uint32_t  last_click_tick = 0;
static int       last_click_x = -100, last_click_y = -100;

void     wm_invalidate(void) { dirty = true; }
uint32_t wm_ticks(void)      { return timer_get_ticks(); }

static int screen_work_h(void) { return gfx_height() - TASKBAR_H; }

static void z_remove(window_t *w) {
    for (int i = 0; i < zcount; i++) {
        if (zorder[i] == w) {
            for (int j = i; j < zcount - 1; j++) zorder[j] = zorder[j + 1];
            zcount--;
            return;
        }
    }
}

window_t *wm_focused(void) {
    for (int i = zcount - 1; i >= 0; i--)
        if (!zorder[i]->minimized) return zorder[i];
    return 0;
}

window_t *wm_create(const char *title, int icon, int app, int w, int h) {
    window_t *win = 0;
    int slot = 0;
    for (; slot < MAX_WINDOWS; slot++)
        if (!windows[slot].used) { win = &windows[slot]; break; }
    if (!win) return 0;

    for (size_t i = 0; i < sizeof(*win); i++) ((uint8_t *)win)[i] = 0;
    win->used = true;
    kstrncpy(win->title, title, sizeof(win->title) - 1);
    win->icon = icon;
    win->app  = app;
    win->w = w; win->h = h;
    win->min_w = 260; win->min_h = 160;
    int cascade = (zcount % 6) * 28;
    win->x = (gfx_width() - w) / 2 - 80 + cascade;
    win->y = (screen_work_h() - h) / 2 - 50 + cascade;
    if (win->x < 8) win->x = 8;
    if (win->y < 8) win->y = 8;
    if (win->x + w > gfx_width()) win->x = gfx_width() - w;
    if (win->y + h > screen_work_h()) win->y = screen_work_h() - h > 0 ? screen_work_h() - h : 0;

    win_seq[slot] = next_seq++;
    zorder[zcount++] = win;
    desktop_close_popups();
    dirty = true;
    return win;
}

void wm_close(window_t *win) {
    if (!win || !win->used) return;
    if (win->close) win->close(win);
    z_remove(win);
    win->used = false;
    if (drag_win == win)  { drag_win = 0; drag_mode = DRAG_NONE; }
    if (hover_win == win) hover_win = 0;
    if (hover_title_win == win) hover_title_win = 0;
    dirty = true;
}

void wm_focus(window_t *win) {
    if (!win || !win->used) return;
    win->minimized = false;
    z_remove(win);
    zorder[zcount++] = win;
    dirty = true;
}

void wm_minimize(window_t *win) {
    if (!win) return;
    win->minimized = true;
    z_remove(win);                   /* drop to the bottom of the stack */
    for (int j = zcount; j > 0; j--) zorder[j] = zorder[j - 1];
    zorder[0] = win;
    zcount++;
    dirty = true;
}

void wm_toggle_maximize(window_t *win) {
    if (!win || !win->resizable) return;
    if (win->maximized) {
        win->x = win->rx; win->y = win->ry; win->w = win->rw; win->h = win->rh;
        win->maximized = false;
    } else {
        win->rx = win->x; win->ry = win->y; win->rw = win->w; win->rh = win->h;
        win->x = 0; win->y = 0; win->w = gfx_width(); win->h = screen_work_h();
        win->maximized = true;
    }
    dirty = true;
}

window_t *wm_find_app(int app) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].used && windows[i].app == app) return &windows[i];
    return 0;
}

int wm_list(window_t **out, int max) {
    int n = 0;
    uint32_t last = 0;
    while (n < max) {
        int best = -1;
        for (int i = 0; i < MAX_WINDOWS; i++)
            if (windows[i].used && win_seq[i] > last && (best < 0 || win_seq[i] < win_seq[best]))
                best = i;
        if (best < 0) break;
        out[n++] = &windows[best];
        last = win_seq[best];
    }
    return n;
}

void wm_set_title(window_t *win, const char *title) {
    kstrncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';
    dirty = true;
}

/* ---- Geometry helpers ------------------------------------------------------- */

static void client_rect(window_t *w, int *x, int *y, int *cw, int *ch) {
    *x = w->x + 1; *y = w->y + TITLE_H; *cw = w->w - 2; *ch = w->h - TITLE_H - 1;
}

static window_t *window_at(int x, int y) {
    for (int i = zcount - 1; i >= 0; i--) {
        window_t *w = zorder[i];
        if (!w->minimized && ui_hit(x, y, w->x, w->y, w->w, w->h)) return w;
    }
    return 0;
}

/* -1 none, 0 minimize, 1 maximize, 2 close */
static int title_button_at(window_t *w, int x, int y) {
    if (y < w->y || y >= w->y + TITLE_H) return -1;
    int right = w->x + w->w - 1;
    if (x >= right - 46)                          return 2;
    if (w->resizable && x >= right - 92)          return 1;
    if (x >= right - (w->resizable ? 138 : 92))   return 0;
    return -1;
}

static bool in_grip(window_t *w, int x, int y) {
    return w->resizable && !w->maximized &&
           x >= w->x + w->w - 14 && y >= w->y + w->h - 14;
}

/* ---- Drawing ---------------------------------------------------------------- */

static void draw_title_buttons(window_t *w, bool active) {
    int right = w->x + w->w - 1;
    int count = w->resizable ? 3 : 2;
    for (int b = 0; b < count; b++) {
        int kind = w->resizable ? b : (b == 0 ? 0 : 2);
        int bx = right - 46 * (count - b);
        bool hov = (hover_title_win == w && hover_title_btn == kind);
        uint32_t fg = active ? theme.win_fg : theme.win_dim;
        if (hov) {
            uint32_t bg = (kind == 2) ? 0xC42B1C : theme.hover;
            if (kind == 2) fg = 0xFFFFFF;
            int rad = (kind == 2 && !w->maximized) ? WIN_RADIUS - 1 : 0;
            gfx_fill_round_rect(bx, w->y + 1, 46, TITLE_H - 1, rad, bg);
            if (rad) gfx_fill_rect(bx, w->y + 1, 46 - rad, TITLE_H - 1, bg),
                     gfx_fill_rect(bx, w->y + 1 + rad, 46, TITLE_H - 1 - rad, bg);
        }
        int cx = bx + 18, cy = w->y + TITLE_H / 2 - 5;
        if (kind == 0) {
            gfx_fill_rect(cx, cy + 5, 10, 1, fg);
        } else if (kind == 1) {
            if (w->maximized) {
                gfx_draw_rect(cx, cy + 2, 8, 8, fg);
                gfx_fill_rect(cx + 2, cy, 8, 1, fg);
                gfx_fill_rect(cx + 9, cy, 1, 8, fg);
            } else {
                gfx_draw_rect(cx, cy, 10, 10, fg);
            }
        } else {
            gfx_draw_line(cx, cy, cx + 10, cy + 10, fg);
            gfx_draw_line(cx + 10, cy, cx, cy + 10, fg);
            gfx_draw_line(cx + 1, cy, cx + 10, cy + 9, fg);
            gfx_draw_line(cx + 9, cy, cx, cy + 9, fg);
        }
    }
}

#define CORNER_SAVE (WIN_RADIUS * WIN_RADIUS * 2)

static void draw_window(window_t *w) {
    bool active = (w == wm_focused());
    int r = w->maximized ? 0 : WIN_RADIUS;

    if (!w->maximized) gfx_drop_shadow(w->x, w->y, w->w, w->h, active ? 14 : 8);

    /* Remember what is under the bottom corners so client painting can't square them off */
    static uint32_t saved[CORNER_SAVE];
    int n = 0;
    for (int j = 0; j < r; j++) {
        int inset = gfx_round_inset(r, j);
        int yy = w->y + w->h - 1 - j;
        for (int i = 0; i < inset; i++) {
            saved[n++] = gfx_get_pixel(w->x + i, yy);
            saved[n++] = gfx_get_pixel(w->x + w->w - 1 - i, yy);
        }
    }

    gfx_fill_round_rect(w->x, w->y, w->w, w->h, r, theme.win_bg);

    uint32_t tbg = active ? theme.title_bg : theme.title_inactive;
    gfx_fill_round_rect(w->x, w->y, w->w, TITLE_H + r, r, tbg);
    gfx_fill_rect(w->x, w->y + TITLE_H, w->w, r + 1, theme.win_bg);

    icon_draw(w->icon, w->x + 12, w->y + (TITLE_H - 16) / 2, 16);
    gfx_set_clip(w->x + 36, w->y, w->w - 36 - 140, TITLE_H);
    gfx_draw_text(w->x + 38, w->y + (TITLE_H - FONT_H) / 2, w->title, active ? theme.win_fg : theme.win_dim);
    gfx_reset_clip();
    draw_title_buttons(w, active);

    int cx, cy, cw, ch;
    client_rect(w, &cx, &cy, &cw, &ch);
    if (w->paint) {
        gfx_set_clip(cx, cy, cw, ch);
        w->paint(w, cx, cy, cw, ch);
        gfx_reset_clip();
    }

    n = 0;
    for (int j = 0; j < r; j++) {
        int inset = gfx_round_inset(r, j);
        int yy = w->y + w->h - 1 - j;
        for (int i = 0; i < inset; i++) {
            gfx_put_pixel(w->x + i, yy, saved[n++]);
            gfx_put_pixel(w->x + w->w - 1 - i, yy, saved[n++]);
        }
    }

    if (w->resizable && !w->maximized) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j <= i; j++)
                gfx_fill_rect(w->x + w->w - 6 - (i - j) * 4, w->y + w->h - 14 + i * 4 + 2, 2, 2, theme.win_dim);
    }

    gfx_draw_round_rect(w->x, w->y, w->w, w->h, r, active ? gfx_blend(theme.border, theme.accent, 90) : theme.border);
}

static void render(void) {
    gfx_reset_clip();
    desktop_draw_background();
    for (int i = 0; i < zcount; i++)
        if (!zorder[i]->minimized) draw_window(zorder[i]);
    desktop_draw_overlay();
}

/* ---- Input ------------------------------------------------------------------ */

static void send_mouse(window_t *w, int ev, uint8_t buttons) {
    if (!w || !w->mouse) return;
    int cx, cy, cw, ch;
    client_rect(w, &cx, &cy, &cw, &ch);
    w->mouse(w, ev, mx - cx, my - cy, buttons);
}

static void set_hover_client(window_t *w) {
    if (hover_win != w) {
        if (hover_win) send_mouse(hover_win, EV_LEAVE, 0);
        hover_win = w;
    }
}

static void handle_move(void) {
    if (drag_mode == DRAG_MOVE) {
        int nx = mx - drag_dx, ny = my - drag_dy;
        if (ny < 0) ny = 0;
        if (ny > screen_work_h() - TITLE_H) ny = screen_work_h() - TITLE_H;
        drag_win->x = nx; drag_win->y = ny;
        dirty = true;
        return;
    }
    if (drag_mode == DRAG_RESIZE) {
        int nw = mx - drag_win->x + drag_dx, nh = my - drag_win->y + drag_dy;
        if (nw < drag_win->min_w) nw = drag_win->min_w;
        if (nh < drag_win->min_h) nh = drag_win->min_h;
        drag_win->w = nw; drag_win->h = nh;
        dirty = true;
        return;
    }
    if (drag_mode == DRAG_CLIENT) {
        send_mouse(drag_win, EV_MOVE, mbtn);
        return;
    }

    window_t *tw = 0;
    int tb = -1;
    window_t *cw = 0;
    if (!desktop_overlay_mouse(EV_MOVE, mx, my, mbtn)) {
        window_t *w = window_at(mx, my);
        if (w) {
            tb = title_button_at(w, mx, my);
            if (tb >= 0) tw = w;
            else if (my >= w->y + TITLE_H) cw = w;
        }
    }
    if (tw != hover_title_win || tb != hover_title_btn) {
        hover_title_win = tw; hover_title_btn = tb;
        dirty = true;
    }
    set_hover_client(cw);
    if (cw) send_mouse(cw, EV_MOVE, mbtn);
}

static void handle_down(uint8_t button) {
    bool dbl = false;
    if (button == MOUSE_LEFT) {
        uint32_t now = wm_ticks();
        int ddx = mx - last_click_x, ddy = my - last_click_y;
        dbl = (now - last_click_tick) < 45 && ddx * ddx + ddy * ddy < 36;
        last_click_tick = dbl ? 0 : now;
        last_click_x = mx; last_click_y = my;
    }

    if (desktop_overlay_mouse(dbl ? EV_DBLCLICK : EV_DOWN, mx, my, button)) return;

    window_t *w = window_at(mx, my);
    if (!w) {
        desktop_background_mouse(dbl ? EV_DBLCLICK : EV_DOWN, mx, my, button);
        return;
    }
    wm_focus(w);

    if (button == MOUSE_LEFT) {
        int tb = title_button_at(w, mx, my);
        if (tb == 2) { wm_close(w); return; }
        if (tb == 1) { wm_toggle_maximize(w); return; }
        if (tb == 0) { wm_minimize(w); return; }
        if (my < w->y + TITLE_H) {
            if (dbl) { wm_toggle_maximize(w); return; }
            if (w->maximized) {               /* pull a maximized window off the top */
                int rel = (mx - w->x) * w->rw / (w->w ? w->w : 1);
                wm_toggle_maximize(w);
                w->x = mx - rel;
                w->y = 0;
            }
            drag_mode = DRAG_MOVE; drag_win = w;
            drag_dx = mx - w->x; drag_dy = my - w->y;
            return;
        }
        if (in_grip(w, mx, my)) {
            drag_mode = DRAG_RESIZE; drag_win = w;
            drag_dx = w->x + w->w - mx; drag_dy = w->y + w->h - my;
            return;
        }
    }
    if (my >= w->y + TITLE_H) {
        send_mouse(w, EV_DOWN, button);
        if (dbl) send_mouse(w, EV_DBLCLICK, button);
        if (button == MOUSE_LEFT) { drag_mode = DRAG_CLIENT; drag_win = w; }
    }
}

static void handle_up(void) {
    if (drag_mode == DRAG_CLIENT && drag_win) send_mouse(drag_win, EV_UP, MOUSE_LEFT);
    drag_mode = DRAG_NONE;
    drag_win = 0;
}

static void handle_key(char c) {
    if (desktop_key(c)) return;
    window_t *w = wm_focused();
    if (w && w->key) w->key(w, c);
}

/* ---- Main loop -------------------------------------------------------------- */

void gui_run(void) {
    theme_apply();
    desktop_init();
    desktop_splash();
    mouse_init(gfx_width(), gfx_height());
    mx = gfx_width() / 2; my = gfx_height() / 2;

    uint32_t last_tick = wm_ticks();
    bool cursor_moved = false;

    for (;;) {
        mouse_event_t ev;
        bool any = false;
        while (mouse_poll(&ev)) {
            any = true;
            uint8_t prev = mbtn;
            mbtn = ev.buttons;
            if (ev.x != mx || ev.y != my) {
                mx = ev.x; my = ev.y;
                cursor_moved = true;
                handle_move();
            }
            if ((mbtn & MOUSE_LEFT)  && !(prev & MOUSE_LEFT))  handle_down(MOUSE_LEFT);
            if ((mbtn & MOUSE_RIGHT) && !(prev & MOUSE_RIGHT)) handle_down(MOUSE_RIGHT);
            if (!(mbtn & MOUSE_LEFT) && (prev & MOUSE_LEFT))   handle_up();
        }

        char c;
        while ((c = keyboard_try_getchar()) != 0) { any = true; handle_key(c); }

        uint32_t now = wm_ticks();
        if (now - last_tick >= 10) {
            last_tick = now;
            desktop_tick();
            for (int i = 0; i < MAX_WINDOWS; i++)
                if (windows[i].used && windows[i].tick) windows[i].tick(&windows[i]);
        }

        if (dirty) {
            dirty = false;
            render();
            gfx_set_cursor_pos(mx, my);
            gfx_present();
            cursor_moved = false;
        } else if (cursor_moved) {
            gfx_move_cursor(mx, my);
            cursor_moved = false;
        }

        if (!any) __asm__ volatile ("hlt");
    }
}
