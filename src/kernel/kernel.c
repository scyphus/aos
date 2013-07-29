/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"


static struct kmem_slab_page_hdr *kmem_slab_head;

/*
 * Print a panic message and hlt processor
 */
void
panic(const char *s)
{
    kprintf("%s\r\n", s);
    arch_crash();
}

static int lock;

void kbd_init(void);
void kbd_event(void);

/*
 * Entry point to C function for BSP called from asm.s
 */
void
kmain(void)
{
    /* Initialize the lock varialbe */
    lock = 0;

    kmem_slab_head = NULL;

    kbd_init();

    /* Initialize architecture-related devices */
    arch_bsp_init();

    arch_busy_usleep(10000);

    kprintf("\r\nStarting a shell.  Press Esc to power off the machine:\r\n");
    kprintf("> ");
}

/*
 * Entry point to C function for AP called from asm.s
 */
void
apmain(void)
{
    /* Initialize this AP */
    arch_ap_init();
}

void
kintr_int32(void)
{
}
void
kintr_int33(void)
{
    arch_spin_lock(&lock);

    kbd_event();

    arch_spin_unlock(&lock);
}

/*
 * Local timer interrupt
 */
void
kintr_loc_tmr(void)
{
    arch_clock_update();
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
        break;
    default:
        ;
    }
}


/*
 * Shell process
 */
int
ktask_shell_main(int argc, const char *const argv[])
{
    return 0;
}


/*
 * Idle process
 */
int
ktask_idle_main(int argc, const char *const argv[])
{
    return 0;
}






/*
 * Memory allocation
 */
void *
kmalloc(u64 sz)
{
    struct kmem_slab_page_hdr **slab;
    u8 *bitmap;
    u64 n;
    u64 i;
    u64 j;
    u64 cnt;

    if ( sz > PAGESIZE / 2 ) {
        return phys_mem_alloc_pages(((sz - 1) / PAGESIZE) + 1);
    } else {
        n = ((PAGESIZE - sizeof(struct kmem_slab_page_hdr)) / (16 + 1));
        /* Search slab */
        slab = &kmem_slab_head;
        while ( NULL != *slab ) {
            bitmap = (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr);
            cnt = 0;
            for ( i = 0; i < n; i++ ) {
                if ( 0 == bitmap[i] ) {
                    cnt++;
                    if ( cnt * 16 >= sz ) {
                        bitmap[i - cnt + 1] |= 2;
                        for ( j = i - cnt + 1; j <= i; j++ ) {
                            bitmap[j] |= 1;
                        }
                        return (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr)
                            + n + (i - cnt + 1) * 16;
                    }
                } else {
                    cnt = 0;
                }
            }
            slab = &(*slab)->next;
        }
        /* Not found */
        *slab = phys_mem_alloc_pages(1);
        if ( NULL == (*slab) ) {
            /* Error */
            return NULL;
        }
        (*slab)->next = NULL;
        bitmap = (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr);
        for ( i = 0; i < n; i++ ) {
            bitmap[i] = 0;
        }
        bitmap[0] |= 2;
        for ( i = 0; i < (sz - 1) / 16 + 1; i++ ) {
            bitmap[i] |= 1;
        }

        return (u8 *)(*slab) + sizeof(struct kmem_slab_page_hdr) + n;
    }
    return NULL;
}

/*
 * Free allocated memory
 */
void
kfree(void *ptr)
{
    struct kmem_slab_page_hdr *slab;
    u8 *bitmap;
    u64 n;
    u64 i;
    u64 off;

    if ( 0 == (u64)ptr % PAGESIZE ) {
        phys_mem_free_pages(ptr);
    } else {
        /* Slab */
        /* ToDo: Free page */
        n = ((PAGESIZE - sizeof(struct kmem_slab_page_hdr)) / (16 + 1));
        slab = (struct kmem_slab_page_hdr *)(((u64)ptr / PAGESIZE) * PAGESIZE);
        off = (u64)ptr % PAGESIZE;
        off = (off - sizeof(struct kmem_slab_page_hdr) - n) / 16;
        bitmap = (u8 *)slab + sizeof(struct kmem_slab_page_hdr);
        if ( off >= n ) {
            /* Error */
            return;
        }
        if ( !(bitmap[off] & 2) ) {
            /* Error */
            return;
        }
        bitmap[off] &= ~(u64)2;
        for ( i = off; i < n && (!(bitmap[i] & 2) || bitmap[i] != 0); i++ ) {
            bitmap[off] &= ~(u64)1;
        }
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
