/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "kernel.h"

extern struct netdev_list *netdev_head;

/*
 * Temporary: Keyboard drivers
 */
void kbd_init(void);
void kbd_event(void);
int kbd_read(void);

/*
 * Shell process
 */
void
proc_shell(void)
{
    char cmdbuf[256];
    int pos;
    int c;

    pos = 0;
    cmdbuf[0] = 0;
    kprintf("> ");

    for ( ;; ) {
        c = kbd_read();
        if ( c > 0 ) {
            if ( c == 0x08 ) {
                /* Backspace */
                if ( pos > 0 ) {
                    arch_putc(c);
                    pos--;
                    cmdbuf[pos] = '\0';
                }
            } else if ( c == '\r' ) {
                /* Exec */
                kprintf("\r\n");
                if ( 0 == kstrcmp("upa", cmdbuf) ) {
                    panic("Executing user PANIC!!");
                } else if ( 0 == kstrcmp("off", cmdbuf) ) {
                    arch_poweroff();
                } else if ( 0 == kstrcmp("show interfaces", cmdbuf) ) {
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
                } else if ( 0 == kstrcmp("start spt", cmdbuf) ) {
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
                } else if ( 0 == kstrcmp("start routing", cmdbuf) ) {
                    //__asm__ __volatile__ ("sti;");
                    struct netdev_list *list;
                    list = netdev_head;
                    list->netdev->routing_test(list->netdev);
                } else if ( 0 == kstrcmp("uptime", cmdbuf) ) {
                    u64 x = arch_clock_get();
                    kprintf("uptime: %llu.%.9llu sec\r\n",
                            x / 1000000000, x % 1000000000);
                } else if ( 0 == kstrcmp("", cmdbuf) ) {
                    /* Nothing to do */
                } else if ( 0 == kstrcmp("echo ", cmdbuf) ) {
                    kprintf("\r\n");
                } else if ( 0 == kstrncmp("echo ", cmdbuf, 5) ) {
                    kprintf("%s\r\n", cmdbuf+5);
                } else {
                    kprintf("%s: Command not found.\r\n", cmdbuf);
                }
                kprintf("> ");
                pos = 0;
                cmdbuf[0] = 0;
            } else {
                if ( pos > 254 ) {
                    kprintf("\r\nError: Command must not exceeds 255 bytes.");
                    kprintf("\r\n> ");
                    pos = 0;
                    cmdbuf[0] = 0;
                }
                cmdbuf[pos++] = c;
                cmdbuf[pos] = '\0';
                arch_putc(c);
            }
        } else {
            __asm__ __volatile__ ("sti;hlt;cli");
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
