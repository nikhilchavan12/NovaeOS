/* =============================================================================
 * NovaeOS - Shell Header (kernel/shell/shell.h)
 * ============================================================================= */

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>

/* Called for full-screen commands (calc, notepad...) when running inside the GUI.
 * Returns true if a desktop app was opened instead. */
typedef bool (*shell_gui_launcher_t)(const char *cmd, const char *arg);

void shell_run(void);
void shell_exec(const char *line);
void shell_set_gui_launcher(shell_gui_launcher_t launcher);

#endif /* SHELL_H */
