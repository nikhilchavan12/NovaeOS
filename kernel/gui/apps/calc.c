/* =============================================================================
 * NovaeOS - Calculator (kernel/gui/apps/calc.c)
 * Standard calculator with operator precedence and parentheses
 * ============================================================================= */

#include "../gui.h"
#include "../../drivers/keyboard.h"
#include "../../lib/kstring.h"

typedef struct {
    char expr[48];
    char history[64];
    bool evaluated;
    int  hover, pressed;
} calc_t;

static calc_t calc_state;

/* ---- Evaluator ---------------------------------------------------------- */

static const char *ep;
static int eval_err;   /* 0 ok, 1 syntax, 2 divide by zero */

static double parse_expr(void);

static double parse_number(void) {
    double v = 0, scale = 0;
    bool any = false;
    while ((*ep >= '0' && *ep <= '9') || *ep == '.') {
        if (*ep == '.') {
            if (scale) { eval_err = 1; return 0; }
            scale = 1;
        } else {
            v = v * 10 + (*ep - '0');
            if (scale) scale *= 10;
            any = true;
        }
        ep++;
    }
    if (!any) { eval_err = 1; return 0; }
    return scale ? v / scale : v;
}

static double parse_factor(void) {
    if (*ep == '-') { ep++; return -parse_factor(); }
    if (*ep == '+') { ep++; return parse_factor(); }
    if (*ep == '(') {
        ep++;
        double v = parse_expr();
        if (*ep == ')') ep++;            /* missing ')' at the end is tolerated */
        else if (*ep) eval_err = 1;
        return v;
    }
    return parse_number();
}

static double parse_term(void) {
    double v = parse_factor();
    while (!eval_err && (*ep == '*' || *ep == '/')) {
        char op = *ep++;
        double r = parse_factor();
        if (op == '*') v *= r;
        else if (r == 0) eval_err = 2;
        else v /= r;
    }
    return v;
}

static double parse_expr(void) {
    double v = parse_term();
    while (!eval_err && (*ep == '+' || *ep == '-')) {
        char op = *ep++;
        double r = parse_term();
        v = (op == '+') ? v + r : v - r;
    }
    return v;
}

static double evaluate(const char *s, int *err) {
    ep = s;
    eval_err = 0;
    double v = parse_expr();
    if (*ep && !eval_err) eval_err = 1;
    *err = eval_err;
    return v;
}

static double fsqrt(double v) {
    __asm__ ("fsqrt" : "+t"(v));
    return v;
}

static void format_number(double v, char *out) {
    if (v != v || v > 1e15 || v < -1e15) { kstrcpy(out, "Overflow"); return; }
    int n = 0;
    if (v < 0) { out[n++] = '-'; v = -v; }
    long long ip = (long long)v;
    long long fp = (long long)((v - (double)ip) * 1000000000.0 + 0.5);
    if (fp >= 1000000000LL) { ip++; fp -= 1000000000LL; }
    char tmp[24];
    int t = 0;
    do { tmp[t++] = (char)('0' + ip % 10); ip /= 10; } while (ip);
    while (t) out[n++] = tmp[--t];
    if (fp) {
        out[n++] = '.';
        char frac[10];
        for (int i = 8; i >= 0; i--) { frac[i] = (char)('0' + fp % 10); fp /= 10; }
        int last = 8;
        while (last > 0 && frac[last] == '0') last--;
        for (int i = 0; i <= last; i++) out[n++] = frac[i];
    }
    out[n] = '\0';
    if (n == 2 && out[0] == '-' && out[1] == '0') kstrcpy(out, "0");
}

/* ---- Input handling ------------------------------------------------------ */

static bool is_op(char c) { return c == '+' || c == '-' || c == '*' || c == '/'; }

static void show_error(calc_t *c, int err) {
    kstrcpy(c->history, c->expr);
    kstrcpy(c->expr, err == 2 ? "Cannot divide by zero" : "Invalid input");
    c->evaluated = true;
}

static bool expr_is_error(calc_t *c) { return c->expr[0] && !(c->expr[0] == '-' || c->expr[0] == '(' || c->expr[0] == '.' || (c->expr[0] >= '0' && c->expr[0] <= '9')); }

