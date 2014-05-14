/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "../../kernel/kernel.h"

struct netdev_list *netdev_head;

/*
 * Initialize
 */
void
netdev_init(void)
{
    netdev_head = NULL;
}

/*
 * Add one device
 */
int
netdev_add_device(const char *name, const u8 *macaddr, void *vendor)
{
    struct netdev_list **list;
    int i;

    list = &netdev_head;
    while ( NULL != *list ) {
        list = &(*list)->next;
    }
    *list = kmalloc(sizeof(struct netdev_list));
    if ( NULL == *list ) {
        return -1;
    }
    (*list)->next = NULL;
    (*list)->netdev = kmalloc(sizeof(struct netdev));
    if ( NULL == (*list)->netdev ) {
        kfree(*list);
        return -1;
    }
    for ( i = 0; i < NETDEV_MAX_NAME - 1 && 0 != name[i]; i++ ) {
        (*list)->netdev->name[i] = name[i];
    }
    (*list)->netdev->name[i] = 0;

    for ( i = 0; i < 6; i++ ) {
        (*list)->netdev->macaddr[i] = macaddr[i];
    }

    (*list)->netdev->vendor = vendor;

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
