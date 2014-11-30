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


struct tcpopts {
    int mss;
    int wscale;
};

#define TCP_MAX_SESSIONS 64
static struct tcp_session sessions[TCP_MAX_SESSIONS];





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
        sum += (u32)*cur;
        sum = (sum & 0xffff) + (sum >> 16);
        nleft -= 2;
        cur += 1;
    }
    if ( 1 == nleft ) {
        last.uc[0] = *(const u8 *)cur;
        last.uc[1] = 0;
        sum += last.us;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    //sum += (sum >> 16);
    ret = ~sum;

    return ret;
}

static u16
_ip_checksum(const u8 *data, int len)
{
    /* Compute checksum */
    const u16 *tmp;
    u32 cs;
    int i;

    tmp = (const u16 *)data;
    cs = 0;
    for ( i = 0; i < len / 2; i++ ) {
        cs += (u32)tmp[i];
        cs = (cs & 0xffff) + (cs >> 16);
    }
    cs = 0xffff - cs;

    return cs;
}


/*
 * Allocate a packet
 */
int
palloc(struct net *net)
{
    int head;
    if ( net->papp.ring.head == net->papp.ring.tail ) {
        /* No buffer available */
        return -1;
    }
    head = net->papp.ring.head;
    net->papp.ring.head = (net->papp.ring.head + 1) & net->papp.wrap;

    return head;
}

/*
 * Free a packet
 */
void
pfree(struct net *net, int idx)
{
    net->papp.ring.tail = (net->papp.ring.tail + 1) & net->papp.wrap;
    net->papp.ring.desc[net->papp.ring.tail] = idx;
}

/*
 * PAPP for IP
 */
int
net_papp_host_port_ip(struct net *net, void *data, u8 *hdr, int hdrlen, u8 *pkt,
                      u8 **pktp)
{
    int ret;
    struct net_papp_meta_host_port_ip *mdata;
    u32 nh;
    u64 macaddr;
    struct ethhdr *ehdr;
    struct iphdr *iphdr;
    u64 rnd;

    mdata = (struct net_papp_meta_host_port_ip *)data;

    /* Check the buffer length first */
    if ( unlikely(hdrlen < sizeof(struct ethhdr) + sizeof(struct iphdr)) ) {
        return -1;
    }

    /* Lookup next hop */
    ret = net_rib4_lookup(&mdata->hport->rib4, mdata->daddr, &nh);
    if ( ret < 0 ) {
        /* No route to host */
        return -NET_PAPP_ENORIBENT;
    }
    if ( !nh ) {
        /* Local scope */
        nh = mdata->daddr;
    }

    /* Lookup MAC address table */
    ret = net_arp_resolve(&mdata->hport->arp, nh, &macaddr);
    if ( ret < 0 ) {
        return -NET_PAPP_ENOARPENT;
    }

    /* Ethernet */
    ehdr = (struct ethhdr *)hdr;
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
    iphdr = (struct iphdr *)(hdr + sizeof(struct ethhdr));
    iphdr->ip_ihl = 5;
    iphdr->ip_version = 4;
    iphdr->ip_tos = 0;
    iphdr->ip_len = 0;
    iphdr->ip_id = rnd;
    iphdr->ip_off = 0;
    iphdr->ip_ttl = 64;
    iphdr->ip_proto = mdata->proto;
    /* Set source address */
    iphdr->ip_src = mdata->saddr;
    /* Set destination address */
    iphdr->ip_dst = mdata->daddr;

    /* Payload */
    *pktp = pkt + sizeof(struct ethhdr) + sizeof(struct iphdr);

    /* Return the offset */
    return sizeof(struct ethhdr) + sizeof(struct iphdr);
}

/*
 * PAPP for TCP
 */
