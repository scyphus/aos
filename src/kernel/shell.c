/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
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
void kbd_init(void);
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
    if ( 0 == kstrcmp("interfaces", argv[1]) ) {
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
            if ( pdata->flags ) {
                kprintf("Processor #%d is active.\r\n", i);
            }
        }

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
    pkt[0] = 0x90;
    pkt[1] = 0xe2;
    pkt[2] = 0xba;
    pkt[3] = 0x6a;
    pkt[4] = 0x0c;
    pkt[5] = 0x40;
    //90:e2:ba:6a:0c:40
    //90:e2:ba:6a:0c:41
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

    ixgbe_tx_test(list->netdev, pkt, pktsz + 18 - 4, blk);
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

    if ( 0 == kstrcmp("upa", argv[0]) ) {
        _builtin_panic(argv);
    } else if ( 0 == kstrcmp("off", argv[0]) ) {
        _builtin_off(argv);
    } else if ( 0 == kstrcmp("uptime", argv[0]) ) {
        _builtin_uptime(argv);
    } else if ( 0 == kstrcmp("show", argv[0]) ) {
        _builtin_show(argv);
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
proc_shell(int argc, const char *const argv[])
{
    struct kshell kshell;
    volatile int c;

    _init(&kshell);

    for ( ;; ) {
        c = kbd_read();
        if ( c > 0 ) {
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
        } else {
            hlt1();
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
