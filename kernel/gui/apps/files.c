/* =============================================================================
 * NovaeOS - File Explorer (kernel/gui/apps/files.c)
 * ============================================================================= */

#include "../gui.h"
#include "../../drivers/keyboard.h"
#include "../../fs/vfs.h"
#include "../../lib/kstring.h"

#define SIDEBAR_W 190
#define TOOLBAR_H 46
#define ROW_H 30
#define HEADER_H 30

typedef struct {
    char       names[VFS_MAX_FILES][VFS_MAX_FILENAME];
    vfs_type_t types[VFS_MAX_FILES];
    int        count, selected, hover_row, hover_btn;
} files_t;

static files_t files_state;
static const char *buttons[] = { "New", "Open", "Delete", "Refresh" };
#define BTN_COUNT 4

static void reload(files_t *f) {
    f->count = 0;
    vfs_list(f->names, f->types, &f->count);
    if (f->selected >= f->count) f->selected = f->count - 1;
}

void files_refresh_all(void) {
    if (wm_find_app(APP_FILES)) { reload(&files_state); wm_invalidate(); }
}

static void open_selected(files_t *f) {
    if (f->selected >= 0 && f->selected < f->count) notepad_open(f->names[f->selected]);
}

static void do_button(files_t *f, int b) {
    if (b == 0) {
        char name[VFS_MAX_FILENAME], num[12];
        for (int n = 1; n < 100; n++) {
            kstrcpy(name, "New file ");
            ui_utoa((uint32_t)n, num);
            kstrcat(name, num);
            kstrcat(name, ".txt");
            if (!vfs_exists(name)) break;
        }
        vfs_create(name, VFS_TYPE_FILE);
        reload(f);
        for (int i = 0; i < f->count; i++) if (!kstrcmp(f->names[i], name)) f->selected = i;
    } else if (b == 1) {
        open_selected(f);
    } else if (b == 2) {
        if (f->selected >= 0 && f->selected < f->count) vfs_delete(f->names[f->selected]);
        reload(f);
    } else {
        reload(f);
    }
    wm_invalidate();
}

static void files_paint(window_t *w, int x0, int y0, int cw, int ch) {
    files_t *f = w->data;
    gfx_fill_rect(x0, y0, cw, TOOLBAR_H, theme.title_bg);
    for (int b = 0; b < BTN_COUNT; b++)
        ui_button(x0 + 10 + b * 86, y0 + 8, 80, 30, buttons[b], f->hover_btn == b, b == 0);
    gfx_fill_rect(x0, y0 + TOOLBAR_H - 1, cw, 1, theme.border);

    /* Sidebar */
    int sy = y0 + TOOLBAR_H;
    gfx_fill_rect(x0, sy, SIDEBAR_W, ch - TOOLBAR_H, theme.panel);
    gfx_fill_rect(x0 + SIDEBAR_W, sy, 1, ch - TOOLBAR_H, theme.border);
    icon_draw(ICON_COMPUTER, x0 + 14, sy + 14, 16);
    gfx_draw_text(x0 + 38, sy + 14, "This PC", theme.win_fg);
    gfx_fill_round_rect(x0 + 8, sy + 40, SIDEBAR_W - 16, 32, 4, theme.hover);
    gfx_fill_round_rect(x0 + 8, sy + 48, 3, 16, 1, theme.accent);
    icon_draw(ICON_FOLDER, x0 + 22, sy + 48, 16);
    gfx_draw_text(x0 + 46, sy + 48, "NovaeVFS (R:)", theme.win_fg);

    uint32_t used = 0;
    for (int i = 0; i < f->count; i++) used += (uint32_t)vfs_get_size(f->names[i]);
    uint32_t cap = VFS_MAX_FILES * VFS_MAX_FILE_SIZE;
    gfx_draw_text(x0 + 14, sy + 92, "RAM disk", theme.win_dim);
    gfx_fill_round_rect(x0 + 14, sy + 114, SIDEBAR_W - 28, 8, 4, theme.border);
    int bar = (int)((SIDEBAR_W - 28) * (used > cap ? cap : used) / cap);
    gfx_fill_round_rect(x0 + 14, sy + 114, bar < 8 ? 8 : bar, 8, 4, theme.accent);
    char buf[48], num[24];
    ui_format_size(used, num);
    kstrcpy(buf, num); kstrcat(buf, " of 256 KB");
    gfx_draw_text(x0 + 14, sy + 130, buf, theme.win_dim);

    /* File list */
    int lx = x0 + SIDEBAR_W + 1, lw = cw - SIDEBAR_W - 1;
    gfx_fill_rect(lx, sy, lw, ch - TOOLBAR_H, theme.win_bg);
    gfx_draw_text(lx + 46, sy + 8, "Name", theme.win_dim);
    gfx_draw_text(lx + lw - 250, sy + 8, "Type", theme.win_dim);
    gfx_draw_text(lx + lw - 100, sy + 8, "Size", theme.win_dim);
    gfx_fill_rect(lx + 8, sy + HEADER_H - 2, lw - 16, 1, theme.border);
    for (int i = 0; i < f->count; i++) {
        int ry = sy + HEADER_H + i * ROW_H;
        if (i == f->selected)       gfx_fill_round_rect(lx + 6, ry, lw - 12, ROW_H - 2, 4, gfx_blend(theme.win_bg, theme.accent, 50));
        else if (i == f->hover_row) gfx_fill_round_rect(lx + 6, ry, lw - 12, ROW_H - 2, 4, theme.hover);
        icon_draw(f->types[i] == VFS_TYPE_DIR ? ICON_FOLDER : ICON_FILE, lx + 18, ry + 6, 16);
        gfx_draw_text(lx + 46, ry + 6, f->names[i], theme.win_fg);
        gfx_draw_text(lx + lw - 250, ry + 6, f->types[i] == VFS_TYPE_DIR ? "Folder" : "Text Document", theme.win_dim);
        ui_format_size((uint32_t)vfs_get_size(f->names[i]), num);
        gfx_draw_text(lx + lw - 100, ry + 6, num, theme.win_dim);
    }
    ui_utoa((uint32_t)f->count, num);
    kstrcpy(buf, num); kstrcat(buf, f->count == 1 ? " item" : " items");
    gfx_draw_text(lx + 12, y0 + ch - 22, buf, theme.win_dim);
}

