/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "kernel.h"
#include "../drivers/pci/pci.h"

#define CMDBUF_SIZE 4096
#define ARGS_MAX 128

extern struct netdev_list *netdev_head;

/*
 * Temporary: Keyboard drivers
 */
void kbd_event(void);
volatile int kbd_read(void);
void hlt1(void);

struct kshell {
    char cmdbuf[CMDBUF_SIZE];
    int pos;
};

struct kcmd {
    char **cmds;
};



/* Clear */
static void
_init(struct kshell *kshell)
{
    kshell->pos = 0;
    kshell->cmdbuf[0] = '\0';
    kprintf("> ");
}

/*
 * upa
 */
static int
_builtin_panic(char *const argv[])
{
    panic("Executing user PANIC!!");

    return 0;
}

/*
 * off
 */
int
_builtin_off(char *const argv[])
{
    arch_poweroff();

    return 0;
}

/*
 * uptime
 */
int
_builtin_uptime(char *const argv[])
{
    u64 x;

    x = arch_clock_get();
    kprintf("uptime: %llu.%.9llu sec\r\n", x / 1000000000, x % 1000000000);

    return 0;
}

/*
 * show
 */
int ixgbe_check_buffer(struct netdev *);
#define MAX_PROCESSORS  256
struct p_data {
    u32 flags;          /* bit 0: enabled (working); bit 1 reserved */
} __attribute__ ((packed));

int
_builtin_show(char *const argv[])
{
    if ( 0 == kstrcmp("interfaces", argv[1])
         || 0 == kstrcmp("int", argv[1])
         || 0 == kstrcmp("if", argv[1]) ) {
        struct netdev_list *list;
        list = netdev_head;
        while ( list ) {
            kprintf(" %s\r\n", list->netdev->name);
            kprintf("   HWADDR: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
                    list->netdev->macaddr[0],
                    list->netdev->macaddr[1],
                    list->netdev->macaddr[2],
                    list->netdev->macaddr[3],
                    list->netdev->macaddr[4],
                    list->netdev->macaddr[5]);
            list = list->next;
        }
    } else if ( 0 == kstrcmp("pci", argv[1]) ) {
        struct pci *list;
        list = pci_list();
        while ( list ) {
            kprintf("%x.%x.%x %.4x:%.4x\r\n", list->device->bus,
                    list->device->slot, list->device->func,
                    list->device->vendor_id, list->device->device_id);
            list = list->next;
        }
    } else if ( 0 == kstrcmp("processors", argv[1]) ) {
#define	P_DATA_SIZE             0x10000
#define P_DATA_BASE             (u64)0x01000000
        struct p_data *pdata;
        int i;
        for ( i = 0; i < MAX_PROCESSORS; i++ ) {
            pdata = (struct p_data *)((u64)P_DATA_BASE + i * P_DATA_SIZE);
            if ( pdata->flags & 1 ) {
                kprintf("Processor #%d is active.\r\n", i);
            }
        }

    } else {
        kprintf("show <interfaces|pci|processors>\r\n");
    }

    return 0;
}

/*
 * test packet
 */
