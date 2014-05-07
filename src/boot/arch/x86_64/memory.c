/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "boot.h"

/* Flags */
#define PHYS_MEM_USED           (u64)1
#define PHYS_MEM_WIRED          (u64)(1<<1)
#define PHYS_MEM_HEAD           (u64)(1<<2)
#define PHYS_MEM_UNAVAIL        (u64)(1<<16)

#define PHYS_MEM_IS_FREE(x)     (0 == (x)->flags ? 1 : 0)
#define FLOOR(val, base)        ((val) / (base)) * (base)
#define CEIL(val, base)         (((val) - 1) / (base) + 1) * (base)

/*
 * Boot monitor memory management
 */
struct bmem_slab_page_hdr {
    struct bmem_slab_page_hdr *next;
    u64 reserved;
} __attribute__ ((packed));


/*
 * Physical memory page
 */
struct phys_mem_page {
    u64 flags;
    struct phys_mem_page *lru;
} __attribute__ ((packed));

/*
 * Physical memory
 */
struct phys_mem {
    u64 nr;
    struct phys_mem_page *pages;
} __attribute__ ((packed));



static int memory_lock;
static int memory_page_lock;
struct bmem_slab_page_hdr *bmem_slab_head;

static struct phys_mem *phys_mem;


/*
 * Initialize memory manager
 */
int
memory_init(struct bootinfo *bi)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 nr;
    u64 addr;
    u64 sz;
    u64 a;
    u64 b;
    u64 i;
    u64 j;

    /* Clear lock variable */
    memory_lock = 0;
    memory_page_lock = 0;

    /* Clear physical memory space */
    phys_mem = NULL;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.n <= 0 ) {
        return -1;
    }

    /* Obtain usable memory size */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            if ( bse->base + bse->len > addr ) {
                addr = bse->base + bse->len;
            }
        }
    }

    /* Calculate required memory size for pages */
    nr = CEIL(addr, PAGESIZE) / PAGESIZE;
    sz = nr * sizeof(struct phys_mem_page) + sizeof(struct phys_mem);

    /* Search free space system address map obitaned from BIOS */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE);
            b = FLOOR(bse->base + bse->len, PAGESIZE);

            if ( b < PHYS_MEM_FREE_ADDR ) {
                /* Skip */
                continue;
            } else if ( a < PHYS_MEM_FREE_ADDR ) {
                if ( b - PHYS_MEM_FREE_ADDR >= sz ) {
                    addr = PHYS_MEM_FREE_ADDR;
                    break;
                } else {
                    continue;
                }
            } else {
                if ( b - a >= sz ) {
                    addr = a;
                    break;
                } else {
                    continue;
                }
            }
        }
    }

    /* Could not find */
    if ( 0 == addr ) {
        return -1;
    }

    /* Setup */
    phys_mem = (struct phys_mem *)(addr + nr * sizeof(struct phys_mem_page));
    phys_mem->nr = nr;
    phys_mem->pages = (struct phys_mem_page *)addr;

    /* Reset flags */
    for ( i = 0; i < phys_mem->nr; i++ ) {
        /* Mark as unavailable */
        phys_mem->pages[i].flags = PHYS_MEM_UNAVAIL;
    }

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE) / PAGESIZE;
            b = FLOOR(bse->base + bse->len, PAGESIZE) / PAGESIZE;

            /* Mark as unallocated */
            for ( j = a; j < b; j++ ) {
                if ( j >= phys_mem->nr ) {
                    /* Error */
                    return -1;
                }
                /* Unmark unavailable */
                phys_mem->pages[j].flags &= ~PHYS_MEM_UNAVAIL;
                if ( j * PAGESIZE <= PHYS_MEM_FREE_ADDR ) {
                    /* Wired by kernel */
                    phys_mem->pages[j].flags |= PHYS_MEM_WIRED;
                }
            }
        }
    }

    /* Mark self */
    for ( i = addr / PAGESIZE; i < CEIL(addr + sz, PAGESIZE) / PAGESIZE; i++ ) {
        phys_mem->pages[i].flags |= PHYS_MEM_WIRED;
    }

    return 0;
}

/*
 * Allocate n pages for available space
 */