static void files_mouse(window_t *w, int ev, int x, int y, uint8_t buttons) {
    files_t *f = w->data;
    int lw = (w->w - 2) - SIDEBAR_W - 1;
    int btn = -1, row = -1;
    for (int b = 0; b < BTN_COUNT; b++) if (ui_hit(x, y, 10 + b * 86, 8, 80, 30)) btn = b;
    if (x > SIDEBAR_W + 6 && x < SIDEBAR_W + lw && y >= TOOLBAR_H + HEADER_H) {
        int r = (y - TOOLBAR_H - HEADER_H) / ROW_H;
        if (r < f->count) row = r;
    }
    if (ev == EV_LEAVE) btn = row = -1;
    if (btn != f->hover_btn || row != f->hover_row) { f->hover_btn = btn; f->hover_row = row; wm_invalidate(); }
    if (buttons != 1) return;
    if (ev == EV_DOWN) {
        if (btn >= 0) do_button(f, btn);
        else if (y >= TOOLBAR_H + HEADER_H && x > SIDEBAR_W) { f->selected = row; wm_invalidate(); }
    } else if (ev == EV_DBLCLICK && row >= 0) {
        open_selected(f);
    }
}

static void files_key(window_t *w, char c) {
    files_t *f = w->data;
    if (c == KEY_DOWN && f->selected < f->count - 1) f->selected++;
    else if (c == KEY_UP && f->selected > 0) f->selected--;
    else if (c == '\n') open_selected(f);
    else if (c == KEY_DELETE) do_button(f, 2);
    wm_invalidate();
}

void files_open(void) {
    window_t *w = wm_find_app(APP_FILES);
    if (w) { wm_focus(w); return; }
    w = wm_create("File Explorer", ICON_FOLDER, APP_FILES, 760, 480);
    if (!w) return;
    for (size_t i = 0; i < sizeof(files_state); i++) ((uint8_t *)&files_state)[i] = 0;
    files_state.selected = -1;
    files_state.hover_row = files_state.hover_btn = -1;
    reload(&files_state);
    w->resizable = true;
    w->min_w = 560; w->min_h = 260;
    w->data  = &files_state;
    w->paint = files_paint;
    w->mouse = files_mouse;
    w->key   = files_key;
}