int ixgbe_tx_test(struct netdev *, u8 *, int, int);
int
_builtin_test(char *const argv[])
{
    struct netdev_list *list;
    u8 *pkt;
    //int pktsz = 64 - 18;
    int i;
    int sz;
    int blk;
    char *s;

    s = argv[1];
    sz = 0;
    while ( *s ) {
        sz *= 10;
        sz += *s - '0';
        s++;
    }
    s = argv[2];
    blk = 0;
    while ( *s ) {
        blk *= 10;
        blk += *s - '0';
        s++;
    }
    kprintf("Testing: %d/%d\r\n", sz, blk);

    int pktsz = sz - 18;

    pkt = kmalloc(9200);

    list = netdev_head;
    /* dst (multicast) */
#if 0
    pkt[0] = 0x01;
    pkt[1] = 0x00;
    pkt[2] = 0x5e;
    pkt[3] = 0x00;
    pkt[4] = 0x00;
    pkt[5] = 0x01;
#else
#if 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x6a;
    pkt[4] = 0x0c;
    pkt[5] = 0x40;
    //90:e2:ba:6a:0c:40
    //90:e2:ba:6a:0c:41
#else
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x68;
    pkt[4] = 0xb4;
    pkt[5] = 0xb4;
#endif
#endif

    /* src */
    pkt[6] = list->netdev->macaddr[0];
    pkt[7] = list->netdev->macaddr[1];
    pkt[8] = list->netdev->macaddr[2];
    pkt[9] = list->netdev->macaddr[3];
    pkt[10] = list->netdev->macaddr[4];
    pkt[11] = list->netdev->macaddr[5];

    /* type = IP (0800) */
    pkt[12] = 0x08;
    pkt[13] = 0x00;
    /* IP header */
    pkt[14] = 0x45;
    pkt[15] = 0x00;
    pkt[16] = (pktsz >> 8) & 0xff;
    pkt[17] = pktsz & 0xff;
    /* ID / fragment */
    pkt[18] = 0x26;
    pkt[19] = 0x6d;
    pkt[20] = 0x00;
    pkt[21] = 0x00;
    /* TTL/protocol */
    pkt[22] = 0x64;
    pkt[23] = 17;
    /* checksum */
    pkt[24] = 0x00;
    pkt[25] = 0x00;
    /* src: 192.168.56.2 */
    pkt[26] = 192;
    pkt[27] = 168;
    pkt[28] = 100;
    pkt[29] = 2;
    /* dst */
#if 0
    pkt[30] = 224;
    pkt[31] = 0;
    pkt[32] = 0;
    pkt[33] = 1;
#else
    pkt[30] = 10;
    pkt[31] = 0;
    pkt[32] = 0;
    pkt[33] = 100;
#endif

    /* UDP */
    pkt[34] = 0xff;
    pkt[35] = 0xff;
    pkt[36] = 0xff;
    pkt[37] = 0xfe;
    pkt[38] = (pktsz - 20) >> 8;
    pkt[39] = (pktsz - 20) & 0xff;
    pkt[40] = 0x00;
    pkt[41] = 0x00;
    for ( i = 42; i < pktsz + 14; i++ ) {
        pkt[i] = 0;
    }

    /* Compute checksum */
    u16 *tmp;
    u32 cs;
    pkt[24] = 0x0;
    pkt[25] = 0x0;
    tmp = (u16 *)pkt;
    cs = 0;
    for ( i = 7; i < 17; i++ ) {
        cs += (u32)tmp[i];
        cs = (cs & 0xffff) + (cs >> 16);
    }
    cs = 0xffff - cs;
    pkt[24] = cs & 0xff;
    pkt[25] = cs >> 8;

    ixgbe_tx_test(list->next->netdev, pkt, pktsz + 18 - 4, blk);
    //kprintf("%llx: pkt\r\n", list->netdev);
    //list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);

    return 0;
}

int ixgbe_forwarding_test(struct netdev *, struct netdev *);
int
_builtin_test2(char *const argv[])
{
    struct netdev_list *list;

    list = netdev_head;
    ixgbe_forwarding_test(list->netdev, list->next->netdev);
    return 0;
}


int
atoi(const char *s)
{
    int i;
    int sign;

    if ( '-' == *s ) {
        /* Minus */
        sign = -1;
        s++;
    } else if ( '+' == *s ) {
        sign = 1;
        s++;
    } else {
        sign = 1;
    }

    i = 0;
    while ( '\0' != *s ) {
        i *= 10;
        if ( *s < '0' || *s > '9' ) {
            return sign * i;
        } else {
            i += *s - '0';
        }
        s++;
    }

    return sign * i;
}


struct fib {
    int sz;
    int n;
    u64 *ipaddr;
    u64 *macaddr;
};
static struct fib fib;
u64 rdtsc(void);

