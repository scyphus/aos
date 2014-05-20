/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

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

struct peth {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;
    int (*sendpkt)(u8 *pkt, u32 len, struct peth *peth);
    int (*recvpkt)(u8 *pkt, u32 len, struct peth *peth);
};

struct veth {
    struct peth *parent;
    u8 macaddr[6];
    int vlan;
};
#if 0
struct net_ip {
    struct ipv4_addr **ipv4_addrs;
    struct ipv6_addr **ipv6_addrs;
    struct veth **veths;
};
#endif


void rng_init(void);
void rng_stir(void);
u32 rng_random(void);



#define TASK_POLICY_KERNEL      0
#define TASK_POLICY_DRIVER      1
#define TASK_POLICY_USER        3
#define TASK_KSTACK_SIZE        4096
#define TASK_USTACK_SIZE        4096 * 0x10
#define TASK_STACK_GUARD        16
#define TASK_QUEUE_LEN          0x10
void task_restart(void);
void halt(void);

/*
 * Task
 */
struct ktask {
    /* Task ID */
    u64 id;
    const char *name;

    /* Main routine */
    int (*main)(int argc, const char *const argv[]);
    int argc;
    const char *const *argv;

    /* For scheduler */
    int pri;
    u64 cred;

    /* State */
    int state;

    /* Archtecture-specific structure */
    void *arch;
};

/*
 * Task queue for scheduler
 */
struct ktask_queue_entry {
    struct ktask *ktask;
};
struct ktask_queue {
    int nent;
    struct ktask_queue_entry *entries;
    volatile int head;
    volatile int tail;
};


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
struct ktask * ktask_alloc(void);
int ktask_enqueue(struct ktask *);
struct ktask * ktask_dequeue(void);
void sched(void);


/* in util.c */
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, int);
int kmemcmp(const u8 *, const u8 *, int);
void * kmemcpy(void *, const void *, u64);
void * kmemset(void *, int, u64);
void kmem_init(void);
void * kmalloc(u64);
void kfree(void *);
int kstrlen(const char *);
char * kstrdup(const char *);

/* in shell.c */
int proc_shell(int, const char *const []);
/* in router.c */
void proc_router(void);


/* Architecture-dependent functions */
void arch_init(void);
void arch_bsp_init(void);
void arch_ap_init(void);

void arch_putc(int);
void arch_busy_usleep(u64);
void arch_crash(void);

void arch_poweroff(void);

void arch_spin_lock(volatile int *);
void arch_spin_unlock(volatile int *);

void * arch_memcpy(void *, const void *, u64);

void * arch_alloc_task(struct ktask *, void (*entry)(struct ktask *), int);
int arch_set_next_task(struct ktask *);
struct ktask * arch_get_next_task(void);

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
