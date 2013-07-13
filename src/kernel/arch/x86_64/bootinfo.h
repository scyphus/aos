/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_BOOTINFO_H
#define _KERNEL_BOOTINFO_H

struct bootinfo {
    struct sysaddrmap {
        u64 n;
        struct bootinfo_sysaddrmap_entry *entries;
    } sysaddrmap;
};
struct bootinfo_sysaddrmap_entry {
    u64 base;
    u64 len;
    u32 type;
    u32 attr;
};

#endif /* _KERNEL_BOOTINFO_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