static u64
_mgmt_operate(u8 *data)
{
    u64 ret;
    u16 vid;
    u64 ipaddr;
    u64 prefix;
    int prefixlen;
    u64 mac;
    int i;
    int found;
    u64 t0;
    u64 t1;

    if ( data[0] == 1 ) {
        prefix = ((((u64)data[1]) << 56) | (((u64)data[2]) << 48)
                  | (((u64)data[3]) << 40) | (((u64)data[4]) << 32));
        prefixlen = data[5];
        ipaddr = ((((u64)data[6]) << 56) | (((u64)data[7]) << 48)
                  | (((u64)data[8]) << 40) | (((u64)data[9]) << 32));
        found = -1;
        for ( i = 0; i < fib.n; i++ ) {
            if ( fib.ipaddr[i] == ipaddr ) {
                found = i;
                break;
            }
        }
        t0 = rdtsc();
        if ( found < 0 ) {
            /* Not found */
            kprintf("FIB not found.\r\n");
        } else {
            //kprintf("FIB: %llx/%d %llx %llx (%d)\r\n", prefix, prefixlen,
            //ipaddr, fib.macaddr[found], found);
            ptcam_add_entry(tcam, prefix, prefixlen, fib.macaddr[found]);
        }
        t1 = rdtsc();
        ret = t1 - t0;
    } else if ( data[0] == 2 ) {
        vid = (((u16)data[1] << 8) | data[2]);
        ipaddr = ((((u64)data[3]) << 56) | (((u64)data[4]) << 48)
                  | (((u64)data[5]) << 40) | (((u64)data[6]) << 32));
        mac = ((((u64)data[7])) | (((u64)data[8]) << 8)
               | (((u64)data[9]) << 16) | (((u64)data[10]) << 24)
               | (((u64)data[11]) << 32) | (((u64)data[12]) << 40));

        /* Insert to the fib */
        if ( fib.n + 1 >= fib.sz ) {
            kprintf("FIB is full\r\n");
        } else {
            found = -1;
            for ( i = 0; i < fib.n; i++ ) {
                if ( fib.ipaddr[i] == ipaddr ) {
                    found = i;
                    break;
                }
            }
            if ( found < 0 ) {
                fib.ipaddr[fib.n] = ipaddr;
                fib.macaddr[fib.n] = mac;
                //kprintf("MAC: %llx %llx\r\n", fib.ipaddr[fib.n], fib.macaddr[fib.n]);
                fib.n++;
            }
        }
        ret = 0;
    } else if ( data[0] == 3 ) {
        /* Commit */
        kprintf("Commit start\r\n");
        t0 = rdtsc();
        ptcam_commit(tcam);
        t1 = rdtsc();
        kprintf("Commit done. %x\r\n", t1 - t0);
        ret = t1 - t0;
    } else if ( data[0] == 4 ) {
        /* Lookup */
        ipaddr = ((((u64)data[1]) << 56) | (((u64)data[2]) << 48)
                  | (((u64)data[3]) << 40) | (((u64)data[4]) << 32));

        u64 cc = 0;
        u64 aa = 0;
        u64 dd = 0;
        u64 cm0;
        u64 cf0;
        u64 cm1;
        u64 cf1;
        cc = 0;
        __asm__ __volatile__ ("rdpmc" : "=a"(aa), "=d"(dd)  : "c"(cc) );
        cm0 = (dd<<32) | aa;
        cc = 1;
        __asm__ __volatile__ ("rdpmc" : "=a"(aa), "=d"(dd)  : "c"(cc) );
        cf0 = (dd<<32) | aa;

        u64 tmp;
#if 0
        t0 = rdtsc();
        tmp = ptcam_lookup(tcam, ipaddr);
        t1 = rdtsc();
#endif

        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
        t0 = (dd<<32) | aa;
        tmp = ptcam_lookup(tcam, ipaddr);
        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
        t1 = (dd<<32) | aa;

#if 0
        kprintf("Lookup: %llx %llx (%d.%d.%d.%d)\r\n", ipaddr, tmp, data[1],
                data[2], data[3], data[4]);
#endif
#if 0
        cc = 0;
        __asm__ __volatile__ ("rdpmc" : "=a"(aa), "=d"(dd)  : "c"(cc) );
        cm1 = (dd<<32) | aa;
        cc = 1;
        __asm__ __volatile__ ("rdpmc" : "=a"(aa), "=d"(dd)  : "c"(cc) );
        cf1 = (dd<<32) | aa;
#if 0
        kprintf("LLC misses / refs: %d %d\r\n", cm1 - cm0, cf1 - cf0);
#endif
        kprintf("Cycles: %d\r\n", cf1 - cf0);
#endif

        ret = t1 - t0;
    } else if ( data[0] == 5 ) {
        t0 = rdtsc();
        t1 = rdtsc();
        ret = t1 - t0;
    } else if ( data[0] == 6 ) {
        u64 aa = 0;
        u64 dd = 0;

        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc"
                              : "=a"(aa), "=d"(dd) : );
        t0 = (dd<<32) | aa;
        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc"
                              : "=a"(aa), "=d"(dd) : );
        t1 = (dd<<32) | aa;
        ret = t1 - t0;
    } else {
        t0 = rdtsc();
        arch_busy_usleep(1000 * 1000);
        t1 = rdtsc();
        ret = t1 - t0;
    }

    return ret;
}

