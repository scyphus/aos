/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "kernel.h"

/* Temporary */
int arch_dbg_printf(const char *fmt, ...);

static int lock;
extern struct netdev_list *netdev_head;

#define ND_TABLE_SIZE           4096
#define ARP_TABLE_SIZE          4096
#define NAT66_TABLE_SIZE        65536
#define ARP_TIMEOUT             300


typedef int (*router_rx_cb_t)(u8 *, u32, int);

int e1000_routing(struct netdev *, router_rx_cb_t);
int e1000_tx_buf(struct netdev *, u8 **, u16 **, u16);
int e1000_tx_commit(struct netdev *);

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


struct router_nat4 {

};

struct nat66_entry {
    u8 orig_addr[16];
    u8 priv_addr[16];
    u64 expire;
    int state;
};
struct router_nat66 {
    int sz;
    struct nat66_entry *ent;
};

struct arp_entry {
    u8 protoaddr[4];
    u8 hwaddr[6];
    /*void *netif;*/
    u64 expire;
    int state;
};

struct router_arp {
    int sz;
    struct arp_entry *ent;
};


struct nd_entry {
    u8 neighbor[16];
    u8 linklayeraddr[6];
    void *netif;
    u64 expire;
    int state;
};
struct router_nd {
    int sz;
    struct nd_entry *ent;
};

struct router {
    struct router_nat66 nat66;
};


/* RIB */
struct rib6 {
    u8 ipaddr[16];
    int preflen;
    /* Outgoing interface */
    struct netdev *next;
    int vlan;
};
/* RIB+ND */
struct fib6 {
    struct rib6 *rib6;
    u8 dstmac[6];
};


struct ipv4_route {
    u8 addr[4];
    u8 mask;
};

struct ipv4_addr {
    u8 addr[4];
    u8 mask;
    int flags;
};
struct ipv4_addr_list {
    struct ipv4_addr *addr;
    struct ipv4_addr_list *next;
};
struct ipv6_addr {
    u8 addr[16];
    u8 preflen;
    int scope;
    int flags;
};
struct ipv6_addr_list {
    struct ipv6_addr *addr;
    struct ipv6_addr_list *next;
};
struct l3if {
    char *name;
    struct netdev *netdev;
    int vlan;
    struct ipv4_addr_list *ip4list;
    struct ipv6_addr_list *ip6list;
    /* Neighbor info */
    struct router_arp arp;
    struct router_nd nd;
};
struct l3if_list {
    struct l3if *l3if;
    struct l3if_list *next;
};

static struct l3if_list *l3if_head;

/*
680
nat-pool-panda
203.178.158.192/26
2001:200:0:ff68::/64

910
exp-panda
172.16.92.0/23
2001:200:0:ff91::/64
 */



static int
_ipv4_add_route(u8 *prefix, int mask, u8 *nexthop)
{
    return 0;
}







/*
 * Add IPv4 address
 */
static int
_add_ipv4_addr(const char *name, u8 a0, u8 a1, u8 a2, u8 a3, u8 mask)
{
    struct l3if_list *l3if_list;
    struct l3if *l3if;
    struct ipv4_addr_list *ipv4_addr_list;
    struct ipv4_addr *ipv4_addr;

    /* Search the interface */
    l3if = NULL;
    l3if_list = l3if_head;
    while ( NULL != l3if_list ) {
        if ( 0 == kstrcmp(l3if_list->l3if->name, name) ) {
            l3if = l3if_list->l3if;
            break;
        }
        l3if_list = l3if_list->next;
    }
    if ( NULL == l3if ) {
        /* Not found */
        return -1;
    }

    /* Allocate for IPv4 address */
    ipv4_addr_list = kmalloc(sizeof(struct ipv4_addr_list));
    if ( NULL == ipv4_addr_list ) {
        return -1;
    }
    ipv4_addr = kmalloc(sizeof(struct ipv4_addr));
    if ( NULL == ipv4_addr ) {
        kfree(ipv4_addr_list);
        return -1;
    }
    ipv4_addr->addr[0] = a0;
    ipv4_addr->addr[1] = a1;
    ipv4_addr->addr[2] = a2;
    ipv4_addr->addr[3] = a3;
    ipv4_addr->mask = mask;
    ipv4_addr->flags = 0;

    /* Prepend */
    ipv4_addr_list->addr = ipv4_addr;
    ipv4_addr_list->next = l3if->ip4list;
    l3if->ip4list = ipv4_addr_list;

    return 0;
}

