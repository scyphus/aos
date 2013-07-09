/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"

#define VGA_VRAM_TEXTMODE       0x000b8000
#define VGA_TEXTMODE_X          80
#define VGA_TEXTMODE_Y          25
#define VGA_TEXTMODE_XY         (VGA_TEXTMODE_X * VGA_TEXTMODE_Y)

static int cursor;
static unsigned char vga_text[VGA_TEXTMODE_XY];
void apic_test(void);

/*
 * Update cursor
 */
void
vga_update_cursor(void)
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

/*
 * Put a character to VGA display
 */
void
vga_putc(int c)
{
    int i;
    u64 addr;
    u16 *ptr;

    addr = VGA_VRAM_TEXTMODE;

    if ( '\r' == c ) {
        /* Carriage return */
        cursor = cursor / VGA_TEXTMODE_X * VGA_TEXTMODE_X;
    } else if ( '\n' == c ) {
        /* New line */
        cursor = cursor + VGA_TEXTMODE_X;
    } else {
        /* Other characters */
        ptr = (u16 *)(addr + (cursor) * 2);
        *ptr = (0x07 << 8) | (u8)c;
        vga_text[cursor] = c;
        cursor++;
    }

    /* Draw it again */
    if ( cursor >= VGA_TEXTMODE_XY ) {
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
        cursor -= VGA_TEXTMODE_X;
    }
    vga_update_cursor();
}

void
print_reg(u64 x)
{
    int i;
    int c;
    for ( i = 0; i < 16; i++ ) {
        c = (x>>(64-i*4-4))&0xf;
        if ( c < 10 ) {
            vga_putc('0'+c);
        } else {
            vga_putc('a'+c-10);
        }
    }
}

/*
 * Compute TSC frequency
 */
u64
calib_tsc_freq(void)
{
    return 0;
}

u64
get_invariant_tsc_frequency(void)
{
    u64 val;
    int family;
    int model;

    /* Check invariant TSC support first */
    if ( !is_invariant_tsc() ) {
        /* Invariant TSC is not available */
        return 0;
    }

    /* MSR_PLATFORM_INFO */
    val = rdmsr(0xce);
    /* Get maximum non-turbo ratio [15:8] */
    val = (val >> 8) & 0xff;

    family = get_cpu_family();
    model = get_cpu_model();
    if ( 0x06 == family && (0x2a == model || 0x2d == model || 0x3a == model) ) {
        /* SandyBridge (06_2AH, 06_2DH) and IvyBridge (06_3AH) */
        val *= 100000000; /* 100 MHz */
    } else if ( 0x06 == family
                && (0x1a == model || 0x1e == model || 0x1f == model
                    || 0x25 == model || 0x2c == model || 0x2e == model
                    || 0x2f == model) ) {
        /* Nehalem (06_1AH, 06_1EH, 06_1FH, 06_2EH)
           and Westmere (06_2FH, 06_2CH, 06_25H) */
        val *= 133330000; /* 133.33 MHz */
    } else {
        /* Unknown model */
        val = 0;
    }

    return val;
}

/*
 * Search clock sources
 */
void
search_clock_sources(void)
{
    u64 tscfreq;

    /* Get invariant TSC frequency */
    tscfreq = get_invariant_tsc_frequency();
    if ( tscfreq ) {
        vga_putc('F');
        vga_putc('q');
        vga_putc(':');
        print_reg(tscfreq);
        vga_putc('.');
    }

    /* Get ACPI */
}

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
kprintf_putc(int c)
{
    vga_putc(c);
    return 0;
}

int
kprintf_percent(int pad)
{
    int i;

    for ( i = 0; i < pad; i++ ) {
        kprintf_putc(' ');
    }
    kprintf_putc('%');

    return 0;
}

int
kprintf_decimal(long long int val, int zero, int pad, int prec)
{
    long long int q;
    int r;
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

    /* Padding */
    if ( pad > prec && pad > ptr ) {
        for ( i = 0; i < pad - prec; i++ ) {
            if ( zero ) {
                kprintf_putc('0');
            } else {
                kprintf_putc(' ');
            }
        }
    }

    /* Precision */
    if ( prec > ptr ) {
        for ( i = 0; i < prec - ptr; i++ ) {
            kprintf_putc('0');
        }
    }

    /* Value */
    for ( i = 0; i < ptr; i++ ) {
        kprintf_putc(buf[ptr - i - 1]);
    }


    return 0;
}

