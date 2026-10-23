/* =============================================================================
 * NovaeOS - Interactive Shell (kernel/shell/shell.c)
 * Full-featured command-line interface with all built-in apps
 * ============================================================================= */

#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../lib/kstring.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Theme Colors (glassmorphism-inspired terminal)
 * ============================================================================ */
#define COLOR_BG       VGA_COLOR_BLACK
#define COLOR_FG       VGA_COLOR_WHITE
#define COLOR_ACCENT   VGA_COLOR_LIGHT_CYAN
#define COLOR_PROMPT   VGA_COLOR_LIGHT_GREEN
#define COLOR_ERROR    VGA_COLOR_LIGHT_RED
#define COLOR_WARNING  VGA_COLOR_LIGHT_BROWN
#define COLOR_SUCCESS  VGA_COLOR_LIGHT_GREEN
#define COLOR_INFO     VGA_COLOR_LIGHT_BLUE
#define COLOR_HEADER   VGA_COLOR_LIGHT_MAGENTA
#define COLOR_DIM      VGA_COLOR_DARK_GREY
#define COLOR_YELLOW   VGA_COLOR_LIGHT_BROWN

/* Command history */
#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][256];
static int  history_count = 0;
static int  history_pos   = 0;

/* ============================================================================
 * Banner / Splash Screen
 * ============================================================================ */
