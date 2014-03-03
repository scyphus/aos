/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"

static int lock;
static struct kmem_slab_page_hdr *kmem_slab_head;

extern struct netdev_list *netdev_head;

/*
 * Temporary: Keyboard drivers
 */
void kbd_init(void);
void kbd_event(void);
int kbd_read(void);

void proc_shell(void);

/* arch.c */
int dbg_printf(const char *fmt, ...);
void arch_bsp_init(void);

/*
 * Print a panic message and hlt processor
 */
void
panic(const char *s)
{
    kprintf("%s\r\n", s);
    arch_crash();
}

/*
 * Entry point to C function for BSP called from asm.s
 */
void
kmain(void)
{
    /* Initialize the lock varialbe */
    lock = 0;

    kmem_slab_head = NULL;

    //__asm__ ("mov %rax,%dr1");
    //__asm__ ("mov %%rax,%%dr0" :: "a"(&kmain));
    kbd_init();
    //__asm__ ("mov %%rax,%%dr1" :: "a"(&kbd_init));
    //__asm__ ("mov %%rax,%%dr2" :: "a"(&arch_bsp_init));

    /* Initialize architecture-related devices */
    arch_bsp_init();
    //__asm__ ("mov %%rax,%%dr5" :: "a"(&kbd_init));

    arch_busy_usleep(10000);

    kprintf("\r\nStarting a shell.  Press Esc to power off the machine:\r\n");

#if 0
    rng_init();
    rng_stir();
    int i;
    for ( i = 0; i < 16; i++ ) {
        kprintf("RNG: %.8x\r\n", rng_random());
    }
#endif

    proc_shell();
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

/*
 * PIT interrupt
 */
void
kintr_int32(void)
{
    arch_clock_update();
}

/*
 * Keyboard interrupt
 */
void
kintr_int33(void)
{
    kbd_event();
}

/*
 * Local timer interrupt
 */
void
kintr_loc_tmr(void)
{
    arch_clock_update();
}

/*
 * Interrupt service routine for all vectors
 */
void
kintr_isr(u64 vec)
{
    switch ( vec ) {
    case IV_TMR:
        kintr_int32();
        break;
    case IV_KBD:
        kintr_int33();
        break;
    case IV_LOC_TMR:
        kintr_loc_tmr();
        break;
    default:
        ;
    }
}

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


void set_cr3(u64);
u32 bswap32(u32);
u64 bswap64(u64);
u64 popcnt(u64);
#if 0
u64
setup_routing_page_table(void)
{
    u64 base;
    int nr0;
    int nr1;
    int nr2;
    int nr3;
    int i;

    /* 4GB + 32GB = 36GB */
    nr0 = 1;
    nr1 = 1;
    nr2 = 0x24;
    nr3 = 0x4000;

    /* Initialize page table */
    base = kmalloc((nr0 + nr1 + nr2 + nr3) * 4096);
    for ( i = 0; i < (nr0 + nr1 + nr2 + nr3) * 4096; i++ ) {
        *(u8 *)(base+i) = 0;
    }

    /* PML4E */
    *(u64 *)base = base + 0x1007;

    /* PDPTE */
    for ( i = 0; i < 0x24; i++ ) {
        *(u64 *)(base+0x1000+i*8) = base + (u64)0x1000 * i + 0x2007;
    }

    /* PDE */
    for ( i = 0; i < 0x4 * 0x200; i++ ) {
        *(u64 *)(base+0x2000+i*8) = (u64)0x00200000 * i + 0x183;
    }
    for ( i = 0; i < 0x20 * 0x200; i++ ) {
        *(u64 *)(base+0x6000+i*8) = base + (u64)0x1000 * i + 0x26007;
    }

    rng_init();
    rng_stir();
    u64 rt = kmalloc(4096 * 0x1000);
    for ( i = 0; i < 4096 * 0x1000 / 8; i++ ) {
        *(u64 *)(rt+i*8) = ((u64)rng_random()<<32) | (u64)rng_random();
    }

    /* PTE */
#if 0
    for ( i = 0; i < 0x4000 * 0x200; i++ ) {
        *(u64 *)(base+0x26000+i*8) = (u64)0x3 + (rt + (i * 8) % (4096 * 0x1000));
    }
#else
    for ( i = 0; i < 0x20 * 0x200; i++ ) {
        *(u64 *)(base+0x6000+i*8) = (u64)0x83 + (rt & 0xffffffc00000ULL);
    }
#endif
    kprintf("ROUTING TABLE: %llx %llx\r\n", base, rt);
    set_cr3(base);

    dbg_printf("Looking up routing table\r\n");
    u64 x;
    u64 tmp = 0;
    for ( x = 0; x < 0x100000000ULL; x++ ) {
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + (u64)bswap32(x));
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + (u64)rng_random());
        //bswap32(x);
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + (u64)(x^tmp)&0xffffffff);
        //tmp = *(u64 *)(u64)(0x100000000ULL + ((u64)(tmp&0xffffffffULL) << 3));
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + ((u64)x << 3));
        //tmp ^= *(u64 *)(u64)(0x100000000);
        //kprintf("ROUTING TABLE: %llx\r\n", *(u64 *)(u64)(0x100000000 + x));

        popcnt(x);