static int
_check_ipv4(struct l3if *l3if, const u8 *addr)
{
    struct ipv4_addr_list *addr_list;

    addr_list = l3if->ip4list;
    while ( NULL != addr_list ) {
        if ( 0 == kmemcmp(addr_list->addr->addr, addr, 4) ) {
            return 0;
        }
        addr_list = addr_list->next;
    }

    return -1;
}

/*
 * Add IPv6 address
 */
static int
_add_ipv6_addr(const char *name, u16 a0, u16 a1, u16 a2, u16 a3,
               u16 a4, u16 a5, u16 a6, u16 a7, u8 preflen)
{
    struct l3if_list *l3if_list;
    struct l3if *l3if;
    struct ipv6_addr_list *ipv6_addr_list;
    struct ipv6_addr *ipv6_addr;

    /* Search the interface */
    l3if = NULL;
    l3if_list = l3if_head;
    while ( NULL != l3if_list ) {
        if ( 0 == kstrcmp(l3if_list->l3if->name, name) ) {
            l3if = l3if_list->l3if;
            break;
        }
        l3if_list = l3if_list->next;
    }
    if ( NULL == l3if ) {
        /* Not found */
        return -1;
    }

    /* Allocate for IPv6 address */
    ipv6_addr_list = kmalloc(sizeof(struct ipv6_addr_list));
    if ( NULL == ipv6_addr_list ) {
        return -1;
    }
    ipv6_addr = kmalloc(sizeof(struct ipv6_addr));
    if ( NULL == ipv6_addr ) {
        kfree(ipv6_addr_list);
        return -1;
    }
    ipv6_addr->addr[0] = (a0 >> 8);
    ipv6_addr->addr[1] = a0 & 0xff;
    ipv6_addr->addr[2] = (a1 >> 8);
    ipv6_addr->addr[3] = a1 & 0xff;
    ipv6_addr->addr[4] = (a2 >> 8);
    ipv6_addr->addr[5] = a2 & 0xff;
    ipv6_addr->addr[6] = (a3 >> 8);
    ipv6_addr->addr[7] = a3 & 0xff;
    ipv6_addr->addr[8] = (a4 >> 8);
    ipv6_addr->addr[9] = a4 & 0xff;
    ipv6_addr->addr[10] = (a5 >> 8);
    ipv6_addr->addr[11] = a5 & 0xff;
    ipv6_addr->addr[12] = (a6 >> 8);
    ipv6_addr->addr[13] = a6 & 0xff;
    ipv6_addr->addr[14] = (a7 >> 8);
    ipv6_addr->addr[15] = a7 & 0xff;
    ipv6_addr->preflen = preflen;
    ipv6_addr->flags = 0;

    /* Prepend */
    ipv6_addr_list->addr = ipv6_addr;
    ipv6_addr_list->next = l3if->ip6list;
    l3if->ip6list = ipv6_addr_list;

    return 0;
}



/*
 * Create an L3 interface
 */
static int
_create_l3interface(const char *name, struct netdev *netdev, int vlan)
{
    int i;
    struct l3if_list *l3if_list;
    struct l3if *l3if;

    /* Allocate for L3 interface instance */
    l3if_list = kmalloc(sizeof(struct l3if_list));
    if ( NULL == l3if_list ) {
        return -1;
    }
    l3if = kmalloc(sizeof(struct l3if));
    if ( NULL == l3if ) {
        kfree(l3if_list);
        return -1;
    }

    /* Copy the name */
    l3if->name = kstrdup(name);
    if ( NULL == l3if->name ) {
        kfree(l3if_list);
        kfree(l3if);
        return -1;
    }

    /* Allocate ARP table */
    l3if->arp.sz = ARP_TABLE_SIZE;
    l3if->arp.ent = kmalloc(sizeof(struct arp_entry) * l3if->arp.sz);
    if ( NULL == l3if->arp.ent ) {
        /* Error */
        panic("Could not allocate memory for ARP table.\r\n");
    }
    for ( i = 0; i < l3if->arp.sz; i++ ) {
        l3if->arp.ent[i].state = -1;
    }

    /* Allocate ND table */
    l3if->nd.sz = ND_TABLE_SIZE;
    l3if->nd.ent = kmalloc(sizeof(struct nd_entry) * l3if->nd.sz);
    if ( NULL == l3if->nd.ent ) {
        /* Error */
        panic("Could not allocate memory for ND table.\r\n");
    }
    for ( i = 0; i < l3if->arp.sz; i++ ) {
        l3if->nd.ent[i].state = -1;
    }

    /* Set other parameters */
    l3if->netdev = netdev;
    l3if->vlan = vlan;
    l3if->ip4list = NULL;
    l3if->ip6list = NULL;

    /* Prepend */
    l3if_list->l3if = l3if;
    l3if_list->next = l3if_head;
    l3if_head = l3if_list;

    return 0;
}

