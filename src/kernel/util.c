/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"

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

/*
 * Print out string
 */
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
kvprintf(const char *fmt, va_list ap)
{
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

    return 0;
}
int
kprintf(const char *fmt, ...)
{
    va_list ap;
    __asm__ ("mov %%rax,%%dr3" :: "a"(fmt));

    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

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
