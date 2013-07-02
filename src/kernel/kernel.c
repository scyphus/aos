/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>

#define VGA_VRAM_TEXTMODE       0x000b8000
#define VGA_TEXTMODE_X          80
#define VGA_TEXTMODE_Y          25
#define VGA_TEXTMODE_XY         (VGA_TEXTMODE_X * VGA_TEXTMODE_Y)

static int cursor;
static unsigned char vga_text[VGA_TEXTMODE_XY];

void
update_cursor(void)
{
    u16 val;
    u16 addr = 0x3d4;

    /* Low */
    val = ((cursor & 0xff) << 8) | 0x0f;
    __asm__ __volatile__ ( "outw %%ax,%%dx" : : "a"(val), "d"(addr) );
    /* High */
    val = (((cursor >> 8) & 0xff) << 8) | 0x0e;
    __asm__ __volatile__ ( "outw %%ax,%%dx" : : "a"(val), "d"(addr) );
}

void
vga_putc(int c)
{
    int i;
    unsigned long addr;
    unsigned short *ptr;

    addr = VGA_VRAM_TEXTMODE;

    ptr = (unsigned short *)(addr + (cursor) * 2);
    *ptr = (0x07 << 8) | (unsigned char)c;
    vga_text[cursor] = c;
    cursor++;

    /* Draw it again */
    if ( cursor == VGA_TEXTMODE_XY ) {
        for ( i = 0; i < VGA_TEXTMODE_XY - VGA_TEXTMODE_X; i++ ) {
            ptr = (unsigned short *)(addr + i * 2);
            vga_text[i] = vga_text[i + VGA_TEXTMODE_X];
            *ptr = (0x07 << 8) | vga_text[i];
        }
        for ( ; i < VGA_TEXTMODE_XY; i++ ) {
            ptr = (unsigned short *)(addr + i * 2);
            vga_text[i] = ' ';
            *ptr = (0x07 << 8) | vga_text[i];
        }
        cursor -= VGA_TEXTMODE_X;
    }
    update_cursor();
}

/*
 * Entry point to C function from asm.s
 */
void
cstart(void)
{
    cursor = 0;
    char *str = "Welcome to AOS!  Now this message is printed by C function.";

    while ( *str ) {
        vga_putc(*str);
        str++;
    }
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