/* Register an ARP entry */
static int
_register_arp(struct l3if *l3if, u8 *ipaddr, u8 *macaddr)
{
    int i;
    u64 nowms;
    u64 expire;

    nowms = arch_clock_get() / 1000 / 1000;
    expire = nowms + 300 * 1000;

    for ( i = 0; i < ARP_TABLE_SIZE; i++ ) {
        /* Check the existing entry */
        if ( l3if->arp.ent[i].state >= 0 ) {
            if ( 0 == kmemcmp(l3if->arp.ent[i].protoaddr, ipaddr, 4) ) {
                /* Found then update it */
                l3if->arp.ent[i].state = 1;
                l3if->arp.ent[i].expire = expire;
                return 0;
            }
        }
    }

    for ( i = 0; i < ARP_TABLE_SIZE; i++ ) {
        /* Search available entry */
        if ( -1 == l3if->arp.ent[i].state ) {
            kmemcpy(l3if->arp.ent[i].protoaddr, ipaddr, 4);
            kmemcpy(l3if->arp.ent[i].hwaddr, macaddr, 6);
            l3if->arp.ent[i].state = 1;
            l3if->arp.ent[i].expire = expire;

            arch_dbg_printf("Registered an ARP entry \r\n");
            return 0;
        }
    }

    return -1;
}


/*
 * RX callback
 */
static int
_rx_cb(u8 *pkt, u32 len, int vlan)
{
    struct l3if_list *l3if_list;
    struct l3if *l3if;

    arch_dbg_printf("YYY VLAN=%d len=%d\r\n", vlan, len);
    arch_dbg_printf(" %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x"
                    " %02x %02x %02x %02x\r\n",
                    pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5], pkt[6],
                    pkt[7], pkt[8], pkt[9], pkt[10], pkt[11], pkt[12], pkt[13]);


    /* Search vlan interface */
    l3if_list = l3if_head;
    l3if = NULL;
    while ( NULL != l3if_list ) {
        if ( vlan == l3if_list->l3if->vlan ) {
            l3if = l3if_list->l3if;
            break;
        }
        l3if_list = l3if_list->next;
    }
    if ( NULL == l3if ) {
        /* The corresponding VLAN interface was not found */
        return -1;
    }

    /* Check the destination */
    if ( !(0xff == pkt[0] && 0xff == pkt[1] && 0xff == pkt[2]
           && 0xff == pkt[3] && 0xff == pkt[4] && 0xff == pkt[5])
         || !kmemcmp(pkt, l3if->netdev->macaddr, 6) ) {
        /* Neither broadcast nor unicast to this interface */
        return -1;
    }

    if ( 0x08 == pkt[12] && 0x06 == pkt[13] ) {
        /* ARP */
        if ( len < 14 + 28 ) {
            return -1;
        }
        /* Valid ARP */
        if ( 0x00 == pkt[14] && 0x01 == pkt[15]
             && 0x08 == pkt[16] && 0x00 == pkt[17]
             && 0x06 == pkt[18] && 0x04 == pkt[19] ) {
            /* HW = 0001, PROTO = 0800, HLEN = 06, PLEN = 04 */

            /* 1 = Req, 2 = Res */
            u16 mode = (((u16)pkt[20]) << 8) | pkt[21];

            if ( 1 == mode ) {
                /* Check the destination address */
                if ( _check_ipv4(l3if, pkt+38) < 0 ) {
                    return -1;
                }
                /* ARP request */
                _register_arp(l3if, pkt+28, pkt+22);

                u8 *txpkt;
                u16 *txlen;
                e1000_tx_buf(l3if->netdev, &txpkt, &txlen, vlan);

                kmemcpy(txpkt, pkt+22, 6);
                kmemcpy(txpkt+6, l3if->netdev->macaddr, 6);
                txpkt[12] = 0x08;
                txpkt[13] = 0x06;
                txpkt[14] = 0x00;
                txpkt[15] = 0x01;
                txpkt[16] = 0x08;
                txpkt[17] = 0x00;
                txpkt[18] = 0x06;
                txpkt[19] = 0x04;
                txpkt[20] = 0x00;
                txpkt[21] = 0x02;
                kmemcpy(txpkt+22, l3if->netdev->macaddr, 6);
                kmemcpy(txpkt+28, pkt+38, 4);
                kmemcpy(txpkt+32, pkt+22, 6);
                kmemcpy(txpkt+38, pkt+28, 4);
                *txlen = 42;

                e1000_tx_commit(l3if->netdev);
            } else if ( 2 == mode ) {
                /* ARP reply, then check the destination */
                if ( !kmemcmp(pkt+32, l3if->netdev->macaddr, 6) ) {
                    return -1;
                }
                /* Check the IP address */
                if ( _check_ipv4(l3if, pkt+38) < 0 ) {
                    return -1;
                }
                /* Register */
                _register_arp(l3if, pkt+28, pkt+22);
            }
        }
    } else if ( 0x08 == pkt[12] && 0x00 == pkt[13] ) {
        /* IP then check the destination */

        u8 *dstmac = pkt;
        u8 *srcmac = pkt + 6;
        struct iphdr *ip = (struct iphdr *)(pkt + 14);

        if ( 0 == _check_ipv4(l3if, ip->ip_dst) ) {
            arch_dbg_printf("Received an IP packet. %x\r\n", ip->ip_vhl);
        }

#if 0
        u8 ver = pkt[14] >> 4;
        u8 ihl = pkt[14] & 0xf;
        u16 totallen = (((u16)pkt[16]) << 8) | pkt[17];
        u16 ident = (((u16)pkt[18]) << 8) | pkt[19];
        u8 ttl = pkt[22];
        u16 chksum = (((u16)pkt[24]) << 8) | pkt[25];
        u8 *srcip = pkt + 26;
        u8 *dstip = pkt + 30;
#endif

    }

    return 0;
}

