/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <midos/const.h>
#include "../../kernel.h"

/*
 * Page allocator
 * --> memory allocator
 */

/* 16 byte alignment */
#define align(x) ((((u64)(x)-1) & ~0xfULL) + 0x10)

static int kmem_lock = 0;

void
kmem_init(void)
{
    u64 i;
    u64 j;
    struct bi_sysaddrmap_entry *entries;
    struct bi_sysaddrmap_entry *bse;
    u64 a;
    u64 b;

    /* Initialize lock variable */
    kmem_lock = 0;

    /* Check the number of address map entries */
    if ( bootinfo->sysaddrmap->num <= 0 ) {
        /* Call panic? */
        return;
    }

    /* Obtain entries */
    entries = (struct bi_sysaddrmap_entry *)
        ((u8 *)bootinfo->sysaddrmap + sizeof(struct bi_sysaddrmap_hdr));

    /* Obtain memory range */
    bse = &entries[bootinfo->sysaddrmap->num-1];
    kmem_info->npgs = (bse->base + bse->len) >> PAGESIZE_BIT;
    kmem_info->pages = (struct kmem_page *)
        align((u8 *)kmem_info + sizeof(struct kmem_info));

    /* Reset flags */
    for ( i = 0; i < kmem_info->npgs; i++ ) {
        kmem_info->pages[i].flags = KMEM_RESERVED;
    }

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bootinfo->sysaddrmap->num; i++ ) {
        bse = &entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = bse->base >> PAGESIZE_BIT;
            b = (bse->base + bse->len) >> PAGESIZE_BIT;

            /* Check */
            if ( a >= kmem_info->npgs ) {
                /* Call panic? */
                continue;
            }
            /* Incomplete page? */
            if ( (a << PAGESIZE_BIT) != bse->base ) {
                /* Skip incomplete page */
                a++;
            }

            /* Mark as unallocated */
            for ( j = a; j < b; j++ ) {
                kmem_info->pages[j].flags &= ~(u64)KMEM_RESERVED;
            }
        }
    }

    /* Mark pages used in kernel */
    a = (u64)kmem_info->pages + sizeof(struct kmem_page) * kmem_info->npgs;
    a = (align(a) - 1) >> PAGESIZE_BIT;
    for ( i = 0; i <= a; i++ ) {
        kmem_info->pages[i].flags |= KMEM_KERNEL;
    }
}


/*
 * Allocate n pages for kernel
 * Now we support a very naive allocation algorithm; first find.  Buddy/Slab
 * will be implemented?
 */
void *
kmem_alloc_pages(u64 n)
{
    u64 i;
    u64 f;
    u64 cnt;

    /* Check address first */
    if ( n > kmem_info->npgs ) {
        return NULL;
    }

    spin_lock(&kmem_lock);
    cnt = 0;
    for ( i = 0; i < kmem_info->npgs; i++ ) {
        if ( KMEM_IS_FREE_PAGE(&kmem_info->pages[i]) ) {
            if ( !cnt ) {
                f = i;
            }
            cnt++;
            if ( cnt == n ) {
                break;
            }
        } else {
            f = 0;
            cnt = 0;
        }
    }

    if ( cnt == n ) {
        kmem_info->pages[i].flags = KMEM_HEAD;
        for ( i = f; i < f + n; i++ ) {
            kmem_info->pages[i].flags |= KMEM_ALLOC;
        }
        spin_unlock(&kmem_lock);
        return (void *)(f << PAGESIZE_BIT);
    } else {
        spin_unlock(&kmem_lock);
        return NULL;
    }
}

void
kmem_free_pages(void *page)
{
    u64 i;
    u64 p;

    p = (u64)page >> PAGESIZE_BIT;
    if ( p >= kmem_info->npgs ) {
        /* Invalid page number */
        return;
    }

    spin_lock(&kmem_lock);

    if ( !(kmem_info->pages[p].flags & KMEM_HEAD) ) {
        /* Invalid page type */
        spin_unlock(&kmem_lock);
        return;
    }
    kmem_info->pages[p].flags &= ~KMEM_HEAD;

    /* Free */
    for ( i = p; i < kmem_info->npgs; i++ ) {
        if ( kmem_info->pages[i].flags & KMEM_HEAD ) {
            /* Another allocation begins */
            break;
        } else if ( kmem_info->pages[i].flags & KMEM_ALLOC ) {
            /* Free */
            kmem_info->pages[i].flags &= ~KMEM_ALLOC;
        } else {
            /* This allocation ends */
            break;
        }
    }
    spin_unlock(&kmem_lock);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