int
_mgmt_main(int argc, char *argv[])
{
    /* Search network device for management */
    struct netdev_list *list;
    u8 pkt[512];
    u8 pkt2[512];
    u8 *ip;
    u8 *udp;
    u8 *data;
    int n;
    u64 ret;


    u64 pmc = (1<<22) | (1<<17) | (1<<16) | (0x41 << 8) | 0x2e;
    u64 zero = 0;
    u64 msr = 0x186;
    __asm__ __volatile__ ("wrmsr" :: "a"(pmc), "c"(msr), "d"(zero) );
    msr = 0x187;
    //pmc = (1<<22) | (0<<21) | (1<<17) | (1<<16) | (0x4f << 8) | 0x2e;
    pmc = (1<<22) | (0<<21) | (1<<17) | (1<<16) | (0x00 << 8) | 0x3c;
    __asm__ __volatile__ ("wrmsr" :: "a"(pmc), "c"(msr), "d"(zero) );

    /* FIXME */
    fib.n = 0;
    fib.sz = 4096;
    fib.ipaddr = kmalloc(sizeof(u64) * fib.sz);
    fib.macaddr = kmalloc(sizeof(u64) * fib.sz);

    /* FIXME: Choose the first interface for management */
    list = netdev_head;

    kprintf("MGMT: %s %x\r\n", list->netdev->name, list->netdev->recvpkt);
    while ( 1 ) {
        n = list->netdev->recvpkt(pkt, 512, list->netdev);
        if ( n >= 60 && 0x08 == pkt[12] && 0x00 == pkt[13] ) {
            //kprintf("XXXX\r\n");
            ip = pkt + 14;
            if ( 17 == ip[9] ) {
                udp = ip + (int)(ip[0] & 0xf) * 4;
                /* Check port 5000 */
                if ( udp[2] == 0x13 && udp[3] == 0x88 ) {
                    data = udp + 8;
                    ret = _mgmt_operate(data);
#if 0
                    kprintf("%d (%d) %x %x %x %x\r\n",
                            n, udp - pkt, udp[0], udp[1], udp[2], udp[3]);
#endif
                    kmemcpy(pkt2, pkt + 6, 6);
                    kmemcpy(pkt2 + 6, pkt, 6);
                    pkt2[12] = 0x08;
                    pkt2[13] = 0x00;
                    pkt2[14] = 0x45;
                    pkt2[15] = 0;
                    pkt2[16] = 0;
                    pkt2[17] = 20 + 8 + 8;
                    pkt2[18] = 0;
                    pkt2[19] = 0;
                    pkt2[20] = 0;
                    pkt2[21] = 0;
                    pkt2[22] = 64;
                    pkt2[23] = 17;
                    pkt2[24] = 0;
                    pkt2[25] = 0;
                    kmemcpy(pkt2 + 26, pkt + 30, 4);
                    kmemcpy(pkt2 + 30, pkt + 26, 4);
                    pkt2[34] = 0x13;
                    pkt2[35] = 0x88;
                    pkt2[36] = 0x13;
                    pkt2[37] = 0x88;
                    pkt2[38] = 0;
                    pkt2[39] = 8 + 8;
                    pkt2[40] = 0;
                    pkt2[41] = 0;
                    *(u64 *)(pkt2 + 42) = ret;
                    kmemset(pkt2 + 50, 0, 8);

                    /* Compute checksum */
                    u16 *tmp;
                    u32 cs;
                    int i;
                    pkt2[24] = 0x0;
                    pkt2[25] = 0x0;
                    tmp = (u16 *)pkt2;
                    cs = 0;
                    for ( i = 7; i < 17; i++ ) {
                        cs += (u32)tmp[i];
                        cs = (cs & 0xffff) + (cs >> 16);
                    }
                    cs = 0xffff - cs;
                    pkt2[24] = cs & 0xff;
                    pkt2[25] = cs >> 8;

                    //kprintf("ret = %x\r\n", ret);
                    list->netdev->sendpkt(pkt2, 60, list->netdev);

                }
            }
        }
    }

    //list->netdev;
    while ( 1 ) {
        //__asm__ __volatile__ ("hlt");
        arch_busy_usleep(100000);
    }

    return 0;
}
int
_routing_main(int argc, char *argv[])
{
    struct netdev_list *list;

    list = netdev_head;

    kprintf("Started routing: %s => %s\r\n", list->next->netdev->name,
            list->next->next->netdev->name);
    ixgbe_forwarding_test(list->next->netdev, list->next->next->netdev);

    return 0;
}

