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
extern struct processor_table *processors;

struct syscall *syscall_table;

/* FIXME */
struct ptcam *tcam;
struct dxr *dxr;

/*
 * Temporary: Keyboard drivers
 */
void kbd_event(void);
int kbd_read(void);
void e1000_init(void);

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

    /* Initialize architecture-related devices */
    arch_bsp_init();

    /* Wait for 10ms */
    arch_busy_usleep(10000);

    /* Initialize processor table */
    if ( processor_init() < 0 ) {
        kprintf("Error on processor initialization\r\n");
        return;
    }

    /* Initialize the scheduler */
    sched_init();

    /* Initialize the random number generator */
    rng_init();
    rng_stir();

    /* Initialize IRQ handlers */
    irq_handler_table_init();

    /* Print out a message */
    kprintf("\r\nStarting a shell.  Press Esc to power off the machine:\r\n");

    /* FIXME */
    tcam = ptcam_init();
#if DXR
    dxr = dxr_init();
#endif

    syscall_init();

    e1000_init();

    ktask_init();

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
    //kprintf("Received IPI #%d\r\n", this_cpu());
    //sched();
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
            irq_handler_table[3].handler(3, irq_handler_table[3].user);
        }
        break;
    case IV_IRQ(16):
        if ( irq_handler_table[16].handler ) {
            irq_handler_table[16].handler(16, irq_handler_table[16].user);
        }
        break;
    case IV_IRQ(17):
        if ( irq_handler_table[17].handler ) {
            irq_handler_table[17].handler(17, irq_handler_table[17].user);
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
