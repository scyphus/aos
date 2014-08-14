/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include "boot.h"

struct blkdev_list *blkdev_head;

/*
 * Boot function
 */
void
cboot(void)
{
    blkdev_head = NULL;

    /* Initialize PCI */
    memory_init((struct bootinfo *)BOOTINFO_ADDR);
    pci_init();
    ahci_init();

    ohci_init();

    if ( NULL != blkdev_head ) {
        fat_load_kernel(blkdev_head->blkdev, 0x10000);
    }

    /* Long jump */
    ljmp64((void *)0x10000);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
