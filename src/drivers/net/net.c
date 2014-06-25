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

struct iphdr {
    u8 ip_vhl;
    u8 ip_tos;
    u16 ip_len;
    u16 ip_id;
    u16 ip_off;
    u8 ip_ttl;
    u8 ip_proto;
    u16 ip_sum;
    u8 ip_src[4];
    u8 ip_dst[4];
} __attribute__ ((packed));

struct icmp_hdr {
    u8 type;
    u8 code;
    u16 checksum;
    u16 ident;
    u16 seq;
    // data...
} __attribute__ ((packed));

struct ip6hdr {
    u32 ip6_vtf;
    u16 ip6_len;
    u8 ip6_next;
    u8 ip6_limit;
    u8 ip6_src[16];
    u8 ip6_dst[16];
} __attribute__ ((packed));

struct icmp6_hdr {
    u8 type;
    u8 code;
    u16 checksum;
    // data...
} __attribute__ ((packed));


#define ARP_LIFETIME 300 * 1000

/*
 * Register an ARP entry
 */
int
net_register_arp(const u8 *ipaddr, const u8 *macaddr)
{
    u64 nowms;
    u64 expire;

    /* Get the current time in milliseconds */
    nowms = arch_clock_get() / 1000 / 1000;
    expire = nowms + ARP_LIFETIME;


    return -1;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
