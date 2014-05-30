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

static volatile int ktask_lock;
static struct ktask_queue *ktaskq;
static struct ktask_table *ktasks;

int ktask_idle_main(int argc, char *argv[]);
int ktask_init_main(int argc, char *argv[]);

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
    ktaskq->head = NULL;
    ktaskq->tail = NULL;

    return 0;
}

/*
 * Scheduler
 */
extern struct processor_table *processors;
void
sched(void)
{
    struct ktask_queue_entry *e;
    struct ktask *t;

    /* Obtain the current task */
    t = arch_get_current_task();
    if ( NULL != t && TASK_STATE_RUNNING == t->state ) {
        /* Decrement the credit */
        t->cred--;
        if ( t->cred > 0 ) {
            return;
        }
    }

    /* Check if the next task is already scheduled */
    t = arch_get_next_task();
    if ( NULL != t ) {
        /* Already scheduled */
        return;
    }

    /* Dequeue a task */
    e = sched_ktask_dequeue();
    if ( NULL == e ) {
        if ( NULL == arch_get_current_task() ) {
            /* No task is running, then schedule idle process */
            processors->prs[processors->map[this_cpu()]].idle->cred = 16;
            arch_set_next_task(processors->prs[processors->map[this_cpu()]].idle);
            return;
        } else {
            /* No task to be scheduled */
            return;
        }
    }

    /* If the task is not ready, enqueue it and dequeue another one. */
    while ( TASK_STATE_READY != e->ktask->state ) {
        /* Schedule it again */
        sched_ktask_enqueue(e);
        e = sched_ktask_dequeue();
    }

    /* Set the next task */
    e->ktask->cred = 16;
    arch_set_next_task(e->ktask);
    e->ktask->state = TASK_STATE_RUNNING;
    sched_ktask_enqueue(e);
}

/*
 * Enqueue a kernel task
 */
int
sched_ktask_enqueue(struct ktask_queue_entry *e)
{
    int ret;

    arch_spin_lock(&ktask_lock);

    e->next = NULL;
    if ( NULL == ktaskq->tail ) {
        /* Empty */
        ktaskq->head = e;
        ktaskq->tail = e;
    } else {
        ktaskq->tail->next = e;
        ktaskq->tail = e;
    }

    arch_spin_unlock(&ktask_lock);

    return ret;
}

/*
 * Dequeue a kernel task
 */
struct ktask_queue_entry *
sched_ktask_dequeue(void)
{
    struct ktask_queue_entry *e;

    arch_spin_lock(&ktask_lock);

    e = ktaskq->head;
    if ( NULL != e ) {
        ktaskq->head = e->next;
        if ( NULL == ktaskq->head ) {
            ktaskq->tail = NULL;
        }
    }

    arch_spin_unlock(&ktask_lock);

    return e;
}

/*
 * Create new task queue entry
 */
struct ktask_queue_entry *
ktask_queue_entry_new(struct ktask *t)
{
    struct ktask_queue_entry *e;

    /* Allocate memory space for a task queue entry */
    e = kmalloc(sizeof(struct ktask_queue_entry));
    if ( NULL == e ) {
        return NULL;
    }
    e->ktask = t;
    e->next = NULL;

    return e;
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
    argv[0] = kstrdup("kernel");
    if ( NULL == argv[0] ) {
        kfree(argv);
        return -1;
    }

    /* Allocate memory for new task */
    ktasks = kmalloc(sizeof(struct ktask_table));
    if ( NULL == ktasks ) {
        kfree(argv[0]);
        kfree(argv);
        return -1;
    }
    /* Kernel task */
    ktasks->kernel = ktask_alloc(TASK_POLICY_KERNEL);
    ktasks->kernel->main = &ktask_kernel_main;
    ktasks->kernel->argc = 1;
    ktasks->kernel->argv = argv;
    ktasks->kernel->id = 0;
    ktasks->kernel->name = "[kernel]";
    ktasks->kernel->state = TASK_STATE_READY;

    sched_ktask_enqueue(ktask_queue_entry_new(ktasks->kernel));

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

    /* Set return code */
    t->ret = ret;

    /* Avoid returning to wrong point (no returning point in the stack) */
    arch_idle();
}


/*
 * Allocate a task
 */
struct ktask *
ktask_alloc(int policy)
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
    t->state = TASK_STATE_READY;
    t->arch = arch_alloc_task(t, &ktask_entry, policy);

    t->main = NULL;
    t->argc = 0;
    t->argv = NULL;

    return t;
}


/*
 * Add a task to task table
 */
struct ktask *
ktask_new(char *name, int (*main)(int, char **))
{
    return NULL;
}

/*
 * Exit a task
 */
int
ktask_destroy(struct ktask *t)
{
    return 0;
}


/*
 * Init
 */
int
ktask_kernel_main(int argc, char *argv[])
{
    u64 x;

    while ( 1 ) {
        arch_clock_update();
        arch_scall(1);
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
