/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <aos/const.h>

#define IV_TMR          0x20
#define IV_KBD          0x21
#define IV_LOC_TMR      0x50

#define PAGESIZE        4096


/* DRIVER */

#define NETDEV_MAX_NAME 32
struct netdev {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;

    int (*sendpkt)(u8 *pkt, u32 len, struct netdev *netdev);
    int (*recvpkt)(u8 *pkt, u32 len, struct netdev *netdev);
    int (*routing_test)(struct netdev *netdev);
};
struct netdev_list {
    struct netdev *netdev;
    struct netdev_list *next;
};

void rng_init(void);
void rng_stir(void);
u32 rng_random(void);



struct proc {
    /* Stack frame */
    void *rp;

    /* Stack */
    void *kstack;
    void *ustack;

    /* Process information */
    u64 id;
    const char *name;

    /* Main routine */
    int (*main)(int argc, const char *const argv[]);

    /* For scheduler */
    int pri;
    u64 cred;

    /* State */
    int state;
} __attribute__ ((packed));

/*
 * Kernel memory management
 */
struct kmem_slab_page_hdr {
    struct kmem_slab_page_hdr *next;
    u64 reserved;
} __attribute__ ((packed));


/*
 * Call out queue
 */
struct callout_entry {
    u64 fire_clock;
    void (*func)(u64 x);
    struct callout_entry *next;
};
struct callout_queue {
    struct callout_entry *ent;
    struct callout_queue *next;
} __attribute__ ((packed));
struct callout_queue callout[100];


typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))


/* in kernel.c */
int kprintf(const char *, ...);
int kvprintf(const char *, va_list);
void panic(const char *);


/* in util.c */
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, int);
void kmem_init(void);
void * kmalloc(u64);
void kfree(void *);

/* in shell.c */
void proc_shell(void);


/* Architecture-dependent functions */
void arch_init(void);
void arch_bsp_init(void);
void arch_ap_init(void);

void arch_putc(int);
void arch_busy_usleep(u64);
void arch_crash(void);

void arch_poweroff(void);

void arch_spin_lock(int *);
void arch_spin_unlock(int *);

/* Clock and timer */
void arch_clock_update(void);
u64 arch_clock_get(void);

/* Physical memory management */
int phys_mem_wire(void *, u64);
void * phys_mem_alloc_pages(u64);
void phys_mem_free_pages(void *);

#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
