/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../../kernel.h"

#define KBD_ENC_BUF_PORT        0x0060

struct kbd_status {
    int lshift;
    int rshift;
    int capslock;
};

static struct kbd_status stat;
static unsigned char buf[128];
static u32 pos;

void
kbd_init(void)
{
    stat.lshift = 0;
    stat.rshift = 0;
    stat.capslock = 0;
    pos = 0;
    //0x3a
}

/*
 * Read from buffer
 */
u8
kbd_enc_read_buf(void)
{
    return inb(KBD_ENC_BUF_PORT);
}

static unsigned char keymap_base[] =
    "  1234567890-=\x08\tqwertyuiop  \r asdfghjkl;'` \\zxcvbnm                          "
    "                                                                             ";

static unsigned char keymap_shift[] =
    "  1234567890-=\x08\tQWERTYUIOP  \r ASDFGHJKL;'` \\ZXCVBNM                          "
    "                                                                             ";

/*
 * Keyboard event handler
 */
void
kbd_event(void)
{
    u8 scan_code;

    scan_code = kbd_enc_read_buf();
    if ( !(0x80 & scan_code) ) {
        if ( scan_code == 1 ) {
            /* Escape key */
            arch_poweroff();
        }
        /* Pressed */
        switch ( scan_code ) {
        case 0x2a:
            /* Left shift */
            stat.lshift = 1;
            break;
        case 0x36:
            /* Right shift */
            stat.rshift = 1;
            break;
        case 0x3a:
            /* Caps lock */
            break;
        default:
            if ( (stat.lshift | stat.rshift) ^ stat.capslock ) {
                arch_putc(keymap_shift[scan_code]);
            } else {
                arch_putc(keymap_base[scan_code]);
            }
        }
    } else {
        /* Released */
        switch ( scan_code ) {
        case 0xaa:
            /* Left shift */
            stat.lshift = 0;
            break;
        case 0xb6:
            /* Right shift */
            stat.rshift = 0;
            break;
        case 0xba:
            /* Caps lock */
            stat.capslock = ~1;
            break;
        default:
            ;
        }
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
