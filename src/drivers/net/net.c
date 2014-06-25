/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

/* $Id$ */

#include <aos/const.h>


/* ARP */
struct net_arp_entry {
    u8 protoaddr[4];
    u8 hwaddr[6];
    u64 expire;
    int state;
    void *netif;
};

/* ND */
struct net_nd_entry {
    u8 neighbor[16];
    u8 linklayeraddr[6];
    void *netif;
    u64 expire;
    int state;
};




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
