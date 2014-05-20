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
        /* No task to be scheduled */
        return;
    }
    arch_set_next_task(t);
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
