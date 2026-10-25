/* =============================================================================
 * NovaeOS - Framebuffer text console (kernel/gfx/fbcon.h)
 * Shows the 80x25 VGA text console when the bootloader left a graphics mode
 * ============================================================================= */

#ifndef FBCON_H
#define FBCON_H

void fbcon_init(void);   /* requires gfx_init() to have succeeded */

#endif /* FBCON_H */