int
_tx_main(int argc, char *argv[])
{
    struct netdev_list *list;
    u8 *pkt;
    //int pktsz = 64 - 18;
    int i;
    int sz;
    int blk;
    char *s;

#if 0
    s = argv[1];
    sz = 0;
    while ( *s ) {
        sz *= 10;
        sz += *s - '0';
        s++;
    }
    s = argv[2];
    blk = 0;
    while ( *s ) {
        blk *= 10;
        blk += *s - '0';
        s++;
    }
#endif
    sz = 64;
    blk = 64;
    kprintf("Testing: %d/%d\r\n", sz, blk);

    int pktsz = sz - 18;

    pkt = kmalloc(9200);

    list = netdev_head;
    /* dst (multicast) */
#if 0
    pkt[0] = 0x01;
    pkt[1] = 0x00;
    pkt[2] = 0x5e;
    pkt[3] = 0x00;
    pkt[4] = 0x00;
    pkt[5] = 0x01;
#else
#if 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x6a;
    pkt[4] = 0x0c;
    pkt[5] = 0x40;
    //90:e2:ba:6a:0c:40
    //90:e2:ba:6a:0c:41
#else
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x68;
    pkt[4] = 0xb4;
    pkt[5] = 0xb4;
#endif
#endif

    /* src */
    pkt[6] = list->netdev->macaddr[0];
    pkt[7] = list->netdev->macaddr[1];
    pkt[8] = list->netdev->macaddr[2];
    pkt[9] = list->netdev->macaddr[3];
    pkt[10] = list->netdev->macaddr[4];
    pkt[11] = list->netdev->macaddr[5];

    /* type = IP (0800) */
    pkt[12] = 0x08;
    pkt[13] = 0x00;
    /* IP header */
    pkt[14] = 0x45;
    pkt[15] = 0x00;
    pkt[16] = (pktsz >> 8) & 0xff;
    pkt[17] = pktsz & 0xff;
    /* ID / fragment */
    pkt[18] = 0x26;
    pkt[19] = 0x6d;
    pkt[20] = 0x00;
    pkt[21] = 0x00;
    /* TTL/protocol */
    pkt[22] = 0x64;
    pkt[23] = 17;
    /* checksum */
    pkt[24] = 0x00;
    pkt[25] = 0x00;
    /* src: 192.168.56.2 */
    pkt[26] = 192;
    pkt[27] = 168;
    pkt[28] = 100;
    pkt[29] = 2;
    /* dst */
#if 0
    pkt[30] = 224;
    pkt[31] = 0;
    pkt[32] = 0;
    pkt[33] = 1;
#else
    pkt[30] = 10;
    pkt[31] = 0;
    pkt[32] = 0;
    pkt[33] = 100;
#endif

    /* UDP */
    pkt[34] = 0xff;
    pkt[35] = 0xff;
    pkt[36] = 0xff;
    pkt[37] = 0xfe;
    pkt[38] = (pktsz - 20) >> 8;
    pkt[39] = (pktsz - 20) & 0xff;
    pkt[40] = 0x00;
    pkt[41] = 0x00;
    for ( i = 42; i < pktsz + 14; i++ ) {
        pkt[i] = 0;
    }

    /* Compute checksum */
    u16 *tmp;
    u32 cs;
    pkt[24] = 0x0;
    pkt[25] = 0x0;
    tmp = (u16 *)pkt;
    cs = 0;
    for ( i = 7; i < 17; i++ ) {
        cs += (u32)tmp[i];
        cs = (cs & 0xffff) + (cs >> 16);
    }
    cs = 0xffff - cs;
    pkt[24] = cs & 0xff;
    pkt[25] = cs >> 8;

    ixgbe_tx_test(list->next->netdev, pkt, pktsz + 18 - 4, blk);
    //kprintf("%llx: pkt\r\n", list->netdev);
    //list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);

    return 0;
}