#if 0
        u64 m;
        __asm__ __volatile__ ("popcntq %1,%0" : "=r"(m) : "r"(x) );
        if ( m != popcnt(x) ) {
            kprintf("%.16llx %.16llx %.16llx\r\n", x, popcnt(x), m);
        }
#endif
    }
    dbg_printf("done : %llx\r\n", tmp);


    return 0;
}
#endif

/*
 * Shell process
 */
void
proc_shell(void)
{
    unsigned char cmdbuf[256];
    int pos;
    int c;

    //setup_routing_page_table();

    pos = 0;
    cmdbuf[0] = 0;
    kprintf("> ");

    for ( ;; ) {
        c = kbd_read();
        if ( c > 0 ) {
            if ( c == 0x08 ) {
                /* Backspace */
                if ( pos > 0 ) {
                    arch_putc(c);
                    pos--;
                    cmdbuf[pos] = '\0';
                }
            } else if ( c == '\r' ) {
                /* Exec */
                kprintf("\r\n");
                if ( 0 == kstrcmp("upa", cmdbuf) ) {
                    panic("Executing user PANIC!!");
                } else if ( 0 == kstrcmp("off", cmdbuf) ) {
                    arch_poweroff();
                } else if ( 0 == kstrcmp("show interfaces", cmdbuf) ) {
                    struct netdev_list *list;
                    list = netdev_head;
                    while ( list ) {
                        kprintf(" %s\r\n", list->netdev->name);
                        kprintf("   HWADDR: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
                                list->netdev->macaddr[0],
                                list->netdev->macaddr[1],
                                list->netdev->macaddr[2],
                                list->netdev->macaddr[3],
                                list->netdev->macaddr[4],
                                list->netdev->macaddr[5]);
                        list = list->next;
                    }
                } else if ( 0 == kstrcmp("start spt", cmdbuf) ) {
                    struct netdev_list *list;
                    u8 *rpkt;
                    u32 pktlen;
                    int ret;

                    u8 *pkt;
                    int pktsz = 64 - 18;
                    pkt = kmalloc(1534);
#define MCT 0
#if MCT
                    /* dst (multicast) */
                    pkt[0] = 0x01;
                    pkt[1] = 0x00;
                    pkt[2] = 0x5e;
                    pkt[3] = 0x00;
                    pkt[4] = 0x00;
                    pkt[5] = 0x01;
#else
                    /* dst (thinkpad T410) */
                    pkt[0] = 0xf0;
                    pkt[1] = 0xde;
                    pkt[2] = 0xf1;
                    pkt[3] = 0x37;
                    pkt[4] = 0xea;
                    pkt[5] = 0xf6;
#endif
#if 0
                    /* src (thinkpad T410) */
                    pkt[6] = 0xf0;
                    pkt[7] = 0xde;
                    pkt[8] = 0xf1;
                    pkt[9] = 0x37;
                    pkt[10] = 0xea;
                    pkt[11] = 0xf6;
#else
                    /* src (thinkpad X201i) */
                    pkt[6] = 0xf0;
                    pkt[7] = 0xde;
                    pkt[8] = 0xf1;
                    pkt[9] = 0x42;
                    pkt[10] = 0x8f;
                    pkt[11] = 0x9c;
#endif
                    /* type = IP (0800) */
                    pkt[12] = 0x08;
                    pkt[13] = 0x00;
                    /* IP header */
                    pkt[14] = 0x45;
                    pkt[15] = 0x00;
                    pkt[16] = (pktsz >> 8) & 0xff;
                    pkt[17] = pktsz & 0xff;
                    /* ID / fragment */
                    pkt[18] = 0x26;
                    pkt[19] = 0x6d;
                    pkt[20] = 0x00;
                    pkt[21] = 0x00;
                    /* TTL/protocol */
#if MCT
                    pkt[22] = 0x01;
                    //pkt[23] = 0x01;
                    pkt[23] = 17;
#else
                    pkt[22] = 0x64;
                    pkt[23] = 17;
#endif
                    /* checksum */
                    pkt[24] = 0x00;
                    pkt[25] = 0x00;
                    /* src */
                    pkt[26] = 192;
                    pkt[27] = 168;
                    pkt[28] = 100;
                    pkt[29] = 2;
                    /* dst */
#if MCT
                    pkt[30] = 224;
                    pkt[31] = 0;
                    pkt[32] = 0;
                    pkt[33] = 1;
#else
                    pkt[30] = 192;
                    pkt[31] = 168;
                    pkt[32] = 200;
                    pkt[33] = 2;
#endif
                    int i;
#if 0
                    /* icmp */
                    pkt[34] = 0x08;
                    pkt[35] = 0x00;
                    /* icmp checksum */
                    pkt[36] = 0x00;
                    pkt[37] = 0x00;
                    for ( i = 38; i < 64 + 34; i++ ) {
                        pkt[i] = 0;
                    }
#else
                    /* UDP */
                    pkt[34] = 0xff;
                    pkt[35] = 0xff;
                    pkt[36] = 0xff;
                    pkt[37] = 0xfe;
                    pkt[38] = (pktsz - 20) >> 8;
                    pkt[39] = (pktsz - 20) & 0xff;
                    pkt[40] = 0x00;
                    pkt[41] = 0x00;
                    for ( i = 42; i < pktsz + 14; i++ ) {
                        pkt[i] = 0;
                    }
#endif
                    /* Compute checksum */
                    u16 *tmp;
                    u32 cs;
                    pkt[24] = 0x0;
                    pkt[25] = 0x0;
                    tmp = (u16 *)pkt;
                    cs = 0;
                    for ( i = 7; i < 17; i++ ) {
                        cs += (u32)tmp[i];
                        cs = (cs & 0xffff) + (cs >> 16);
                    }
                    cs = 0xffff - cs;
                    pkt[24] = cs & 0xff;
                    pkt[25] = cs >> 8;

                    for ( ;; ) {
                        list = netdev_head;
                        while ( list ) {
                            //list->netdev->sendpkt(pkt, 1518-4, list->netdev);
                            list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);
#if 0
                            ret = list->netdev->recvpkt(&rpkt, &pktlen, list->netdev);
                            if ( ret ) {
                                list->netdev->sendpkt(pkt, 64-4, list->netdev);
                            }
#endif
                            list = list->next;
                        }
                        //__asm__ __volatile__ ("hlt;");
                    }
                } else if ( 0 == kstrcmp("start routing", cmdbuf) ) {
                    //__asm__ __volatile__ ("sti;");
                    struct netdev_list *list;
                    list = netdev_head;
                    list->netdev->routing_test(list->netdev);
                } else if ( 0 == kstrcmp("uptime", cmdbuf) ) {
                    u64 x = arch_clock_get();
                    kprintf("uptime: %llu.%.9llu sec\r\n",
                            x / 1000000000, x % 1000000000);
                } else if ( 0 == kstrcmp("", cmdbuf) ) {
                    /* Nothing to do */
                } else if ( 0 == kstrcmp("echo ", cmdbuf) ) {
                    kprintf("\r\n");
                } else if ( 0 == kstrncmp("echo ", cmdbuf, 5) ) {
                    kprintf("%s\r\n", cmdbuf+5);
                } else {
                    kprintf("%s: Command not found.\r\n", cmdbuf);
                }
                kprintf("> ");
                pos = 0;
                cmdbuf[0] = 0;
            } else {
                if ( pos > 254 ) {
                    kprintf("\r\nError: Command must not exceeds 255 bytes.");
                    kprintf("\r\n> ");
                    pos = 0;
                    cmdbuf[0] = 0;
                }
                cmdbuf[pos++] = c;
                cmdbuf[pos] = '\0';
                arch_putc(c);
            }
        } else {
            __asm__ __volatile__ ("sti;hlt;cli");
        }
    }

    return 0;
}


