/*_
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "kernel.h"

/*
 * Ring buffer
 */
struct netxx {
    int fd;
    u8 *buf;
    u8 *adu;
};


struct netsc_ring {
    struct netsc_desc *desc;
    u32 size;
    u32 head;
    u32 tail;
};

struct netsc_desc {
    u8 *buf;
    u32 sz;
    u32 stat;
};


static struct netsc_ring netsc_ring;

/*
 * Initialize netsc
 */
void
netsc_init(void)
{
    netsc_ring.size = 1024;
    netsc_ring.desc = kmalloc(sizeof(struct netsc_desc) * netsc_ring.size);
    netsc_ring.head = 0;
    netsc_ring.tail = 0;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
