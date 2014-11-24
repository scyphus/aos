/*_
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "kernel.h"
#include "../drivers/pci/pci.h"

extern struct netdev_list *netdev_head;
extern struct net gnet;

int net_init(struct net *);
int net_rx(struct net *, struct net_port *, u8 *, int, int);
int net_sc_rx_ether(struct net *, u8 *, int, void *);
int net_sc_rx_port_host(struct net *, u8 *, int, void *);
int net_rib4_add(struct net_rib4 *, const u32, int, u32);
u32 bswap32(u32);

/*
 * Like inet_pton
 */
static int
str2v4addr(const char *s, u32 *addr, int *mask)
{
    u32 a;
    int d;
    const char *s0;
    int i;

    a = 0;

    for ( i = 0; i < 4; i++ ) {
        /* . */
        if ( i != 0 ) {
            if ( '.' != *s ) {
                return -1;
            }
            s++;
        }
        /* Take a digit */
        s0 = s;
        while ( *s && *s >= '0' && *s <= '9' ) {
            s++;
        }
        if ( s == s0 ) {
            return -1;
        } else if ( s - s0 > 3 ) {
            return -1;
        } else if ( s - s0 != 1 && '0' == *s0 ) {
            return -1;
        }
        d = 0;
        while ( s != s0 ) {
            d *= 10;
            d += (*s0) - '0';
            s0++;
        }
        if ( d > 255 || d < 0 ) {
            return -1;
        }
        a <<= 8;
        a += d;
    }

    if ( '/' == *s ) {
        /* With mask */
        s++;
        s0 = s;
        d = 0;
        while ( *s ) {
            if ( *s < '0' || *s > '9' )  {
                return -1;
            }
            d *= 10;
            d += (*s) - '0';
            s++;
        }
        if ( d < 0 || d > 32 ) {
            return -1;
        }
        *mask = d;
        *addr = a;
    } else if ( *s ) {
        return -1;
    } else {
        *mask = -1;
        *addr = a;
    }

    return 0;
}


int
mgmt_tcp_server(void)
{
    struct tcp_session *sess;

    /* Allocate */
    sess = kmalloc(sizeof(struct tcp_session));
    sess->state = TCP_CLOSED;
    sess->rwin.sz = 1024 * 1024;
    sess->rwin.buf = kmalloc(sizeof(u8) * sess->rwin.sz);
    sess->twin.sz = 1024 * 1024;
    sess->twin.buf = kmalloc(sizeof(u8) * sess->twin.sz);
    sess->lipaddr = 0;
    sess->lport = 80;
    sess->ripaddr = 0;
    sess->rport = 0;
    sess->mss = 536;
    sess->wscale = 1;

    return 0;
}


/*
 * SYN
 */
int
mgmt_tcp_syn_recv(void)
{
    /* Send SYN+ACK */

    return 0;
}




int net_tcp_trigger(struct net *);

/*
 * Management process
 * start mgmt <cpuid> <nic> <ip address/mask> <default-gw>
 */
int
mgmt_main(int argc, char *argv[])
{
    /* Search network device for management */
    struct netdev_list *list;
    struct netdev *netdev;
    char *nic;
    char *ipaddr;
    char *gw;
    int m;
    int ret;
    u32 ipa;
    int ipm;
    u32 gwa;

    /* Check the argument first */
    if ( argc < 4 ) {
        kprintf("Invalid argument: %s <nic> <ip address/mask> <default-gw>\r\n",
                argv[0]);
        return -1;
    }

    nic = argv[1];
    ipaddr = argv[2];
    gw = argv[3];

    /* Obtain the specified netdev */
    netdev = NULL;
    list = netdev_head;
    while ( NULL != list ) {
        if ( 0 == kstrcmp(nic, list->netdev->name) ) {
            netdev = list->netdev;
            break;
        }
        list = list->next;
    }
    if ( NULL == netdev ) {
        kprintf("netdev %s cannot be found\r\n", nic);
        return -1;
    }

    ret = str2v4addr(ipaddr, &ipa, &ipm);
    if ( ret < 0 || ipm < 0 ) {
        /* Wrint IP address */
        kprintf("%s is wrong IP address format\r\n", ipaddr);
        return -1;
    }
    ret = str2v4addr(gw, &gwa, &m);
    if ( ret < 0 || m >= 0 ) {
        /* Wrint gateway address */
        kprintf("%s is wrong gateway address format\r\n", gw);
        return -1;
    }


    /* Network */
    int i;
    int n;
    u8 *pkt;
    struct net_port port;
    struct net_port_host hport;

    arch_busy_usleep(1000);

    pkt = alloca(gnet.sys_mtu);

    /* Port */
    kmemcpy(hport.macaddr, netdev->macaddr, 6);
    hport.ip4addr.nr = 1;
    hport.ip4addr.addrs = kmalloc(sizeof(u32) * 1);
    if ( NULL == hport.ip4addr.addrs ){
        return -1;
    }
    hport.ip4addr.addrs[0] = bswap32(ipa);
    hport.arp.sz = 4096;
    hport.arp.entries = kmalloc(sizeof(struct net_arp_entry) * hport.arp.sz);
    for ( i = 0; i < hport.arp.sz; i++ ) {
        hport.arp.entries[i].state = -1;
    }
    hport.ip6addr.nr = 0;
    hport.port = &port;
    port.netdev = netdev;
    port.next.data = (void *)&hport;
    port.next.func = net_sc_rx_port_host;
    hport.tx.func = NULL;
    hport.tx.data = (void *)&port;

    /* Routing table */
    hport.rib4.nr = 0;
    hport.rib4.entries = NULL;
    net_rib4_add(&hport.rib4, bswap32(ipa & (0xffffffffULL<<(32-ipm))), ipm, 0);
    net_rib4_add(&hport.rib4, 0, 0, bswap32(gwa));

    u64 tsc0 = rdtsc();
    u64 tsc1;
    for ( ;; ) {
        n = port.netdev->recvpkt(pkt, gnet.sys_mtu, port.netdev);
        if ( n <= 0 ) {
            continue;
        }
        port.next.func(&gnet, pkt, n, port.next.data);

        /* Trigger for TCP transmission */
        tsc1 = rdtsc();
        if ( tsc1 - tsc0 > 3000 * 1000 * 100 /* ~100ms FIXME */ ) {
            net_tcp_trigger(&gnet);
            tsc0 = tsc1;
        }
    }

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
