/* =============================================================================
 * NovaeOS - Notepad (kernel/gui/apps/notepad.c)
 * Plain-text editor that loads and saves files in NovaeVFS
 * ============================================================================= */

#include "../gui.h"
#include "../../drivers/keyboard.h"
#include "../../fs/vfs.h"
#include "../../mm/heap.h"
#include "../../lib/kstring.h"

#define LINE_H 18
#define TOOLBAR_H 44
#define STATUS_H 26

typedef struct {
    char name[VFS_MAX_FILENAME];
    char text[VFS_MAX_FILE_SIZE];
    int  len, cur, scroll;
    bool modified, name_focus;
    int  hover;               /* 0 = New, 1 = Save */
    const char *status;
} notepad_t;

static void update_title(window_t *w) {
    notepad_t *n = w->data;
    char t[64];
    kstrcpy(t, n->modified ? "*" : "");
    kstrcat(t, n->name[0] ? n->name : "Untitled");
    kstrcat(t, " - Notepad");
    wm_set_title(w, t);
}

static void cursor_pos(notepad_t *n, int *line, int *col) {
    int l = 0, c = 0;
    for (int i = 0; i < n->cur; i++) {
        if (n->text[i] == '\n') { l++; c = 0; } else c++;
    }
    *line = l; *col = c;
}

static int line_start(notepad_t *n, int line) {
    int i = 0;
    while (line > 0 && i < n->len) { if (n->text[i++] == '\n') line--; }
    return i;
}

static int line_length(notepad_t *n, int start) {
    int i = start;
    while (i < n->len && n->text[i] != '\n') i++;
    return i - start;
}

static void move_to(notepad_t *n, int line, int col) {
    if (line < 0) line = 0;
    int s = line_start(n, line);
    int len = line_length(n, s);
    n->cur = s + (col < len ? col : len);
}

static void insert(notepad_t *n, char c) {
    if (n->len >= VFS_MAX_FILE_SIZE - 1) { n->status = "File is full (4 KB limit)"; return; }
    for (int i = n->len; i > n->cur; i--) n->text[i] = n->text[i - 1];
    n->text[n->cur++] = c;
    n->len++;
    n->modified = true;
}

static void delete_at(notepad_t *n, int pos) {
    if (pos < 0 || pos >= n->len) return;
    for (int i = pos; i < n->len - 1; i++) n->text[i] = n->text[i + 1];
    n->len--;
    n->modified = true;
}

static void save(window_t *w) {
    notepad_t *n = w->data;
    if (!n->name[0]) { n->name_focus = true; n->status = "Type a file name, then press Enter"; return; }
    if (!vfs_exists(n->name) && vfs_create(n->name, VFS_TYPE_FILE) < 0) { n->status = "Could not create file"; return; }
    n->text[n->len] = '\0';
    vfs_write(n->name, n->len ? n->text : "", (size_t)n->len);
    n->modified = false;
    n->name_focus = false;
    n->status = "Saved";
    files_refresh_all();
}

static void np_key(window_t *w, char c) {
    notepad_t *n = w->data;
    n->status = 0;
    if (n->name_focus) {
        int l = (int)kstrlen(n->name);
        if (c == '\n') save(w);
        else if (c == KEY_ESC) n->name_focus = false;
        else if (c == '\b') { if (l) n->name[l - 1] = '\0'; }
        else if ((unsigned char)c > 0x20 && c < 0x7F && c != '/' && l < VFS_MAX_FILENAME - 1) { n->name[l] = c; n->name[l + 1] = '\0'; }
    } else {
        int line, col;
        cursor_pos(n, &line, &col);
        switch (c) {
        case '\b':       if (n->cur > 0) { n->cur--; delete_at(n, n->cur); } break;
        case KEY_DELETE: delete_at(n, n->cur); break;
        case KEY_LEFT:   if (n->cur > 0) n->cur--; break;
        case KEY_RIGHT:  if (n->cur < n->len) n->cur++; break;
        case KEY_UP:     if (line > 0) move_to(n, line - 1, col); break;
        case KEY_DOWN:   move_to(n, line + 1, col); break;
        case KEY_HOME:   n->cur -= col; break;
        case KEY_END:    n->cur = line_start(n, line) + line_length(n, line_start(n, line)); break;
        case '\t':       for (int i = 0; i < 4; i++) insert(n, ' '); break;
        default:
            if (c == '\n' || ((unsigned char)c >= 0x20 && c < 0x7F)) insert(n, c);
        }
    }
    update_title(w);
    wm_invalidate();
}