void lapic_send_fixed_ipi(u8 vector);
int this_cpu();
int
_builtin_start(char *const argv[])
{
    u8 id;
    struct ktask *t;

    id = 1;

    /* FIXME: This is experimental. This must be free but...  */
    t = ktask_alloc(TASK_POLICY_KERNEL);
    t->argv = NULL;
    t->id = -2;
    t->name = NULL;
    t->state = TASK_STATE_READY;

    /* Start command */
    if ( 0 == kstrcmp("mgmt", argv[1]) ) {
        /* Start management process */
#if 0
        struct netdev_list *list;
        list = netdev_head;
        kfree(t);
        u8 pkt[4096];
        list->netdev->recvpkt(pkt, 4096, list->netdev);
#endif
#if 1
        id = atoi(argv[2]);
        t->main = &_mgmt_main;
        arch_set_next_task_other_cpu(t, id);
        kprintf("Launch mgmt @ CPU #%d\r\n", id);
        lapic_send_ns_fixed_ipi(id, IV_IPI);
#endif
    } else if ( 0 == kstrcmp("tx", argv[1]) ) {
        /* Start Tx */
        id = atoi(argv[2]);
        t->main = &_tx_main;
        arch_set_next_task_other_cpu(t, id);
        kprintf("Launch routing @ CPU #%d\r\n", id);
        lapic_send_ns_fixed_ipi(id, IV_IPI);
    } else if ( 0 == kstrcmp("routing", argv[1]) ) {
        /* Start routing */
        id = atoi(argv[2]);
        t->main = &_routing_main;
        arch_set_next_task_other_cpu(t, id);
        kprintf("Launch routing @ CPU #%d\r\n", id);
        lapic_send_ns_fixed_ipi(id, IV_IPI);
    } else {
        ktask_free(t);
        kprintf("start <routing|mgmt> <id>\r\n");
    }

    return 0;
}
extern struct processor_table *processors;
int
_builtin_stop(char *const argv[])
{
    u8 id;

    id = atoi(argv[1]);
    if ( 0 != id ) {
        /* Stop command */
        processors->prs[processors->map[id]].idle->cred = 16;
        arch_set_next_task_other_cpu(processors->prs[processors->map[id]].idle, id);
        /* IPI */
        lapic_send_ns_fixed_ipi(id, IV_IPI);
    } else {
        kprintf("stop <id>\r\n");
    }
}

int
_builtin_help(char *const argv[])
{
    kprintf("Supported commands:\r\n");
    kprintf("    ?       Display help\r\n");
    kprintf("    show    Display information\r\n");
    kprintf("    uptime  Display time since boot in seconds\r\n");
    kprintf("    off     Power off\r\n");
    kprintf("    start   Start a daemon\r\n");
    kprintf("    stop    Stop a daemon\r\n");
}


/*
 * Parse command
 */
char **
_parse_cmd(const char *cmd)
{
    int i;
    int j;
    int k;
    int escaped;
    char buf[CMDBUF_SIZE];
    char **argv;
    int argc;
    char *arg;

    /* Allocate for the returned value */
    argv = kmalloc(sizeof(char *) * ARGS_MAX);
    if ( NULL == argv ) {
        return NULL;
    }

    /* Init */
    argc = 0;
    i = 0;
    j = 0;
    buf[0] = '\0';
    escaped = 0;
    while ( '\0' != cmd[i] ) {
        if ( escaped ) {
            buf[j++] = cmd[i];
            buf[j] = '\0';
            escaped = 0;
        } else {
            if ( ' ' == cmd[i] ) {
                if ( argc >= ARGS_MAX - 1 ) {
                    /* Overflow */
                    for ( k = 0; k < argc; k++ ) {
                        kfree(argv[k]);
                    }
                    kfree(argv);
                    return NULL;
                }
                /* Copy the buffer */
                arg = kstrdup(buf);
                if ( NULL == arg ) {
                    /* Memory error */
                    for ( k = 0; k < argc; k++ ) {
                        kfree(argv[k]);
                    }
                    kfree(argv);
                    return NULL;
                }
                argv[argc++] = arg;
                /* Reset the buffer */
                j = 0;
                buf[0] = '\0';
            } else if ( '\\' == cmd[i] ) {
                escaped = 1;
            } else {
                buf[j++] = cmd[i];
                buf[j] = '\0';
            }
        }
        i++;
    }
    if ( escaped ) {
        /* Append it to the tail */
        buf[j++] = '\\';
        buf[j] = '\0';
    }

    if ( j != 0 ) {
        /* Buffer is not empty, then copy the buffer */
        if ( argc >= ARGS_MAX - 1 ) {
            /* Overflow */
            for ( k = 0; k < argc; k++ ) {
                kfree(argv[k]);
            }
            kfree(argv);
            return NULL;
        }
        arg = kstrdup(buf);
        if ( NULL == arg ) {
            for ( k = 0; k < argc; k++ ) {
                kfree(argv[k]);
            }
        }
        argv[argc++] = arg;
    }

    /* Set NULL as a terminator */
    argv[argc] = NULL;

    return argv;
}


