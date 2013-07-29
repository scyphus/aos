/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_MEMORY_H
#define _KERNEL_MEMORY_H

#include <aos/const.h>
#include "bootinfo.h"

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

int phys_mem_init(struct bootinfo *);
int phys_mem_wire(void *, u64);

void * phys_mem_alloc_pages(u64);
void phys_mem_free_pages(void *);

#endif /* _KERNEL_MEMORY_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
