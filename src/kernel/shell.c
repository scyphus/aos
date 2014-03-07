/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "kernel.h"

#define CMDBUF_SIZE 4096
#define ARGS_MAX 128

extern struct netdev_list *netdev_head;

/*
 * Temporary: Keyboard drivers
 */
void kbd_init(void);
void kbd_event(void);
int kbd_read(void);
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
int
_builtin_show(char *const argv[])
{
    struct netdev_list *list;

    if ( 0 == kstrcmp("interfaces", argv[1]) ) {
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
    }

    return 0;
}

/*
 * test packet
 */
int
_builtin_test(char *const argv[])
{
    struct netdev_list *list;
    u8 *pkt;
    int pktsz = 64 - 18;
    int i;

    pkt = kmalloc(1534);

    list = netdev_head;
    while ( list ) {
        /* dst (multicast) */
#if 0
        pkt[0] = 0x01;
        pkt[1] = 0x00;
        pkt[2] = 0x5e;
        pkt[3] = 0x00;
        pkt[4] = 0x00;
        pkt[5] = 0x01;
#else
        pkt[0] = 0x0a;
        pkt[1] = 0x00;
        pkt[2] = 0x27;
        pkt[3] = 0x00;
        pkt[4] = 0x00;
        pkt[5] = 0x00;
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
        pkt[22] = 0x01;
        pkt[23] = 17;
        /* checksum */
        pkt[24] = 0x00;
        pkt[25] = 0x00;
        /* src: 192.168.56.2 */
        pkt[26] = 192;
        pkt[27] = 168;
        pkt[28] = 56;
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
        pkt[32] = 56;
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

        kprintf("%llx: pkt\r\n", list->netdev);
        list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);

        list = list->next;
    }

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
 * show
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
    return;

#if 0
    if ( 0 == kstrcmp("upa", kshell->cmdbuf) ) {
        _builtin_panic();
    } else if ( 0 == kstrcmp("off", kshell->cmdbuf) ) {
        _builtin_off();
    } else if ( 0 == kstrcmp("show interfaces", kshell->cmdbuf) ) {
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
    } else if ( 0 == kstrcmp("start spt", kshell->cmdbuf) ) {
        struct netdev_list *list;

        u8 *pkt;
        int pktsz = 64 - 18;
        pkt = kmalloc(1534);
#define MCT 0
#if MCT
        /* dst (multicast) */
        pkt[0] = 0x01;
        pkt[1] = 0x00;
        pkt[2] = 0x5e;
        pkt[3] = 0x00;
        pkt[4] = 0x00;
        pkt[5] = 0x01;
#else
        /* dst (thinkpad T410) */
        pkt[0] = 0xf0;
        pkt[1] = 0xde;
        pkt[2] = 0xf1;
        pkt[3] = 0x37;
        pkt[4] = 0xea;
        pkt[5] = 0xf6;
#endif
#if 0
        /* src (thinkpad T410) */
        pkt[6] = 0xf0;
        pkt[7] = 0xde;
        pkt[8] = 0xf1;
        pkt[9] = 0x37;
        pkt[10] = 0xea;
        pkt[11] = 0xf6;
#else
        /* src (thinkpad X201i) */
        pkt[6] = 0xf0;
        pkt[7] = 0xde;
        pkt[8] = 0xf1;
        pkt[9] = 0x42;
        pkt[10] = 0x8f;
        pkt[11] = 0x9c;
#endif
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
#if MCT
        pkt[22] = 0x01;
        //pkt[23] = 0x01;
        pkt[23] = 17;
#else
        pkt[22] = 0x64;
        pkt[23] = 17;
#endif
        /* checksum */
        pkt[24] = 0x00;
        pkt[25] = 0x00;
        /* src */
        pkt[26] = 192;
        pkt[27] = 168;
        pkt[28] = 100;
        pkt[29] = 2;
        /* dst */
#if MCT
        pkt[30] = 224;
        pkt[31] = 0;
        pkt[32] = 0;
        pkt[33] = 1;
#else
        pkt[30] = 192;
        pkt[31] = 168;
        pkt[32] = 200;
        pkt[33] = 2;
#endif
        int i;
#if 0
        /* icmp */
        pkt[34] = 0x08;
        pkt[35] = 0x00;
        /* icmp checksum */
        pkt[36] = 0x00;
        pkt[37] = 0x00;
        for ( i = 38; i < 64 + 34; i++ ) {
            pkt[i] = 0;
        }
#else
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
#endif
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

        for ( ;; ) {
            list = netdev_head;
            while ( list ) {
                //list->netdev->sendpkt(pkt, 1518-4, list->netdev);
                list->netdev->sendpkt(pkt, pktsz + 18 - 4, list->netdev);
#if 0
                ret = list->netdev->recvpkt(&rpkt, &pktlen, list->netdev);
                if ( ret ) {
                    list->netdev->sendpkt(pkt, 64-4, list->netdev);
                }
#endif
                list = list->next;
            }
            //__asm__ __volatile__ ("hlt;");
        }
    } else if ( 0 == kstrcmp("start routing", kshell->cmdbuf) ) {
        //__asm__ __volatile__ ("sti;");
        struct netdev_list *list;
        list = netdev_head;
        list->netdev->routing_test(list->netdev);
    } else if ( 0 == kstrcmp("uptime", kshell->cmdbuf) ) {
        u64 x = arch_clock_get();
        kprintf("uptime: %llu.%.9llu sec\r\n",
                x / 1000000000, x % 1000000000);
    } else if ( 0 == kstrcmp("", kshell->cmdbuf) ) {
        /* Nothing to do */
    } else if ( 0 == kstrcmp("echo ", kshell->cmdbuf) ) {
        kprintf("\r\n");
    } else if ( 0 == kstrncmp("echo ", kshell->cmdbuf, 5) ) {
        kprintf("%s\r\n", kshell->cmdbuf + 5);
    } else {
        kprintf("%s: Command not found.\r\n", kshell->cmdbuf);
    }
#endif
    kprintf("> ");
    kshell->pos = 0;
    kshell->cmdbuf[0] = 0;
}

/*
 * Shell process
 */
void
proc_shell(void)
{
    struct kshell kshell;
    int c;

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
                /* FIXME: This prevents hanging up but I don't know the cause */
                arch_busy_usleep(1);
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

    return;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
