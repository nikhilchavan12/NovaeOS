/* =============================================================================
 * NovaeOS - PS/2 Keyboard Driver (kernel/drivers/keyboard.h)
 * ============================================================================= */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KB_BUFFER_SIZE 256

/* Special keys delivered through the character buffer */
#define KEY_SUPER  0x10
#define KEY_UP     0x11
#define KEY_DOWN   0x12
#define KEY_LEFT   0x13
#define KEY_RIGHT  0x14
#define KEY_HOME   0x15
#define KEY_END    0x16
#define KEY_ESC    0x1B
#define KEY_DELETE 0x7F

void keyboard_init(void);
bool keyboard_has_input(void);
char keyboard_getchar(void);
char keyboard_try_getchar(void);   /* 0 if no key is waiting */
void keyboard_read_line(char *buf, int max_len);

#endif /* KEYBOARD_H */
