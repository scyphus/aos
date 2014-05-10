/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "boot.h"

struct blkdev_list *blkdev_head;

/*
 * Boot function
 */
void
cboot(void)
{
    //unsigned short *x = (unsigned short *)0x000b8000;
    //*x = (0x07 << 8) | 'x';

    blkdev_head = NULL;

    /* Initialize PCI */
    memory_init((struct bootinfo *)0x00008000);
    pci_init();
    ahci_init();

    fat_load_kernel(blkdev_head->blkdev, 0x10000);

    /* Long jump */
    ljmp64((void *)0x10000);
    //halt64();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
