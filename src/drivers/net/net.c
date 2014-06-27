/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "../../kernel/kernel.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

u16 bswap16(u16);
u32 bswap32(u32);


struct ethhdr {
    u8 dst[6];
    u8 src[6];
    u16 type;
} __attribute__ ((packed));
struct ethhdr1q {
    u16 vlan;
    u16 type;
} __attribute__ ((packed));

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

#define ARP_STATE_INVAL         -1
#define ARP_STATE_DYNAMIC       1

/*
 * Register an ARP entry with an IPv4 address and a MAC address
 */
int
net_arp_register(struct net_arp_table *t, const u32 ipaddr, const u64 macaddr,
                 int flag)
{
    int i;
    u64 nowms;
    u64 expire;

    /* Get the current time in milliseconds */
    nowms = arch_clock_get() / 1000 / 1000;
    expire = nowms + ARP_LIFETIME;

    for ( i = 0; i < t->sz; i++ ) {
        if ( t->entries[i].state >= 0 ) {
            if ( t->entries[i].protoaddr == ipaddr ) {
                /* Found the corresponding entry */
                if ( ARP_STATE_DYNAMIC == t->entries[i].state ) {
                    /* Update expiration time */
                    t->entries[i].expire = expire;
                }
                return 0;
            }
        }
    }

    /* Not found */
    for ( i = 0; i < t->sz; i++ ) {
        /* Search available entry */
        if ( ARP_STATE_INVAL == t->entries[i].state ) {
            t->entries[i].protoaddr = ipaddr;
            t->entries[i].hwaddr = macaddr;
            t->entries[i].state = ARP_STATE_DYNAMIC;
            t->entries[i].expire = expire;

            return 0;
        }
    }

    return -1;
}

/*
 * Resolve an ARP entry with an IPv4 address
 */
int
net_arp_resolve(struct net_arp_table *t, u32 ipaddr, u64 *macaddr)
{
    int i;

    for ( i = 0; i < t->sz; i++ ) {
        if ( t->entries[i].state >= 0 ) {
            if ( ipaddr == t->entries[i].protoaddr ) {
                /* Found the entry */
                *macaddr = t->entries[i].hwaddr;
                return 0;
            }
        }
    }

    return -1;
}

/*
 * Unregister an ARP entry
 */
int
net_arp_unregister(struct net_arp_table *t, const u32 ipaddr)
{
    int i;

    for ( i = 0; i < t->sz; i++ ) {
        if ( t->entries[i].state >= 0 ) {
            if ( ipaddr == t->entries[i].protoaddr ) {
                /* Found the entry */
                t->entries[i].state = ARP_STATE_INVAL;
                return 0;
            }
        }
    }

    return -1;
}
















/*
 * IPv4
 */
static int
_rx_ipv4(struct net *net, struct net_bridge *bridge, u8 *pkt, int len)
{
    return -1;
}

/*
 * IPv6
 */
static int
_rx_ipv6(struct net *net, struct net_bridge *bridge, u8 *pkt, int len)
{
    return -1;
}

static int
_rx_arp(struct net *net, struct net_bridge *bridge, u8 *pkt, int len)
{
    return -1;
}


/*
 * 802.1Q
 */
static int
_rx_802_1q(struct net *net, struct net_port *port, u8 *pkt, int len,
           int vlan)
{
    struct net_bridge *bridge;
    struct ethhdr1q *hdr;

    if ( unlikely(vlan > 0) ) {
        /* VLAN is already specified */
        return -1;
    }

    /* Check the length first */
    if ( unlikely(len < sizeof(struct ethhdr1q)) ) {
        return -1;
    }

    hdr = (struct ethhdr1q *)pkt;
    vlan = hdr->vlan;
    if ( vlan <= 0 || vlan >= 4096 ) {
        return -1;
    }
    bridge = port->bridges[vlan];

    switch ( hdr->type ) {
    case 0x0800:
        /* IPv4 */
        return _rx_ipv4(net, bridge, pkt + sizeof(struct ethhdr),
                        len - sizeof(struct ethhdr));
        break;
    case 0x0806:
        /* ARP */
        return _rx_arp(net, bridge, pkt + sizeof(struct ethhdr),
                       len - sizeof(struct ethhdr));
        break;
    case 0x86dd:
        /* IPv6 */
        return _rx_ipv6(net, bridge, pkt + sizeof(struct ethhdr),
                        len - sizeof(struct ethhdr));
        break;
    default:
        ;
    }

    return -1;
}

/*
 * Rx packet handler
 */
int
net_rx(struct net *net, struct net_port *port, u8 *pkt, int len, int vlan)
{
    struct net_bridge *bridge;
    struct ethhdr *ehdr;

    /* Check the length first */
    if ( unlikely(len < sizeof(struct ethhdr)) ) {
        return -1;
    }

    /* Find out the corresponding bridge */
    if ( vlan < 0 ) {
        vlan = 0;
    }
    if ( vlan >= 4096 ) {
        return -1;
    }
    bridge = port->bridges[vlan];

    /* Check layer 2 information first */
    ehdr = (struct ethhdr *)pkt;
    switch ( bswap16(ehdr->type) ) {
    case 0x0800:
        /* IPv4 */
        return _rx_ipv4(net, bridge, pkt + sizeof(struct ethhdr),
                        len - sizeof(struct ethhdr));
        break;
    case 0x0806:
        /* ARP */
        return _rx_arp(net, bridge, pkt + sizeof(struct ethhdr),
                       len - sizeof(struct ethhdr));
        break;
    case 0x8100:
        /* 802.1Q */
        return _rx_802_1q(net, port, pkt + sizeof(struct ethhdr),
                          len - sizeof(struct ethhdr), vlan);
        break;
    case 0x86dd:
        /* IPv6 */
        return _rx_ipv6(net, bridge, pkt + sizeof(struct ethhdr),
                        len - sizeof(struct ethhdr));
        break;
    default:
        /* Other */
        ;
    }

    return -1;
}


/*
 * Tx packet handler
 */
int
net_tx(struct net *net, u8 *pkt, int len)
{
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