/*
 * Idle process
 */
int
kproc_idle_main(int argc, const char *const argv[])
{
    return 0;
}






/*
 * Memory allocation
 */
void *
kmalloc(u64 sz)
{
    struct kmem_slab_page_hdr **slab;
    u8 *bitmap;
    u64 n;
    u64 i;
    u64 j;
    u64 cnt;

    if ( sz > PAGESIZE / 2 ) {
        return phys_mem_alloc_pages(((sz - 1) / PAGESIZE) + 1);
    } else {
        n = ((PAGESIZE - sizeof(struct kmem_slab_page_hdr)) / (16 + 1));
        /* Search slab */
        slab = &kmem_slab_head;
        while ( NULL != *slab ) {
            bitmap = (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr);
            cnt = 0;
            for ( i = 0; i < n; i++ ) {
                if ( 0 == bitmap[i] ) {
                    cnt++;
                    if ( cnt * 16 >= sz ) {
                        bitmap[i - cnt + 1] |= 2;
                        for ( j = i - cnt + 1; j <= i; j++ ) {
                            bitmap[j] |= 1;
                        }
                        return (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr)
                            + n + (i - cnt + 1) * 16;
                    }
                } else {
                    cnt = 0;
                }
            }
            slab = &(*slab)->next;
        }
        /* Not found */
        *slab = phys_mem_alloc_pages(1);
        if ( NULL == (*slab) ) {
            /* Error */
            return NULL;
        }
        (*slab)->next = NULL;
        bitmap = (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr);
        for ( i = 0; i < n; i++ ) {
            bitmap[i] = 0;
        }
        bitmap[0] |= 2;
        for ( i = 0; i < (sz - 1) / 16 + 1; i++ ) {
            bitmap[i] |= 1;
        }

        return (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr) + n;
    }
    return NULL;
}

/*
 * Free allocated memory
 */
void
kfree(void *ptr)
{
    struct kmem_slab_page_hdr *slab;
    u8 *bitmap;
    u64 n;
    u64 i;
    u64 off;

    if ( 0 == (u64)ptr % PAGESIZE ) {
        phys_mem_free_pages(ptr);
    } else {
        /* Slab */
        /* ToDo: Free page */
        n = ((PAGESIZE - sizeof(struct kmem_slab_page_hdr)) / (16 + 1));
        slab = (struct kmem_slab_page_hdr *)(((u64)ptr / PAGESIZE) * PAGESIZE);
        off = (u64)ptr % PAGESIZE;
        off = (off - sizeof(struct kmem_slab_page_hdr) - n) / 16;
        bitmap = (u8 *)slab + sizeof(struct kmem_slab_page_hdr);
        if ( off >= n ) {
            /* Error */
            return;
        }
        if ( !(bitmap[off] & 2) ) {
            /* Error */
            return;
        }
        bitmap[off] &= ~(u64)2;
        for ( i = off; i < n && (!(bitmap[i] & 2) || bitmap[i] != 0); i++ ) {
            bitmap[off] &= ~(u64)1;
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
