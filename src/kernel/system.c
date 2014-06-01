/*_
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include <aos/types.h>
#include "kernel.h"

/*
 * Read n bytes from the file descriptor and store to buf
 */
ssize_t
read(int fd, void *buf, size_t n)
{
    struct ktask *self;
    struct ktask *src;

    /* Disable interrupts */
    arch_disable_interrupts();

    /* Get current task */
    self = arch_get_current_task();

    /* Get the source task */

    if ( TASK_STATE_BLOCKED == src->state ) {
        /* Change the state to ready */
        src->state = TASK_STATE_READY;
    } else {
        /* Change the state to blocked, and wait until the source writes here */
        self->state = TASK_STATE_BLOCKED;
    }

    /* Enable interrupts */
    arch_enable_interrupts();

    kmemcpy(buf, self->msg.u.data.buf, n);


    return -1;
}

/*
 * Write n bytes to the file descriptor from buf
 */
ssize_t
write(int fd, const void *buf, size_t n)
{
    struct ktask *self;
    struct ktask *dst;

    /* Disable interrupts */
    arch_disable_interrupts();

    /* Get current task */
    self = arch_get_current_task();

    /* Get the destination task */

    if ( TASK_STATE_BLOCKED == dst->state ) {
        /* Change the state to ready */
        dst->state = TASK_STATE_READY;
    } else {
        /* Change the state to blocked, and wait until the destination reads
           this */
        self->state = TASK_STATE_BLOCKED;
    }

    /* Enable interrupts */
    arch_enable_interrupts();

    return -1;
}


/*
 * Semaphore
 */
struct semaphore *
semaphore_new(void)
{
    struct semaphore * sem;

    sem = kmalloc(sizeof(struct semaphore));
    if ( NULL == sem ) {
        return NULL;
    }

    sem->lock = 0;
    sem->count = 0;
    sem->waiting.head = NULL;
    sem->waiting.tail = NULL;

    return sem;
}
int
semaphore_up(struct semaphore *sem)
{
    arch_spin_lock_intr(&sem->lock);
    if ( sem->count <= 0 ) {
        /* Execute one of the waiting list */
        if ( NULL != sem->waiting.head ) {
            //notify
        }
    } else {
        sem->count++;
    }
    arch_spin_unlock_intr(&sem->lock);

    return 0;
}
int
semaphore_down(struct semaphore *sem, struct ktask_queue_entry *e)
{
    arch_spin_lock_intr(&sem->lock);

    if ( sem->count <= 0 ) {
        /* Enqueue */
        e->next = NULL;
        /* Append the task to the queue */
        if ( NULL == sem->waiting.tail ) {
            /* Enqueue and execute */
            sem->waiting.head = e;
            sem->waiting.tail = e;
        } else {
            sem->waiting.tail->next = e;
            sem->waiting.tail = e;
        }
    } else {
        sem->count--;
    }

    arch_spin_unlock_intr(&sem->lock);

    return 0;
}

/*
 * Message
 */
int
kmsg_send(struct ktask *dst, struct kmsg *msg)
{
    struct ktask *self;

    /* Disable interrupts */
    arch_disable_interrupts();

    /* Get current task */
    self = arch_get_current_task();

    /* Enable interrupts */
    arch_enable_interrupts();

    return 0;
}

int
kmsg_recv(struct ktask *src, struct kmsg *msg)
{
    struct ktask *self;

    /* Disable interrupts */
    arch_disable_interrupts();

    /* Get current task */
    self = arch_get_current_task();

    /* Enable interrupts */
    arch_enable_interrupts();

    return 0;
}


void
shalt(void)
{
    arch_scall(1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