int
net_papp_tcp(struct net *net, struct tcp_session *sess, u8 *hdr, int hdrlen,
             u8 *pkt, u8 **pktp)
{
    struct net_papp_meta_host_port_ip mdata;
    struct tcp_hdr *tcp;
    int len;
    int ret;

    /* Prepare meta data */
    mdata.hport = ((struct net_port *)(sess->tx->data))->next.data;
    mdata.saddr = sess->lipaddr;
    mdata.daddr = sess->ripaddr;
    mdata.flags = 0;
    mdata.proto = IP_TCP;

    /* Prepare a packet buffer for ACK */
    ret = net_papp_host_port_ip(net, &mdata, hdr, hdrlen, pkt, pktp);
    if ( ret < 0 ) {
        /* Error */
        return ret;
    }

    tcp = (struct tcp_hdr *)(hdr + ret);
    tcp->sport = sess->lport;
    tcp->dport = sess->rport;
    tcp->seqno = bswap32(sess->seq);
    tcp->ackno = bswap32(sess->ack);
    tcp->flag_ns = 0;
    tcp->flag_reserved = 0;
    tcp->offset = 5;
    tcp->flag_fin = 0;
    tcp->flag_syn = 0;
    tcp->flag_rst = 0;
    tcp->flag_psh = 1;
    tcp->flag_ack = 1;
    tcp->flag_urg = 0;
    tcp->flag_ece = 0;
    tcp->flag_cwr = 0;
    //tcp2->wsize = 0xffff;
    tcp->wsize = bswap16(
        ((sess->twin.sz + sess->twin.pos0 - sess->twin.pos1 - 1)
         % sess->twin.sz) >> sess->wscale);
    tcp->checksum = 0;
    tcp->urgptr = 0;    /* Check the buffer */

    return ret + sizeof(struct tcp_hdr);
}






/*
 * Packet allocation
 */
void *
tcp_papp(struct net *net, struct tcp_session *sess, int sz, int *asz)
{
    u8 *p;
    u8 *h;
    int idx;

    idx = palloc(net);
    if ( idx < 0 ) {
        return NULL;
    }

    p = net->papp.pkt.base + (idx * net->papp.pkt.sz);
    h = net->papp.hdr.base + (idx * net->papp.hdr.sz);

    //ptr = (u64)p;
    //idx = (ptr - net->papp.pkt.base) / net->papp.pkt.sz;

    return p;
}



/*
 * Parse TCP option
 */
int
tcp_opt_parse(u8 *tcpopts, int optlen, struct tcpopts *opts)
{
    u8 type;
    u8 len;
    u8 *val;

    opts->mss = -1;
    opts->wscale = -1;
    while ( optlen > 0 ) {
        type = *tcpopts;

        switch ( type ) {
        case TCP_OPT_EOL:
            /* End of option */
            optlen = 0;         /* not elegant... */
            continue;
        case TCP_OPT_NOP:
            /* NOP */
            tcpopts++;
            optlen--;
            continue;
        }

        /* TLV */
        len = *(tcpopts + 1);
        if ( unlikely(optlen < len || len < 2) ) {
            /* Invalid option format */
            return -1;
        }
        val = tcpopts + 2;

        tcpopts += len;
        optlen -= len;

        switch ( type ) {
        case TCP_OPT_MSS:
            /* MSS */
            if ( 4 == len ) {
                /* Valid MSS */
                opts->mss = bswap16(*(u16 *)val);
            }
            break;
        case TCP_OPT_WSCALE:
            /* Window scale option */
            if ( 3 == len ) {
                /* Valid option */
                opts->wscale = *val;
            }
            break;
        }
    }

    return 0;
}

/*
 * Send an ACK packet
 */
int
tcp_send_ack(struct net *net, struct net_stack_chain_next *tx,
             struct tcp_session *sess, u32 seq, int syn, int fin)
{
    u8 buf[256];
    int len;
    int plen;
    u8 *p;
    struct net_hps_host_port_ip_data mdata;
    struct tcp_hdr *tcp2;
    int ret;

    /* Prepare meta data */
    mdata.hport = ((struct net_port *)(tx->data))->next.data;
    mdata.saddr = sess->lipaddr;
    mdata.daddr = sess->ripaddr;
    mdata.flags = 0;
    mdata.proto = IP_TCP;
    len = sizeof(struct tcp_hdr);
    if ( syn ) {
        len += 8;
    }