static void print_banner(void) {
    vga_clear();

    /* Top decorative bar */
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    for (int i = 0; i < 80; i++) vga_putchar('=');

    /* ASCII Art Logo */
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("\n");
    vga_puts("     _   _                         ___  ____  \n");
    vga_puts("    | \\ | | _____   ____ _  ___   / _ \\/ ___| \n");
    vga_puts("    |  \\| |/ _ \\ \\ / / _` |/ _ \\ | | | \\___ \\ \n");
    vga_puts("    | |\\  | (_) \\ V / (_| |  __/ | |_| |___) |\n");
    vga_puts("    |_| \\_|\\___/ \\_/ \\__,_|\\___|  \\___/|____/ \n\n");

    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("                     32-bit x86 Operating System  v1.0.0\n");

    /* Bottom decorative bar */
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    for (int i = 0; i < 80; i++) vga_putchar('=');

    /* System info line */
    vga_set_color(COLOR_INFO, COLOR_BG);
    vga_puts("\n  Arch: x86 (i686)   Mode: Protected   Memory: ");
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    size_t free_mb = (pmm_get_free_blocks() * 4) / 1024;
    vga_printf("%u MB free", (uint32_t)free_mb);
    vga_set_color(COLOR_INFO, COLOR_BG);
    vga_puts("   FS: NovaeVFS\n");

    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("  Kernel loaded by GRUB Multiboot | Type 'help' to see commands\n");

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    for (int i = 0; i < 80; i++) vga_putchar('-');
    vga_puts("\n\n");

    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * Print the shell prompt
 * ============================================================================ */
static void print_prompt(void) {
    vga_set_color(COLOR_PROMPT, COLOR_BG);
    vga_puts("novae");
    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("@");
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("kernel");
    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts(":/$ ");
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * HELP command
 * ============================================================================ */
static void cmd_help(void) {
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  NovaeOS Built-in Commands\n");
    vga_set_color(COLOR_DIM, COLOR_BG);
    for (int i = 0; i < 30; i++) vga_puts("-");
    vga_puts("\n");

    struct { const char *cmd; const char *desc; } cmds[] = {
        /* System */
        {"help",        "Show this help message"},
        {"clear",       "Clear the screen"},
        {"reboot",      "Reboot the system"},
        {"shutdown",    "Power off the system"},
        {"uname",       "Print kernel information"},
        {"uptime",      "Show system uptime"},
        {"meminfo",     "Show memory statistics"},
        /* File system */
        {"ls",          "List files in filesystem"},
        {"cat <file>",  "Display file contents"},
        {"write <f>",   "Write text to a file"},
        {"touch <f>",   "Create an empty file"},
        {"rm <file>",   "Delete a file"},
        {"mv <old> <new>","Rename a file"},
        {"cp <src> <dst>","Copy a file"},
        /* Apps */
        {"calc",        "Interactive calculator"},
        {"notepad",     "Simple text editor"},
        {"clock",       "Show date/time clock"},
        {"calendar",    "Display ASCII calendar"},
        {"sysmon",      "System monitor dashboard"},
        {"settings",    "System settings panel"},
        {"fetch",       "System info (like neofetch)"},
        /* Fun */
        {"echo <text>", "Print text to screen"},
        {"banner <t>",  "Print big ASCII banner"},
        {"color <n>",   "Change terminal color scheme"},
        {"matrix",      "Matrix rain animation"},
        {"snake",       "Play snake game"},
        {NULL, NULL}
    };

    for (int i = 0; cmds[i].cmd; i++) {
        vga_set_color(COLOR_YELLOW, COLOR_BG);
        vga_printf("  %-22s", cmds[i].cmd);
        vga_set_color(COLOR_FG, COLOR_BG);
        vga_printf("%s\n", cmds[i].desc);
    }
    vga_set_color(COLOR_DIM, COLOR_BG);
    for (int i = 0; i < 30; i++) vga_puts("-");
    vga_puts("\n");
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * UNAME command
 * ============================================================================ */
static void cmd_uname(void) {
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("NovaeOS ");
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("1.0.0 #1 SMP x86 (i686) GNU/0\n");
    vga_puts("Built with GCC | NASM | GNU ld | GRUB Multiboot\n");
}

/* ============================================================================
 * UPTIME command
 * ============================================================================ */
static void cmd_uptime(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t secs  = ticks / 100;
    uint32_t mins  = secs  / 60;
    uint32_t hours = mins  / 60;
    secs  %= 60;
    mins  %= 60;
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("Uptime: ");
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("%u hours, %u minutes, %u seconds  (ticks: %u)\n",
               hours, mins, secs, ticks);
}

/* ============================================================================
 * MEMINFO command
 * ============================================================================ */
static void cmd_meminfo(void) {
    size_t total = pmm_get_total_blocks() * 4;
    size_t free  = pmm_get_free_blocks()  * 4;
    size_t used  = total - free;

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  Memory Information\n");
    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("  ----------------------------\n");

    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("  Total:    %u KB  (%u MB)\n", (uint32_t)total, (uint32_t)(total / 1024));
    vga_printf("  Used:     %u KB  (%u MB)\n", (uint32_t)used,  (uint32_t)(used  / 1024));

    vga_set_color(COLOR_SUCCESS, COLOR_BG);
    vga_printf("  Free:     %u KB  (%u MB)\n", (uint32_t)free,  (uint32_t)(free  / 1024));

    /* Usage bar */
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("  Usage:  [");
    int bar_width = 40;
    int filled    = total > 0 ? (int)(used * bar_width / total) : 0;
    vga_set_color(COLOR_ERROR, COLOR_BG);
    for (int i = 0; i < filled;      i++) vga_putchar('#');
    vga_set_color(COLOR_SUCCESS, COLOR_BG);
    for (int i = filled; i < bar_width; i++) vga_putchar('.');
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("]\n\n");
}

/* ============================================================================
 * LS command
 * ============================================================================ */
static void cmd_ls(void) {
    char names[VFS_MAX_FILES][VFS_MAX_FILENAME];
    vfs_type_t types[VFS_MAX_FILES];
    int count = 0;
    vfs_list(names, types, &count);

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  Filesystem Contents:\n");
    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("  Name                         Type    Size\n");
    vga_puts("  ---------------------------- ------- --------\n");

    for (int i = 0; i < count; i++) {
        vga_set_color(types[i] == VFS_TYPE_DIR ? COLOR_ACCENT : COLOR_FG, COLOR_BG);
        vga_printf("  %-28s ", names[i]);
        vga_set_color(COLOR_DIM, COLOR_BG);
        vga_puts(types[i] == VFS_TYPE_DIR ? "[dir]  " : "[file] ");
        vga_set_color(COLOR_INFO, COLOR_BG);
        vga_printf("%u B\n", (uint32_t)vfs_get_size(names[i]));
    }

    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_printf("\n  %d item(s)\n\n", count);
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * CAT command
 * ============================================================================ */
static void cmd_cat(const char *filename) {
    if (!filename || filename[0] == '\0') {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: cat <filename>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }

    char buf[VFS_MAX_FILE_SIZE + 1];
    int result = vfs_read(filename, buf, sizeof(buf));
    if (result < 0) {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_printf("cat: %s: No such file\n", filename);
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }

    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_printf("-- %s (%d bytes) --\n", filename, result);
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts(buf);
    if (result > 0 && buf[result - 1] != '\n') vga_puts("\n");
}

/* ============================================================================
 * WRITE command - write text to file
 * ============================================================================ */
static void cmd_write(const char *filename) {
    if (!filename || filename[0] == '\0') {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: write <filename>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    if (!vfs_exists(filename)) {
        vfs_create(filename, VFS_TYPE_FILE);
    }

    vga_set_color(COLOR_INFO, COLOR_BG);
    vga_printf("Writing to '%s'. Enter text (type '.done' on a new line to finish):\n", filename);
    vga_set_color(COLOR_FG, COLOR_BG);

    /* Clear existing content */
    vfs_write(filename, "", 1);

    char line[256];
    while (1) {
        vga_set_color(COLOR_DIM, COLOR_BG);
        vga_puts("  > ");
        vga_set_color(COLOR_FG, COLOR_BG);
        keyboard_read_line(line, sizeof(line));
        if (kstrcmp(line, ".done") == 0) break;
        vfs_append(filename, line, kstrlen(line));
        vfs_append(filename, "\n", 1);
    }
    vga_set_color(COLOR_SUCCESS, COLOR_BG);
    vga_printf("Saved %u bytes to '%s'\n", (uint32_t)vfs_get_size(filename), filename);
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * TOUCH command
 * ============================================================================ */
static void cmd_touch(const char *filename) {
    if (!filename || filename[0] == '\0') {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: touch <filename>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    if (vfs_exists(filename)) {
        vga_set_color(COLOR_WARNING, COLOR_BG);
        vga_printf("touch: '%s' already exists\n", filename);
    } else {
        vfs_create(filename, VFS_TYPE_FILE);
        vga_set_color(COLOR_SUCCESS, COLOR_BG);
        vga_printf("Created '%s'\n", filename);
    }
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * RM command
 * ============================================================================ */
static void cmd_rm(const char *filename) {
    if (!filename || filename[0] == '\0') {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: rm <filename>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    if (vfs_delete(filename) == 0) {
        vga_set_color(COLOR_SUCCESS, COLOR_BG);
        vga_printf("Deleted '%s'\n", filename);
    } else {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_printf("rm: '%s': No such file\n", filename);
    }
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * MV command
 * ============================================================================ */
static void cmd_mv(const char *src, const char *dst) {
    if (!src || !dst) {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: mv <source> <destination>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    if (vfs_rename(src, dst) == 0) {
        vga_set_color(COLOR_SUCCESS, COLOR_BG);
        vga_printf("Renamed '%s' -> '%s'\n", src, dst);
    } else {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_printf("mv: failed (file not found or name conflict)\n");
    }
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * CP command
 * ============================================================================ */
static void cmd_cp(const char *src, const char *dst) {
    if (!src || !dst) {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: cp <source> <destination>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    char buf[VFS_MAX_FILE_SIZE + 1];
    int n = vfs_read(src, buf, sizeof(buf));
    if (n < 0) {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_printf("cp: '%s': No such file\n", src);
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    if (!vfs_exists(dst)) vfs_create(dst, VFS_TYPE_FILE);
    vfs_write(dst, buf, (size_t)n);
    vga_set_color(COLOR_SUCCESS, COLOR_BG);
    vga_printf("Copied '%s' -> '%s' (%d bytes)\n", src, dst, n);
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * ECHO command
 * ============================================================================ */
static void cmd_echo(const char *text) {
    if (text && text[0]) {
        vga_puts(text);
        vga_puts("\n");
    } else {
        vga_puts("\n");
    }
}

/* ============================================================================
 * FETCH command (like neofetch)
 * ============================================================================ */
static void cmd_fetch(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t secs  = ticks / 100;
    size_t free_mb = (pmm_get_free_blocks() * 4) / 1024;
    size_t total_mb= (pmm_get_total_blocks()* 4) / 1024;

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n   ____  _  ________  \n");
    vga_puts("  |    \\| |/  ______| \n");
    vga_puts("  |     \\ | |  ___   \n");
    vga_puts("  |  |\\  | |  |___| \n");
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("  |__| \\___|_______| \n\n");

    struct { const char *label; } info_labels[] = {
        {"OS"}, {"Kernel"}, {"Arch"}, {"Shell"}, {"Memory"},
        {"Uptime"}, {"FS"}, {"CPU"}, {NULL}
    };
    const char *info_vals[] = {
        "NovaeOS 1.0.0",
        "NovaeKernel v1.0 (Multiboot)",
        "x86 (i686) Protected Mode",
        "NovaeShell 1.0",
        NULL, /* formatted below */
        NULL,
        "NovaeVFS (in-memory)",
        "QEMU i386 / Real x86",
        NULL
    };

    char mem_buf[64];
    char up_buf[64];

    /* Format memory */
    kitoa((int)((total_mb - free_mb)), mem_buf, 10);
    kstrcat(mem_buf, " MB / ");
    char tmp[16]; kitoa((int)total_mb, tmp, 10);
    kstrcat(mem_buf, tmp); kstrcat(mem_buf, " MB");
    info_vals[4] = mem_buf;

    /* Format uptime */
    kitoa((int)(secs / 60), up_buf, 10);
    kstrcat(up_buf, " min "); kitoa((int)(secs % 60), tmp, 10);
    kstrcat(up_buf, tmp); kstrcat(up_buf, " sec");
    info_vals[5] = up_buf;

    for (int i = 0; info_labels[i].label; i++) {
        vga_set_color(COLOR_ACCENT, COLOR_BG);
        vga_printf("  %-10s", info_labels[i].label);
        vga_set_color(COLOR_DIM, COLOR_BG);
        vga_puts(": ");
        vga_set_color(COLOR_FG, COLOR_BG);
        if (info_vals[i]) vga_puts(info_vals[i]);
        vga_puts("\n");
    }

    /* Color palette */
    vga_puts("\n  ");
    for (int c = 0; c < 16; c++) {
        vga_set_color((vga_color_t)c, (vga_color_t)c);
        vga_puts("  ");
    }
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("\n\n");
}

/* ============================================================================
 * CALCULATOR app
 * ============================================================================ */
static int safe_div(int a, int b) { return b ? a / b : 0; }
static int safe_mod(int a, int b) { return b ? a % b : 0; }

static void cmd_calc(void) {
    vga_clear();
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  ╔══════════════════════════════╗\n");
    vga_puts("  ║      NovaeOS Calculator      ║\n");
    vga_puts("  ║  Operators: + - * / % ( )   ║\n");
    vga_puts("  ║  Type 'exit' to quit         ║\n");
    vga_puts("  ╚══════════════════════════════╝\n\n");

    char expr[128];
    while (1) {
        vga_set_color(COLOR_PROMPT, COLOR_BG);
        vga_puts("  calc> ");
        vga_set_color(COLOR_FG, COLOR_BG);
        keyboard_read_line(expr, sizeof(expr));

        if (kstrcmp(expr, "exit") == 0 || kstrcmp(expr, "quit") == 0) break;
        if (expr[0] == '\0') continue;

        /* Simple expression evaluator: one operator */
        char *p = expr;
        while (*p == ' ') p++;

        /* Find operator */
        char *op_pos = NULL;
        char op = 0;
        /* Search right-to-left to handle operator order */
        for (char *q = p + kstrlen(p) - 1; q >= p + 1; q--) {
            if (*q == '+' || *q == '-') { op_pos = q; op = *q; break; }
        }
        if (!op_pos) {
            for (char *q = p + kstrlen(p) - 1; q >= p + 1; q--) {
                if (*q == '*' || *q == '/' || *q == '%') {
                    op_pos = q; op = *q; break;
                }
            }
        }

        if (!op_pos) {
            /* Just a number */
            vga_set_color(COLOR_INFO, COLOR_BG);
            vga_printf("  = %d\n", katoi(p));
            vga_set_color(COLOR_FG, COLOR_BG);
            continue;
        }

        *op_pos = '\0';
        int a = katoi(p);
        int b = katoi(op_pos + 1);
        int result = 0;

        switch (op) {
            case '+': result = a + b; break;
            case '-': result = a - b; break;
            case '*': result = a * b; break;
            case '/': result = safe_div(a, b); break;
            case '%': result = safe_mod(a, b); break;
        }

        vga_set_color(COLOR_SUCCESS, COLOR_BG);
        vga_printf("  = %d\n", result);
        vga_set_color(COLOR_FG, COLOR_BG);
    }
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("\n");
}

/* ============================================================================
 * NOTEPAD app
 * ============================================================================ */
static void cmd_notepad(void) {
    vga_clear();
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  ╔══════════════════════════════════════════╗\n");
    vga_puts("  ║           NovaeOS Notepad               ║\n");
    vga_puts("  ╠══════════════════════════════════════════╣\n");
    vga_puts("  ║  :new <name>  Create/open file           ║\n");
    vga_puts("  ║  :save        Save current file          ║\n");
    vga_puts("  ║  :load <name> Load a file                ║\n");
    vga_puts("  ║  :list        List all files             ║\n");
    vga_puts("  ║  :quit        Exit notepad               ║\n");
    vga_puts("  ╚══════════════════════════════════════════╝\n\n");

    static char notepad_buf[VFS_MAX_FILE_SIZE];
    static char current_file[64] = "";
    int buf_len = 0;
    char line[256];

    while (1) {
        vga_set_color(COLOR_PROMPT, COLOR_BG);
        if (current_file[0])
            vga_printf("  [%s]> ", current_file);
        else
            vga_puts("  [no file]> ");
        vga_set_color(COLOR_FG, COLOR_BG);
        keyboard_read_line(line, sizeof(line));

        if (kstrcmp(line, ":quit") == 0 || kstrcmp(line, ":q") == 0) break;

        if (kstarts_with(line, ":new ")) {
            kstrncpy(current_file, line + 5, 64);
            buf_len = 0;
            notepad_buf[0] = '\0';
            vga_set_color(COLOR_INFO, COLOR_BG);
            vga_printf("  Opened new file: %s\n", current_file);
            vga_set_color(COLOR_FG, COLOR_BG);
        } else if (kstrcmp(line, ":save") == 0) {
            if (current_file[0] == '\0') {
                vga_set_color(COLOR_ERROR, COLOR_BG);
                vga_puts("  No file open. Use ':new <name>'\n");
            } else {
                if (!vfs_exists(current_file)) vfs_create(current_file, VFS_TYPE_FILE);
                vfs_write(current_file, notepad_buf, (size_t)buf_len);
                vga_set_color(COLOR_SUCCESS, COLOR_BG);
                vga_printf("  Saved %d bytes to '%s'\n", buf_len, current_file);
            }
            vga_set_color(COLOR_FG, COLOR_BG);
        } else if (kstarts_with(line, ":load ")) {
            const char *fname = line + 6;
            int n = vfs_read(fname, notepad_buf, sizeof(notepad_buf));
            if (n < 0) {
                vga_set_color(COLOR_ERROR, COLOR_BG);
                vga_printf("  File not found: %s\n", fname);
            } else {
                buf_len = n;
                kstrncpy(current_file, fname, 64);
                vga_set_color(COLOR_INFO, COLOR_BG);
                vga_printf("  Loaded %d bytes from '%s'\n", n, fname);
                vga_set_color(COLOR_FG, COLOR_BG);
                vga_puts("  Content:\n");
                vga_puts(notepad_buf);
                vga_puts("\n");
            }
            vga_set_color(COLOR_FG, COLOR_BG);
        } else if (kstrcmp(line, ":list") == 0) {
            cmd_ls();
        } else {
            /* Append text line */
            size_t ll = kstrlen(line);
            if (buf_len + (int)ll + 1 < VFS_MAX_FILE_SIZE) {
                kmemcpy(notepad_buf + buf_len, line, ll);
                buf_len += (int)ll;
                notepad_buf[buf_len++] = '\n';
                notepad_buf[buf_len]   = '\0';
            }
        }
    }
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("\n");
}

/* ============================================================================
 * CLOCK command
 * ============================================================================ */
static void cmd_clock(void) {
    vga_clear();
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n");

    /* Display ASCII clock art */
    uint32_t ticks = timer_get_ticks();
    uint32_t total_secs = ticks / 100;
    uint32_t h = (total_secs / 3600) % 24;
    uint32_t m = (total_secs / 60) % 60;
    uint32_t s = total_secs % 60;

    vga_puts("  ╔════════════════════════╗\n");
    vga_puts("  ║      NovaeOS Clock     ║\n");
    vga_puts("  ╠════════════════════════╣\n");
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_printf("  ║      %02u : %02u : %02u      ║\n", h, m, s);
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╠════════════════════════╣\n");
    vga_set_color(COLOR_FG, COLOR_BG);

    /* Date (approximate from boot ticks - in a real OS we'd use RTC) */
    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("  ║  (time since boot)     ║\n");
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╚════════════════════════╝\n\n");

    vga_set_color(COLOR_INFO, COLOR_BG);
    vga_printf("  Uptime: %u seconds (%u minutes)\n", total_secs, total_secs / 60);
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * CALENDAR command
 * ============================================================================ */
static void cmd_calendar(void) {
    vga_clear();
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n");
    vga_puts("  ╔══════════════════════════════════╗\n");
    vga_puts("  ║         October  2026            ║\n");
    vga_puts("  ╠══════════════════════════════════╣\n");
    vga_puts("  ║  Sun  Mon  Tue  Wed  Thu  Fri  Sat ║\n");
    vga_puts("  ╠══════════════════════════════════╣\n");

    /* October 2026 starts on Thursday */
    const int start_day = 4;  /* 0=Sun, 4=Thu */
    int day = 1;
    int days_in_month = 31;

    for (int row = 0; row < 6; row++) {
        vga_puts("  ║ ");
        for (int col = 0; col < 7; col++) {
            int cell = row * 7 + col;
            if (cell < start_day || day > days_in_month) {
                vga_set_color(COLOR_DIM, COLOR_BG);
                vga_puts("     ");
            } else {
                if (day == 23) {  /* Today! */
                    vga_set_color(COLOR_HEADER, COLOR_BG);
                    vga_printf(" [%2d]", day);
                } else if (col == 0 || col == 6) {
                    vga_set_color(COLOR_ERROR, COLOR_BG);
                    vga_printf("  %2d ", day);
                } else {
                    vga_set_color(COLOR_FG, COLOR_BG);
                    vga_printf("  %2d ", day);
                }
                day++;
            }
        }
        vga_set_color(COLOR_ACCENT, COLOR_BG);
        vga_puts("║\n");
    }
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╚══════════════════════════════════╝\n");
    vga_set_color(COLOR_INFO, COLOR_BG);
    vga_puts("  [Today is highlighted]\n\n");
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * SYSMON command
 * ============================================================================ */
static void cmd_sysmon(void) {
    vga_clear();
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n");

    size_t free_b  = pmm_get_free_blocks()  * 4;
    size_t total_b = pmm_get_total_blocks() * 4;
    size_t used_b  = total_b - free_b;
    uint32_t ticks = timer_get_ticks();
    uint32_t secs  = ticks / 100;

    char names[VFS_MAX_FILES][VFS_MAX_FILENAME];
    vfs_type_t types[VFS_MAX_FILES];
    int fcount = 0;
    vfs_list(names, types, &fcount);

    vga_puts("  ╔══════════════════════════════════════════════════════════════════════════╗\n");
    vga_puts("  ║                      NovaeOS System Monitor                             ║\n");
    vga_puts("  ╠══════════════════════════════════════════════════════════════════════════╣\n");

    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("  ║  Uptime:    %u seconds                                                   ║\n", secs);
    vga_printf("  ║  Ticks:     %u (100 Hz timer)                                           ║\n", ticks);

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╠══════════════════════════════════════════════════════════════════════════╣\n");

    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("  ║  MEMORY                                                                  ║\n");
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("  ║  Total: %-6u KB   Used: %-6u KB   Free: %-6u KB                     ║\n",
               (uint32_t)total_b, (uint32_t)used_b, (uint32_t)free_b);

    vga_puts("  ║  [");
    int bw = 60;
    int filled = total_b > 0 ? (int)(used_b * bw / total_b) : 0;
    vga_set_color(VGA_COLOR_LIGHT_RED, COLOR_BG);
    for (int i = 0; i < filled; i++) vga_putchar('|');
    vga_set_color(VGA_COLOR_DARK_GREY, COLOR_BG);
    for (int i = filled; i < bw; i++) vga_putchar('.');
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("]  %u%%\n", (uint32_t)(total_b > 0 ? used_b * 100 / total_b : 0));

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╠══════════════════════════════════════════════════════════════════════════╣\n");
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("  ║  FILESYSTEM                                                              ║\n");
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("  ║  Files: %d   Type: NovaeVFS (in-memory, flat)                           ║\n", fcount);

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╠══════════════════════════════════════════════════════════════════════════╣\n");
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("  ║  HARDWARE                                                                ║\n");
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_puts("  ║  CPU:    i686 Protected Mode (32-bit)                                    ║\n");
    vga_puts("  ║  Timer:  PIT 8253/8254 @ 100 Hz                                         ║\n");
    vga_puts("  ║  KB:     PS/2 Keyboard (Scan Set 1)                                     ║\n");
    vga_puts("  ║  VGA:    Text Mode 80x25, 16 colors                                     ║\n");
    vga_puts("  ║  GDT:    5 descriptors (null, kcode, kdata, ucode, udata)               ║\n");
    vga_puts("  ║  IDT:    256 entries (32 exceptions + 16 IRQs + 208 user)               ║\n");

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("  ╚══════════════════════════════════════════════════════════════════════════╝\n\n");
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * SETTINGS command
 * ============================================================================ */
static void cmd_settings(void) {
    vga_clear();
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  ╔══════════════════════════════════════╗\n");
    vga_puts("  ║         NovaeOS Settings             ║\n");
    vga_puts("  ╠══════════════════════════════════════╣\n");
    vga_puts("  ║  1. Change color scheme              ║\n");
    vga_puts("  ║  2. System information               ║\n");
    vga_puts("  ║  3. Memory diagnostics               ║\n");
    vga_puts("  ║  4. Filesystem info                  ║\n");
    vga_puts("  ║  5. About NovaeOS                    ║\n");
    vga_puts("  ║  0. Back to shell                    ║\n");
    vga_puts("  ╚══════════════════════════════════════╝\n\n");

    char choice[8];
    vga_set_color(COLOR_PROMPT, COLOR_BG);
    vga_puts("  Choice: ");
    vga_set_color(COLOR_FG, COLOR_BG);
    keyboard_read_line(choice, sizeof(choice));

    switch (choice[0]) {
        case '1':
            vga_puts("\n  Color schemes: 1=Default 2=Ocean 3=Fire 4=Hacker\n");
            vga_puts("  (Type 'color <n>' in the shell to apply)\n");
            break;
        case '2':
            cmd_uname();
            break;
        case '3':
            cmd_meminfo();
            break;
        case '4':
            cmd_ls();
            break;
        case '5':
            vga_set_color(COLOR_HEADER, COLOR_BG);
            vga_puts("\n  NovaeOS v1.0.0\n");
            vga_set_color(COLOR_FG, COLOR_BG);
            vga_puts("  A 32-bit x86 operating system kernel\n");
            vga_puts("  Written in C and NASM assembly\n");
            vga_puts("  Features: GDT, IDT, PIC, PIT, PS/2 KB,\n");
            vga_puts("            VMM, Heap Allocator, VFS, Shell\n");
            break;
        default:
            break;
    }
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * COLOR command - change color scheme
 * ============================================================================ */
static void cmd_color(const char *scheme) {
    vga_clear();
    if (!scheme || scheme[0] == '\0' || kstrcmp(scheme, "1") == 0) {
        /* Default */
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_puts("Applied default color scheme.\n");
    } else if (kstrcmp(scheme, "2") == 0) {
        /* Ocean */
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE);
        vga_puts("Applied ocean color scheme.\n");
    } else if (kstrcmp(scheme, "3") == 0) {
        /* Fire */
        vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_RED);
        vga_puts("Applied fire color scheme.\n");
    } else if (kstrcmp(scheme, "4") == 0) {
        /* Hacker */
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Applied hacker green scheme.\n");
    }
}

/* ============================================================================
 * BANNER command - big text
 * ============================================================================ */
static void cmd_banner(const char *text) {
    if (!text || text[0] == '\0') {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_puts("Usage: banner <text>\n");
        vga_set_color(COLOR_FG, COLOR_BG);
        return;
    }
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("\n  *** ");
    vga_set_color(COLOR_ACCENT, COLOR_BG);
    for (const char *p = text; *p; p++) {
        vga_putchar(*p);
        vga_putchar(' ');
    }
    vga_set_color(COLOR_HEADER, COLOR_BG);
    vga_puts("***\n\n");
    vga_set_color(COLOR_FG, COLOR_BG);
}

/* ============================================================================
 * MATRIX animation
 * ============================================================================ */
static uint32_t rand_seed = 0x12345678;
static uint32_t next_rand(void) {
    rand_seed ^= rand_seed << 13;
    rand_seed ^= rand_seed >> 17;
    rand_seed ^= rand_seed << 5;
    return rand_seed;
}

static void cmd_matrix(void) {
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("  [Matrix Rain - press any key to stop]\n\n");

    /* Simple matrix: fill screen with random chars */
    for (int frame = 0; frame < 200; frame++) {
        for (int row = 1; row < VGA_HEIGHT; row++) {
            for (int col = 0; col < VGA_WIDTH; col++) {
                uint32_t r = next_rand();
                /* Gradient: bright green for 'rain head' */
                vga_color_t fg = (r % 10 == 0) ? VGA_COLOR_WHITE :
                                 (r % 3  == 0) ? VGA_COLOR_LIGHT_GREEN :
                                                  VGA_COLOR_GREEN;
                char c = (r % 10 < 1) ? ' ' : (char)('!' + (r % 93));
                vga_putchar_at(c, fg, VGA_COLOR_BLACK, (size_t)col, (size_t)row);
            }
        }
        /* Brief delay */
        for (volatile int d = 0; d < 500000; d++);

        /* Check for keypress */
        if (keyboard_has_input()) {
            keyboard_getchar();
            break;
        }
    }
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_clear();
    vga_puts("\n  [Exited Matrix]\n");
}

/* ============================================================================
 * SNAKE game
 * ============================================================================ */
#define SNAKE_FIELD_W  60
#define SNAKE_FIELD_H  18
#define SNAKE_MAX_LEN  200

typedef struct { int x; int y; } Point;

static void cmd_snake(void) {
    vga_clear();

    vga_set_color(COLOR_ACCENT, COLOR_BG);
    vga_puts("\n  SNAKE GAME\n");
    vga_set_color(COLOR_DIM, COLOR_BG);
    vga_puts("  Controls: W=Up  A=Left  S=Down  D=Right  Q=Quit\n\n");

    Point snake[SNAKE_MAX_LEN];
    int   snake_len = 3;
    int   dx = 1, dy = 0;
    Point food;

    /* Init snake in middle */
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = 20 - i;
        snake[i].y = 10;
    }

    /* Random food position */
    food.x = (int)(next_rand() % (SNAKE_FIELD_W - 2)) + 1;
    food.y = (int)(next_rand() % (SNAKE_FIELD_H - 2)) + 1;

    int score = 0;
    bool running = true;

    while (running) {
        /* Draw border */
        int off_x = 8, off_y = 3;
        vga_set_color(COLOR_ACCENT, COLOR_BG);
        for (int x = 0; x < SNAKE_FIELD_W + 2; x++) {
            vga_putchar_at('-', COLOR_ACCENT, COLOR_BG, (size_t)(off_x + x), (size_t)(off_y - 1));
            vga_putchar_at('-', COLOR_ACCENT, COLOR_BG, (size_t)(off_x + x), (size_t)(off_y + SNAKE_FIELD_H));
        }
        for (int y = 0; y < SNAKE_FIELD_H; y++) {
            vga_putchar_at('|', COLOR_ACCENT, COLOR_BG, (size_t)(off_x - 1),              (size_t)(off_y + y));
            vga_putchar_at('|', COLOR_ACCENT, COLOR_BG, (size_t)(off_x + SNAKE_FIELD_W), (size_t)(off_y + y));
        }

        /* Clear interior */
        for (int y = 0; y < SNAKE_FIELD_H; y++)
            for (int x = 0; x < SNAKE_FIELD_W; x++)
                vga_putchar_at(' ', COLOR_FG, COLOR_BG, (size_t)(off_x + x), (size_t)(off_y + y));

        /* Draw food */
        vga_putchar_at('*', VGA_COLOR_LIGHT_RED, COLOR_BG, (size_t)(off_x + food.x), (size_t)(off_y + food.y));

        /* Draw snake */
        for (int i = 0; i < snake_len; i++) {
            char ch = (i == 0) ? '@' : '#';
            vga_color_t c = (i == 0) ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_GREEN;
            vga_putchar_at(ch, c, COLOR_BG, (size_t)(off_x + snake[i].x), (size_t)(off_y + snake[i].y));
        }

        /* Score */
        vga_set_color(COLOR_INFO, COLOR_BG);
        vga_set_cursor(8, 1);
        vga_printf("Score: %d   Length: %d   (WASD to move, Q to quit)", score, snake_len);

        /* Wait for input or auto-step */
        char key = 0;
        for (volatile int d = 0; d < 2000000; d++) {
            if (keyboard_has_input()) { key = keyboard_getchar(); break; }
        }

        /* Handle input */
        if (key == 'q' || key == 'Q') { running = false; break; }
        if ((key == 'w' || key == 'W') && dy == 0) { dx = 0;  dy = -1; }
        if ((key == 's' || key == 'S') && dy == 0) { dx = 0;  dy =  1; }
        if ((key == 'a' || key == 'A') && dx == 0) { dx = -1; dy =  0; }
        if ((key == 'd' || key == 'D') && dx == 0) { dx =  1; dy =  0; }

        /* Move snake */
        int nx = snake[0].x + dx;
        int ny = snake[0].y + dy;

        /* Check wall collision */
        if (nx <= 0 || nx >= SNAKE_FIELD_W - 1 || ny <= 0 || ny >= SNAKE_FIELD_H - 1) {
            running = false; break;
        }

        /* Check self collision */
        for (int i = 1; i < snake_len; i++) {
            if (snake[i].x == nx && snake[i].y == ny) { running = false; break; }
        }
        if (!running) break;

        /* Check food */
        bool ate = (nx == food.x && ny == food.y);
        if (ate) {
            score += 10;
            food.x = (int)(next_rand() % (SNAKE_FIELD_W - 2)) + 1;
            food.y = (int)(next_rand() % (SNAKE_FIELD_H - 2)) + 1;
            if (snake_len < SNAKE_MAX_LEN) snake_len++;
        }

        /* Shift body */
        for (int i = snake_len - 1; i > 0; i--) snake[i] = snake[i - 1];
        snake[0].x = nx;
        snake[0].y = ny;
    }

    vga_clear();
    vga_set_color(COLOR_ERROR, COLOR_BG);
    vga_puts("\n  GAME OVER!\n");
    vga_set_color(COLOR_FG, COLOR_BG);
    vga_printf("  Final Score: %d   Snake Length: %d\n\n", score, snake_len);
}

/* ============================================================================
 * REBOOT / SHUTDOWN
 * ============================================================================ */
static void cmd_reboot(void) {
    vga_set_color(COLOR_WARNING, COLOR_BG);
    vga_puts("Rebooting system...\n");
    /* Pulse keyboard controller reset line */
    __asm__ volatile (
        "1: inb $0x64, %%al\n"
        "   testb $2, %%al\n"
        "   jnz 1b\n"
        "   movb $0xFE, %%al\n"
        "   outb %%al, $0x64\n"
        : : : "eax"
    );
    __asm__ volatile ("hlt");
}

static void cmd_shutdown(void) {
    vga_set_color(COLOR_WARNING, COLOR_BG);
    vga_puts("Shutting down... (ACPI S5 / QEMU poweroff)\n");
    /* ACPI shutdown (works in QEMU with ACPI) */
    __asm__ volatile (
        "movw $0x2000, %%ax\n"
        "movw $0x604,  %%dx\n"
        "outw %%ax, %%dx\n"
        : : : "eax", "edx"
    );
    /* Fallback: halt */
    __asm__ volatile ("cli; hlt");
}

/* ============================================================================
 * Command history management
 * ============================================================================ */
static void history_push(const char *cmd) {
    if (cmd[0] == '\0') return;
    /* Don't add duplicate of last command */
    if (history_count > 0 && kstrcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0)
        return;
    kstrncpy(history[history_count % HISTORY_SIZE], cmd, 255);
    history_count++;
    history_pos = history_count;
}

/* ============================================================================
 * Command dispatch
 * ============================================================================ */
static void dispatch(char *line) {
    /* Tokenize: command + args */
    char *saveptr = NULL;
    char *cmd     = kstrtok(line, " \t", &saveptr);
    if (!cmd) return;

    /* Rest of line after command */
    const char *rest = saveptr ? saveptr : "";

    /* Two-word args */
    char *arg1 = kstrtok(NULL, " \t", &saveptr);
    char *arg2 = kstrtok(NULL, " \t", &saveptr);

    if (kstrcmp(cmd, "help")     == 0) cmd_help();
    else if (kstrcmp(cmd, "clear")    == 0) vga_clear();
    else if (kstrcmp(cmd, "cls")      == 0) vga_clear();
    else if (kstrcmp(cmd, "reboot")   == 0) cmd_reboot();
    else if (kstrcmp(cmd, "shutdown") == 0) cmd_shutdown();
    else if (kstrcmp(cmd, "poweroff") == 0) cmd_shutdown();
    else if (kstrcmp(cmd, "uname")    == 0) cmd_uname();
    else if (kstrcmp(cmd, "uptime")   == 0) cmd_uptime();
    else if (kstrcmp(cmd, "meminfo")  == 0) cmd_meminfo();
    else if (kstrcmp(cmd, "ls")       == 0) cmd_ls();
    else if (kstrcmp(cmd, "dir")      == 0) cmd_ls();
    else if (kstrcmp(cmd, "cat")      == 0) cmd_cat(arg1);
    else if (kstrcmp(cmd, "type")     == 0) cmd_cat(arg1);
    else if (kstrcmp(cmd, "write")    == 0) cmd_write(arg1);
    else if (kstrcmp(cmd, "touch")    == 0) cmd_touch(arg1);
    else if (kstrcmp(cmd, "rm")       == 0) cmd_rm(arg1);
    else if (kstrcmp(cmd, "del")      == 0) cmd_rm(arg1);
    else if (kstrcmp(cmd, "mv")       == 0) cmd_mv(arg1, arg2);
    else if (kstrcmp(cmd, "cp")       == 0) cmd_cp(arg1, arg2);
    else if (kstrcmp(cmd, "echo")     == 0) cmd_echo(rest);
    else if (kstrcmp(cmd, "calc")     == 0) cmd_calc();
    else if (kstrcmp(cmd, "notepad")  == 0) cmd_notepad();
    else if (kstrcmp(cmd, "clock")    == 0) cmd_clock();
    else if (kstrcmp(cmd, "cal")      == 0) cmd_calendar();
    else if (kstrcmp(cmd, "calendar") == 0) cmd_calendar();
    else if (kstrcmp(cmd, "sysmon")   == 0) cmd_sysmon();
    else if (kstrcmp(cmd, "top")      == 0) cmd_sysmon();
    else if (kstrcmp(cmd, "settings") == 0) cmd_settings();
    else if (kstrcmp(cmd, "fetch")    == 0) cmd_fetch();
    else if (kstrcmp(cmd, "neofetch") == 0) cmd_fetch();
    else if (kstrcmp(cmd, "color")    == 0) cmd_color(arg1);
    else if (kstrcmp(cmd, "banner")   == 0) cmd_banner(rest);
    else if (kstrcmp(cmd, "matrix")   == 0) cmd_matrix();
    else if (kstrcmp(cmd, "snake")    == 0) cmd_snake();
    else {
        vga_set_color(COLOR_ERROR, COLOR_BG);
        vga_printf("novae: command not found: %s\n", cmd);
        vga_set_color(COLOR_DIM, COLOR_BG);
        vga_puts("Type 'help' to see available commands.\n");
        vga_set_color(COLOR_FG, COLOR_BG);
    }
}

/* ============================================================================
 * Shell main loop
 * ============================================================================ */
void shell_run(void) {
    print_banner();

    /* Show MOTD */
    char motd[VFS_MAX_FILE_SIZE];
    if (vfs_read("motd.txt", motd, sizeof(motd)) > 0) {
        vga_set_color(COLOR_DIM, COLOR_BG);
        vga_puts(motd);
        vga_set_color(COLOR_FG, COLOR_BG);
    }

    char line[256];

    while (1) {
        print_prompt();
        keyboard_read_line(line, sizeof(line));
        history_push(line);
        dispatch(line);
    }
}
