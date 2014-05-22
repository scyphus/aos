/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 * Copyright (c) 2014 Hirochika Asai
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"

static volatile int ktask_lock;
static struct ktask_queue *ktaskq;
static struct ktask_table *ktasks;

/*
 * Initialize the scheduler
 */
int
sched_init(void)
{
    ktask_lock = 0;

    /* Initialize kernel task queue */
    ktaskq = kmalloc(sizeof(struct ktask_queue));
    if ( NULL == ktaskq ) {
        /* Print out a message */
        kprintf("Error on memory allocation for task queue\r\n");
        return -1;
    }
    ktaskq->nent = TASK_QUEUE_LEN;
    ktaskq->entries = kmalloc(sizeof(struct ktask_queue_entry) * ktaskq->nent);
    if ( NULL == ktaskq->entries ) {
        /* Print out a message */
        kprintf("Error on memory allocation for task queue entries\r\n");
        return -1;
    }
    ktaskq->head = 0;
    ktaskq->tail = 0;

    return 0;
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
    t = sched_ktask_dequeue();
    if ( NULL == t ) {
        if ( NULL == arch_get_current_task() ) {
            /* No task is running, then schedule idle process */
            arch_set_next_task(ktasks->idle);
            return;
        } else {
            /* No task to be scheduled */
            return;
        }
    }
    //kprintf("xxx %x\r\n", t);
    arch_set_next_task(t);
    if ( t != ktasks->idle ) {
        sched_ktask_enqueue(t);
    }
}

/*
 * Enqueue a kernel task
 */
int
sched_ktask_enqueue(struct ktask *t)
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
        mfence();
        ktaskq->tail = ntail;
    }

    arch_spin_unlock(&ktask_lock);

    return ret;
}

/*
 * Dequeue a kernel task
 */
struct ktask *
sched_ktask_dequeue(void)
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
 * Kernel task
 */
int
ktask_init(void)
{
    char **argv;

    /* Prepare arguments */
    argv = kmalloc(sizeof(char *) * 1);
    if ( NULL == argv ) {
        return -1;
    }
    argv[0] = kstrdup("idle");
    if ( NULL == argv[0] ) {
        kfree(argv);
        return -1;
    }

    /* Allocate memory for new task */
    ktasks = kmalloc(sizeof(struct ktask_table));
    if ( NULL == ktasks ) {
        return -1;
    }

    /* Idle task */
    ktasks->idle = ktask_alloc();
    ktasks->idle->main = &ktask_idle_main;
    ktasks->idle->argc = 1;
    ktasks->idle->argv = argv;
    ktasks->idle->id = -1;


    /* Prepare arguments */
    argv = kmalloc(sizeof(char *) * 1);
    if ( NULL == argv ) {
        return -1;
    }
    argv[0] = kstrdup("kernel");
    if ( NULL == argv[0] ) {
        kfree(argv);
        return -1;
    }

    /* Kernel task */
    ktasks->kernel = ktask_alloc();
    ktasks->kernel->main = &ktask_kernel_main;
    ktasks->kernel->argc = 1;
    ktasks->kernel->argv = argv;
    ktasks->kernel->id = 0;

    sched_ktask_enqueue(ktasks->kernel);

    return 0;
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
    arch_idle();
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
 * Init
 */
int
ktask_kernel_main(int argc, char *argv[])
{
    while ( 1 ) {
        //sched_ktask_enqueue(ktasks->idle);
        //sched_ktask_enqueue(ktasks->kernel);
        //arch_clock_update();
        hlt1();
    }

    return 0;
}

/*
 * Init
 */
int
ktask_init_main(int argc, char *argv[])
{
    return 0;
}

/*
 * Idle task
 */
int
ktask_idle_main(int argc, char *argv[])
{
    arch_idle();

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