    /* Prepare a packet buffer for ACK */
    ret = net_hps_host_port_ip_pre(net, &mdata, buf, 256, &p);
    if ( ret < len ) {
        return -1;
    }
    tcp2 = (struct tcp_hdr *)p;
    tcp2->sport = sess->lport;
    tcp2->dport = sess->rport;
    tcp2->seqno = bswap32(seq);
    tcp2->ackno = bswap32(sess->ack);
    tcp2->flag_ns = 0;
    tcp2->flag_reserved = 0;
    if ( syn ) {
        tcp2->offset = 5 + 2;
    } else {
        tcp2->offset = 5;
    }
    tcp2->flag_fin = fin;
    tcp2->flag_syn = syn;
    tcp2->flag_rst = 0;
    tcp2->flag_psh = 0;
    tcp2->flag_ack = 1;
    tcp2->flag_urg = 0;
    tcp2->flag_ece = 0;
    tcp2->flag_cwr = 0;
    //tcp2->wsize = 0xffff;
    tcp2->wsize = bswap16(1024);
    tcp2->checksum = 0;
    tcp2->urgptr = 0;

    if ( syn ) {
        u8 *opt;
        opt = p + sizeof(struct tcp_hdr);
        opt[0] = 2;
        opt[1] = 4;
        opt[2] = 0x05;
        opt[3] = 0xb4;
        opt[4] = 3;
        opt[5] = 3;
        opt[6] = 10;            /* Window scale option */
        opt[7] = 0;
    }

    /* Pseudo checksum */
    struct tcp_phdr4 *ptcp;
    plen = sizeof(struct tcp_phdr4);
    if ( syn ) {
        plen += 8;
    }
    ptcp = alloca(plen);
    kmemcpy(&ptcp->sport, tcp2, len);
    ptcp->saddr = sess->lipaddr;
    ptcp->daddr = sess->ripaddr;
    ptcp->zeros = 0;
    ptcp->proto = IP_TCP;
    ptcp->tcplen = bswap16(len + 0 /*payload*/);
    tcp2->checksum = _checksum((u8 *)ptcp, plen);

    if ( syn || fin ) {
        sess->seq++;
    }

    return net_hps_host_port_ip_post(net, &mdata, buf, len);
}

int
tcp_send_data(struct net *net, struct net_stack_chain_next *tx,
              struct tcp_session *sess, u32 seq, const u8 *pkt, u32 plen)
{
    u8 buf[4096];
    int len;
    u8 *p;
    struct net_hps_host_port_ip_data mdata;
    struct tcp_hdr *tcp2;
    int ret;

    mdata.hport = ((struct net_port *)(tx->data))->next.data;
    /* FIXME: implement the source address selection */
    mdata.saddr = sess->lipaddr;
    mdata.daddr = sess->ripaddr;
    mdata.flags = 0;
    mdata.proto = IP_TCP;
    len = sizeof(struct tcp_hdr);

    /* Prepare a packet buffer for ACK */
    ret = net_hps_host_port_ip_pre(net, &mdata, buf, 4096, &p);
    if ( ret < len ) {
        return -1;
    }
    tcp2 = (struct tcp_hdr *)p;
    tcp2->sport = sess->lport;
    tcp2->dport = sess->rport;
    tcp2->seqno = bswap32(seq);
    tcp2->ackno = bswap32(sess->ack);
    tcp2->flag_ns = 0;
    tcp2->flag_reserved = 0;
    tcp2->offset = 5;
    tcp2->flag_fin = 0;
    tcp2->flag_syn = 0;
    tcp2->flag_rst = 0;
    tcp2->flag_psh = 1;
    tcp2->flag_ack = 1;
    tcp2->flag_urg = 0;
    tcp2->flag_ece = 0;
    tcp2->flag_cwr = 0;
    //tcp2->wsize = 0xffff;
    tcp2->wsize = bswap16(
        ((sess->twin.sz + sess->twin.pos0 - sess->twin.pos1 - 1)
         % sess->twin.sz) >> sess->wscale);
    tcp2->checksum = 0;
    tcp2->urgptr = 0;    /* Check the buffer */

