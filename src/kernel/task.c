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

#define DEFAULT_CREDIT 8

static volatile int ktask_lock;
static volatile int ktask_fork_lock;
static struct ktask_queue *ktaskq;
static struct ktask_table *ktasks;
static struct ktltask_table *ktltasks;

extern struct processor_table *processors;

int ktask_idle_main(int argc, char *argv[]);
int ktask_init_main(int argc, char *argv[]);
void ktask_free(struct ktask *);

/* FIXME: To be moved to somewhere else */
int kbd_driver_main(int argc, char *argv[]);

/*
 * Initialize the scheduler
 */
int
sched_init(void)
{
    ktask_lock = 0;
    ktask_fork_lock = 0;

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
void
sched_switch(void)
{
    struct ktask_queue_entry *e;
    struct ktask *t;

    /* Obtain the current task */
    t = arch_get_current_task();
    if ( NULL != t && TASK_STATE_RUNNING == t->state ) {
        /* Still running, then decrement the preempted credit */
        t->cred--;
        if ( t->cred > 0 ) {
            return;
        }
    }

    /* Check if the next task is already scheduled */
    t = arch_get_next_task();
    if ( NULL != t ) {
        /* Already scheduled, then task_restart() will run the scheduled task */
        return;
    }

    /* Dequeue a task */
    e = sched_ktask_dequeue();
    if ( NULL == e ) {
        if ( NULL == arch_get_current_task() ) {
            /* No task is running, then schedule the idle process */
            processor_this()->idle->cred = DEFAULT_CREDIT;
            arch_set_next_task(processor_this()->idle);
            return;
        } else {
            /* No task to be scheduled */
            return;
        }
    }

    /* If the task is not ready, enqueue it and dequeue another one. */
    t = arch_get_current_task();
    while ( TASK_STATE_READY != e->ktask->state ) {
        /* Schedule it again */
        e->ktask->scheduled = -1;
        e = sched_ktask_dequeue();
    }

    /* Set the next task */
    e->ktask->cred = DEFAULT_CREDIT;
    arch_set_next_task(e->ktask);
}

/*
 * Run scheduling
 */
void
sched(void)
{
    int i;

    /* Run scheduling from task table */
    for ( i = 0; i < TASK_TABLE_SIZE; i++ ) {
        if ( NULL == ktasks->tasks[i].ktask
             && ktasks->tasks[i].ktask->scheduled < 0 ) {
            /* Schedule here */
            if ( TASK_STATE_READY == ktasks->tasks[i].ktask->state ) {
                sched_ktask_enqueue(&ktasks->tasks[i]);
            }
        }
    }
}

/*
 * Prepare tickless processor
 */
void
sched_tickless_prepare(void)
{
    arch_set_next_task(processor_this()->idle);
}

/*
 * Enqueue a kernel task
 */
int
sched_ktask_enqueue(struct ktask_queue_entry *e)
{
    int ret;

    ret = 0;
    arch_spin_lock(&ktask_lock);

    e->ktask->scheduled = 1;

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
    int ret;
    int i;
    struct ktask *t;

    /* Assign an idle task to each processor */
    for ( i = 0; i < processors->n; i++ ) {
        t = ktask_alloc(TASK_POLICY_KERNEL);
        t->main = &ktask_idle_main;
        t->id = -1;
        t->name = "[idle]";
        t->argv = NULL;
        t->state = TASK_STATE_READY;
        processors->prs[i].idle = t;
    }

    /* Allocate and initialize the tickless task table */
    ktltasks = kmalloc(sizeof(struct ktltask_table));
    if ( NULL == ktltasks ) {
        return -1;
    }
    (void)kmemset(ktltasks->tasks, 0,
                  sizeof(struct ktask *) * TL_TASK_TABLE_SIZE);

    /* Allocate and initialize the task table */
    ktasks = kmalloc(sizeof(struct ktask_table));
    if ( NULL == ktasks ) {
        kfree(ktltasks);
        return -1;
    }
    (void)kmemset(ktasks->tasks, 0,
                  sizeof(struct ktask_queue_entry *) * TASK_TABLE_SIZE);

    /* Initialize the kernel main task */
    argv = kmalloc(sizeof(char *) * 2);
    if ( NULL == argv ) {
        kfree(ktasks);
        kfree(ktltasks);
        return -1;
    }
    argv[0] = kstrdup("kernel");
    argv[1] = NULL;
    if ( NULL == argv[0] ) {
        kfree(argv);
        kfree(ktasks);
        kfree(ktltasks);
        return -1;
    }
    ret = ktask_fork_execv(TASK_POLICY_KERNEL, &ktask_kernel_main, argv);
    if ( ret < 0 ) {
        kfree(argv[0]);
        kfree(argv);
        kfree(ktasks);
        kfree(ktltasks);
        return -1;
    }

    return 0;
}

/*
 * New task
 */
int
ktask_fork_execv(int policy, int (*main)(int, char *[]), char **argv)
{
    struct ktask *t;
    int i;
    int tid;

    /* Disable interrupts and lock */
    arch_spin_lock_intr(&ktask_fork_lock);

    /* Search available PID from the table */
    tid = -1;
    for ( i = 0; i < TASK_TABLE_SIZE; i++ ) {
        if ( NULL ==  ktasks->tasks[i].ktask ) {
            /* Found an available space */
            tid = i;
            break;
        }
    }

    /* Corresponding task ID found? */
    if ( tid < 0 ) {
        arch_spin_unlock_intr(&ktask_fork_lock);
        return -1;
    }

    /* Allocate task */
    t = ktask_alloc(policy);
    if ( NULL == t ) {
        arch_spin_unlock_intr(&ktask_fork_lock);
        return -1;
    }
    t->main = main;
    t->argv = argv;
    t->id = tid;
    t->name = NULL;
    t->state = TASK_STATE_READY;

    ktasks->tasks[tid].ktask = t;
    t->qe = &ktasks->tasks[tid];

    /* Enqueue to the scheduler */
    sched_ktask_enqueue(&ktasks->tasks[tid]);

    /* Unlock and enable interrupts */
    arch_spin_unlock_intr(&ktask_fork_lock);

    return 0;
}

/*
 * Change the state of a kernel task
 */
int
ktask_change_state(struct ktask *t, int state)
{
    int old;

    old = t->state;
    t->state = state;

    if ( TASK_STATE_READY != old && TASK_STATE_READY == state ) {
        /* Not ready => Ready, then schedule this */
        /* FIXME: This should be done in the scheduler */
        //sched_ktask_enqueue(t->qe);
    }

    return 0;
}

/*
 * Entry point for kernel task
 */
void
ktask_entry(struct ktask *t)
{
    int ret;
    int argc;
    char **tmp;

    argc = 0;
    if ( NULL != t->argv ) {
        tmp = t->argv;
        while ( NULL != *tmp ) {
            argc++;
            tmp++;
        }
    }

    /* Get the arguments from the stack pointer */
    ret = t->main(argc, t->argv);

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
    t->scheduled = -1;
    t->state = TASK_STATE_ALLOC;
    t->arch = arch_alloc_task(t, &ktask_entry, policy);

    t->main = NULL;
    t->argv = NULL;

    return t;
}

/*
 * Free a task
 */
void
ktask_free(struct ktask *t)
{
    arch_free_task(t->arch);
    kfree(t);
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
 * New tickless task
 */
int
ktltask_fork_execv(int policy, int pid, int (*main)(int, char *[]), char **argv)
{
    struct ktask *t;
    int i;
    int tid;

    /* Disable interrupts and lock */
    arch_spin_lock_intr(&ktask_fork_lock);

    /* Search available PID from the table */
    tid = -1;
    for ( i = 0; i < TL_TASK_TABLE_SIZE; i++ ) {
        if ( NULL ==  ktltasks->tasks[i] ) {
            /* Found an available space */
            tid = i;
            break;
        }
    }

    /* Corresponding task ID found? */
    if ( tid < 0 ) {
        arch_spin_unlock_intr(&ktask_fork_lock);
        return -1;
    }

    /* Allocate task */
    t = ktask_alloc(policy);
    if ( NULL == t ) {
        arch_spin_unlock_intr(&ktask_fork_lock);
        return -1;
    }
    t->main = main;
    t->argv = argv;
    t->id = tid;
    t->name = NULL;
    t->state = TASK_STATE_READY;

    ktltasks->tasks[tid] = t;

    /* Enqueue to the scheduler */
    arch_set_next_task_other_cpu(ktltasks->tasks[tid], pid);

    /* Unlock and enable interrupts */
    arch_spin_unlock_intr(&ktask_fork_lock);

    /* Run at the processor */
    lapic_send_ns_fixed_ipi(pid, IV_IPI);

    return 0;
}


/*
 * New tickless task
 */
int
ktltask_stop(int pid)
{
    /* Stop command */
    processor_get(pid)->idle->cred = 16;
    arch_set_next_task_other_cpu(processor_get(pid)->idle, pid);
    /* IPI */
    lapic_send_ns_fixed_ipi(pid, IV_IPI);

    return 0;
}



/*
 * Kernel main task
 */
int
ktask_kernel_main(int argc, char *argv[])
{
    int tid;

    /* Launch the keyboard driver */
    tid = ktask_fork_execv(TASK_POLICY_DRIVER, &kbd_driver_main, NULL);
    if ( tid < 0 ) {
        kprintf("Cannot fork-exec keyboard driver.\r\n");
    }
    /* Launch the shell process */
    tid = ktask_fork_execv(TASK_POLICY_KERNEL, &shell_main, NULL);
    if ( tid < 0 ) {
        kprintf("Cannot fork-exec a shell.\r\n");
    }

#if 0
    /* Notify to all processors except for BSP */
    int i;
    for ( i = i; i < processors->n; i++ ) {
        //lapic_send_fixed_ipi(IV_IPI);
        lapic_send_ns_fixed_ipi(processors->prs[i].id, IV_IPI);
    }
#endif

    while ( 1 ) {
#if 0
        /* Run scheduling from task table */
        for ( i = 0; i < TASK_TABLE_SIZE; i++ ) {
            if ( NULL == ktasks->tasks[i].ktask
                 && ktasks->tasks[i].ktask->scheduled < 0 ) {
                /* Schedule here */
                if ( TASK_STATE_READY == ktasks->tasks[i].ktask->state ) {
                    sched_ktask_enqueue(&ktasks->tasks[i]);
                }
            }
        }
#endif
        arch_scall(SYSCALL_HLT);
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
    for ( ;; ) {
        /* Execute the architecture-specific idle procedure */
        arch_idle();
    }

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
