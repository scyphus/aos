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
volatile static u64 globaldata;

extern struct ktask_table *ktasks;
extern struct ktltask_table *ktltasks;

extern struct net gnet;

/*
 * Temporary: Keyboard drivers
 */
ssize_t read(int, void *, size_t);

struct kshell {
    char cmdbuf[CMDBUF_SIZE];
    int pos;
};

struct kcmd {
    char **cmds;
};


struct cmd {
    char *s;
    char *desc;
    int (*f)(char *const []);
};

struct cmd cmds[] = {
    { .s = "show", .desc = "Show information", .f = NULL },
    { .s = "set", .desc = "Set a configuration", .f = NULL },
    { .s = "unset", .desc = "Set a configuration", .f = NULL },
    { .s = "request", .desc = "Request a command", .f = NULL },
};

/*
  0800277a98ea 192.168.56.11
  0800275af6dc 192.168.56.12
 */
const char *config = "\
e0 {hw 08:00:27:7a:98:ea; ip 192.168.56.11/24;}  \
e1 {hw 08:00:27:5a:f6:dc; ip 192.168.56.12/24;}  \
";





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
 * Reset
 */
int
_builtin_reset(char *const argv[])
{
    u8 c;

    arch_disable_interrupts();
    do {
        c = inb(0x64);
        if ( 0 != (c & 1) ) {
            inb(0x60);
        }
    } while ( 0 != (c & 2) );

    /* CPU reset */
    outb(0x64, 0xfe);

    arch_enable_interrupts();

    return 0;
}

/*
 * uptime
 */
int
_builtin_uptime(char *const argv[])
{
    u64 clk;

    clk = arch_clock_get();
    kprintf("Uptime: %llu.%.9llu sec\r\n", clk / 1000000000, clk % 1000000000);

    return 0;
}

/*
 * show
 */
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
            kprintf("   hwaddr: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n",
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
            kprintf("%.2x.%.2x.%.2x %.4x:%.4x\r\n", list->device->bus,
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
    } else if ( 0 == kstrcmp("processes", argv[1]) ) {
        int i;
        const char *sstr;
        for ( i = 0; i < TASK_TABLE_SIZE; i++ ) {
            if ( NULL != ktasks->tasks[i].ktask ) {
                switch ( ktasks->tasks[i].ktask->state ) {
                case TASK_STATE_READY:
                    sstr = "READY";
                    break;
                case TASK_STATE_RUNNING:
                    sstr = "RUNNING";
                    break;
                case TASK_STATE_BLOCKED:
                    sstr = "BLOCKED";
                    break;
                default:
                    sstr = "Unknown";
                    break;
                }
                kprintf("[%.4d] %s %s\r\n", ktasks->tasks[i].ktask->id,
                        ktasks->tasks[i].ktask->name, sstr);
            }
        }
        for ( i = 0; i < TL_TASK_TABLE_SIZE; i++ ) {
            if ( NULL != ktltasks->tasks[i] ) {
                switch ( ktltasks->tasks[i]->state ) {
                case TASK_STATE_READY:
                    sstr = "READY";
                    break;
                case TASK_STATE_RUNNING:
                    sstr = "RUNNING";
                    break;
                case TASK_STATE_BLOCKED:
                    sstr = "BLOCKED";
                    break;
                default:
                    sstr = "Unknown";
                    break;
                }
                kprintf("[*%.3d] %s %s\r\n", ktltasks->tasks[i]->id,
                        ktltasks->tasks[i]->name, sstr);
            }
        }
    } else if ( 0 == kstrcmp("clock", argv[1]) ) {
        u64 ts;
        int s;
        int h;
        int m;
        int x;
        int b;
        int c;
        int d;
        int e;
        int f;
        int yy;
        int mm;
        int dd;

        ts = arch_time();

        s = ts % 86400;
        ts /= 86400;
        h = s / 3600;
        m = (s / 60) % 60;
        s = s % 60;
        x = (ts * 4 + 102032) / 146097 + 15;
        b = ts + 2442113 + x - (x / 4);
        c = (b * 20 - 2442) / 7305;
        d = b - 365 * c - c / 4;
        e = d * 1000 / 30601;
        f = d - e * 30 - e * 601 / 1000;

        if ( e < 14 ) {
            yy = c - 4716;
            mm = e - 1;
            dd = f;
        } else {
            yy = c - 4715;
            mm = e - 13;
            dd = f;
        }
        kprintf("%04d-%02d-%02d %02d:%02d:%02d\r\n", yy, mm, dd, h, m, s);
    } else {
        kprintf("show <interfaces|pci|processors|processes|clock>\r\n");
    }

    return 0;
}