static void calc_input(calc_t *c, char ch) {
    size_t len = kstrlen(c->expr);
    if (expr_is_error(c)) { c->expr[0] = '\0'; len = 0; }

    if (ch == 'C') { c->expr[0] = '\0'; c->history[0] = '\0'; c->evaluated = false; return; }
    if (ch == '\b') {
        if (c->evaluated) c->expr[0] = '\0';
        else if (len) c->expr[len - 1] = '\0';
        c->evaluated = false;
        return;
    }
    if (ch == '=') {
        if (!len) return;
        int err;
        double v = evaluate(c->expr, &err);
        if (err) { show_error(c, err); return; }
        kstrcpy(c->history, c->expr);
        kstrcat(c->history, " =");
        format_number(v, c->expr);
        c->evaluated = true;
        return;
    }
    if (ch == 'q' || ch == 'r' || ch == '%' || ch == 'n') {   /* square, root, percent, negate */
        int err;
        double v = len ? evaluate(c->expr, &err) : (err = 0, 0.0);
        if (err) { show_error(c, err); return; }
        static const char *names[] = { "sqr(", "sqrt(", "percent(", "negate(" };
        int k = ch == 'q' ? 0 : ch == 'r' ? 1 : ch == '%' ? 2 : 3;
        if (k == 1 && v < 0) { kstrcpy(c->history, "sqrt of negative"); kstrcpy(c->expr, "Invalid input"); c->evaluated = true; return; }
        kstrcpy(c->history, names[k]);
        kstrcat(c->history, len ? c->expr : "0");
        kstrcat(c->history, ")");
        v = k == 0 ? v * v : k == 1 ? fsqrt(v) : k == 2 ? v / 100 : -v;
        format_number(v, c->expr);
        c->evaluated = true;
        return;
    }
    if (len >= sizeof(c->expr) - 2) return;

    if (is_op(ch)) {
        c->evaluated = false;
        if (!len) { if (ch != '-') { kstrcpy(c->expr, "0"); len = 1; } }
        else if (is_op(c->expr[len - 1])) { c->expr[--len] = '\0'; }
        else if (c->expr[len - 1] == '(' && ch != '-') return;
    } else {
        if (c->evaluated) { c->expr[0] = '\0'; len = 0; }
        c->evaluated = false;
    }
    c->expr[len] = ch;
    c->expr[len + 1] = '\0';
}

/* ---- Layout -------------------------------------------------------------- */

#define ROWS 6
#define COLS 4
static const char *labels[ROWS][COLS] = {
    { "(",      ")",      "C",      "\x1b" },
    { "x\xfd",  "\xfb" "x", "%",    "\xf6" },
    { "7",      "8",      "9",      "x" },
    { "4",      "5",      "6",      "-" },
    { "1",      "2",      "3",      "+" },
    { "\xf1",   "0",      ".",      "=" },
};
static const char actions[ROWS][COLS] = {
    { '(', ')', 'C', '\b' },
    { 'q', 'r', '%', '/' },
    { '7', '8', '9', '*' },
    { '4', '5', '6', '-' },
    { '1', '2', '3', '+' },
    { 'n', '0', '.', '=' },
};

#define GRID_TOP 140

static void button_rect(int cw, int ch, int r, int c, int *x, int *y, int *w, int *h) {
    int bw = (cw - 12 - 3 * (COLS - 1)) / COLS;
    int bh = (ch - GRID_TOP - 6 - 3 * (ROWS - 1)) / ROWS;
    *x = 6 + c * (bw + 3);
    *y = GRID_TOP + r * (bh + 3);
    *w = bw; *h = bh;
}

static int button_at(window_t *win, int px, int py) {
    int cw = win->w - 2, ch = win->h - TITLE_H - 1;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int x, y, w, h;
            button_rect(cw, ch, r, c, &x, &y, &w, &h);
            if (ui_hit(px, py, x, y, w, h)) return r * COLS + c;
        }
    return -1;
}

/* Replace '*' and '/' with the display glyphs x and the CP437 division sign */
static void pretty(const char *src, char *out, int max) {
    int n = 0;
    for (int i = 0; src[i] && n < max - 1; i++)
        out[n++] = src[i] == '*' ? 'x' : src[i] == '/' ? (char)0xF6 : src[i];
    out[n] = '\0';
}