int
kprintf_hexdecimal(unsigned long long int val, int zero, int pad, int prec,
                   int cap)
{
    unsigned long long int q;
    int r;
    int ptr;
    int sz;
    int i;
    char *buf;

    /* Calculate the maximum buffer size */
    sz = 2 * sizeof(long long int);
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

    /* Padding */
    if ( pad > prec && pad > ptr ) {
        for ( i = 0; i < pad - prec && i < pad - ptr ; i++ ) {
            if ( zero ) {
                kprintf_putc('0');
            } else {
                kprintf_putc(' ');
            }
        }
    }

    /* Precision */
    if ( prec > ptr ) {
        for ( i = 0; i < prec - ptr; i++ ) {
            kprintf_putc('0');
        }
    }

    /* Value */
    for ( i = 0; i < ptr; i++ ) {
        kprintf_putc(buf[ptr - i - 1]);
    }


    return 0;
}

int
kprintf_string(const char *s)
{
    while ( *s ) {
        kprintf_putc(*s);
        s++;
    }
    return 0;
}

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
            } else if ( 'x' == *fmt ) {
                switch ( mod ) {
                case PRINTF_MOD_LONG:
                    ld = va_arg(ap, long int);
                    kprintf_hexdecimal(ld, zero, pad, prec, 0);
                    break;
                case PRINTF_MOD_LONGLONG:
                    lld = va_arg(ap, long long int);
                    kprintf_hexdecimal(lld, zero, pad, prec, 0);
                    break;
                default:
                    d = va_arg(ap, int);
                    kprintf_hexdecimal(d, zero, pad, prec, 0);
                }
                fmt++;
            } else if ( 'X' == *fmt ) {
                switch ( mod ) {
                case PRINTF_MOD_LONG:
                    ld = va_arg(ap, long int);
                    kprintf_hexdecimal(ld, zero, pad, prec, 1);
                    break;
                case PRINTF_MOD_LONGLONG:
                    lld = va_arg(ap, long long int);
                    kprintf_hexdecimal(lld, zero, pad, prec, 1);
                    break;
                default:
                    d = va_arg(ap, int);
                    kprintf_hexdecimal(d, zero, pad, prec, 1);
                }
                fmt++;
            } else if ( 's' == *fmt ) {
                s = va_arg(ap, char *);
                kprintf_string(s);
                fmt++;
            } else {
                vga_putc('%');
                fmt = fmt_tmp;
            }
        } else {
            kprintf_putc(*fmt);
            fmt++;
        }
    }


    va_end(ap);

    return 0;
}

/*
 * Entry point to C function from asm.s
 */
void
cstart(void)
{
    idt_init();
    apic_test();

    cursor = 0;
    char *str = "Welcome to AOS!  Now this message is printed by C function.";

    kprintf("%s\r\n", str);

    idt_setup_intr_gate(13, &intr_gpf);

    u64 x;
    /* MSR_PERF_STAT */
    x = rdmsr(0x198);
    kprintf("%.16x\r\n", x);
    /* MSR_PLATFORM_ID */
    x = rdmsr(0x17);
    kprintf("%.16x\r\n", x);
    /* MSR_PLATFORM_INFO */
    x = rdmsr(0xce);
    kprintf("%.16x\r\n", x);

    __asm__ __volatile__ ("movl $0x80000007,%%eax;cpuid;movq %%rdx,%%rax" : "=a"(x) : );
    kprintf("%.16x\r\n", x);

    __asm__ __volatile__ ("movl $0x1,%%eax;cpuid" : "=a"(x) : );
    kprintf("%.16x\r\n", x);

    /* MSR_FSB_FREQ */
    x = rdmsr(0xcd);
    kprintf("%.16x\r\n", x);

    search_clock_sources();

    acpi_load_rsdp();

    kprintf("\r\n");
    print_reg(pm_tmr_port);
    kprintf("\r\n");
    print_reg(ioapic_base);
    kprintf("\r\n");


    __asm__ __volatile__ ("inl %%dx,%%eax" : "=a"(x) : "d"(pm_tmr_port) );
    print_reg(x);

    kprintf("\r\n");
    __asm__ __volatile__ ("inl %%dx,%%eax" : "=a"(x) : "d"(pm_tmr_port) );
    print_reg(x);


#if 0

    u64 tsc1;
    u64 tsc2;
    vga_putc('x');
    vga_putc('/');
    tsc1 = rdtsc();
    for ( ;; ) {
        tsc2 = rdtsc();
        if ( tsc2 - tsc1 > 266660000000 ) {
            break;
        }
        pause();
    }
    vga_putc('y');
#endif

}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