void *
phys_mem_alloc_pages(u64 n)
{
    u64 i;
    u64 f;
    u64 cnt;

    /* Check address first */
    if ( n > phys_mem->nr ) {
        return NULL;
    }

    spin_lock(&memory_page_lock);

    cnt = 0;
    for ( i = 0; i < phys_mem->nr; i++ ) {
        if ( PHYS_MEM_IS_FREE(&phys_mem->pages[i]) ) {
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
        phys_mem->pages[i].flags = PHYS_MEM_HEAD;
        for ( i = f; i < f + n; i++ ) {
            phys_mem->pages[i].flags |= PHYS_MEM_USED;
        }
        spin_unlock(&memory_page_lock);
        return (void *)(f * PAGESIZE);
    } else {
        spin_unlock(&memory_page_lock);
        return NULL;
    }
}

/*
 * Free allocated pages
 */
void
phys_mem_free_pages(void *page)
{
    u64 i;
    u64 p;

    p = (u64)page / PAGESIZE;
    if ( p >= phys_mem->nr ) {
        /* Invalid page number */
        return;
    }

    spin_lock(&memory_page_lock);

    if ( !(phys_mem->pages[p].flags & PHYS_MEM_HEAD) ) {
        /* Invalid page type */
        spin_unlock(&memory_page_lock);
        return;
    }
    phys_mem->pages[p].flags &= ~PHYS_MEM_HEAD;

    /* Free */
    for ( i = p; i < phys_mem->nr; i++ ) {
        if ( phys_mem->pages[i].flags & PHYS_MEM_HEAD ) {
            /* Another allocation begins */
            break;
        } else if ( phys_mem->pages[i].flags & PHYS_MEM_USED ) {
            /* Free */
            phys_mem->pages[i].flags &= ~PHYS_MEM_USED;
        } else {
            /* This allocation ends */
            break;
        }
    }

    spin_unlock(&memory_page_lock);
}


/*
 * Memory allocation
 */
void *
bmalloc(u64 sz)
{
    struct bmem_slab_page_hdr **slab;
    u8 *bitmap;
    u64 n;
    u64 i;
    u64 j;
    u64 cnt;
    void *ret;

    spin_lock(&memory_lock);

    if ( sz > PAGESIZE / 2 ) {
        ret = phys_mem_alloc_pages(((sz - 1) / PAGESIZE) + 1);
        spin_unlock(&memory_lock);
        return ret;
    } else {
        n = ((PAGESIZE - sizeof(struct bmem_slab_page_hdr)) / (16 + 1));
        /* Search slab */
        slab = &bmem_slab_head;
        while ( NULL != *slab ) {
            bitmap = (u8 *)(*slab) + sizeof(struct bmem_slab_page_hdr);
            cnt = 0;
            for ( i = 0; i < n; i++ ) {
                if ( 0 == bitmap[i] ) {
                    cnt++;
                    if ( cnt * 16 >= sz ) {
                        bitmap[i - cnt + 1] |= 2;
                        for ( j = i - cnt + 1; j <= i; j++ ) {
                            bitmap[j] |= 1;
                        }
                        ret = (u8 *)(*slab) + sizeof(struct bmem_slab_page_hdr)
                            + n + (i - cnt + 1) * 16;
                        spin_unlock(&memory_lock);
                        return ret;
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
            spin_unlock(&memory_lock);
            return NULL;
        }
        (*slab)->next = NULL;
        bitmap = (u8 *)(*slab) + sizeof(struct bmem_slab_page_hdr);
        for ( i = 0; i < n; i++ ) {
            bitmap[i] = 0;
        }
        bitmap[0] |= 2;
        for ( i = 0; i < (sz - 1) / 16 + 1; i++ ) {
            bitmap[i] |= 1;
        }

        ret = (u8 *)(*slab) + sizeof(struct bmem_slab_page_hdr) + n;
        spin_unlock(&memory_lock);
        return ret;
    }
    spin_unlock(&memory_lock);
    return NULL;
}

/*
 * Free allocated memory
 */
void
bfree(void *ptr)
{
    struct bmem_slab_page_hdr *slab;
    u8 *bitmap;
    u64 n;
    u64 i;
    u64 off;

    spin_lock(&memory_lock);

    if ( 0 == (u64)ptr % PAGESIZE ) {
        phys_mem_free_pages(ptr);
    } else {
        /* Slab */
        /* ToDo: Free page */
        n = ((PAGESIZE - sizeof(struct bmem_slab_page_hdr)) / (16 + 1));
        slab = (struct bmem_slab_page_hdr *)(((u64)ptr / PAGESIZE) * PAGESIZE);
        off = (u64)ptr % PAGESIZE;
        off = (off - sizeof(struct bmem_slab_page_hdr) - n) / 16;
        bitmap = (u8 *)slab + sizeof(struct bmem_slab_page_hdr);
        if ( off >= n ) {
            /* Error */
            spin_unlock(&memory_lock);
            return;
        }
        if ( !(bitmap[off] & 2) ) {
            /* Error */
            spin_unlock(&memory_lock);
            return;
        }
        bitmap[off] &= ~(u64)2;
        for ( i = off; i < n && (!(bitmap[i] & 2) || bitmap[i] != 0); i++ ) {
            bitmap[off] &= ~(u64)1;
        }
    }
    spin_unlock(&memory_lock);
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