/*
 * Execute command
 */
static void
_exec_cmd(struct kshell *kshell)
{
    char **argv;
    char **tmp;

    /* Parse the command */
    argv = _parse_cmd(kshell->cmdbuf);
    if ( NULL == argv ) {
        kprintf("Error: Command could not be parsed.\r\n");
        return;
    }
    if ( NULL == argv[0] ) {
        kfree(argv);

        kprintf("> ");
        kshell->pos = 0;
        kshell->cmdbuf[0] = 0;
        return;
    }

    if ( 0 == kstrcmp("?", argv[0]) ) {
        _builtin_help(argv);
    } else if ( 0 == kstrcmp("upa", argv[0]) ) {
        _builtin_panic(argv);
    } else if ( 0 == kstrcmp("off", argv[0]) ) {
        _builtin_off(argv);
    } else if ( 0 == kstrcmp("uptime", argv[0]) ) {
        _builtin_uptime(argv);
    } else if ( 0 == kstrcmp("show", argv[0])
                || 0 == kstrcmp("sh", argv[0])
                || 0 == kstrcmp("sho", argv[0]) ) {
        _builtin_show(argv);
    } else if ( 0 == kstrcmp("start", argv[0]) ) {
        _builtin_start(argv);
    } else if ( 0 == kstrcmp("stop", argv[0]) ) {
        _builtin_stop(argv);
    } else if ( 0 == kstrcmp("test", argv[0]) ) {
        _builtin_test(argv);
    } else if ( 0 == kstrcmp("test2", argv[0]) ) {
        _builtin_test2(argv);
    } else {
        kprintf("%s: Command not found.\r\n", argv[0]);
    }

    /* Free */
    tmp = argv;
    while ( NULL != *tmp ) {
        kfree(*tmp);
        tmp++;
    }
    kfree(argv);

    kprintf("> ");
    kshell->pos = 0;
    kshell->cmdbuf[0] = 0;
}

/*
 * Shell process
 */
int
proc_shell(int argc, char *argv[])
{
    struct kshell kshell;
    volatile int c;
    volatile int ret;
    int fd;

    _init(&kshell);

    /* Open keyboard */
    fd = open("/dev/kbd", 0);
    if ( fd < 0 ) {
        kprintf("Cannot open keyboard\r\n");
        return -1;
    }

    for ( ;; ) {
        //ret = read(fd, &c, 1);
        ret = c = kbd_read();
        if ( ret > 0 ) {
            if ( c == 0x08 ) {
                /* Backspace */
                if ( kshell.pos > 0 ) {
                    arch_putc(c);
                    kshell.pos--;
                    kshell.cmdbuf[kshell.pos] = '\0';
                }
            } else if ( c == '\r' ) {
                /* Exec */
                kprintf("\r\n");
                _exec_cmd(&kshell);
            } else {
                if ( kshell.pos >= CMDBUF_SIZE - 1 ) {
                    kprintf("\r\nError: Command must not exceeds %d bytes.",
                            CMDBUF_SIZE - 1);
                    kprintf("\r\n> %s", kshell.cmdbuf);
                } else {
                    kshell.cmdbuf[kshell.pos++] = c;
                    kshell.cmdbuf[kshell.pos] = '\0';
                    arch_putc(c);
                }
            }
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
