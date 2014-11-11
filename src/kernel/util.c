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

//struct kmem_slab_page_hdr *kmem_slab_head;
struct kmem_slab_root *kmem_slab_head;
static volatile int kmem_lock;

/*
 * Put a character to the standard output of the kernel
 */
int
kputc(int c)
{
    arch_putc(c);

    return 0;
}

/*
 * Put a % character with paddings to the standard output of the kernel
 */
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

/*
 * Print a decimal value to the standard output of the kernel
 */
int
kprintf_decimal(long long int val, int zero, int pad, int prec)
{
    long long int q;
    long long int r;
    int ptr;
    int sz;
    int i;
    char *buf;

    /* FIXME: Support "signed" integer */

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

/*
 * Print a hexdecimal value to the standard output of the kernel
 */
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
    if ( NULL == s ) {
        s = "(null)";
    }
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

/*
 * Print out formatted string
 */
int
kprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);

    return 0;
}

/*
 * Initialize the kernel memory
 */
void
kmem_init(void)
{
    int npg;
    int i;

    npg = (sizeof(struct kmem_slab_root) - 1) / PAGESIZE + 1;

    kmem_slab_head = phys_mem_alloc_pages(npg);
    for ( i = 0; i < 8; i++ ) {
        kmem_slab_head->gslabs[i].partial = NULL;
        kmem_slab_head->gslabs[i].full = NULL;
        kmem_slab_head->gslabs[i].free = NULL;
    }
    kmem_lock = 0;
}

/*
 * Memory allocation
 * ** FIXME **
 */
void *
kmalloc(u64 sz)
{
    u64 tmp;
    u64 bsz;
    u64 asz;
    void *ret;
    int i;
    u64 npg;
    struct kmem_slab *hdr;

    /* Calculate aligned size */
    tmp = sz - 1;
    bsz = 0;
    while ( tmp ) {
        bsz++;
        tmp >>= 1;
    }
    /* No smaller than 32 (= 1<<5) */
    if ( bsz >= 5 ) {
        asz = (1<<bsz);
        bsz -= 5;
    } else {
        asz = (1<<5);
        bsz = 0;
    }

    arch_spin_lock(&kmem_lock);

    if ( bsz < 6 ) {
        /* Small objects */
        if ( kmem_slab_head->gslabs[bsz].partial ) {
            /* Has partial list */
            hdr = kmem_slab_head->gslabs[bsz].partial;
            ret = (void *)((u64)hdr->obj_head + hdr->free * asz);
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                hdr->free = -1;
                /* Becomes full */
                kmem_slab_head->gslabs[bsz].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = kmem_slab_head->gslabs[bsz].full;
                kmem_slab_head->gslabs[bsz].full = hdr;
            } else {
                /* Search free space */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        } else if ( kmem_slab_head->gslabs[bsz].free ) {
            /* Has free list */
            hdr = kmem_slab_head->gslabs[bsz].free;
            ret = (void *)((u64)hdr->obj_head + hdr->free * asz);
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                hdr->free = -1;
                /* Becomes full */
                kmem_slab_head->gslabs[bsz].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = kmem_slab_head->gslabs[bsz].full;
                kmem_slab_head->gslabs[bsz].full = hdr;
            } else {
                /* To partial */
                hdr->next = kmem_slab_head->gslabs[bsz].partial;
                kmem_slab_head->gslabs[bsz].partial = hdr;

                /* Search free space */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        } else {
            /* No free space, then allocate new page */
            npg = (((1<<(bsz+8)) - 1) / PAGESIZE) + 1;
            hdr = phys_mem_alloc_pages(npg);
            if ( NULL == hdr ) {
                arch_spin_unlock(&kmem_lock);
                return NULL;
            }
            hdr->nr = (npg * PAGESIZE - sizeof(struct kmem_slab)) / (asz + 1);
            hdr->nused = 0;
            hdr->free = 0;
            hdr->obj_head = (void *)((u64)hdr + (npg * PAGESIZE)
                                     - (asz * hdr->nr));
            hdr->next = NULL;
            kmemset(hdr->marks, 0, hdr->nr);

            ret = (void *)((u64)hdr->obj_head + hdr->free * asz);
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                hdr->free = -1;
                /* Becomes full */
                kmem_slab_head->gslabs[bsz].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = kmem_slab_head->gslabs[bsz].full;
                kmem_slab_head->gslabs[bsz].full = hdr;
            } else {
                /* To partial */
                hdr->next = kmem_slab_head->gslabs[bsz].partial;
                kmem_slab_head->gslabs[bsz].partial = hdr;
                /* Search free space */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        }
    } else {
        /* Large objects */
        ret = phys_mem_alloc_pages(((sz - 1) / PAGESIZE) + 1);
    }

    arch_spin_unlock(&kmem_lock);
    return ret;
}