static void np_paint(window_t *w, int x0, int y0, int cw, int ch) {
    notepad_t *n = w->data;
    gfx_fill_rect(x0, y0, cw, TOOLBAR_H, theme.title_bg);
    ui_button(x0 + 8, y0 + 7, 64, 30, "New", n->hover == 0, false);
    ui_button(x0 + 78, y0 + 7, 64, 30, "Save", n->hover == 1, true);
    gfx_draw_text(x0 + 160, y0 + 14, "File name:", theme.win_dim);
    ui_textbox(x0 + 246, y0 + 7, cw - 256 > 120 ? cw - 256 : 120, 30, n->name, n->name_focus);
    gfx_fill_rect(x0, y0 + TOOLBAR_H - 1, cw, 1, theme.border);

    int ey = y0 + TOOLBAR_H, eh = ch - TOOLBAR_H - STATUS_H;
    gfx_fill_rect(x0, ey, cw, eh, theme.input_bg);
    int vis = (eh - 8) / LINE_H;
    int line, col;
    cursor_pos(n, &line, &col);
    if (line < n->scroll) n->scroll = line;
    if (line >= n->scroll + vis) n->scroll = line - vis + 1;

    gfx_set_clip(x0 + 4, ey, cw - 8, eh);
    int l = 0, c = 0;
    for (int i = 0; i <= n->len; i++) {
        int py = ey + 6 + (l - n->scroll) * LINE_H;
        if (i == n->cur && !n->name_focus && l >= n->scroll && (wm_ticks() / 50) % 2 == 0)
            gfx_fill_rect(x0 + 10 + c * FONT_W, py, 2, FONT_H, theme.win_fg);
        if (i == n->len) break;
        if (n->text[i] == '\n') { l++; c = 0; continue; }
        if (l >= n->scroll && l < n->scroll + vis)
            gfx_draw_char(x0 + 10 + c * FONT_W, py, (uint8_t)n->text[i], theme.win_fg);
        c++;
    }
    gfx_set_clip(x0, y0, cw, ch);

    int sy = y0 + ch - STATUS_H;
    gfx_fill_rect(x0, sy, cw, STATUS_H, theme.title_bg);
    gfx_fill_rect(x0, sy, cw, 1, theme.border);
    char buf[64], num[12];
    kstrcpy(buf, "Ln ");
    ui_utoa((uint32_t)line + 1, num); kstrcat(buf, num);
    kstrcat(buf, ", Col ");
    ui_utoa((uint32_t)col + 1, num); kstrcat(buf, num);
    gfx_draw_text(x0 + 10, sy + 5, buf, theme.win_dim);
    ui_utoa((uint32_t)n->len, num);
    kstrcpy(buf, num); kstrcat(buf, " characters   UTF-8");
    gfx_draw_text(x0 + cw - 10 - gfx_text_width(buf), sy + 5, buf, theme.win_dim);
    if (n->status) gfx_draw_text(x0 + 160, sy + 5, n->status, theme.accent);
}

static void np_mouse(window_t *w, int ev, int x, int y, uint8_t buttons) {
    notepad_t *n = w->data;
    int h = ui_hit(x, y, 8, 7, 64, 30) ? 0 : ui_hit(x, y, 78, 7, 64, 30) ? 1 : -1;
    if (ev == EV_LEAVE) h = -1;
    if (h != n->hover) { n->hover = h; wm_invalidate(); }
    if (ev != EV_DOWN || buttons != 1) return;
    n->status = 0;
    if (h == 0) {
        n->len = n->cur = n->scroll = 0;
        n->name[0] = '\0';
        n->modified = false;
    } else if (h == 1) {
        save(w);
    } else if (y < TOOLBAR_H) {
        n->name_focus = x >= 246;
    } else {
        n->name_focus = false;
        int line = n->scroll + (y - TOOLBAR_H - 6) / LINE_H;
        move_to(n, line, (x - 10 + FONT_W / 2) / FONT_W);
    }
    update_title(w);
    wm_invalidate();
}

static void np_tick(window_t *w) { if (w == wm_focused()) wm_invalidate(); }
static void np_close(window_t *w) { kfree(w->data); }

void notepad_open(const char *filename) {
    notepad_t *n = kmalloc(sizeof(notepad_t));
    if (!n) return;
    window_t *w = wm_create("Notepad", ICON_NOTEPAD, APP_NOTEPAD, 640, 460);
    if (!w) { kfree(n); return; }
    kmemset(n, 0, sizeof(*n));
    n->hover = -1;
    if (filename && filename[0]) {
        kstrncpy(n->name, filename, VFS_MAX_FILENAME - 1);
        int r = vfs_read(filename, n->text, sizeof(n->text));
        n->len = r > 0 ? r : 0;
        if (r < 0) n->status = "New file - press Save to create it";
    }
    w->resizable = true;
    w->min_w = 420; w->min_h = 220;
    w->data  = n;
    w->paint = np_paint;
    w->mouse = np_mouse;
    w->key   = np_key;
    w->tick  = np_tick;
    w->close = np_close;
    update_title(w);
}
