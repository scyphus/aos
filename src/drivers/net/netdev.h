/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#ifndef _DRIVERS_NETDEV_H
#define _DRIVERS_NETDEV_H

#define NETDEV_MAX_NAME 32

#if 0
struct netdev {
    char name[NETDEV_MAX_NAME];
    u8 macaddr[6];

    void *vendor;
};

struct netdev_list {
    struct netdev *netdev;
    struct netdev_list *next;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

    void netdev_init(void);
    int netdev_add_device(const u8 *, void *);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS_NETDEV_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