/*
 * Free allocated memory
 * ** FIXME **
 */
void
kfree(void *ptr)
{
    struct kmem_slab *hdr;
    struct kmem_slab **hdrp;
    int i;
    int j;
    int found;
    u64 asz;

    arch_spin_lock(&kmem_lock);

    if ( 0 == (u64)ptr % PAGESIZE ) {
        phys_mem_free_pages(ptr);
    } else {

        for ( i = 0; i < 8; i++ ) {
            asz = (1<<(i + 5));

            /* Search from partial */
            hdrp = &kmem_slab_head->gslabs[i].partial;

            while ( NULL != *hdrp ) {
                hdr = *hdrp;

                found = -1;
                for ( j = 0; j < hdr->nr; j++ ) {
                    if ( ptr == (void *)((u64)hdr->obj_head + j * asz) ) {
                        /* Found */
                        found = j;
                        break;
                    }
                }
                if ( found >= 0 ) {
                    hdr->nused--;
                    hdr->marks[found] = 0;
                    hdr->free = found;
                    if ( hdr->nused <= 0 ) {
                        /* To free list */
                        *hdrp = hdr->next;
                        hdr->next = kmem_slab_head->gslabs[i].free;
                        kmem_slab_head->gslabs[i].free = hdr;
                    }
                    arch_spin_unlock(&kmem_lock);
                    return;
                }
                hdrp = &hdr->next;
            }

            /* Search from full */
            hdrp = &kmem_slab_head->gslabs[i].full;

            while ( NULL != *hdrp ) {
                hdr = *hdrp;

                found = -1;
                for ( j = 0; j < hdr->nr; j++ ) {
                    if ( ptr == (void *)((u64)hdr->obj_head + j * asz) ) {
                        /* Found */
                        found = j;
                        break;
                    }
                }
                if ( found >= 0 ) {
                    hdr->nused--;
                    hdr->marks[found] = 0;
                    hdr->free = found;
                    if ( hdr->nused <= 0 ) {
                        /* To free list */
                        *hdrp = hdr->next;
                        hdr->next = kmem_slab_head->gslabs[i].free;
                        kmem_slab_head->gslabs[i].free = hdr;
                    } else {
                        /* To partial list */
                        *hdrp = hdr->next;
                        hdr->next = kmem_slab_head->gslabs[i].partial;
                        kmem_slab_head->gslabs[i].partial = hdr;
                    }
                    arch_spin_unlock(&kmem_lock);
                    return;
                }
                hdrp = &hdr->next;
            }
        }
    }

    arch_spin_unlock(&kmem_lock);
}

/*
 * Compare string
 */
int
kstrcmp(const char *a, const char *b)
{
    while ( *a || *b ) {
        if ( *a > *b ) {
            return 1;
        } else if ( *a < *b ) {
            return -1;
        }
        a++;
        b++;
    }

    return 0;
}

/*
 * Compare string not more than n characters
 */
int
kstrncmp(const char *a, const char *b, int n)
{
    while ( (*a || *b) && n > 0 ) {
        if ( *a > *b ) {
            return 1;
        } else if ( *a < *b ) {
            return -1;
        }
        a++;
        b++;
        n--;
    }

    return 0;
}

/*
 * Count the length of string
 */
int
kstrlen(const char *s)
{
    int len;

    len = 0;
    while ( '\0' != s[len] ) {
        len++;
    }
    return len;
}

/*
 * Duplicate the string
 */
char *
kstrdup(const char *s)
{
    char *ns;
    int len;
    int i;

    len = kstrlen(s);
    ns = kmalloc(len + 1);
    if ( NULL == ns ) {
        return NULL;
    }
    for ( i = 0; i < len + 1; i++ ) {
        ns[i] = s[i];
    }

    return ns;
}

/*
 * Compare memory
 */
int
kmemcmp(const u8 *a, const u8 *b, int n)
{
    while ( n > 0 ) {
        if ( *a > *b ) {
            return 1;
        } else if ( *a < *b ) {
            return -1;
        }
        a++;
        b++;
        n--;
    }

    return 0;
}

/*
 * Copy string to string
 */
void *
kmemcpy(void *a, const void *b, u64 sz)
{
    return arch_memcpy(a, b, sz);
}

/*
 * Fill a byte string with a byte value
 */
void *
kmemset(void *b, int c, u64 len)
{
    int i;

    for ( i = 0; i < len; i++ ) {
        *((u8 *)b + i) = c;
    }

    return b;
}




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
