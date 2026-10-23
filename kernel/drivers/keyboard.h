/* =============================================================================
 * NovaeOS - PS/2 Keyboard Driver (kernel/drivers/keyboard.h)
 * ============================================================================= */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KB_BUFFER_SIZE 256

void keyboard_init(void);
bool keyboard_has_input(void);
char keyboard_getchar(void);
void keyboard_read_line(char *buf, int max_len);

#endif /* KEYBOARD_H */
