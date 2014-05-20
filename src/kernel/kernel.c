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

/*
 * Entry point to C function for BSP called from asm.s
 */
void
kmain(void)
{
    /* Initialize the lock varialbe */
    lock = 0;

    /* Initialize kmem */
    kmem_init();

    /* Initialize keyboard */
    kbd_init();

    /* Initialize architecture-related devices */
    arch_bsp_init();

    /* Wait for 10ms */
    arch_busy_usleep(10000);

    /* Initialize the scheduler */
    sched_init();

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

    struct ktask *t;

    t = ktask_alloc();
    t->main = &proc_shell;
    t->argc = 0;
    t->argv = NULL;

    sched_ktask_enqueue(t);
    sched();
    task_restart();
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
 * Entry point to C function for AP called from asm.s
 */
void
apmain(void)
{
    /* Initialize this AP */
    arch_ap_init();

    struct ktask *t;

    t = ktask_alloc();
    t->main = &ktask_idle_main;
    t->argc = 0;
    t->argv = NULL;

    arch_set_next_task(t);

    task_restart();
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
 * IPI
 */
void
kintr_ipi(void)
{
}

/*
 * Interrupt service routine for all vectors
 */
void lapic_send_fixed_ipi(u8 vector);
int this_cpu();
void
kintr_isr(u64 vec)
{
    switch ( vec ) {
    case IV_TMR:
        kintr_int32();
        break;
    case IV_KBD:
        kintr_int33();
        lapic_send_fixed_ipi(0x51);
        break;
    case IV_LOC_TMR:
        kintr_loc_tmr();
        /* Run task scheduler */
        sched();
        break;
    case IV_IPI:
        kintr_ipi();
        break;
    default:
        ;
    }
}

/*
 * Idle process
 */
int
ktask_idle_main(int argc, const char *const argv[])
{
    halt();

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
