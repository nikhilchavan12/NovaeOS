/* =============================================================================
 * NovaeOS - PS/2 Keyboard Driver (kernel/drivers/keyboard.c)
 * Handles IRQ1, scan code set 1, circular input buffer
 * ============================================================================= */

#include "keyboard.h"
#include "vga.h"
#include "../cpu/idt.h"
#include <stddef.h>

#define KB_DATA_PORT  0x60
#define KB_STATUS_PORT 0x64

/* Scan code set 1 -> ASCII lookup table */
static const char sc_ascii_normal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',  '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',  0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char sc_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',  0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* Circular key buffer */
static char   kb_buffer[KB_BUFFER_SIZE];
static size_t kb_head = 0;  /* Write index */
static size_t kb_tail = 0;  /* Read index */

static bool shift_pressed  = false;
static bool caps_lock      = false;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void kb_buffer_push(char c) {
    size_t next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {   /* Not full */
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

static char kb_buffer_pop(void) {
    if (kb_head == kb_tail) return 0;  /* Empty */
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(KB_DATA_PORT);

    if (scancode & 0x80) {
        /* Key release */
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36)   /* Shift */
            shift_pressed = false;
    } else {
        /* Key press */
        if (scancode == 0x2A || scancode == 0x36) {  /* Shift */
            shift_pressed = true;
        } else if (scancode == 0x3A) {                /* Caps Lock */
            caps_lock = !caps_lock;
        } else {
            char c = 0;
            bool upper = shift_pressed ^ caps_lock;
            if (upper && scancode < 128)
                c = sc_ascii_shift[scancode];
            else if (scancode < 128)
                c = sc_ascii_normal[scancode];

            if (c)
                kb_buffer_push(c);
        }
    }
}

void keyboard_init(void) {
    idt_register_handler(33, keyboard_callback);  /* IRQ1 = INT 33 */
}

bool keyboard_has_input(void) {
    return kb_head != kb_tail;
}

char keyboard_getchar(void) {
    while (!keyboard_has_input())
        __asm__ volatile ("hlt");
    return kb_buffer_pop();
}

/* Reads a line with backspace support and echo */
void keyboard_read_line(char *buf, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putchar('\b');
            }
        } else {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    buf[i] = '\0';
}
