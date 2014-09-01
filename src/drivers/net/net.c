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
    u64 dst:48;
    u64 src:48;
    u16 type;
} __attribute__ ((packed));
struct ethhdr1q {
    u16 vlan;
    u16 type;
} __attribute__ ((packed));

struct iphdr {
    u8 ip_ihl:4;
    u8 ip_version:4;
    u8 ip_tos;
    u16 ip_len;
    u16 ip_id;
    u16 ip_off;
    u8 ip_ttl;
    u8 ip_proto;
    u16 ip_sum;
    u32 ip_src;
    u32 ip_dst;
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

struct ip_arp {
    u16 hw_type;
    u16 protocol;
    u8 hlen;
    u8 plen;
    u16 opcode;
    u64 src_mac:48;
    u32 src_ip;
    u64 dst_mac:48;
    u32 dst_ip;
} __attribute__ ((packed));



struct ether {
    u64 dstmac;
    u64 srcmac;
    int vlan;
    int type;
};


static int _rx_ipif(struct net *, struct net_ipif *, u8 *, int, struct ether *);


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
 * Compute checksum
 */
static u16
_checksum(const u8 *buf, int len)
{
    int nleft;
    u32 sum;
    const u16 *cur;
    union {
        u16 us;
        u8 uc[2];
    } last;
    u16 ret;

    nleft = len;
    sum = 0;
    cur = (const u16 *)buf;

    while ( nleft > 1 ) {
        sum += *cur;
        cur += 1;
        nleft -= 2;
    }
    if ( 1 == nleft ) {
        last.uc[0] = *(const u8 *)cur;
        last.uc[1] = 0;
        sum += last.us;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    ret = ~sum;

    return ret;
}
#if 0
static u16
_checksum(u8 *data, int len)
{
    /* Compute checksum */
    u16 *tmp;
    u32 cs;
    int i;

    tmp = (u16 *)data;
    cs = 0;
    for ( i = 0; i < len / 2; i++ ) {
        cs += (u32)tmp[i];
        cs = (cs & 0xffff) + (cs >> 16);
    }
    cs = 0xffff - cs;

    return cs;
}
#endif

/*
 * Comput ICMPv6 checksum
 */
static u16
_icmpv6_checksum(const u8 *src, const u8 *dst, u32 len, const u8 *data)
{
    u8 phdr[40];
    int nleft;
    u32 sum;
    const u16 *cur;
    union {
        u16 us;
        u8 uc[2];
    } last;
    u16 ret;
    int i;

    kmemcpy(phdr, src, 16);
    kmemcpy(phdr+16, dst, 16);
    phdr[32] = (len >> 24) & 0xff;
    phdr[33] = (len >> 16) & 0xff;
    phdr[34] = (len >> 8) & 0xff;
    phdr[35] = len & 0xff;
    phdr[36] = 0;
    phdr[37] = 0;
    phdr[38] = 0;
    phdr[39] = 58;

    sum = 0;
    for ( i = 0; i < 20; i++ ) {
        sum += *(const u16 *)(phdr + i * 2);
    }

    nleft = len;
    cur = (const u16 *)data;
    while ( nleft > 1 ) {
        sum += *cur;
        cur += 1;
        nleft -= 2;
    }
    if ( 1 == nleft ) {
        last.uc[0] = *(const u8 *)cur;
        last.uc[1] = 0;
        sum += last.us;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    ret = ~sum;

    return ret;
}







/*
 * Tx packet handler
 */
static int
_tx_port(struct net *net, struct net_port *port, u8 *pkt, int len)
{
    port->netdev->sendpkt(pkt, len, port->netdev);
    return -1;
}

/*
 * Bridge
 */
static int
_tx_bridge(struct net *net, struct net_bridge *bridge, u8 *pkt, int len,
           u64 dstmac, void *srcport, int type)
{
    int i;
    struct net_port *port;
    struct net_ipif *ipif;
    struct ethhdr *ehdr;
    struct ether eth;

    /* Lookup FDB */
    for ( i = 0; i < bridge->fdb.nr; i++ ) {
        if ( bridge->fdb.entries[i].macaddr == dstmac ) {
            switch ( bridge->fdb.entries[i].type ) {
            case NET_FDB_PORT_DYNAMIC:
            case NET_FDB_PORT_STATIC:
                port = bridge->fdb.entries[i].u.port;
                return _tx_port(net, port, pkt, len);
            case NET_FDB_IPIF:
                ipif = bridge->fdb.entries[i].u.ipif;
                ehdr = (struct ethhdr *)pkt;
                eth.dstmac = ehdr->dst;
                eth.srcmac = ehdr->src;
                eth.type = ehdr->type;
                eth.vlan = -1;
                return _rx_ipif(net, ipif, pkt, len, &eth);
            case NET_FDB_INVAL:
            default:
                break;
            }
        }
    }

    /* Unknown unicast */
    for ( i = 0; i < bridge->nr; i++ ) {
        port = bridge->ports[i];
        if ( srcport != port ) {
            /* Transmit */
            _tx_port(net, port, pkt, len);
        }
    }
    for ( i = 0; i < bridge->nr_ipif; i++ ) {
        ipif = bridge->ipifs[i];
        if ( srcport != ipif ) {
            /* Transmit */
            _rx_ipif(net, ipif, pkt, len, &eth);
        }
    }

    return 0;
}



/*
 * ICMP
 */
static int
_rx_icmp(struct net *net, struct net_ipif *ipif, u8 *pkt, int len,
         struct iphdr *iphdr)
{
    struct icmp_hdr *hdr;
    u8 *rpkt;

    /* Check the length first */
    if ( unlikely(len < sizeof(struct icmp_hdr)) ) {
        return -1;
    }
    hdr = (struct icmp_hdr *)pkt;

    switch ( hdr->type ) {
    case 8:
        /* ICMP echo request */
        rpkt = alloca(len + sizeof(struct iphdr) + sizeof(struct ethhdr));
        if ( NULL == rpkt ) {
            return -1;
        }

        break;
    default:
        ;
    }

    return -1;
}



/*
 * TCP
 */
static int
_rx_tcp(struct net *net, struct net_ipif *ipif, u8 *pkt, int len)
{
    return -1;
}


/*
 * IPv4
 */
static int
_rx_ipv4(struct net *net, struct net_ipif *ipif, u8 *pkt, int len)
{
    struct iphdr *hdr;
    int hdrlen;
    int iplen;
    u8 *payload;

    /* FIXME: Should support multiple IP addresses on a single interface */
    if ( unlikely(NULL == ipif->ipv4) ) {
        return -1;
    }

    /* Check the length first */
    if ( unlikely(len < sizeof(struct iphdr)) ) {
        return -1;
    }
    hdr = (struct iphdr *)pkt;

    /* Check the version */
    if ( unlikely(4 != hdr->ip_version) ) {
        return -1;
    }

    /* Header length */
    hdrlen = hdr->ip_ihl * 4;
    iplen = bswap16(hdr->ip_len);
    if ( unlikely(len < iplen) ) {
        return -1;
    }

    /* Check the destination */
    if ( ipif->ipv4->addr != hdr->ip_dst ) {
        return -1;
    }
    payload = pkt + hdrlen;

    switch ( hdr->ip_proto ) {
    case 0x01:
        /* ICMP */
        return _rx_icmp(net, ipif, payload, iplen - hdrlen, hdr);
    case 0x06:
        /* TCP */
        return _rx_tcp(net, ipif, payload, iplen - hdrlen);
    case 0x11:
        /* UDP */
        break;
    default:
        /* Unsupported */
        ;
    }

    return -1;
}

/*
 * IPv6
 */
static int
_rx_ipv6(struct net *net, struct net_ipif *ipif, u8 *pkt, int len)
{

    return -1;
}

/*
 * ARP
 */
static int
_rx_arp(struct net *net, struct net_ipif *ipif, u8 *pkt, int len)
{
    struct ip_arp *arp;
    u8 *rpkt;

    /* Check the length first */
    if ( unlikely(len < sizeof(struct ip_arp)) ) {
        return -1;
    }

    arp = (struct ip_arp *)pkt;
    if ( 1 != bswap16(arp->hw_type) ) {
        /* Invalid ARP */
        return -1;
    }
    if ( 0x0800 != bswap16(arp->protocol) ) {
        /* Invalid ARP */
        return -1;
    }
    if ( 0x06 != arp->hlen ) {
        /* Invalid ARP */
        return -1;
    }
    if ( 0x04 != arp->plen ) {
        /* Invalid ARP */
        return -1;
    }

    /* FIXME: Should support multiple IP addresses on a single interface */
    if ( unlikely(NULL == ipif->ipv4) ) {
        return -1;
    }

    /* Check the destination IP address */
    switch ( bswap16(arp->opcode) ) {
    case 1:
        /* ARP request */
        if ( arp->dst_ip == ipif->ipv4->addr ) {
            /* Register the entry */
            net_arp_register(&ipif->ipv4->arp, arp->src_ip, arp->src_mac, 0);
            /* Send an ARP reply */
            u64 srcmac = ipif->mac;
            u64 dstmac = arp->src_mac;
            u64 srcip = ipif->ipv4->addr;
            u64 dstip = arp->src_ip;

            rpkt = alloca(net->sys_mtu);
            struct ethhdr *ehdr = (struct ethhdr *)rpkt;
            ehdr->dst = dstmac;
            ehdr->src = srcmac;
            ehdr->type = bswap16(0x0806);
            struct ip_arp *arp2 = (struct ip_arp *)(rpkt + sizeof(struct ethhdr));
            arp2->hw_type = bswap16(1);
            arp2->protocol = bswap16(0x0800);
            arp2->hlen = 0x06;
            arp2->plen = 0x04;
            arp2->opcode = bswap16(2);
            arp2->src_mac = srcmac;
            arp2->src_ip = srcip;
            arp2->dst_mac = dstmac;
            arp2->dst_ip = dstip;

            /* Search port */
            _tx_bridge(net, ipif->bridge, rpkt,
                       sizeof(struct ethhdr) + sizeof(struct ip_arp),
                       dstmac, ipif, 0);

            return 0;
        }
        break;
    case 2:
        /* ARP reply */
        if ( arp->dst_mac == ipif->mac
             && arp->dst_ip == ipif->ipv4->addr ) {
            /* Just register the entry */
            net_arp_register(&ipif->ipv4->arp, arp->src_ip, arp->src_mac, 0);
            return 0;
        }
        break;
    case 3:
        /* RARP request (not supported) */
        return -1;
    case 4:
        /* RARP reply (not supported) */
        return -1;
    default:
        /* Unsupported opcode */
        return -1;
    }

    return -1;
}

/*
 * IP interface
 */
static int
_rx_ipif(struct net *net, struct net_ipif *ipif, u8 *pkt, int len,
         struct ether *eth)
{
    switch ( eth->type ) {
    case 0x0800:
        /* IPv4 */
        return _rx_ipv4(net, ipif, pkt, len);
    case 0x0806:
        /* ARP */
        return _rx_arp(net, ipif, pkt, len);
    case 0x86dd:
        /* IPv6 */
        return _rx_ipv6(net, ipif, pkt, len);
    default:
        ;
    }

    return -1;
}

/*
 * Bridge
 */
static int
_rx_bridge(struct net *net, struct net_bridge *bridge, u8 *pkt, int len,
           struct ether *eth, void *srcport, int type)
{
    struct net_port *port;
    struct net_ipif *ipif;
    int i;
    int flag;

    /* Register FDB */
    flag = 0;
    for ( i = 0; i < bridge->fdb.nr; i++ ) {
        if ( eth->srcmac == bridge->fdb.entries[i].macaddr ) {
            switch ( bridge->fdb.entries[i].type ) {
            case NET_FDB_PORT_STATIC:
            case NET_FDB_IPIF:
                /* No need to update the static entry */
                flag = 1;
                break;
            case NET_FDB_PORT_DYNAMIC:
            case NET_FDB_INVAL:
            default:
                /* Update the dynamic entry */
                if ( type ) {
                    bridge->fdb.entries[i].u.port = srcport;
                    bridge->fdb.entries[i].type = NET_FDB_PORT_DYNAMIC;
                } else {
                    bridge->fdb.entries[i].u.ipif = srcport;
                    bridge->fdb.entries[i].type = NET_FDB_IPIF;
                }
                flag = 1;
                break;
            }
        }
    }
    if ( !flag ) {
        for ( i = 0; i < bridge->fdb.nr; i++ ) {
            if ( eth->srcmac == bridge->fdb.entries[i].macaddr ) {
                if ( NET_FDB_INVAL == bridge->fdb.entries[i].type ) {
                    /* Update the dynamic entry */
                    if ( type ) {
                        bridge->fdb.entries[i].u.port = srcport;
                        bridge->fdb.entries[i].type = NET_FDB_PORT_DYNAMIC;
                    } else {
                        bridge->fdb.entries[i].u.ipif = srcport;
                        bridge->fdb.entries[i].type = NET_FDB_IPIF;
                    }
                    break;
                }
            }
        }
    }

    /* Lookup FDB */
    for ( i = 0; i < bridge->fdb.nr; i++ ) {
        if ( eth->dstmac == bridge->fdb.entries[i].macaddr ) {
            /* Found the entry */
            switch ( bridge->fdb.entries[i].type ) {
            case NET_FDB_PORT_DYNAMIC:
            case NET_FDB_PORT_STATIC:
                /* To port */
                port = bridge->fdb.entries[i].u.port;
                return _tx_port(net, port, pkt, len);
                break;
            case NET_FDB_IPIF:
                /* To IP interface */
                ipif = bridge->fdb.entries[i].u.ipif;
                return _rx_ipif(net, ipif, pkt, len, eth);
            case NET_FDB_INVAL:
            default:
                break;
            }
        }
    }

    /* Unknown unicast */
    for ( i = 0; i < bridge->nr; i++ ) {
        port = bridge->ports[i];
        if ( srcport != port ) {
            /* Transmit */
            _tx_port(net, port, pkt, len);
        }
    }
    for ( i = 0; i < bridge->nr_ipif; i++ ) {
        ipif = bridge->ipifs[i];
        if ( srcport != ipif ) {
            /* Transmit */
            _rx_ipif(net, ipif, pkt, len, eth);
        }
    }

    return -1;
}

/*
 * 802.1Q
 */
static int
_rx_802_1q(struct net *net, struct net_port *port, u8 *pkt, int len,
           struct ether *eth)
{
    struct net_bridge *bridge;
    struct ethhdr1q *hdr;

    if ( unlikely(eth->vlan > 0) ) {
        /* VLAN is already specified */
        return -1;
    }

    /* Check the length first */
    if ( unlikely(len < sizeof(struct ethhdr1q)) ) {
        return -1;
    }

    hdr = (struct ethhdr1q *)pkt;
    /* Update VLAN information */
    eth->vlan = bswap16(hdr->vlan);
    if ( eth->vlan <= 0 || eth->vlan >= 4096 ) {
        return -1;
    }
    bridge = port->bridges[eth->vlan];

    return _rx_bridge(net, bridge, pkt + sizeof(struct ethhdr1q),
                      len - sizeof(struct ethhdr1q), eth, port, 1);
}

/*
 * Rx packet handler
 */
int
net_rx(struct net *net, struct net_port *port, u8 *pkt, int len, int vlan)
{
    struct ether eth;
    struct net_bridge *bridge;
    struct ethhdr *ehdr;

    /* Check the length first */
    if ( unlikely(len < sizeof(struct ethhdr)) ) {
        return -1;
    }

    /* Find out the corresponding bridge */
    eth.vlan = vlan;
    if ( eth.vlan < 0 ) {
        eth.vlan = 0;
    }
    if ( eth.vlan >= 4096 ) {
        return -1;
    }
    bridge = port->bridges[eth.vlan];

    /* Check layer 2 information first */
    ehdr = (struct ethhdr *)pkt;
    eth.type = bswap16(ehdr->type);
    eth.dstmac = ehdr->dst;
    eth.srcmac = ehdr->src;
    if ( 0x8100 == eth.type ) {
        /* 802.1Q */
        return _rx_802_1q(net, port, pkt + sizeof(struct ethhdr),
                          len - sizeof(struct ethhdr), &eth);
    } else {
        return _rx_bridge(net, bridge, pkt + sizeof(struct ethhdr),
                          len - sizeof(struct ethhdr), &eth, port, 1);
    }
}


/*
 * Initialize the network driver
 */
int
net_init(struct net *net)
{
    net->sys_mtu = 9000;

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
