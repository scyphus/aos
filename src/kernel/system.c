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

#define FD_SIZE  0x10
#define FD_BUF  4096 * 16

/* File descriptor: character device */
struct filedesc {
    struct ktask *owner;
    int stat;
    u8 *buf;
    int rpos;
    int wpos;
    int n;
};

struct filedesc fds[FD_SIZE];

/*
 * Open a file
 */
int
open(const char *path, int oflag)
{
    int fd;
    struct ktask *self;

    /* Disable interrupts */
    arch_disable_interrupts();

    if ( 0 == kstrcmp("/dev/kbd", path) ) {
        fd = 0;
    } else {
        fd = -1;
    }

    /* Get current task */
    self = arch_get_current_task();

    if ( fd >= FD_SIZE ) {
        fd = -1;
    }

    if ( fd >= 0 ) {
        fds[fd].buf = kmalloc(FD_BUF);
        if ( NULL != fds[fd].buf ) {
            fds[fd].n = FD_BUF;
            fds[fd].owner = self;
            fds[fd].rpos = 0;
            fds[fd].wpos = 0;
            fds[fd].stat = 0;
        } else {
            fd = -1;
        }
    }

    /* Enable interrupts */
    arch_enable_interrupts();

    return fd;
}


/*
 * Read n bytes from the file descriptor and store to buf
 */
ssize_t
read(int fd, void *buf, size_t n)
{
    struct ktask *self;
    ssize_t nr;

    for ( ;; ) {
        /* Disable interrupts */
        arch_disable_interrupts();

        /* Check whether it is open */
        if ( fd < 0 || fd >= FD_SIZE ) {
            /* Enable interrupts */
            arch_enable_interrupts();
            return -1;
        }
        if ( NULL == fds[fd].owner ) {
            /* Enable interrupts */
            arch_enable_interrupts();
            return -1;
        }

        if ( fds[fd].rpos == fds[fd].wpos ) {
            /* No buffered data, then block the current task */
            self = arch_get_current_task();
            self->state = TASK_STATE_BLOCKED;
            /* Enable interrupts */
            arch_enable_interrupts();
            /* FIXME: Invokes a context switch  */
            __asm__ __volatile__ ("int $0x40");
        } else {
            /* FIXME: Check if the buffer becomes not full to tell it to
               the corresponding kernel process */
            if ( fds[fd].wpos > fds[fd].rpos ) {
                nr = fds[fd].wpos - fds[fd].rpos;
                if ( nr > n ) {
                    nr = n;
                }
                kmemcpy(buf, fds[fd].buf + fds[fd].rpos, nr);
                fds[fd].rpos += nr;
            } else {
                nr = FD_BUF + fds[fd].wpos - fds[fd].rpos;
                if ( nr > n ) {
                    nr = n;
                }
                if ( FD_BUF - fds[fd].rpos <= nr ) {
                    kmemcpy(buf, fds[fd].buf + fds[fd].rpos, nr);
                } else {
                    kmemcpy(buf, fds[fd].buf + fds[fd].rpos,
                            FD_BUF - fds[fd].rpos);
                    kmemcpy(buf + FD_BUF - fds[fd].rpos,
                            fds[fd].buf + fds[fd].rpos,
                            nr - FD_BUF + fds[fd].rpos);
                }
                fds[fd].rpos = (fds[fd].rpos + nr) % FD_BUF;
            }

            /* Enable interrupts */
            arch_enable_interrupts();

            return nr;
        }
        /* pause */
    }

    return -1;
}


/*
 * Write to the file descriptor buffer from interrupt disabled function
 */
int
putc_buffer_irq(int fd, unsigned char c)
{
    int nwpos;

    /* Check whether it is open */
    if ( fd < 0 || fd >= FD_SIZE ) {
        return -1;
    }
    if ( NULL == fds[fd].owner ) {
        return -1;
    }

    nwpos = fds[fd].wpos + 1;
    if ( nwpos == fds[fd].wpos ) {
        /* Buffer is full */
        return -1;
    }
    fds[fd].buf[fds[fd].wpos] = c;
    fds[fd].wpos = nwpos;

    /* Notify */
    fds[fd].owner->state = TASK_STATE_READY;

    return 0;
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
    dst = fds[fd].owner;

#if 0
    if ( TASK_STATE_BLOCKED == dst->state ) {
        /* Change the state to ready */
        dst->state = TASK_STATE_READY;
    } else {
        /* Change the state to blocked, and wait until the destination reads
           this */
        self->state = TASK_STATE_BLOCKED;
    }
#endif

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
    arch_scall(SYSCALL_HLT);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
