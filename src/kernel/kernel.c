/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"

void arch_putc(int);
void arch_init(void);


typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))


#define PRINTF_MOD_NONE         0
#define PRINTF_MOD_LONG         1
#define PRINTF_MOD_LONGLONG     2


int
kputc(int c)
{
    arch_putc(c);
    return 0;
}

int
kprintf_percent(int pad)
{
    int i;

    for ( i = 0; i < pad; i++ ) {
        kputc(' ');
    }
    kputc('%');

    return 0;
}

int
kprintf_decimal(long long int val, int zero, int pad, int prec)
{
    long long int q;
    long long int r;
    int ptr;
    int sz;
    int i;
    char *buf;

    /* Calculate the maximum buffer size */
    sz = 3 * sizeof(long long int);
    buf = alloca(sz);

    ptr = 0;
    q = val;
    while ( q ) {
        r = q % 10;
        q = q / 10;
        buf[ptr] = r + '0';
        ptr++;
    }
    if ( !ptr ) {
        buf[ptr] = '0';
        ptr++;
    }

    /* Padding */
    if ( pad > prec && pad > ptr ) {
        for ( i = 0; i < pad - prec; i++ ) {
            if ( zero ) {
                kputc('0');
            } else {
                kputc(' ');
            }
        }
    }

    /* Precision */
    if ( prec > ptr ) {
        for ( i = 0; i < prec - ptr; i++ ) {
            kputc('0');
        }
    }

    /* Value */
    for ( i = 0; i < ptr; i++ ) {
        kputc(buf[ptr - i - 1]);
    }


    return 0;
}

int
kprintf_hexdecimal(unsigned long long int val, int zero, int pad, int prec,
                   int cap)
{
    unsigned long long int q;
    unsigned long long int r;
    int ptr;
    int sz;
    int i;
    char *buf;

    /* Calculate the maximum buffer size */
    sz = 2 * sizeof(unsigned long long int);
    buf = alloca(sz);

    ptr = 0;
    q = val;
    while ( q ) {
        r = q & 0xf;
        q = q >> 4;
        if ( r < 10 ) {
            buf[ptr] = r + '0';
        } else if ( cap ) {
            buf[ptr] = r - 10 + 'A';
        } else {
            buf[ptr] = r - 10 + 'a';
        }
        ptr++;
    }
    if ( !ptr ) {
        buf[ptr] = '0';
        ptr++;
    }

    /* Padding */
    if ( pad > prec && pad > ptr ) {
        for ( i = 0; i < pad - prec && i < pad - ptr ; i++ ) {
            if ( zero ) {
                kputc('0');
            } else {
                kputc(' ');
            }
        }
    }

    /* Precision */
    if ( prec > ptr ) {
        for ( i = 0; i < prec - ptr; i++ ) {
            kputc('0');
        }
    }

    /* Value */
    for ( i = 0; i < ptr; i++ ) {
        kputc(buf[ptr - i - 1]);
    }


    return 0;
}

int
kprintf_string(const char *s)
{
    while ( *s ) {
        kputc(*s);
        s++;
    }
    return 0;
}

/*
 * Format and print a message
 */