/*
 * request
 */
int
_builtin_request(char *const argv[])
{
    if ( 0 == kstrcmp("system", argv[1]) ) {
        if ( 0 == kstrcmp("power-off", argv[2]) ) {
            arch_poweroff();
        } else if ( 0 == kstrcmp("reset", argv[2]) ) {
            _builtin_reset(argv);
        } else {
            kprintf("request system <power-off|reset>\r\n");
        }
    } else {
        kprintf("request <system>\r\n");
    }

    return 0;
}

/*
 * test packet
 */
int ixgbe_tx_test(struct netdev *, u8 *, int, int);
int ixgbe_tx_test2(struct netdev *, u8 *, int, int);
int ixgbe_tx_test3(struct netdev *, u8 *, int, int);
int ixgbe_tx_test4(struct netdev *, struct netdev *, struct netdev *,
                   struct netdev *, u8 *, int, int);
int i40e_tx_test(struct netdev *, u8 *, int, int);
int i40e_tx_test2(struct netdev *, u8 *, int, int, int, int);
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
#elsif 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x68;
    pkt[4] = 0xb4;
    pkt[5] = 0xb4;
#else
    pkt[0] = 0x68;
    pkt[1] = 0x05;
    pkt[2] = 0xca;
    pkt[3] = 0x2d;
    pkt[4] = 0x44;
    pkt[5] = 0xb8;
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
    //i40e_tx_test(list->next->netdev, pkt, pktsz + 18 - 4, blk);
    //kprintf("%llx: pkt\r\n", list->netdev);
    //list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);

    return 0;
}

int ixgbe_forwarding_test(struct netdev *, struct netdev *);
int ixgbe_forwarding_test_sub(struct netdev *, struct netdev *);
int ixgbe_routing_test(struct netdev *);
int i40e_forwarding_test(struct netdev *, struct netdev *);
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
        kprintf("Commit start %x\r\n", globaldata);
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

#if 0
        u64 tmp;
        t0 = rdtsc();
        tmp = ptcam_lookup(tcam, ipaddr);
        t1 = rdtsc();