/*
 * Router processess
 */
void
proc_router(void)
{
    struct router *rt;
    int i;

    /* Initialize the lock variable */
    lock = 0;

    /* Initialize interfaces */
    l3if_head = NULL;

    /* Print a starter message */
    arch_dbg_printf("Start router\r\n");

    /* Allocate a router instance */
    rt = kmalloc(sizeof(struct router));

    /* Allocate NAT66 table */
    rt->nat66.sz = ND_TABLE_SIZE;
    rt->nat66.ent = kmalloc(sizeof(struct nd_entry) * rt->nat66.sz);
    if ( NULL == rt->nat66.ent ) {
        /* Error */
        panic("Could not allocate memory for NAT66 table.\r\n");
    }
    for ( i = 0; i < rt->nat66.sz; i++ ) {
        rt->nat66.ent[i].state = -1;
    }

    /* Initialize network interface */
    struct netdev_list *list;
    int ret;
    list = netdev_head;
    ret = _create_l3interface("ve680", list->netdev, 680);
    if ( ret < 0 ) {
        panic("Could not create an interface.\r\n");
    }
    ret = _add_ipv4_addr("ve680", 203, 178, 158, 194, 26);
    if ( ret < 0 ) {
        panic("Could not assign an IPv4 address.\r\n");
    }
    ret = _add_ipv6_addr("ve680", 0x2001, 0x200, 0, 0xff68, 0, 0, 0, 2, 64);
    if ( ret < 0 ) {
        panic("Could not assign an IPv6 address.\r\n");
    }
    ret = _create_l3interface("ve910", list->netdev, 910);
    if ( ret < 0 ) {
        panic("Could not create an interface.\r\n");
    }
    ret = _add_ipv4_addr("ve910", 172, 16, 92, 1, 23);
    if ( ret < 0 ) {
        panic("Could not assign an IPv4 address.\r\n");
    }
    ret = _add_ipv6_addr("ve910", 0x2001, 0x200, 0, 0xff91, 0, 0, 0, 1, 64);
    if ( ret < 0 ) {
        panic("Could not assign an IPv6 address.\r\n");
    }

    /* For testing */
    ret = _create_l3interface("ve0", list->netdev, 0);
    if ( ret < 0 ) {
        panic("Could not create an interface.\r\n");
    }
    ret = _add_ipv4_addr("ve0", 192, 168, 56, 2, 24);
    if ( ret < 0 ) {
        panic("Could not assign an IPv4 address.\r\n");
    }

    e1000_routing(list->netdev, _rx_cb);

    /* Free the router instance */
    kfree(rt->nat66.ent);
    kfree(rt);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
