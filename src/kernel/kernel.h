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

#define MAX_PROCESSORS  256

#define IV_IRQ0         0x20
#define IV_IRQ1         0x21
#define IV_IRQ2         0x22
#define IV_IRQ3         0x23
#define IV_IRQ4         0x24
#define IV_IRQ5         0x25
#define IV_IRQ6         0x26
#define IV_IRQ7         0x27
#define IV_IRQ8         0x28
#define IV_IRQ9         0x29
#define IV_IRQ10        0x2a
#define IV_IRQ11        0x2b
#define IV_IRQ12        0x2c
#define IV_IRQ13        0x2d
#define IV_IRQ14        0x2e
#define IV_IRQ15        0x2f
#define IV_LOC_TMR      0x50
#define IV_IPI          0x51
#define IV_CRASH        0xfe

#define PAGESIZE        4096


/* DRIVER */
#define NETDEV_MAX_NAME 32
struct netdev {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;

    int (*sendpkt)(u8 *pkt, u32 len, struct netdev *netdev);
    int (*recvpkt)(u8 *pkt, u32 len, struct netdev *netdev);
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


#define TASK_STATE_READY        1
#define TASK_STATE_RUNNING      2
#define TASK_STATE_BLOCKED      3


/*
 * Task
 */
struct ktask {
    /* Task ID */
    u64 id;
    const char *name;

    /* Main routine */
    int (*main)(int argc, char *argv[]);
    int argc;
    char **argv;

    /* For scheduler */
    int pri;
    u64 cred;

    /* State */
    int state;

    /* Return code */
    int ret;

    /* Archtecture-specific structure */
    void *arch;


    /* Message */
    struct kmsg_queue *msg_queue;
};

/*
 * Task queue for scheduler
 */
struct ktask_queue_entry {
    struct ktask *ktask;
    struct ktask_queue_entry *next;
};
struct ktask_queue {
    struct ktask_queue_entry *head;
    struct ktask_queue_entry *tail;
};

/*
 * Task table
 */
struct ktask_table {
    /* Kernel tasks */
    struct ktask *kernel;
};

/*
 * Inter-task message
 */
struct kmsg {
    u8 type;
    union {
        u8 msg;
    } u;
};
struct kmq {
    struct kmsq *head;
    struct kmsq *tail;
};

/*
 * Semaphore
 */
struct semaphore {
    int lock;
    int count;
    struct ktask_queue waiting;
};

/*
 * Interrupt handler
 */
struct interrupt_handler {
    void (*handler)(int, void *);
    void *user;
};
#define IRQ_MAX 31
struct interrupt_handler irq_handler_table[IRQ_MAX+1];


/*
 * Processor
 */
#define PROCESSOR_BSP 1
#define PROCESSOR_AP_TICKFULL 2
#define PROCESSOR_AP_TICKLESS 3
struct processor {
    /* Processor ID */
    u8 id;
    u8 type;
    struct ktask *idle;
};
struct processor_table {
    int n;
    struct processor *prs;
    u8 map[MAX_PROCESSORS];
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
struct ktask * ktask_alloc(int);
int ktask_kernel_main(int argc, char *argv[]);
int ktask_idle_main(int argc, char *argv[]);


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

/* in sched.c */
int ktask_init(void);
int sched_init(void);
void sched(void);
int sched_ktask_enqueue(struct ktask_queue_entry *);
struct ktask_queue_entry * sched_ktask_dequeue(void);


/* in shell.c */
int proc_shell(int, const char *const []);
/* in router.c */
void proc_router(void);


/* Architecture-dependent functions in arch.c */
void arch_init(void);
void arch_bsp_init(void);
void arch_ap_init(void);

void arch_putc(int);
void arch_busy_usleep(u64);
void arch_crash(void);

void arch_poweroff(void);

void arch_spin_lock(volatile int *);
void arch_spin_unlock(volatile int *);
void arch_spin_lock_intr(volatile int *);
void arch_spin_unlock_intr(volatile int *);

u8 arch_inb(u16);

void arch_idle(void);

void * arch_memcpy(void *, const void *, u64);

void * arch_alloc_task(struct ktask *, void (*entry)(struct ktask *), int);
int arch_set_next_task(struct ktask *);
struct ktask * arch_get_next_task(void);

int arch_cpu_active(u16);

void arch_scall(u64 nr);

/* Clock and timer */
void arch_clock_update(void);
u64 arch_clock_get(void);

/* Physical memory management */
int phys_mem_wire(void *, u64);
void * phys_mem_alloc_pages(u64);
void phys_mem_free_pages(void *);


/* in system.c */
#include <aos/types.h>
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);



#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