#endif

        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
        t0 = (dd<<32) | aa;
        globaldata = ptcam_lookup(tcam, ipaddr);
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
    } else if ( data[0] == 7 ) {
        /* Lookup */
        ipaddr = ((((u64)data[1]) << 56) | (((u64)data[2]) << 48)
                  | (((u64)data[3]) << 40) | (((u64)data[4]) << 32));
        u64 tmp;
        t0 = rdtsc();
        globaldata = ptcam_lookup(tcam, ipaddr);
        t1 = rdtsc();
        ret = t1 - t0;

#if DXR
    } else if ( data[0] == 10 ) {
        /* Commit */
        kprintf("Commit start\r\n");
        t0 = rdtsc();
        dxr_commit(dxr);
        t1 = rdtsc();
        kprintf("Commit done. %x\r\n", t1 - t0);
        ret = t1 - t0;
    } else if ( data[0] == 11 ) {
        u64 a1;
        u64 a2;
        u64 a3;
        a1 = ((((u64)data[1]) << 24) | (((u64)data[2]) << 16)
                  | (((u64)data[3]) << 8) | (((u64)data[4]) << 0));
        a2 = ((((u64)data[5]) << 24) | (((u64)data[6]) << 16)
                  | (((u64)data[7]) << 8) | (((u64)data[8]) << 0));
        a3 = ((((u64)data[9]) << 24) | (((u64)data[10]) << 16)
                  | (((u64)data[11]) << 8) | (((u64)data[12]) << 0));
        t0 = rdtsc();
        dxr_add_range(dxr, a1, a2, a3);
        t1 = rdtsc();
        ret = t1 - t0;
    } else if ( data[0] == 12 ) {
        /* Lookup */
        ipaddr = ((((u64)data[1]) << 24) | (((u64)data[2]) << 16)
                  | (((u64)data[3]) << 8) | (((u64)data[4]) << 0));

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

        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
        t0 = (dd<<32) | aa;
        globaldata = dxr_lookup(dxr, ipaddr);
        __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
        t1 = (dd<<32) | aa;
        ret = t1 - t0;
    } else if ( data[0] == 12 ) {
        /* Lookup */
        ipaddr = ((((u64)data[1]) << 24) | (((u64)data[2]) << 16)
                  | (((u64)data[3]) << 8) | (((u64)data[4]) << 0));
        u64 tmp;
        t0 = rdtsc();
        globaldata = dxr_lookup(dxr, ipaddr);
        t1 = rdtsc();
        ret = t1 - t0;
#endif
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
static int
_routing_main(int argc, char *argv[])
{
    struct netdev_list *list;

    list = netdev_head;

    kprintf("Started routing: %s => %s\r\n", list->next->netdev->name,
            list->next->next->netdev->name);
    //ixgbe_forwarding_test(list->next->netdev, list->next->next->netdev);
    i40e_forwarding_test(list->next->netdev, list->next->next->netdev);

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

#if 1
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
#else
    sz = 64;
    blk = 64;
#endif
    kprintf("Testing: %d/%d\r\n", sz, blk);

    int pktsz = sz - 18;

    pkt = kmalloc(9200);

    list = netdev_head->next;
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
#elif 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x68;
    pkt[4] = 0xb4;
    pkt[5] = 0xb4;
#elif 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x6a;
    pkt[4] = 0x00;
    pkt[5] = 0xdc;
#elif 1
    pkt[0] = 0x00;
    pkt[1] = 0x40;
    pkt[2] = 0x66;
    pkt[3] = 0x67;
    pkt[4] = 0x72;
    pkt[5] = 0x24;
#else
    pkt[0] = 0x68;
    pkt[1] = 0x05;
    pkt[2] = 0xca;
    pkt[3] = 0x2d;
    pkt[4] = 0x46;
    pkt[5] = 0xe8;
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
    /* src: 192.168.100.2 */
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
    pkt[30] = 192;
    pkt[31] = 168;
    pkt[32] = 200;
    pkt[33] = 1;
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
    kmemset(pkt + 42, 0, pktsz + 14 - 42);
    //for ( i = 42; i < pktsz + 14; i++ ) {
    //    pkt[i] = 0;
    //}

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

    //ixgbe_tx_test(list->netdev, pkt, pktsz + 18 - 4, blk);
    //ixgbe_tx_test2(list->netdev, pkt, pktsz + 18 - 4, blk);
#if 0
    ixgbe_tx_test4(list->netdev, list->next->netdev, list->next->next->netdev,
                   list->next->next->next->netdev,
                   pkt, pktsz + 18 - 4, blk);
#else
    i40e_tx_test2(list->netdev, pkt, pktsz + 18 - 4, blk, 0, 1);
#endif
    //kprintf("%llx: pkt\r\n", list->netdev);
    //list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);

    return 0;
}
int
_tx2_main(int argc, char *argv[])
{
    struct netdev_list *list;
    u8 *pkt;
    //int pktsz = 64 - 18;
    int i;
    int sz;
    int blk;
    char *s;

#if 1
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
#else
    sz = 64;
    blk = 64;
#endif
    kprintf("Testing: %d/%d\r\n", sz, blk);

    int pktsz = sz - 18;

    pkt = kmalloc(9200);

    list = netdev_head->next;
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
#elif 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x68;
    pkt[4] = 0xb4;
    pkt[5] = 0xb4;
#elif 0
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x6a;
    pkt[4] = 0x00;
    pkt[5] = 0xdc;
#elif 0
    pkt[0] = 0x00;
    pkt[1] = 0x40;
    pkt[2] = 0x66;
    pkt[3] = 0x67;
    pkt[4] = 0x72;
    pkt[5] = 0x24;
#else
    pkt[0] = 0x68;
    pkt[1] = 0x05;
    pkt[2] = 0xca;
    pkt[3] = 0x2d;
    pkt[4] = 0x46;
    pkt[5] = 0xe8;
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
    /* src: 192.168.100.2 */
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
    kmemset(pkt + 42, 0, pktsz + 14 - 42);
    //for ( i = 42; i < pktsz + 14; i++ ) {
    //    pkt[i] = 0;
    //}

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

    //ixgbe_tx_test(list->netdev, pkt, pktsz + 18 - 4, blk);
    //ixgbe_tx_test2(list->netdev, pkt, pktsz + 18 - 4, blk);
#if 0
    ixgbe_tx_test4(list->netdev, list->next->netdev, list->next->next->netdev,
                   list->next->next->next->netdev,
                   pkt, pktsz + 18 - 4, blk);
#else
    i40e_tx_test2(list->netdev, pkt, pktsz + 18 - 4, blk, 2, 4);
#endif
    //kprintf("%llx: pkt\r\n", list->netdev);
    //list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);

    return 0;
}
int net_rx(struct net *, struct net_port *, u8 *, int, int);
int net_sc_rx_ether(struct net *, u8 *, int, void *);
int net_sc_rx_port_host(struct net *, u8 *, int, void *);
int net_rib4_add(struct net_rib4 *, const u32, int, u32);
u32 bswap32(u32);


int mgmt_main(int, char *[]);

/*
 * Start a process
 */
int
_builtin_start(char *const argv[])
{
    u8 id;
    int ret;

    /* Processor ID */
    id = atoi(argv[2]);

    /* Start command */
    if ( 0 == kstrcmp("mgmt", argv[1]) ) {
        /* Start management process */
        ret = ktask_fork_execv(TASK_POLICY_KERNEL, &mgmt_main, &argv[2]);
        if ( ret < 0 ) {
            kprintf("Cannot launch mgmt\r\n");
            return -1;
        }
        kprintf("Launch mgmt\r\n");
    } else if ( 0 == kstrcmp("tx", argv[1]) ) {
        /* Start Tx */
        char **argv = kmalloc(sizeof(char *) * 4);
        argv[0] = "tx";
        argv[1] = argv[3] ? kstrdup(argv[3]) : NULL;
        argv[2] = argv[4] ? kstrdup(argv[4]) : NULL;
        argv[3] = NULL;
        ret = ktltask_fork_execv(TASK_POLICY_KERNEL, id, &_tx_main, argv);
        if ( ret < 0 ) {
            kprintf("Cannot launch tx\r\n");
            return -1;
        }
        kprintf("Launch tx @ CPU #%d\r\n", id);
    } else if ( 0 == kstrcmp("tx2", argv[1]) ) {
        /* Start Tx */
        char **argv = kmalloc(sizeof(char *) * 4);
        argv[0] = "tx";
        argv[1] = argv[3] ? kstrdup(argv[3]) : NULL;
        argv[2] = argv[4] ? kstrdup(argv[4]) : NULL;
        argv[3] = NULL;
        ret = ktltask_fork_execv(TASK_POLICY_KERNEL, id, &_tx2_main, argv);
        if ( ret < 0 ) {
            kprintf("Cannot launch tx2\r\n");
            return -1;
        }
        kprintf("Launch tx2 @ CPU #%d\r\n", id);
    } else if ( 0 == kstrcmp("routing", argv[1]) ) {
        /* Start routing */
        ret = ktltask_fork_execv(TASK_POLICY_KERNEL, id, &_routing_main, NULL);
        if ( ret < 0 ) {
            kprintf("Cannot launch routing\r\n");
            return -1;
        }
        kprintf("Launch routing @ CPU #%d\r\n", id);
    } else {
        kprintf("start <routing|mgmt> <id>\r\n");
        return -1;
    }

    return 0;
}

/*
 * Stop a process
 */
int
_builtin_stop(char *const argv[])
{
    u8 id;

    id = atoi(argv[1]);
    if ( 0 != id ) {
        /* Stop command */
        processor_get(id)->idle->cred = 16;
        arch_set_next_task_other_cpu(processor_get(id)->idle, id);
        /* IPI */
        lapic_send_ns_fixed_ipi(id, IV_IPI);
    } else {
        kprintf("stop <id>\r\n");
    }

    return 0;
}

/*
 * Display help
 */
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
    kprintf("    request Request a command\r\n");

    return 0;
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
 * Execute a command
 */
static void
_exec_cmdbuf(char *cmd)
{
    char **argv;
    char **tmp;
    int ret;

    /* Parse the command */
    argv = _parse_cmd(cmd);
    if ( NULL == argv ) {
        kprintf("Error: Command could not be parsed.\r\n");
        return;
    }
    if ( NULL == argv[0] ) {
        kfree(argv);
        return;
    }

    if ( 0 == kstrcmp("?", argv[0]) ) {
        ret = _builtin_help(argv);
    } else if ( 0 == kstrcmp("upa", argv[0]) ) {
        ret = _builtin_panic(argv);
    } else if ( 0 == kstrcmp("off", argv[0]) ) {
        ret =_builtin_off(argv);
    } else if ( 0 == kstrcmp("uptime", argv[0]) ) {
        ret = _builtin_uptime(argv);
    } else if ( 0 == kstrcmp("show", argv[0])
                || 0 == kstrcmp("sh", argv[0])
                || 0 == kstrcmp("sho", argv[0]) ) {
        ret =_builtin_show(argv);
    } else if ( 0 == kstrcmp("request", argv[0]) ) {
        ret = _builtin_request(argv);
    } else if ( 0 == kstrcmp("start", argv[0]) ) {
        ret =_builtin_start(argv);
    } else if ( 0 == kstrcmp("stop", argv[0]) ) {
        ret = _builtin_stop(argv);
    } else if ( 0 == kstrcmp("test", argv[0]) ) {
        ret = _builtin_test(argv);
    } else if ( 0 == kstrcmp("test2", argv[0]) ) {
        ret = _builtin_test2(argv);
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
}

/*
 * Execute command
 */
static void
_exec_cmd(struct kshell *kshell)
{
    _exec_cmdbuf(kshell->cmdbuf);

    kprintf("> ");
    kshell->pos = 0;
    kshell->cmdbuf[0] = 0;
}


/*
 * Shell process
 */
int
shell_main(int argc, char *argv[])
{
    struct kshell kshell;
    volatile int c;
    volatile int ret;
    int fd;

    /* Print the logo */
#if 0
    kprintf("\r\n"
            "                               iiii                           \r\n"
            "                              ii  ii                          \r\n"
            "                               iiii                           \r\n"
            "            ppppp                                             \r\n"
            "    ppp  ppppppppppp           i  i       xxxx          xxxx  \r\n"
            "    pppppppp     ppppp         iiii        xxxx        xxxx   \r\n"
            "    pppppp         ppppp       iiii         xxxxx    xxxxx    \r\n"
            "    pppp            ppppp      iiii          xxxxx  xxxx      \r\n"
            "    ppp              pppp      iiii            xxxxxxxx       \r\n"
            "    ppp               ppp      iiii              xxxxx        \r\n"
            "    ppp              pppp      iiii            xxxxxxxx       \r\n"
            "    ppp              pppp      iiii           xxxx  xxxx      \r\n"
            "    ppppp           ppppp      iiii         xxxxx    xxxxx    \r\n"
            "    ppppppp       ppppp        iiii        xxxx        xxxx   \r\n"
            "    pppppppppppppppppp         iiii       xxxx          xxxx  \r\n"
            "    ppp    pppppppp             ii                            \r\n"
            "    ppp                                                       \r\n"
            "    ppp                                                       \r\n"
            "    ppp                                                       \r\n"
            "    ppp                                                       \r\n"
            "    pp                                                        \r\n"
            "\r\n");
#endif

    _init(&kshell);

    /* Open keyboard */
    fd = open("/dev/kbd", 0);
    if ( fd < 0 ) {
        kprintf("Cannot open keyboard\r\n");
        return -1;
    }

    /* Start-up script */
    _exec_cmdbuf("start mgmt 0 e0 192.168.56.11/24 192.168.56.1");

    for ( ;; ) {
        c = 0;
        ret = read(fd, &c, 1);
        if ( ret > 0 ) {
            if ( c == 0x08 ) {
                /* Backspace */
                if ( kshell.pos > 0 ) {
                    arch_putc(c);
                    kshell.pos--;
                    kshell.cmdbuf[kshell.pos] = '\0';
                }
            } else if ( c == 0x83 ) {
                /* Left */
                if ( kshell.pos > 0 ) {
                    /* FIXME */
                    arch_putc(0x08);
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







int
shell_tcp_recv(struct tcp_session *sess, const u8 *pkt, u32 len)
{
    u8 *buf = alloca(len + 1);
    int i;
    int j;

    for ( i = 0, j = 0; i < len; i++ ) {
        if ( 0xff == pkt[i] ) {
            i += 2;
        } else if ( '\r' == pkt[i] || '\n' == pkt[i] ) {
            /* skip */
        } else {
            buf[j++] = pkt[i];
        }
    }
    buf[j++] = 0;

    _exec_cmdbuf(buf);
    sess->send(sess, "pix> ", 5);

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