int
kprintf(const char *fmt, ...)
{
    va_list ap;

    const char *fmt_tmp;
    /* Leading suffix */
    int zero;
    /* Minimum length */
    int pad;
    /* Precision */
    int prec;
    /* Modifier */
    int mod;

    /* Value */
    const char *s;
    int d;
    long int ld;
    long long int lld;
    unsigned int u;
    unsigned long int lu;
    unsigned long long int llu;


    va_start(ap, fmt);

    while ( *fmt ) {
        if ( '%' == *fmt ) {
            fmt++;

            /* Save current */
            fmt_tmp = fmt;
            /* Reset */
            zero = 0;
            pad = 0;
            prec = 0;
            mod = 0;

            /* Padding with zero? */
            if ( '0' == *fmt ) {
                zero = 1;
                fmt++;
            }
            /* Padding length */
            if ( *fmt >= '1' && *fmt <= '9' ) {
                pad += *fmt - '0';
                fmt++;
                while ( *fmt >= '0' && *fmt <= '9' ) {
                    pad *= 10;
                    pad += *fmt - '0';
                    fmt++;
                }
            }
            /* Precision */
            if ( '.' == *fmt ) {
                fmt++;
                while ( *fmt >= '0' && *fmt <= '9' ) {
                    prec *= 10;
                    prec += *fmt - '0';
                    fmt++;
                }
            }

            /* Modifier */
            if ( 'l' == *fmt ) {
                fmt++;
                if ( 'l' == *fmt ) {
                    mod = PRINTF_MOD_LONGLONG;
                    fmt++;
                } else {
                    mod = PRINTF_MOD_LONG;
                }
            }

            /* Conversion */
            if ( '%' == *fmt ) {
                kprintf_percent(pad);
                fmt++;
            } else if ( 'd' == *fmt ) {
                switch ( mod ) {
                case PRINTF_MOD_LONG:
                    ld = va_arg(ap, long int);
                    kprintf_decimal(ld, zero, pad, prec);
                    break;
                case PRINTF_MOD_LONGLONG:
                    lld = va_arg(ap, long long int);
                    kprintf_decimal(lld, zero, pad, prec);
                    break;
                default:
                    d = va_arg(ap, int);
                    kprintf_decimal(d, zero, pad, prec);
                }
                fmt++;
            } else if ( 'u' == *fmt ) {
                switch ( mod ) {
                case PRINTF_MOD_LONG:
                    lu = va_arg(ap, unsigned long int);
                    kprintf_decimal(lu, zero, pad, prec);
                    break;
                case PRINTF_MOD_LONGLONG:
                    llu = va_arg(ap, unsigned long long int);
                    kprintf_decimal(llu, zero, pad, prec);
                    break;
                default:
                    u = va_arg(ap, unsigned int);
                    kprintf_decimal(u, zero, pad, prec);
                }
                fmt++;
            } else if ( 'x' == *fmt ) {
                switch ( mod ) {
                case PRINTF_MOD_LONG:
                    lu = va_arg(ap, unsigned long int);
                    kprintf_hexdecimal(lu, zero, pad, prec, 0);
                    break;
                case PRINTF_MOD_LONGLONG:
                    llu = va_arg(ap, unsigned long long int);
                    kprintf_hexdecimal(llu, zero, pad, prec, 0);
                    break;
                default:
                    u = va_arg(ap, unsigned int);
                    kprintf_hexdecimal(u, zero, pad, prec, 0);
                }
                fmt++;
            } else if ( 'X' == *fmt ) {
                switch ( mod ) {
                case PRINTF_MOD_LONG:
                    lu = va_arg(ap, unsigned long int);
                    kprintf_hexdecimal(lu, zero, pad, prec, 1);
                    break;
                case PRINTF_MOD_LONGLONG:
                    llu = va_arg(ap, unsigned long long int);
                    kprintf_hexdecimal(llu, zero, pad, prec, 1);
                    break;
                default:
                    u = va_arg(ap, unsigned int);
                    kprintf_hexdecimal(u, zero, pad, prec, 1);
                }
                fmt++;
            } else if ( 's' == *fmt ) {
                s = va_arg(ap, char *);
                kprintf_string(s);
                fmt++;
            } else {
                kputc('%');
                fmt = fmt_tmp;
            }
        } else {
            kputc(*fmt);
            fmt++;
        }
    }

    va_end(ap);

    return 0;
}

/*
 * Print a panic message and hlt processor
 */
void
panic(const char *s)
{
    kprintf("%s\r\n", s);
    /* FIXME: To stop all processors */
    arch_halt();
}

static int lock;

/*
 * Entry point to C function for BSP called from asm.s
 */
void
kmain(void)
{
    /* Initialize the lock varialbe */
    lock = 0;

    /* Initialize architecture-related devices */
    arch_bsp_init();
}

/*
 * Entry point to C function for AP called from asm.s
 */
void
apmain(void)
{
    /* Initialize this AP */
    arch_ap_init();
}











static unsigned char keymap[] =
    "  1234567890-= \tqwertyuiop    asdfghjkl;'` \\zxcvbnm                          "
    "                                                                             ";
void
kintr_int32(void)
{
    arch_putc('x');

    /* To disable, mask */
    //__asm__ __volatile__ ( "movq $0xfee00000,%rdx; movl $0x10000,%eax; movl %eax,0x320(%rdx)" ); // to disable
}

unsigned char
kbd_enc_read_buf(void)
{
    //PORT_KBD_ENC_BUF
    return inb(0x0060);
}

void
kintr_int33(void)
{
    unsigned char scan_code;

    arch_spin_lock(&lock);

    scan_code = kbd_enc_read_buf();

    if ( !(0x80 & scan_code) ) {
        if ( scan_code == 1 ) {
            /* Escape key */
            //poweroff();
        }
        arch_putc(keymap[scan_code]);
    }

    arch_spin_unlock(&lock);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
