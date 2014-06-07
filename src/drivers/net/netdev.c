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
struct netdev *
netdev_add_device(const u8 *macaddr, void *vendor)
{
    struct netdev_list **list;
    int i;
    int num;

    /* Search the tail of the list */
    list = &netdev_head;
    num = 0;
    while ( NULL != *list ) {
        list = &(*list)->next;
        num++;
    }

    /* Allocate memory for the tail of the list */
    *list = kmalloc(sizeof(struct netdev_list));
    if ( NULL == *list ) {
        return NULL;
    }
    (*list)->next = NULL;
    (*list)->netdev = kmalloc(sizeof(struct netdev));
    if ( NULL == (*list)->netdev ) {
        kfree(*list);
        return NULL;
    }
    (*list)->netdev->name[0] = 'e';
    (*list)->netdev->name[1] = '0' + num;
    (*list)->netdev->name[2] = '\0';

    for ( i = 0; i < 6; i++ ) {
        (*list)->netdev->macaddr[i] = macaddr[i];
    }

    (*list)->netdev->vendor = vendor;

    return (*list)->netdev;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
