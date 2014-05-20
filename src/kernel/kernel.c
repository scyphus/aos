/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "kernel.h"

static volatile int lock;
static int routing_processor;

static volatile int ktask_lock;
static struct ktask_queue *ktaskq;

/*
 * Temporary: Keyboard drivers
 */
void kbd_init(void);
void kbd_event(void);
int kbd_read(void);

/* arch.c */
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

int ixgbe_forwarding_test1(struct netdev *, struct netdev *);
int ixgbe_forwarding_test2(struct netdev *, struct netdev *);
extern struct netdev_list *netdev_head;

#define PERFEVTSELx_MSR_BASE 0x186      /* 3B. 18.2.2.2 */
#define PMCx_MSR_BASE 0x0c1             /* 3B. 18.2.2.2 */
#define PERFEVTSELx_EN (1<<22)
#define PERFEVTSELx_OS (1<<17)
#define PERFEVTSELx_USR (1<<16)

/*
 * Entry point to C function for BSP called from asm.s
 */
void
kmain(void)
{
    /* Initialize the lock varialbe */
    lock = 0;
    ktask_lock = 0;

    /* Routing processor */
    routing_processor = -1;

    /* Initialize kmem */
    kmem_init();

    /* Initialize keyboard */
    kbd_init();

    /* Initialize architecture-related devices */
    arch_bsp_init();

    /* Wait for 10ms */
    arch_busy_usleep(10000);

    /* Initialize kernel task queue */
    ktaskq = kmalloc(sizeof(struct ktask_queue));
    if ( NULL == ktaskq ) {
        /* Print out a message */
        kprintf("Error on memory allocation for task queue\r\n");
        return;
    }
    ktaskq->nent = TASK_QUEUE_LEN;
    ktaskq->entries = kmalloc(sizeof(struct ktask_queue_entry) * ktaskq->nent);
    if ( NULL == ktaskq->entries ) {
        /* Print out a message */
        kprintf("Error on memory allocation for task queue entries\r\n");
        return;
    }
    ktaskq->head = 0;
    ktaskq->tail = 0;


    /* Initialize random number generator */
    rng_init();
    rng_stir();

#if 0
    int i;
    for ( i = 0; i < 16; i++ ) {
        kprintf("RNG: %.8x\r\n", rng_random());
    }
#endif

    /* Print out a message */
    kprintf("\r\nStarting a shell.  Press Esc to power off the machine:\r\n");

#if 0
    u64 val;
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PERFEVTSELx_MSR_BASE));
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PERFEVTSELx_MSR_BASE+1));
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PMCx_MSR_BASE));
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PMCx_MSR_BASE+1));
    val = 0x2e | (0x4f << 8) |PERFEVTSELx_EN |PERFEVTSELx_OS|PERFEVTSELx_USR;
    __asm__ __volatile__ ("wrmsr" :: "a"((u32)val), "d"(val>>32), "c"(PERFEVTSELx_MSR_BASE));
    val = 0x2e | (0x41 << 8) |PERFEVTSELx_EN |PERFEVTSELx_OS|PERFEVTSELx_USR;
    __asm__ __volatile__ ("wrmsr" :: "a"((u32)val), "d"(val>>32), "c"(PERFEVTSELx_MSR_BASE+1));

    u32 *data = kmalloc(10000*10000*sizeof(u32));
    int i,j;
    int x;
    for ( i = 0; i < 10000*10000; i++ ) {
        data[i] = i;
    }
    for ( i = 0; i < 10000*10000; i++ ) {
        x = data[i];
    }
    kfree(data);

    u32 a,d;
    __asm__ __volatile__ ("rdpmc" : "=a"(a), "=d"(d) : "c"(1));
    kprintf("RDPMC %x %x\r\n", a, d);
#endif


    struct ktask *t;

    t = ktask_alloc();
    t->main = &proc_shell;
    t->argc = 0;
    t->argv = NULL;

    ktask_enqueue(t);
    sched();
    task_restart();

    halt();
}

/*
 * Entry point for kernel task
 */
void
ktask_entry(struct ktask *t)
{
    int ret;

    /* Get the arguments from the stack pointer */
    ret = t->main(t->argc, t->argv);

    /* ToDo: Handle return value */

    /* Avoid returning to wrong point (no returning point in the stack) */
    halt();
}

/*
 * Allocate a task
 */
struct ktask *
ktask_alloc(void)
{
    struct ktask *t;

    t = kmalloc(sizeof(struct ktask));
    if ( NULL == t ) {
        return NULL;
    }
    t->id = 0;
    t->name = NULL;
    t->pri = 0;
    t->cred = 0;
    t->state = 0;
    t->arch = arch_alloc_task(t, &ktask_entry, TASK_POLICY_KERNEL);

    t->main = NULL;
    t->argc = 0;
    t->argv = NULL;

    return t;
}

/*
 * Enqueue a kernel task
 */
int
ktask_enqueue(struct ktask *t)
{
    int ntail;
    int ret;

    arch_spin_lock(&ktask_lock);

    ntail = (ktaskq->tail + 1) % ktaskq->nent;
    if ( ktaskq->head == ntail ) {
        /* Full */
        ret = -1;
    } else {
        ret = ktaskq->tail;
        ktaskq->entries[ktaskq->tail].ktask = t;
        ktaskq->tail = ntail;
    }

    arch_spin_unlock(&ktask_lock);

    return ret;
}

/*
 * Dequeue a kernel task
 */
struct ktask *
ktask_dequeue(void)
{
    struct ktask *t;

    arch_spin_lock(&ktask_lock);

    t = NULL;
    if ( ktaskq->head != ktaskq->tail ) {
        t = ktaskq->entries[ktaskq->head].ktask;
        mfence();
        ktaskq->head = (ktaskq->head + 1) % ktaskq->nent;
    }

    arch_spin_unlock(&ktask_lock);

    return t;
}

/*
 * Scheduler
 */
void
sched(void)
{
    struct ktask *t;

    t = arch_get_next_task();
    if ( NULL != t ) {
        /* Already scheduled */
        return;
    }
    t = ktask_dequeue();
    if ( NULL == t ) {
        /* No task to be scheduled */
        return;
    }
    arch_set_next_task(t);
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
        /* Run task scheduler */
        sched();
        break;
    default:
        ;
    }
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

    arch_dbg_printf("Looking up routing table\r\n");
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
    arch_dbg_printf("done : %llx\r\n", tmp);


    return 0;
}
#endif

/*
 * Idle process
 */
int
kproc_idle_main(int argc, const char *const argv[])
{
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
