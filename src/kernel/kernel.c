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
struct processor_table *processors;

struct syscall *syscall_table;

/*
 * Temporary: Keyboard drivers
 */
void kbd_init(void);
void kbd_event(void);
int kbd_read(void);

/* FIXME: To be moved to somewhere else */
int kbd_driver_main(int argc, char *argv[]);


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

    /* Initialize architecture-related devices */
    arch_bsp_init();

    /* Initialize keyboard */
    kbd_init();

    /* Wait for 10ms */
    arch_busy_usleep(10000);

    /* Initialize processor table */
    int i;
    int npr;
    struct ktask *t;
    npr = 0;
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        if ( arch_cpu_active(i) ) {
            npr++;
        }
    }
    processors = kmalloc(sizeof(struct processor_table));
    processors->n = npr;
    processors->prs = kmalloc(sizeof(struct processor) * npr);
    npr = 0;
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        if ( arch_cpu_active(i) ) {
            if ( processors->n <= npr ) {
                /* Error */
                kprintf("Error on processor initialization\r\n");
                return;
            }
            processors->prs[npr].id = i;
            if ( i == 0 ) {
                processors->prs[npr].type = PROCESSOR_BSP;
            } else {
                processors->prs[npr].type = PROCESSOR_AP_TICKLESS;
            }
            processors->map[i] = npr;

            npr++;
        }
    }

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

    syscall_init();

    ktask_init();
    for ( i = 0; i < processors->n; i++ ) {
        t = ktask_alloc(TASK_POLICY_KERNEL);
        t->main = &ktask_idle_main;
        t->id = -1;
        t->name = "[idle]";
        t->argc = 0;
        t->argv = NULL;
        t->state = TASK_STATE_READY;
        processors->prs[i].idle = t;
    }

    sched_ktask_enqueue(ktask_queue_entry_new(processors->prs[processors->map[this_cpu()]].idle));

    t = ktask_alloc(TASK_POLICY_KERNEL);
    t->main = &proc_shell;
    t->id = 1;
    t->name = "[shell]";
    t->argc = 0;
    t->argv = NULL;
    t->state = TASK_STATE_READY;
    sched_ktask_enqueue(ktask_queue_entry_new(t));

    t = ktask_alloc(TASK_POLICY_KERNEL);
    t->main = &kbd_driver_main;
    t->id = 2;
    t->name = "[kbd]";
    t->argc = 0;
    t->argv = NULL;
    t->state = TASK_STATE_READY;
    sched_ktask_enqueue(ktask_queue_entry_new(t));

    sched();
    task_restart();
}

/*
 * Entry point to C function for AP called from asm.s
 */
void
apmain(void)
{
    /* Initialize this AP */
    arch_ap_init();

    sched();
    task_restart();
}



#define SYSCALL_MAX_NR  0x10
#define SYSCALL_HLT     1

/*
 * System calls
 */
struct syscall {
    void (*func)(void);
} __attribute__ ((packed));

/*
 * Dummy function for syscall
 */
void
syscall_dummy(void)
{
}

void
syscall_hlt(void)
{
    __asm__ __volatile__ ("hlt");
}

void
syscall_test(void)
{
    kprintf("XXXX\r\n");
}


/*
 * Initialize syscall table
 */
void
syscall_init(void)
{
    u64 sz = (SYSCALL_MAX_NR + 1) * sizeof(struct syscall);
    u64 i;

    syscall_table = (struct syscall *)kmalloc(((sz-1)/PAGESIZE + 1) * PAGESIZE);
    for ( i = 0; i <= SYSCALL_MAX_NR; i++ ) {
        syscall_table[i].func = &syscall_dummy;
    }
    syscall_table[1].func = &syscall_hlt;
    syscall_table[2].func = &syscall_test;

    /* Setup */
    syscall_setup();
}




/*
 * ktask_switched
 */
void
ktask_switched(struct ktask *t)
{
    //kprintf("XXXXXXXX %d\r\n", t->id);
    t->state = TASK_STATE_READY;
}


/*
 * Register an interrupt service routine
 */
int
register_isr(int nr, void (*isr)(void))
{
    return 0;
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
    sched();
    //mfence();
}

/*
 * IPI
 */
void
kintr_ipi(void)
{
    sched();
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
        //sched();
        break;
    case IV_IPI:
        kintr_ipi();
        break;
    default:
        ;
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
