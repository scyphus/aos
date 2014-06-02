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
void kbd_event(void);
int kbd_read(void);


int irq_handler_table_init(void);


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

    /* Initialize IRQ handlers */
    irq_handler_table_init();

    /* Print out a message */
    kprintf("\r\nStarting a shell.  Press Esc to power off the machine:\r\n");

    syscall_init();

    for ( i = 0; i < processors->n; i++ ) {
        t = ktask_alloc(TASK_POLICY_KERNEL);
        t->main = &ktask_idle_main;
        t->id = -1;
        t->name = "[idle]";
        t->argv = NULL;
        t->state = TASK_STATE_READY;
        processors->prs[i].idle = t;
    }
    ktask_init();


    //sched_ktask_enqueue(ktask_queue_entry_new(processors->prs[processors->map[this_cpu()]].idle));

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

    //sched();
    arch_set_next_task(processors->prs[processors->map[this_cpu()]].idle);
    task_restart();
}



#define SYSCALL_MAX_NR  0x10
#define SYSCALL_HLT     1
#define SYSCALL_READ    2
#define SYSCALL_WRITE   3

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
    __asm__ __volatile__ ("sti;hlt;cli;");
}

void
syscall_read(void)
{
    kprintf("XXXX\r\n");
}

void
syscall_write(void)
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
    syscall_table[2].func = &syscall_read;
    syscall_table[3].func = &syscall_write;

    /* Setup */
    syscall_setup();
}


/*
 * Initialize IRQ handler table
 */
int
irq_handler_table_init(void)
{
    int i;

    for ( i = 0; i <= IRQ_MAX; i++ ) {
        irq_handler_table[i].handler = NULL;
        irq_handler_table[i].user = NULL;
    }

    return 0;
}

/*
 * Register an interrupt service routine
 */
int
register_isr(int nr, void (*isr)(void))
{
    return 0;
}

int
register_irq_handler(int irq, void (*handler)(int, void *), void *user)
{
    /* Checl the IRQ # */
    if ( irq < 0 || irq > IRQ_MAX ) {
        return -1;
    }
    /* Check if the handler is already registered */
    if ( irq_handler_table[irq].handler ) {
        /* Already registered */
        return -1;
    }

    irq_handler_table[irq].handler = handler;
    irq_handler_table[irq].user = user;

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
    /* FIXME: Separate IRQ from this switch block */
    switch ( vec ) {
    case IV_IRQ0:
        if ( irq_handler_table[0].handler ) {
            irq_handler_table[0].handler(0, irq_handler_table[0].user);
        }
        kintr_int32();
        break;
    case IV_IRQ1:
        if ( irq_handler_table[1].handler ) {
            irq_handler_table[1].handler(1, irq_handler_table[1].user);
        }
        break;
    case IV_IRQ2:
        if ( irq_handler_table[2].handler ) {
            irq_handler_table[2].handler(2, irq_handler_table[2].user);
        }
        break;
    case IV_IRQ3:
        if ( irq_handler_table[3].handler ) {
            irq_handler_table[3].handler(2, irq_handler_table[3].user);
        }
        break;
    case IV_IRQ32:
        if ( irq_handler_table[32].handler ) {
            irq_handler_table[32].handler(32, irq_handler_table[32].user);
        }
        sched();
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