    /* Pseudo checksum */
    struct tcp_phdr4 *ptcp;
    ptcp = alloca(sizeof(struct tcp_phdr4) + plen);
    kmemcpy(&ptcp->sport, tcp2, len);
    ptcp->saddr = sess->lipaddr;
    ptcp->daddr = sess->ripaddr;
    ptcp->zeros = 0;
    ptcp->proto = IP_TCP;
    ptcp->tcplen = bswap16(len + plen /*payload*/);
    kmemcpy((u8 *)ptcp + sizeof(struct tcp_phdr4), pkt, plen);
    tcp2->checksum = _checksum((u8 *)ptcp, sizeof(struct tcp_phdr4) + plen);

    kmemcpy(p + sizeof(struct tcp_hdr), pkt, plen);

    return net_hps_host_port_ip_post(net, &mdata, buf, len + plen);
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
    struct net_papp_meta_host_port_ip mdata;
    struct icmp_hdr *icmp;

    mdata.hport = ((struct net_port *)(tx->data))->next.data;
    /* FIXME: implement the source address selection */
    mdata.saddr = mdata.hport->ip4addr.addrs[0];
    mdata.daddr = iphdr->ip_src;
    mdata.flags = 0;
    mdata.proto = IP_ICMP;

#if 0
    /* Prepare a packet buffer */
    int idx;
    idx = palloc(net);
    if ( idx < 0 ) {
        return -1;
    }
    u8 *pbuf;
    u8 *hbuf;
    pbuf = net->papp.pkt.base + (idx * net->papp.pkt.sz);
    hbuf = net->papp.hdr.base + (idx * net->papp.hdr.sz);
    ret = net_papp_host_port_ip(net, &mdata, hbuf, net->papp.hdr.sz, pbuf, &p);
    if ( ret < 0 ) {
        if ( -NET_PAPP_ENOARPENT == ret ) {
            /* No ARP entry found, then send an ARP request */
#if 0
            u8 abuf[1024];
            struct ip_arp *arp;
            struct ethhdr *ehdr;
            u64 macaddr;
            ehdr = (struct ethhdr *)abuf;
            ehdr->dst = 0xffffffffffffULL;
            kmemcpy(&macaddr, mdata.hport->macaddr, 6);
            ehdr->src = macaddr;
            ehdr->type = bswap16(ETHERTYPE_ARP);
            arp = (struct ip_arp *)(abuf + sizeof(struct ethhdr));
            arp->hw_type = bswap16(1);
            arp->protocol = bswap16(0x0800);
            arp->hlen = 0x06;
            arp->plen = 0x04;
            arp->opcode = bswap16(ARP_REQUEST);
            arp->src_mac = macaddr;
            arp->src_ip = mdata.saddr;
            arp->dst_mac = 0;
            arp->dst_ip = nh;
            mdata->hport->port->netdev->sendpkt(abuf,
                                                sizeof(struct ethhdr)
                                                + sizeof(struct ip_arp),
                                                mdata->hport->port->netdev);
            return ret;
#endif
        }
        return -1;
    }
#endif
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
    icmp->checksum = _checksum(p, sizeof(struct icmp_hdr) + len);

