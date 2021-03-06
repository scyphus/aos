/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "../../kernel/kernel.h"

#define KBD_ENC_BUF_PORT        0x0060
#define KBD_ENC_STAT_PORT       0x0064

struct kbd_status {
    int lshift;
    int rshift;
    int lctrl;
    int rctrl;
    int capslock;
};

/* Hold keyboard stats and buffer */
static volatile unsigned char buf[256];
static volatile u8 rpos;
static volatile u8 wpos;

static volatile int lock;


/* In system.c */
int putc_buffer_irq(int, unsigned char);

/*
 * Read from buffer
 */
u8
kbd_enc_read_buf(void)
{
    return arch_inb(KBD_ENC_BUF_PORT);
}

static unsigned char keymap_base[] =
    "  1234567890-=\x08\tqwertyuiop[]\r asdfghjkl;'` \\zxcvbnm,./                       "
    "                                                                             ";

static unsigned char keymap_shift[] =
    "  !@#$%^&*()_+\x08\tQWERTYUIOP{}\r ASDFGHJKL:\"~ |ZXCVBNM<>?                       "
    "                                                                             ";

/*
 * IRQ handler
 */
void
kbd_irq_handler(int irq, void *user)
{
    struct kbd_status *stat;
    u8 scan_code;

    stat = (struct kbd_status *)user;

    while ( arch_inb(KBD_ENC_STAT_PORT) & 1 ) {
        scan_code = kbd_enc_read_buf();
        if ( !(0x80 & scan_code) ) {
            if ( scan_code == 1 ) {
                /* Escape key */
                kexit();
                arch_poweroff();
                return;
            }
            /* Pressed */
            switch ( scan_code ) {
            case 0x1d:
                /* Left ctrl */
                stat->lctrl = 1;
                break;
            case 0x2a:
                /* Left shift */
                stat->lshift = 1;
                break;
            case 0x36:
                /* Right shift */
                stat->rshift = 1;
                break;
            case 0x3a:
                /* Caps lock */
                break;
            case 0x5a:
                /* Right ctrl */
                stat->rctrl = 1;
                break;
            case 0x48:
                /* Up */
                buf[wpos++] = 0x86;
                putc_buffer_irq(0, 0x86);
                break;
            case 0x4b:
                /* Left */
                buf[wpos++] = 0x83;
                putc_buffer_irq(0, 0x83);
                break;
            case 0x4d:
                /* Right */
                buf[wpos++] = 0x84;
                putc_buffer_irq(0, 0x84);
                break;
            case 0x50:
                /* Down */
                buf[wpos++] = 0x85;
                putc_buffer_irq(0, 0x85);
                break;
            default:
                if ( (stat->lctrl || stat->rctrl)
                     && 'h' == keymap_base[scan_code] ) {
                    buf[wpos++] = 0x08;
                    putc_buffer_irq(0, 0x08);
                } else {
                    if ( (stat->lshift | stat->rshift) ^ stat->capslock ) {
                        buf[wpos++] = keymap_shift[scan_code];
                        putc_buffer_irq(0, keymap_shift[scan_code]);
                    } else {
                        buf[wpos++] = keymap_base[scan_code];
                        putc_buffer_irq(0, keymap_base[scan_code]);
                    }
                }
            }
        } else {
            /* Released */
            switch ( scan_code ) {
            case 0x9d:
                /* Left ctrl */
                stat->lctrl = 0;
                break;
            case 0xaa:
                /* Left shift */
                stat->lshift = 0;
                break;
            case 0xb6:
                /* Right shift */
                stat->rshift = 0;
                break;
            case 0xba:
                /* Caps lock */
                stat->capslock ^= 1;
                break;
            case 0xda:
                /* Right ctrl */
                stat->rctrl = 0;
                break;
            default:
                ;
            }
        }
    }
}

/*
 * Main
 */
int
kbd_driver_main(int argc, char *argv[])
{
    /* Hold keyboard stats and buffer */
    struct kbd_status stat;
    //int c;

    /* Initialize the keyboard status */
    stat.lshift = 0;
    stat.rshift = 0;
    stat.lctrl = 0;
    stat.rctrl = 0;
    stat.capslock = 0;
    rpos = 0;
    wpos = 0;
    lock = 0;

    /* Until the status register tells no data on buffer */
    while ( arch_inb(KBD_ENC_STAT_PORT) & 1 ) {
        (void)kbd_enc_read_buf();
    }

    /* Register IRQ handler */
    register_irq_handler(1, &kbd_irq_handler, &stat);

    while ( 1 ) {
        /* FIXME */
        if ( rpos != wpos ) {
            /* Send */
            //c = buf[rpos++];
            //write(0, &c, 1);
        } else {
            /* Sleep */
        }
        ktask_change_state(arch_get_current_task(), TASK_STATE_BLOCKED);
    }

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