static void calc_paint(window_t *win, int x0, int y0, int cw, int ch) {
    calc_t *c = win->data;
    gfx_fill_rect(x0, y0, cw, ch, theme.dark ? 0x202020 : 0xF3F3F3);
    gfx_draw_text_bold(x0 + 14, y0 + 8, "Standard", theme.win_fg);

    char hist[64];
    pretty(c->history, hist, sizeof(hist));
    gfx_draw_text(x0 + cw - 14 - gfx_text_width(hist), y0 + 44, hist, theme.win_dim);

    char shown[48];
    pretty(c->expr[0] ? c->expr : "0", shown, sizeof(shown));
    int scale = 3;
    while (scale > 1 && gfx_text_width(shown) * scale > cw - 28) scale--;
    gfx_draw_text_scaled(x0 + cw - 14 - gfx_text_width(shown) * scale, y0 + 70 + (3 - scale) * 8, shown, theme.win_fg, scale);

    for (int r = 0; r < ROWS; r++)
        for (int col = 0; col < COLS; col++) {
            int x, y, w, h;
            button_rect(cw, ch, r, col, &x, &y, &w, &h);
            int id = r * COLS + col;
            char a = actions[r][col];
            bool digit = (a >= '0' && a <= '9') || a == '.' || a == 'n';
            bool eq = a == '=';
            uint32_t bg = eq ? theme.accent : digit ? (theme.dark ? 0x3B3B3B : 0xFFFFFF) : (theme.dark ? 0x323232 : 0xF9F9F9);
            if (id == c->pressed)    bg = eq ? gfx_darken(bg, 40) : gfx_darken(bg, 25);
            else if (id == c->hover) bg = eq ? gfx_lighten(bg, 30) : (theme.dark ? gfx_lighten(bg, 18) : 0xF0F0F0);
            gfx_fill_round_rect(x0 + x, y0 + y, w, h, 5, theme.dark ? 0x2A2A2A : 0xE5E5E5);
            gfx_fill_round_rect(x0 + x, y0 + y, w, h - 1, 5, bg);
            const char *l = labels[r][col];
            int tw = gfx_text_width(l);
            gfx_draw_text_bold(x0 + x + (w - tw) / 2, y0 + y + (h - FONT_H) / 2, l, eq ? 0xFFFFFF : theme.win_fg);
        }
}

static void calc_mouse(window_t *win, int ev, int x, int y, uint8_t buttons) {
    calc_t *c = win->data;
    int b = button_at(win, x, y);
    if (ev == EV_LEAVE) b = -1;
    if (ev == EV_DOWN && buttons == 1) { c->pressed = b; wm_invalidate(); }
    if (ev == EV_UP) {
        if (b >= 0 && b == c->pressed) calc_input(c, actions[b / COLS][b % COLS]);
        c->pressed = -1;
        wm_invalidate();
    }
    if (b != c->hover) { c->hover = b; wm_invalidate(); }
}

static void calc_key(window_t *win, char ch) {
    calc_t *c = win->data;
    if (ch == '\n') ch = '=';
    else if (ch == KEY_ESC) ch = 'C';
    else if (ch == 'x' || ch == 'X') ch = '*';
    else if (ch == KEY_DELETE) ch = 'C';
    if ((ch >= '0' && ch <= '9') || ch == '.' || is_op(ch) || ch == '(' || ch == ')' ||
        ch == '=' || ch == '\b' || ch == 'C' || ch == '%') {
        calc_input(c, ch);
        wm_invalidate();
    }
}

static void calc_close(window_t *win) { (void)win; }

void calc_open(void) {
    window_t *w = wm_find_app(APP_CALC);
    if (w) { wm_focus(w); return; }
    w = wm_create("Calculator", ICON_CALC, APP_CALC, 340, 540);
    if (!w) return;
    for (size_t i = 0; i < sizeof(calc_state); i++) ((uint8_t *)&calc_state)[i] = 0;
    calc_state.hover = calc_state.pressed = -1;
    w->data  = &calc_state;
    w->paint = calc_paint;
    w->mouse = calc_mouse;
    w->key   = calc_key;
    w->close = calc_close;
}
