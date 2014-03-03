/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "arch.h"

#define VGA_VRAM_TEXTMODE       0x000b8000
#define VGA_TEXTMODE_X          80
#define VGA_TEXTMODE_Y          25
#define VGA_TEXTMODE_XY         (VGA_TEXTMODE_X * VGA_TEXTMODE_Y)

static int lock;
static int vga_cursor;
static unsigned char vga_text[VGA_TEXTMODE_XY];

/*
 * Initialize VGA display
 */
void
vga_init(void)
{
    lock = 0;
    vga_cursor = 0;
}

/*
 * Update cursor
 */
void
vga_update_cursor(void)
{
    u16 val;
    u16 addr = 0x3d4;

    /* Low */
    val = ((vga_cursor & 0xff) << 8) | 0x0f;
    outw(addr, val);
    /* High */
    val = (((vga_cursor >> 8) & 0xff) << 8) | 0x0e;
    outw(addr, val);
}

/*
 * Put a character to VGA display
 */
void
vga_putc(int c)
{
    int i;
    u64 addr;
    u16 *ptr;

    spin_lock(&lock);

    addr = VGA_VRAM_TEXTMODE;

    if ( '\r' == c ) {
        /* Carriage return */
        vga_cursor = vga_cursor / VGA_TEXTMODE_X * VGA_TEXTMODE_X;
    } else if ( '\n' == c ) {
        /* New line */
        vga_cursor = vga_cursor + VGA_TEXTMODE_X;
    } else if ( 0x08 == c ) {
        /* Backspace */
        if ( vga_cursor > 0 ) {
            vga_cursor = vga_cursor - 1;
            vga_text[vga_cursor] = ' ';
            ptr = (u16 *)(addr + (vga_cursor) * 2);
            *ptr = (0x07 << 8) | (u8)' ';
        }
    } else {
        /* Other characters */
        ptr = (u16 *)(addr + (vga_cursor) * 2);
        *ptr = (0x07 << 8) | (u8)c;
        vga_text[vga_cursor] = c;
        vga_cursor++;
    }

    /* Draw it again */
    if ( vga_cursor >= VGA_TEXTMODE_XY ) {
        for ( i = 0; i < VGA_TEXTMODE_XY - VGA_TEXTMODE_X; i++ ) {
            ptr = (u16 *)(addr + i * 2);
            vga_text[i] = vga_text[i + VGA_TEXTMODE_X];
            *ptr = (0x07 << 8) | vga_text[i];
        }
        for ( ; i < VGA_TEXTMODE_XY; i++ ) {
            ptr = (u16 *)(addr + i * 2);
            vga_text[i] = ' ';
            *ptr = (0x07 << 8) | vga_text[i];
        }
        vga_cursor -= VGA_TEXTMODE_X;
    }
    vga_update_cursor();

    spin_unlock(&lock);

    return;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
