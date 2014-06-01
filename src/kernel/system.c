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
    return -1;
}

/*
 * Write n bytes to the file descriptor from buf
 */
ssize_t
write(int fd, const void *buf, size_t n)
{
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
    sem->count--;
    arch_spin_unlock_intr(&sem->lock);

    return 0;
}
int
semaphore_down(struct semaphore *sem, struct ktask_queue_entry *e)
{
    int ret;

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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