    return net_hps_host_port_ip_post(net, &mdata, buf,
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
 * IPv4 TCP handler
 */
static int
_ipv4_tcp(struct net *net, struct net_stack_chain_next *tx,
          struct iphdr *iphdr, u8 *pkt, int len)
{
    struct tcp_hdr *tcp;
    struct tcp_session *sess;
    struct tcp_session *lsess;
    int i;

    if ( unlikely(len < sizeof(struct tcp_hdr)) ) {
        return -1;
    }
    tcp = (struct tcp_hdr *)(pkt);

    sess = NULL;
    /* Find the corresponding session */
    for ( i = 0; i < TCP_MAX_SESSIONS; i++ ) {
        if ( TCP_CLOSED != sessions[i].state ) {
            if ( sessions[i].lipaddr == iphdr->ip_dst
                 && sessions[i].lport == tcp->dport
                 && sessions[i].ripaddr == iphdr->ip_src
                 && sessions[i].rport == tcp->sport ) {
                /* Found */
                sess = &sessions[i];
                break;
            }
        }
    }

    if ( tcp->flag_syn && !tcp->flag_ack ) {
        /* SYN */
#if 0
        kprintf("Received a SYN packet %d => %d\r\n",
                bswap16(tcp->sport), bswap16(tcp->dport));
#endif

        if ( NULL != sess ) {
            /* Already in use */
            return -1;
        }

        /* Find a listening port */
        lsess = NULL;
        for ( i = 0; i < TCP_MAX_SESSIONS; i++ ) {
            if ( TCP_LISTEN == sessions[i].state ) {
                if ( (sessions[i].lipaddr == iphdr->ip_dst
                      || 0 == sessions[i].lipaddr)
                     && sessions[i].lport == tcp->dport ) {
                    /* Found */
                    lsess = &sessions[i];
                    break;
                }
            }
        }
        if ( NULL == lsess ) {
            /* No session found */
            return -1;
        }

        /* Find an available session */
        sess = NULL;
        for ( i = 0; i < TCP_MAX_SESSIONS; i++ ) {
            if ( TCP_CLOSED == sessions[i].state ) {
                /* Found */
                sess = &sessions[i];
                break;
            }
        }
        if ( NULL == sess ) {
            /* No session found */
            return -1;
        }

        /* Copy */
        (void)kmemcpy(sess, lsess, sizeof(struct tcp_session));


        u64 rnd;
        int ret;
        ret = rdrand(&rnd);
        if ( ret < 0 ) {
            return ret;
        }

        sess->state = TCP_SYN_RECEIVED;
        sess->rwin.sz = 1024 * 1024;
        sess->rwin.buf = kmalloc(sizeof(u8) * sess->rwin.sz);
        sess->rwin.pos0 = 0;
        sess->rwin.pos1 = 0;
        sess->twin.sz = 1024 * 1024;
        sess->twin.buf = kmalloc(sizeof(u8) * sess->twin.sz);
        sess->twin.pos0 = 0;
        sess->twin.pos1 = 0;
        sess->lipaddr = iphdr->ip_dst;
        sess->lport = tcp->dport;
        sess->ripaddr = iphdr->ip_src;
        sess->rport = tcp->sport;
        sess->rcvwin = bswap16(tcp->wsize);
        sess->mss = 0;
        sess->wscale = 0;
        /* Initial sequence number */
        sess->seq = rnd;
        /* Received sequence number */
        sess->rseqno =  bswap32(tcp->seqno);
        sess->rackno = 0;

        sess->tx = tx;
        sess->net = net;

        /* Parse options */
        u8 *tcpopts = pkt + sizeof(struct tcp_hdr);
        int optsz = (tcp->offset << 2) - sizeof(struct tcp_hdr);
        struct tcpopts opts;
        ret = tcp_opt_parse(tcpopts, optsz, &opts);
        if ( ret < 0 ) {
            /* Invalid option format */
            return -1;
        }
        if ( opts.mss >= 0 ) {
            sess->mss = opts.mss;
        }
        if ( opts.wscale >= 0 ) {
            sess->wscale = opts.wscale;
        }

#if 0
        kprintf("MSS: %d, Window scaling opt: %d\r\n", sess->mss, sess->wscale);
#endif

        /* Last ack number */
        sess->ack = sess->rseqno + 1;
        sess->rseqno++;

        return tcp_send_ack(net, tx, sess, sess->seq, 1, 0);
    } else if ( tcp->flag_fin ) {
#if 0
        kprintf("Received a FIN packet %d => %d\r\n",
                bswap16(tcp->sport), bswap16(tcp->dport));
#endif

        if ( NULL == sess ) {
            /* Not established */
            return -1;
        }

        /* Last ack number */
        if ( sess->ack == bswap32(tcp->seqno) ) {
            /* FIXME */
            sess->state = TCP_CLOSED;

            /* Received all data (FIXME) */
            sess->ack = bswap32(tcp->seqno) + 1;
            return tcp_send_ack(net, tx, sess, sess->seq, 0, 0);
        } else {
            return -1;
        }
    } else {
        if ( NULL == sess ) {
            /* Not established */
            return -1;
        }
        sess->rcvwin = bswap16(tcp->wsize);

        if ( tcp->flag_ack ) {
#if 0
            kprintf("Received an ACK packet %d => %d\r\n",
                    bswap16(tcp->sport), bswap16(tcp->dport));
#endif

            if ( TCP_SYN_RECEIVED == sess->state ) {
                sess->state = TCP_ESTABLISHED;
            }
            sess->rackno = bswap32(tcp->ackno);
            if ( !tcp->flag_syn ) {
                u32 sz = sess->rackno - sess->seq;
                //kprintf("*** %x\r\n", sz);
                int npos0 = (sess->twin.pos0 + sz) % sess->twin.sz;
                if ( (sess->twin.sz + sess->twin.pos1 - npos0) % sess->twin.sz
                     < (sess->twin.sz + sess->twin.pos1 - sess->twin.pos0)
                     % sess->twin.sz ) {
                    sess->twin.pos0 = npos0;
                    sess->seq += sz;
                }
            }
        }
        u32 paylen;
        paylen = len - (tcp->offset << 2);

        if ( paylen > 0 ) {
            u32 tmp;
            tmp = bswap32(tcp->seqno);
            if ( sess->rseqno == tmp ) {
                sess->rseqno = tmp + paylen;
                sess->ack = sess->rseqno;
                sess->recv(sess, pkt + (tcp->offset << 2), paylen);
                int ret = tcp_send_ack(net, tx, sess, sess->seq, 0, 0);
                net_tcp_trigger(net);
                return ret;
            } else {
                /* Invalid: FIXME: send RST? */
                return -1;
            }
        } else {
            return -1;
        }
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
        return _ipv4_tcp(net, tx, hdr, payload, iplen - hdrlen);
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

    struct net_port *port;
    port = (struct net_port *)tx->data;
    return port->netdev->sendpkt(rpkt,
                                 sizeof(struct ethhdr) + sizeof(struct ip_arp),
                                 port->netdev);
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
        /* No entry found. */
        u8 abuf[1024];
        struct ip_arp *arp;
        ehdr = (struct ethhdr *)abuf;
        ehdr->dst = 0xffffffffffffULL;
        kmemcpy(&macaddr, mdata->hport->macaddr, 6);
        ehdr->src = macaddr;
        ehdr->type = bswap16(ETHERTYPE_ARP);
        arp = (struct ip_arp *)(abuf + sizeof(struct ethhdr));
        arp->hw_type = bswap16(1);
        arp->protocol = bswap16(0x0800);
        arp->hlen = 0x06;
        arp->plen = 0x04;
        arp->opcode = bswap16(ARP_REQUEST);
        arp->src_mac = macaddr;
        arp->src_ip = mdata->saddr;
        arp->dst_mac = 0;
        arp->dst_ip = nh;
        mdata->hport->port->netdev->sendpkt(abuf,
                                            sizeof(struct ethhdr)
                                            + sizeof(struct ip_arp),
                                            mdata->hport->port->netdev);
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
    iphdr->ip_proto = mdata->proto;
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
net_hps_host_port_ip_post(struct net *net, void *data, u8 *buf, int iplen)
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











int
_send(struct tcp_session *sess, const u8 *pkt, u32 plen)
{
    int bufsz;

    /* Check the buffer */
    bufsz = (sess->twin.sz + sess->twin.pos0 - sess->twin.pos1 - 1)
        % sess->twin.sz;

    if ( bufsz < plen ) {
        /* No buffer available */
        return -1;
    }

    /* Enqueue to the buffer */
    if ( sess->twin.pos1 + plen > sess->twin.sz ) {
        kmemcpy(sess->twin.buf + sess->twin.pos1, pkt,
                sess->twin.sz - sess->twin.pos1);
        kmemcpy(sess->twin.buf, pkt + sess->twin.sz - sess->twin.pos1,
                plen - sess->twin.sz + sess->twin.pos1);
    } else {
        kmemcpy(sess->twin.buf + sess->twin.pos1, pkt, plen);
    }

    sess->twin.pos1 = (sess->twin.pos1 + plen) % sess->twin.sz;

    return 0;
}

int
net_tcp_trigger(struct net *net)
{
    int i;
    int sz;
    int rcvwin;

    for ( i = 0; i < TCP_MAX_SESSIONS; i++ ) {
        if ( TCP_CLOSED == sessions[i].state ) {
            continue;
        }
        if ( TCP_ESTABLISHED != sessions[i].state ) {
            continue;
        }

        if ( sessions[i].twin.pos0 != sessions[i].twin.pos1 ) {
            /* Buffer */
            sz = (sessions[i].twin.sz + sessions[i].twin.pos1
                  - sessions[i].twin.pos0) % sessions[i].twin.sz;
            rcvwin = sessions[i].rcvwin;
            rcvwin <<= sessions[i].wscale;

            if ( sz > rcvwin ) {
                sz = rcvwin;
            }
            if ( sz <= 0 ) {
                continue;
            }

            /* Send packets */
            u8 *pkt = alloca(sessions[i].mss);
            int pktsz;
            int pos = 0;
            u32 seq = sessions[i].seq;
            while ( sz > 0 ) {
                if ( sz > sessions[i].mss ) {
                    pktsz = sessions[i].mss;
                } else {
                    pktsz = sz;
                }
                if ( sessions[i].twin.pos0  ) {
                    sessions[i].mss;
                }
                pos = sessions[i].twin.pos0;
                if ( pos + pktsz > sessions[i].twin.sz ) {
                    kmemcpy(pkt, sessions[i].twin.buf + pos,
                            sessions[i].twin.sz - pos);
                    kmemcpy(pkt,
                            sessions[i].twin.buf + sessions[i].twin.sz - pos,
                            pktsz - sessions[i].twin.sz + pos);
                } else {
                    kmemcpy(pkt, sessions[i].twin.buf + pos, pktsz);
                }
                //kprintf("**** %x %x\r\n", pos, sz);
                tcp_send_data(net, sessions[i].tx, &sessions[i],
                              seq, pkt, pktsz);
                sz -= pktsz;
                pos = (pos + pktsz) % sessions[i].twin.sz;
                seq += pktsz;
            }
        }
    }

    return 0;
}

/*
 * Initialize the network driver
 */
int shell_tcp_recv(struct tcp_session *, const u8 *, u32);
int
net_init(struct net *net)
{
    /* Set the system MTU */
    net->sys_mtu = 9216;

    int i;
    for ( i = 0; i < TCP_MAX_SESSIONS; i++ ) {
        sessions[i].state = TCP_CLOSED;
    }
    sessions[0].state = TCP_LISTEN;
    sessions[0].lipaddr = 0;
    sessions[0].lport = bswap16(23);
    sessions[0].recv = shell_tcp_recv;
    sessions[0].send = _send;

    /* Initialize PAPP */
    net->papp.len = 1<<7/*128*/;
    net->papp.wrap = (1<<7) - 1;
    net->papp.pkt.sz = ((net->sys_mtu + 4095) / 4096) * 4096;
    net->papp.pkt.base = (u64)kmalloc(net->papp.pkt.sz * net->papp.len);
    net->papp.hdr.sz = 256;
    net->papp.hdr.base = (u64)kmalloc(net->papp.hdr.sz * net->papp.len);
    net->papp.ring.desc = kmalloc(sizeof(int) * net->papp.len);
    net->papp.ring.head = 0;
    net->papp.ring.tail = net->papp.len - 1;
    for ( i = 0; i < net->papp.len; i++ ) {
        net->papp.ring.desc[i] = i;
    }

    return 0;
}

int
net_release(struct net *net)
{
    int i;

    for ( i = 0; i < TCP_MAX_SESSIONS; i++ ) {
        if ( TCP_ESTABLISHED == sessions[i].state ) {
            tcp_send_ack(net, sessions[i].tx, &sessions[i], sessions[i].seq,
                         0, 1);
        }
    }

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
