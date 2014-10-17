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
#include "net.h"

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



static int
rdrand(u64 *x)
{
    u64 r;
    u64 f;

    __asm__ __volatile__ ("rdrand %%rbx;"
                          "pushfq;"
                          "popq %%rax": "=b"(r), "=a"(f));
    *x = r;

    /* Return carry flag */
    return (f & 0x1) - 1;
}


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
 * RIB
 */
int
net_rib4_add(struct net_rib4 *r, u32 pf, int l, u32 nh)
{
    int i;
    int nr;
    struct net_rib4_entry *entries;

    for ( i = 0; i < r->nr; i++ ) {
        if ( r->entries[i].prefix == pf && r->entries[i].length == l ) {
            /* The entry for this prefix already exists */
            return -1;
        }
    }

    /* Not found */
    nr = r->nr + 1;
    entries = kmalloc(sizeof(struct net_rib4_entry) * nr);
    if ( NULL != r->entries ) {
        if ( NULL == entries ) {
            /* Memory error */
            return -1;
        }
        kmemcpy(entries, r->entries, sizeof(struct net_rib4_entry) * (nr - 1));
        kfree(r->entries);
    }
    entries[nr-1].prefix = pf;
    entries[nr-1].length = l;
    entries[nr-1].nexthop = nh;
    r->entries = entries;
    r->nr = nr;

    return 0;
}
int
net_rib4_lookup(struct net_rib4 *r, u32 addr, u32 *nh)
{
    u32 pf;
    int l;
    u32 cand;
    int i;
    u32 mask;

    l = -1;
    for ( i = 0; i < r->nr; i++ ) {
        if ( r->entries[i].length != 32 ) {
            mask = 0xffffffffULL >> (32 - r->entries[i].length);
        } else {
            mask = 0xffffffffULL;
        }
        if ( r->entries[i].prefix == (addr & mask) ) {
            /* Found */
            if ( l < r->entries[i].length ) {
                pf = r->entries[i].prefix;
                l = r->entries[i].length;
                cand = r->entries[i].nexthop;
            }
        }
    }

    if ( l < 0 ) {
        return -1;
    }
    *nh = cand;

    return 0;
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

static u16
_ip_checksum(const u8 *data, int len)
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


/*
 * Compute IPv4 checksum
 */
static u16
_ipv4_checksum(struct iphdr *iphdr)
{
    return _checksum((u8 *)iphdr, (int)iphdr->ip_ihl * 4);
}








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

#if 0
        /* Search port */
        _tx_bridge(net, ipif->bridge, rpkt,
                   sizeof(struct ethhdr) + sizeof(struct ip_arp),
                   dstmac, ipif, 0);
#endif

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
    case ETHERTYPE_IPV4:
        /* IPv4 */
        return _rx_ipv4(net, ipif, pkt, len);
    case ETHERTYPE_ARP:
        /* ARP */
        return _rx_arp(net, ipif, pkt, len);
    case ETHERTYPE_IPV6:
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
 * Rx IP
 */
int
net_sc_rx_ip(struct net *net, u8 *pkt, int len, void *data)
{
    return 0;
}

/*
 * Rx ethernet (called from driver)
 */
int
net_sc_rx_ether(struct net *net, u8 *pkt, int len, void *data)
{
    struct ether eth;
    struct ethhdr *ehdr;
    int ret;

    /* Check the length first */
    if ( unlikely(len < sizeof(struct ethhdr)) ) {
        return -1;
    }

    if ( NULL != data ) {
        /* To be supported */
        return -1;
    }

    /* Check layer 2 information first */
    ehdr = (struct ethhdr *)pkt;
    eth.type = bswap16(ehdr->type);
    eth.dstmac = ehdr->dst;
    eth.srcmac = ehdr->src;

    ret = net_sc_rx_ether(net, pkt + sizeof(struct ethhdr),
                          len - sizeof(struct ethhdr), &eth);

    /* Check by type */
    if ( ETHERTYPE_8021Q == eth.type ) {
        /* 802.1Q: Not supported */
        return -1;
    } else {
        /* Ether */
        kprintf("XXXX\r\n");
    }

    return ret;
}




/*
 * ICMP echo request handler
 */
static int
_ipv4_icmp_echo_request(struct net *net, struct net_stack_chain_next *tx,
                        struct iphdr *iphdr, struct icmp_hdr *icmpreq, u8 *pkt,
                        int len)
{
    int ret;
    u8 buf[4096];
    u8 *p;
    struct net_hps_host_port_ip_data mdata;
    struct icmp_hdr *icmp;

    mdata.hport = ((struct net_port *)(tx->data))->next.data;
    mdata.saddr = bswap32(0xc0a83803ULL);
    mdata.daddr = iphdr->ip_src;
    mdata.flags = 0;

    /* Prepare a packet buffer */
    ret = net_hps_host_port_ip_pre(net, &mdata, buf, 4096, &p);
    if ( ret < len + sizeof(struct icmp_hdr) ) {
        return -1;
    }
    icmp = (struct icmp_hdr *)p;
    icmp->type = ICMP_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->ident = icmpreq->ident;
    icmp->seq = icmpreq->seq;
    kmemcpy(p + sizeof(struct icmp_hdr), pkt, len);
    icmp->checksum = _ip_checksum(p, sizeof(struct icmp_hdr) + len);

    return net_hps_host_pore_ip_post(net, &mdata, buf,
                                     len + sizeof(struct icmp_hdr));
}

/*
 * IPv4 ICMP handler
 */
static int
_ipv4_icmp(struct net *net, struct net_stack_chain_next *tx,
           struct iphdr *iphdr, u8 *pkt, int len)
{
    struct icmp_hdr *icmp;

    if ( unlikely(len < sizeof(struct icmp_hdr)) ) {
        return -1;
    }
    icmp = (struct icmp_hdr *)(pkt);
    switch ( icmp->type ) {
    case ICMP_ECHO_REQUEST:
        return _ipv4_icmp_echo_request(net, tx, iphdr, icmp,
                                       pkt + sizeof(struct icmp_hdr),
                                       len - sizeof(struct icmp_hdr));
    }

    return -1;
}


/*
 * Process an IPv4 packet
 */
static int
_ipv4(struct net *net, struct net_stack_chain_next *tx, u8 *pkt, int len,
      struct net_ip4addr *ip4)
{
    struct iphdr *hdr;
    int hdrlen;
    int iplen;
    u8 *payload;
    int ret;
    int i;

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
    ret = -1;
    for ( i = 0; i < ip4->nr; i++ ) {
        if ( hdr->ip_dst == ip4->addrs[i] ) {
            /* Found */
            ret = 0;
            break;
        }
    }
    if ( ret < 0 ) {
        /* FIXME: Accept multicast should be specified by arguments */
        if ( bswap32(0xe0000001) == hdr->ip_dst ) {
            /* All node multicast */
            ret = 0;
        } else if ( bswap32(0xe0000002) == hdr->ip_dst ) {
            /* All router multicast */
            ret = 0;
        } else {
            return ret;
        }
    }

    /* Take its payload */
    payload = pkt + hdrlen;

    switch ( hdr->ip_proto ) {
    case IP_ICMP:
        /* ICMP */
        return _ipv4_icmp(net, tx, hdr, payload, iplen - hdrlen);
    case IP_TCP:
        /* TCP */
        return -1;
    case IP_UDP:
        /* UDP */
        break;
    default:
        /* Unsupported */
        ;
    }

    return -1;
}


/*
 * Process an ARP request
 */
static int
_arp_request(struct net *net, struct net_stack_chain_next *tx,
             struct ip_arp *arp, u64 macaddr, struct net_ip4addr *ip4,
             struct net_arp_table *atbl)
{
    int i;
    int ret;
    u32 ipaddr;

    /* ARP request */
    ret = -1;
    for ( i = 0; i < ip4->nr; i++ ) {
        if ( arp->dst_ip == ip4->addrs[i] ) {
            /* Found */
            ret = 0;
            ipaddr = ip4->addrs[i];
            /* Register the entry */
            net_arp_register(atbl, arp->src_ip, arp->src_mac, 0);
            break;
        }
    }
    if ( ret < 0 ) {
        return ret;
    }
    /* Send an ARP reply */
    u64 srcmac = macaddr;;
    u64 dstmac = arp->src_mac;
    u64 srcip = ipaddr;
    u64 dstip = arp->src_ip;
    u8 *rpkt;
    rpkt = alloca(net->sys_mtu);
    struct ethhdr *ehdr = (struct ethhdr *)rpkt;
    ehdr->src = srcmac;
    ehdr->dst = dstmac;
    ehdr->type = bswap16(ETHERTYPE_ARP);
    struct ip_arp *arp2 = (struct ip_arp *)
        (rpkt + sizeof(struct ethhdr));
    arp2->hw_type = bswap16(1);
    arp2->protocol = bswap16(0x0800);
    arp2->hlen = 0x06;
    arp2->plen = 0x04;
    arp2->opcode = bswap16(2);
    arp2->src_mac = srcmac;
    arp2->src_ip = srcip;
    arp2->dst_mac = dstmac;
    arp2->dst_ip = dstip;

    return tx->func(net, rpkt, sizeof(struct ethhdr) + sizeof(struct ip_arp),
                    tx->data);
}

/*
 * Process an ARP reply packet
 */
static int
_arp_reply(struct net *net, struct net_stack_chain_next *tx,
           struct ip_arp *arp, u64 macaddr, struct net_ip4addr *ip4,
           struct net_arp_table *atbl)
{
    int i;
    int ret;
    u32 ipaddr;

    /* ARP reply */
    if ( arp->dst_mac != macaddr ) {
        return -1;
    }

    ret = -1;
    for ( i = 0; i < ip4->nr; i++ ) {
        if ( arp->dst_ip == ip4->addrs[i] ) {
            /* Found */
            ret = 0;
            ipaddr = ip4->addrs[i];
            /* Register the entry */
            net_arp_register(atbl, arp->src_ip, arp->src_mac, 0);
            break;
        }
    }
    if ( ret < 0 ) {
        return ret;
    }

    return -1;
}

/*
 * Basic MAC address filter
 */
static int
_mac_addr_filter_basic(u64 mac, struct net_macaddr *addrs)
{
    int i;

    for ( i = 0; i < addrs->nr; i++ ) {
        if ( mac == addrs->addrs[i] ) {
            /* Unicast */
            return 0;
        }
    }
    if ( 0xffffffffffffULL == mac ) {
        /* Broadcast */
        return 0;
    } else if ( mac & 0x010000000000 ) {
        /* Multicast */
        return 0;
    }

    /* Not found */
    return -1;
}

/*
 * Host L3 port stack chain
 */
int
net_sc_rx_port_host(struct net *net, u8 *pkt, int len, void *data)
{
    struct ether eth;
    struct ethhdr *ehdr;
    struct net_port_host *hport;
    struct ip_arp *arp;
    u64 macaddr;
    int ret;
    struct net_macaddr mac;

    /* Check the length and data first */
    if ( unlikely( len < sizeof(struct ethhdr) ) ) {
        return -1;
    }
    if ( unlikely( NULL == data ) ) {
        return -1;
    }

    /* Stack chain data */
    hport = (struct net_port_host *)data;

    /* Get the MAC address of this port */
    kmemcpy(&macaddr, hport->macaddr, 6);
    mac.nr = 1;
    mac.addrs = &macaddr;

    /* Check layer 2 information first */
    ehdr = (struct ethhdr *)pkt;
    eth.type = bswap16(ehdr->type);
    eth.dstmac = ehdr->dst;
    eth.srcmac = ehdr->src;

    /* Filter */
    if ( _mac_addr_filter_basic(eth.dstmac, &mac) < 0 ) {
        /* The destination MAC address doesn't mache this port */
        return -1;
    }

    /* Check the ethernet type */
    switch ( eth.type ) {
    case ETHERTYPE_ARP:
        /* Check the length first */
        if ( unlikely(len - sizeof(struct ethhdr) < sizeof(struct ip_arp)) ) {
            return -1;
        }
        arp = (struct ip_arp *)(pkt + sizeof(struct ethhdr));
        /* Validate the ARP packet */
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

        /* Check the destination IP address */
        switch ( bswap16(arp->opcode) ) {
        case ARP_REQUEST:
            /* ARP request */
            ret = _arp_request(net, &hport->tx, arp, macaddr, &hport->ip4addr,
                               &hport->arp);
            break;
        case ARP_REPLY:
            /* ARP reply */
            ret = _arp_reply(net, &hport->tx, arp, macaddr, &hport->ip4addr,
                             &hport->arp);
            break;
        case RARP_REQUEST:
        case RARP_REPLY:
            /* Not supported */
            ret = -1;
            break;
        }
        break;
    case ETHERTYPE_IPV4:
        /* IPv4 */
        ret = _ipv4(net, &hport->tx, pkt + sizeof(struct ethhdr),
                    len - sizeof(struct ethhdr), &hport->ip4addr);
        break;
    default:
        /* To support other protocols */
        ret = -1;
    }

    return ret;
}


/*
 * Host L3 port stack chain for transmission
 */
int
net_sc_tx_port(struct net *net, u8 *pkt, int len, void *data)
{
    struct net_port *port;

    port = (struct net_port *)data;

    return port->netdev->sendpkt(pkt, len, port->netdev);
}

/*
 * Host L3 port stack chain (header/payload separation)
 */
int
net_hps_host_port_ip_pre(struct net *net, void *data, u8 *buf, int len, u8 **p)
{
    struct ethhdr *ehdr;
    struct iphdr *iphdr;
    struct net_hps_host_port_ip_data *mdata;
    u32 nh;
    u64 macaddr;
    int ret;
    u64 rnd;

    /* Check the buffer length first */
    if ( unlikely(len < sizeof(struct ethhdr) + sizeof(struct iphdr)) ) {
        return -1;
    }

    /* Meta data */
    mdata = (struct net_hps_host_port_ip_data *)data;

    /* Lookup next hop */
    ret = net_rib4_lookup(&mdata->hport->rib4, mdata->daddr, &nh);
    if ( ret < 0 ) {
        /* No route to host */
        return -1;
    }
    if ( !nh ) {
        /* Local scope */
        nh = mdata->daddr;
    }

    /* Lookup MAC address table */
    ret = net_arp_resolve(&mdata->hport->arp, nh, &macaddr);
    if ( ret < 0 ) {
        /* No entry found. FIXME: To send an ARP request */
        return ret;
    }

    /* Ethernet */
    ehdr = (struct ethhdr *)buf;
    ehdr->dst = macaddr;
    kmemcpy(&macaddr, mdata->hport->macaddr, 6);
    ehdr->src = macaddr;
    ehdr->type = bswap16(ETHERTYPE_IPV4);

    /* Get a random number */
    ret = rdrand(&rnd);
    if ( ret < 0 ) {
        return ret;
    }

    /* IP */
    iphdr = (struct iphdr *)(buf + sizeof(struct ethhdr));
    iphdr->ip_ihl = 5;
    iphdr->ip_version = 4;
    iphdr->ip_tos = 0;
    iphdr->ip_len = 0;
    iphdr->ip_id = rnd;
    iphdr->ip_off = 0;
    iphdr->ip_ttl = 64;
    iphdr->ip_proto = IP_ICMP;
    /* Set source address */
    iphdr->ip_src = mdata->saddr;
    /* Set destination address */
    iphdr->ip_dst = mdata->daddr;

    /* Payload */
    *p = buf + sizeof(struct ethhdr) + sizeof(struct iphdr);

    return len - sizeof(struct ethhdr) + sizeof(struct iphdr);
}

/*
 * Ethernet
 */
int
net_hps_host_pore_ip_post(struct net *net, void *data, u8 *buf, int iplen)
{
    struct iphdr *iphdr;
    struct net_hps_host_port_ip_data *mdata;
    u8 *p;
    int len;

    /* Meta data */
    mdata = (struct net_hps_host_port_ip_data *)data;

    iphdr = (struct iphdr *)(buf + sizeof(struct ethhdr));
    iphdr->ip_len = bswap16(iplen + sizeof(struct iphdr));
    len = iplen + sizeof(struct ethhdr) + sizeof(struct iphdr);

    p = buf + sizeof(struct ethhdr) + sizeof(struct iphdr);
    iphdr->ip_sum = 0;
    iphdr->ip_sum = _ip_checksum((u8 *)iphdr, sizeof(struct iphdr));

    return mdata->hport->port->netdev->sendpkt(buf, len,
                                               mdata->hport->port->netdev);
}














/*
 * Initialize the network driver
 */
int
net_init(struct net *net)
{
    /* Set the system MTU */
    net->sys_mtu = 9216;

    return 0;
}










#if 0
/*
 * Lookup forwarding database
 */
struct net_fdb_entry *
net_switch_fdb_lookup(struct net_switch *sw)
{
    return NULL;
}



/*
 * Register L3 interface
 */
int
net_l3if_register(struct net *net)
{
    return 0;
}

/*
 * Unregister L3 interface
 */
int
net_l3if_unregister(struct net *net, struct net_l3if *l3if)
{
    return 0;
}

/*
 * Add an IP address to a L3 interface
 */
int
net_l3if_ipv4_addr_add(struct net *net)
{
    return 0;
}

/*
 * Delete an IP address from a L3 interface
 */
int
net_l3if_ipv4_addr_delete(struct net *net)
{
    return 0;
}
#endif







/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
