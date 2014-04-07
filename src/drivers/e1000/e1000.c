/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../pci/pci.h"
#include "../../kernel/kernel.h"

void pause(void);

struct netdev_list *netdev_head;


#define E1000_REG_CTRL  0x00
#define E1000_REG_STATUS 0x08
#define E1000_REG_EEC   0x10
#define E1000_REG_CTRL_EXT 0x18
#define E1000_REG_EERD  0x14
#define E1000_REG_ICR   0x00c0
#define E1000_REG_IMS   0x00d0
#define E1000_REG_IMC   0x00d8
#define E1000_REG_RCTL  0x0100
#define E1000_REG_RDBAL 0x2800
#define E1000_REG_RDBAH 0x2804
#define E1000_REG_RDLEN 0x2808
#define E1000_REG_RDH   0x2810  /* head */
#define E1000_REG_RDT   0x2818  /* tail */
#define E1000_REG_TCTL  0x0400  /* transmit control */
#define E1000_REG_TDBAL 0x3800
#define E1000_REG_TDBAH 0x3804
#define E1000_REG_TDLEN 0x3808
#define E1000_REG_TDH   0x3810  /* head */
#define E1000_REG_TDT   0x3818  /* tail */
#define E1000_REG_MTA   0x5200  /* x128 */
#define E1000_REG_TXDCTL 0x03828
#define E1000_REG_RAL   0x5400
#define E1000_REG_RAH   0x5404

#define E1000_CTRL_FD   1       /* Full duplex */
#define E1000_CTRL_LRST (1<<3)  /* Link reset */
#define E1000_CTRL_ASDE (1<<5)  /* Auto speed detection enable */
#define E1000_CTRL_SLU  (1<<6)  /* Set linkup */
#define E1000_CTRL_PHY_RST      (1<<31)  /* PHY reset */

#define E1000_CTRL_RST  (1<<26)
#define E1000_CTRL_VME  (1<<30)

#define E1000_CTRL_EXT_LINK_MODE_MASK (u32)(3ULL<<22)

#define E1000_RCTL_EN   (1<<1)
#define E1000_RCTL_SBP  (1<<2)
#define E1000_RCTL_UPE  (1<<3)  /* Unicast promiscuous */
#define E1000_RCTL_MPE  (1<<4)  /* Multicast promiscuous */
#define E1000_RCTL_LPE  (1<<5)  /* Long packet reception */
#define E1000_RCTL_BAM  (1<<15) /* Broadcast accept mode */
#define E1000_RCTL_BSEX (1<<25) /* Buffer size extension */
#define E1000_RCTL_SECRC (1<<26) /* Strip ethernet CRC from incoming packet */

#define E1000_RCTL_BSIZE_8192 (2<<16) | E1000_RCTL_BSEX
#define E1000_RCTL_BSIZE_SHIFT 16


#define E1000_TCTL_EN   (1<<1)
#define E1000_TCTL_PSP  (1<<3)  /* pad short packets */
#define E1000_TCTL_MULR (1<<28)

#define E1000_TXDCTL_GRAN_CL 0
#define E1000_TXDCTL_GRAN_DESC (1<<24)
#define E1000_TXDCTL_PTHRESH_SHIFT 0
#define E1000_TXDCTL_HTHRESH_SHIFT 8
#define E1000_TXDCTL_WTHRESH_SHIFT 16
#define E1000_TXDCTL_LTHRESH_SHIFT 25

#define E1000_PRO1000MT         0x100e  /* Intel Pro 1000/MT */
#define E1000_82545EM           0x100f
#define E1000_82541PI           0x107c
#define E1000_82573L            0x109a
#define E1000_82567LM           0x10f5
#define E1000_82577LM           0x10ea
#define E1000_82579LM           0x1502

#define IXGBE_X520              0x10fb

#define IXGBE_REG_RAL0  0xa200 + 8 * 0
#define IXGBE_REG_RAH0  0xa204 + 8 * 0

struct e1000_tx_desc {
    u64 address;
    u16 length;
    u8 cso;
    u8 cmd;
    u8 sta;
    u8 css;
    u16 special;
} __attribute__ ((packed));

struct e1000_rx_desc {
    u64 address;
    u16 length;
    u16 checksum;
    u8 status;
    u8 errors;
    u16 special;
} __attribute__ ((packed));

struct e1000_device {
    u64 mmio;

    u64 rx_base;
    u32 rx_tail;
    u32 rx_bufsz;
    u64 tx_base;
    u32 tx_tail;
    u32 tx_bufsz;

    /* Cache */
    u32 rx_head_cache;
    u32 tx_head_cache;

    u8 macaddr[6];

    struct pci_device *pci_device;
};

/* Prototype declarations */
void e1000_update_hw(void);
struct e1000_device * e1000_init_hw(struct pci_device *);


int e1000_sendpkt(u8 *, u32, struct netdev *);
int e1000_recvpkt(u8 *, u32, struct netdev *);
int e1000_routing_test(struct netdev *);



u16
e1000_eeprom_read_8254x(u64 mmio, u8 addr)
{
    u32 data;

    /* Start */
    *(u32 *)(mmio + E1000_REG_EERD) = ((u32)addr << 8) | 1;

    /* Until it's done */
    while ( !((data = *(u32 *)(mmio + E1000_REG_EERD)) & (1<<4)) ) {
        pause();
    }

    return (u16)((data >> 16) & 0xffff);
}

u16
e1000_eeprom_read(u64 mmio, u8 addr)
{
    u16 data;
    u32 tmp;

    /* Start */
    *(u32 *)(mmio + E1000_REG_EERD) = ((u32)addr << 2) | 1;

    /* Until it's done */
    while ( !((tmp = *(u32 *)(mmio + E1000_REG_EERD)) & (1<<1)) ) {
        pause();
    }
    data = (u16)((tmp >> 16) & 0xffff);

    return data;
}

void
e1000_init(void)
{
    netdev_head = NULL;
    /* Initialize the driver */
    e1000_update_hw();
}

static int
netdev_add_device(const char *name, const u8 *macaddr, void *vendor)
{
    struct netdev_list **list;
    int i;

    list = &netdev_head;
    while ( NULL != *list ) {
        list = &(*list)->next;
    }
    *list = kmalloc(sizeof(struct netdev_list));
    if ( NULL == *list ) {
        return -1;
    }
    (*list)->next = NULL;
    (*list)->netdev = kmalloc(sizeof(struct netdev));
    if ( NULL == (*list)->netdev ) {
        kfree(*list);
        return -1;
    }
    for ( i = 0; i < NETDEV_MAX_NAME - 1 && 0 != name[i]; i++ ) {
        (*list)->netdev->name[i] = name[i];
    }
    (*list)->netdev->name[i] = 0;

    for ( i = 0; i < 6; i++ ) {
        (*list)->netdev->macaddr[i] = macaddr[i];
    }

    (*list)->netdev->vendor = vendor;
    (*list)->netdev->sendpkt = e1000_sendpkt;
    (*list)->netdev->recvpkt = e1000_recvpkt;
    (*list)->netdev->routing_test = e1000_routing_test;

    return 0;
}

void
e1000_update_hw(void)
{
    struct pci *pci;
    struct e1000_device *e1000dev;
    char name[NETDEV_MAX_NAME];
    int idx;

    idx = 0;
    pci = pci_list();
    while ( NULL != pci ) {
        if ( 0x8086 == pci->device->vendor_id ) {
            switch ( pci->device->device_id ) {
            case E1000_PRO1000MT:
            case E1000_82545EM:
            case E1000_82541PI:
            case E1000_82573L:
            case E1000_82567LM:
            case E1000_82577LM:
            case E1000_82579LM:
            case IXGBE_X520:
                e1000dev = e1000_init_hw(pci->device);
                name[0] = 'e';
                name[1] = '0' + idx;
                name[2] = '\0';
                netdev_add_device(name, e1000dev->macaddr, e1000dev);
                idx++;
                break;
            default:
                ;
            }
        }
        pci = pci->next;
    }
}

static __inline__ u32
mmio_read32(u64 base, u64 offset)
{
    return *(u32 *)(base + offset);
}

static __inline__ void
mmio_write32(u64 base, u64 offset, u32 value)
{
    *(u32 *)(base + offset) = value;
}

struct e1000_device *
e1000_init_hw(struct pci_device *pcidev)
{
    struct e1000_device *netdev;
    u16 m16;
    u32 m32;
    int i;

    netdev = kmalloc(sizeof(struct e1000_device));

    /* Assert */
    if ( 0x8086 != pcidev->vendor_id ) {
        kfree(netdev);
        return NULL;
    }

    /* Read MMIO */
    netdev->mmio = pci_read_mmio(pcidev->bus, pcidev->slot, pcidev->func);
    if ( 0 == netdev->mmio ) {
        kfree(netdev);
        return NULL;
    }

    /* Initialize */
    mmio_write32(netdev->mmio, E1000_REG_IMC, 0);
    mmio_write32(netdev->mmio, E1000_REG_CTRL,
                 mmio_read32(netdev->mmio, E1000_REG_CTRL)
                 | E1000_CTRL_RST);
    arch_busy_usleep(100);
    mmio_write32(netdev->mmio, E1000_REG_CTRL,
                 mmio_read32(netdev->mmio, E1000_REG_CTRL)
                 | E1000_CTRL_SLU);
    mmio_write32(netdev->mmio, E1000_REG_CTRL_EXT,
                 mmio_read32(netdev->mmio, E1000_REG_CTRL_EXT)
                 & ~E1000_CTRL_EXT_LINK_MODE_MASK);
#if 0
    mmio_write32(netdev->mmio, E1000_REG_TXDCTL,
                 E1000_TXDCTL_GRAN_DESC
                 | (128 << E1000_TXDCTL_HTHRESH_SHIFT)
                 | (8 << E1000_TXDCTL_PTHRESH_SHIFT));
#endif

    switch ( pcidev->device_id ) {
    case E1000_PRO1000MT:
    case E1000_82545EM:
        /* Read MAC address */
        m16 = e1000_eeprom_read_8254x(netdev->mmio, 0);
        netdev->macaddr[0] = m16 & 0xff;
        netdev->macaddr[1] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read_8254x(netdev->mmio, 1);
        netdev->macaddr[2] = m16 & 0xff;
        netdev->macaddr[3] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read_8254x(netdev->mmio, 2);
        netdev->macaddr[4] = m16 & 0xff;
        netdev->macaddr[5] = (m16 >> 8) & 0xff;
        break;

    case E1000_82541PI:
    case E1000_82573L:
        /* Read MAC address */
        m16 = e1000_eeprom_read(netdev->mmio, 0);
        netdev->macaddr[0] = m16 & 0xff;
        netdev->macaddr[1] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read(netdev->mmio, 1);
        netdev->macaddr[2] = m16 & 0xff;
        netdev->macaddr[3] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read(netdev->mmio, 2);
        netdev->macaddr[4] = m16 & 0xff;
        netdev->macaddr[5] = (m16 >> 8) & 0xff;
        break;

    case E1000_82567LM:
    case E1000_82577LM:
    case E1000_82579LM:
        /* Read MAC address */
        m32 = mmio_read32(netdev->mmio, E1000_REG_RAL);
        netdev->macaddr[0] = m32 & 0xff;
        netdev->macaddr[1] = (m32 >> 8) & 0xff;
        netdev->macaddr[2] = (m32 >> 16) & 0xff;
        netdev->macaddr[3] = (m32 >> 24) & 0xff;
        m32 = mmio_read32(netdev->mmio, E1000_REG_RAH);
        netdev->macaddr[4] = m32 & 0xff;
        netdev->macaddr[5] = (m32 >> 8) & 0xff;
        break;
    case IXGBE_X520:
        /* Read MAC address */
        m32 = mmio_read32(netdev->mmio, IXGBE_REG_RAL0);
        netdev->macaddr[0] = m32 & 0xff;
        netdev->macaddr[1] = (m32 >> 8) & 0xff;
        netdev->macaddr[2] = (m32 >> 16) & 0xff;
        netdev->macaddr[3] = (m32 >> 24) & 0xff;
        m32 = mmio_read32(netdev->mmio, IXGBE_REG_RAH0);
        netdev->macaddr[4] = m32 & 0xff;
        netdev->macaddr[5] = (m32 >> 8) & 0xff;
        break;
    }

    /* Link up */
    mmio_write32(netdev->mmio, E1000_REG_CTRL,
                 mmio_read32(netdev->mmio, E1000_REG_CTRL)
                 | E1000_CTRL_SLU | E1000_CTRL_VME);

    /* Multicast array table */
    for ( i = 0; i < 128; i++ ) {
        mmio_write32(netdev->mmio, E1000_REG_MTA + i * 4, 0);
    }

    /* Enable interrupt (REG_IMS <- 0x1F6DC, then read REG_ICR ) */
    //mmio_write32(netdev->mmio, E1000_REG_IMS, 0x1f6dc);
    //mmio_read32(netdev->mmio, 0xc0);

    /* Start TX/RX */
    struct e1000_rx_desc *rxdesc;
    struct e1000_tx_desc *txdesc;

    netdev->rx_tail = 0;
    netdev->tx_tail = 0;
    netdev->rx_bufsz = 768;
    netdev->tx_bufsz = 768;

    /* Cache */
    netdev->rx_head_cache = 0;
    netdev->tx_head_cache = 0;

    /* ToDo: 16 bytes for alignment */
    netdev->rx_base = (u64)kmalloc(netdev->rx_bufsz
                                   * sizeof(struct e1000_rx_desc) + 16);
    if ( 0 == netdev->rx_base ) {
        kfree(netdev);
        return NULL;
    }
    for ( i = 0; i < netdev->rx_bufsz; i++ ) {
        rxdesc = (struct e1000_rx_desc *)(netdev->rx_base
                                          + i * sizeof(struct e1000_rx_desc));
        rxdesc->address = (u64)kmalloc(8192 + 16);
        /* FIXME: Memory check */
        rxdesc->checksum = 0;
        rxdesc->status = 0;
        rxdesc->errors = 0;
        rxdesc->special = 0;
    }
    mmio_write32(netdev->mmio, E1000_REG_RDBAH, netdev->rx_base >> 32);
    mmio_write32(netdev->mmio, E1000_REG_RDBAL, netdev->rx_base & 0xffffffff);
    mmio_write32(netdev->mmio, E1000_REG_RDLEN,
                 netdev->rx_bufsz * sizeof(struct e1000_rx_desc));
    mmio_write32(netdev->mmio, E1000_REG_RDH, 0);
    /* RDT must be larger than 0 for the initial value to receive the first
       packet but I don't know why */
    mmio_write32(netdev->mmio, E1000_REG_RDT, netdev->rx_bufsz - 1);
    mmio_write32(netdev->mmio, E1000_REG_RCTL,
                 E1000_RCTL_SBP | E1000_RCTL_UPE
                 | E1000_RCTL_MPE | E1000_RCTL_LPE | E1000_RCTL_BAM
                 | E1000_RCTL_BSIZE_8192 | E1000_RCTL_SECRC);
    /* Enable */
    mmio_write32(netdev->mmio, E1000_REG_RCTL,
                 mmio_read32(netdev->mmio, E1000_REG_RCTL) | E1000_RCTL_EN);

    /* ToDo: 16 bytes for alignment */
    netdev->tx_base = (u64)kmalloc(netdev->tx_bufsz
                                   * sizeof(struct e1000_tx_desc) + 16);
    for ( i = 0; i < netdev->tx_bufsz; i++ ) {
        txdesc = (struct e1000_tx_desc *)(netdev->tx_base
                                          + i * sizeof(struct e1000_tx_desc));
        txdesc->address = (u64)kmalloc(8192 + 16);
        txdesc->cmd = 0;
        txdesc->sta = 0;
        txdesc->cso = 0;
        txdesc->css = 0;
        txdesc->special = 0;
    }
    mmio_write32(netdev->mmio, E1000_REG_TDBAH, netdev->tx_base >> 32);
    mmio_write32(netdev->mmio, E1000_REG_TDBAL, netdev->tx_base & 0xffffffff);
    mmio_write32(netdev->mmio, E1000_REG_TDLEN,
                 netdev->tx_bufsz * sizeof(struct e1000_tx_desc));
    mmio_write32(netdev->mmio, E1000_REG_TDH, 0);
    mmio_write32(netdev->mmio, E1000_REG_TDT, 0);
    mmio_write32(netdev->mmio, E1000_REG_TCTL,
                 E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_MULR);

    netdev->pci_device = pcidev;

    return netdev;
}


int
e1000_sendpkt(u8 *pkt, u32 len, struct netdev *netdev)
{
    struct e1000_device *e1000dev;
    struct e1000_tx_desc *txdesc;
    u32 tdh;
    u32 next_tail;

    e1000dev = (struct e1000_device *)netdev->vendor;
    tdh = mmio_read32(e1000dev->mmio, E1000_REG_TDH);

    next_tail = (e1000dev->tx_tail + 1) % e1000dev->tx_bufsz;
#if 0
    if ( tdh == next_tail ) {
    }
#endif
    if ( e1000dev->tx_bufsz -
         ((e1000dev->tx_bufsz - tdh + e1000dev->tx_tail) % e1000dev->tx_bufsz)
         <= 1 ) {
        /* Full */
        return -1;
    }

    int i;
    int j;
    for ( i = 0; i < 1; i++ ) {
        txdesc = (struct e1000_tx_desc *)(e1000dev->tx_base + (e1000dev->tx_tail + i)
                                          * sizeof(struct e1000_tx_desc));
        for ( j = 0; j < len; j++ ) {
            ((u8 *)(txdesc->address))[j] = pkt[j];
        }
        txdesc->length = len;
        txdesc->sta = 0;
        txdesc->special = 0;
        txdesc->css = 0;
        txdesc->cso = 0;
        txdesc->cmd = (1<<3) | (1<<1) | 1;
    }

    e1000dev->tx_tail = next_tail;
    mmio_write32(e1000dev->mmio, E1000_REG_TDT, e1000dev->tx_tail);

    return 0;
}
int
e1000_recvpkt(u8 *pkt, u32 len, struct netdev *netdev)
{
    struct e1000_device *e1000dev;
    struct e1000_rx_desc *rxdesc;
    u32 rdh;

    e1000dev = (struct e1000_device *)netdev->vendor;
    rdh = mmio_read32(e1000dev->mmio, E1000_REG_RDH);
    if ( rdh == e1000dev->rx_tail ) {
        return 0;
    }

    rxdesc = (struct e1000_rx_desc *)(e1000dev->rx_base + e1000dev->rx_tail
                                      * sizeof(struct e1000_rx_desc));

    e1000dev->rx_tail = (e1000dev->rx_tail + 1) % e1000dev->rx_bufsz;
    mmio_write32(e1000dev->mmio, E1000_REG_RDT, e1000dev->rx_tail);

    return rxdesc->length;
}

int
e1000_routing_test(struct netdev *netdev)
{
    struct e1000_device *e1000dev;
    struct e1000_rx_desc *rxdesc;
    struct e1000_tx_desc *txdesc;
    u32 rdh;
    u32 tdh;
    int rx_que;
    int tx_avl;
    int nrp;
    int i;
    int j;
    u8 *rxpkt;
    u8 *txpkt;

    e1000dev = (struct e1000_device *)netdev->vendor;
    for ( ;; ) {
        rdh = mmio_read32(e1000dev->mmio, E1000_REG_RDH);
        tdh = mmio_read32(e1000dev->mmio, E1000_REG_TDH);

        rx_que = (e1000dev->rx_bufsz - e1000dev->rx_tail + rdh)
            % e1000dev->rx_bufsz;

        tx_avl = e1000dev->tx_bufsz
            - ((e1000dev->tx_bufsz - tdh + e1000dev->tx_tail)
               % e1000dev->tx_bufsz);

        if ( rx_que > tx_avl ) {
            nrp = tx_avl;
        } else {
            nrp = rx_que;
        }
        /* Routing */
        for ( i = 0; i < nrp; i++ ) {
            rxdesc = (struct e1000_rx_desc *)
                (e1000dev->rx_base
                 + ((e1000dev->rx_tail + i) % e1000dev->rx_bufsz)
                 * sizeof(struct e1000_rx_desc));
            txdesc = (struct e1000_tx_desc *)
                (e1000dev->tx_base
                 + ((e1000dev->tx_tail + i) % e1000dev->tx_bufsz)
                 * sizeof(struct e1000_tx_desc));

            rxpkt = (u8 *)rxdesc->address;
            txpkt = (u8 *)txdesc->address;

            /* Check */
#if 0
            kprintf("XX: %x %x %d.%d.%d.%d\r\n", rxdesc->special,
                    rxdesc->length, rxpkt[30], rxpkt[31], rxpkt[32], rxpkt[33]);
#endif
            if ( rxdesc->special != 100
                 || rxpkt[30] != 192 || rxpkt[31] != 168
                 || rxpkt[32] != 200 || rxpkt[33] != 2 ) {
                txdesc->length = 0;
                txdesc->sta = 0;
                txdesc->special = 0;
                txdesc->css = 0;
                txdesc->cso = 0;
                txdesc->cmd = 0;
                continue;
            }

            /* dst (MBP) */
            txpkt[0] = 0x40;
            txpkt[1] = 0x6c;
            txpkt[2] = 0x8f;
            txpkt[3] = 0x5b;
            txpkt[4] = 0x8d;
            txpkt[5] = 0x02;
            /* src (thinkpad T410) */
            txpkt[6] = 0xf0;
            txpkt[7] = 0xde;
            txpkt[8] = 0xf1;
            txpkt[9] = 0x37;
            txpkt[10] = 0xea;
            txpkt[11] = 0xf6;
            for ( j = 12; j < rxdesc->length; j++ ) {
                txpkt[j] = rxpkt[j];
            }
            txdesc->length = rxdesc->length;
            txdesc->sta = 0;
            txdesc->css = 0;
            txdesc->cso = 0;
            txdesc->special = 200;
            txdesc->cmd = (1<<3) | (1<<1) | 1 | (1<<6);
        }

        e1000dev->rx_tail = (e1000dev->rx_tail + nrp) % e1000dev->rx_bufsz;
        e1000dev->tx_tail = (e1000dev->tx_tail + nrp) % e1000dev->tx_bufsz;
        mmio_write32(e1000dev->mmio, E1000_REG_RDT, e1000dev->rx_tail);
        mmio_write32(e1000dev->mmio, E1000_REG_TDT, e1000dev->tx_tail);
    }

    return 0;
}


typedef int (*router_rx_cb_t)(const u8 *, u32, int);
int arch_dbg_printf(const char *fmt, ...);


/*
 * Get a valid TX buffer
 */
int
e1000_tx_buf(struct netdev *netdev, u8 **txpkt, u16 **txlen, u16 vlan)
{
    struct e1000_device *e1000dev;
    struct e1000_tx_desc *txdesc;
    int tx_avl;

    /* Retrieve data structure of e1000 driver */
    e1000dev = (struct e1000_device *)netdev->vendor;

    /* Get available TX buffer length */
    tx_avl = e1000dev->tx_bufsz
        - ((e1000dev->tx_bufsz - e1000dev->tx_head_cache + e1000dev->tx_tail)
           % e1000dev->tx_bufsz);
    if ( tx_avl <= 0 ) {
        return -1;
    }

    /* Check the head of TX ring buffer */
    txdesc = (struct e1000_tx_desc *)
        (e1000dev->tx_base
         + (e1000dev->tx_tail % e1000dev->tx_bufsz)
         * sizeof(struct e1000_tx_desc));

    *txpkt = (u8 *)txdesc->address;
    *txlen = &(txdesc->length);

    txdesc->sta = 0;
    txdesc->css = 0;
    txdesc->cso = 0;
    txdesc->special = vlan;
    if ( vlan == 0 ) {
        txdesc->cmd = (1<<3) | (1<<1) | 1;
    } else {
        txdesc->cmd = (1<<3) | (1<<1) | 1 | (1<<6);
    }

    /* Update the tail pointer of the TX buffer */
    e1000dev->tx_tail = (e1000dev->tx_tail + 1) % e1000dev->tx_bufsz;

    return 0;
}
int
e1000_tx_set(struct netdev *netdev, u64 txpkt, u16 txlen, u16 vlan)
{
    struct e1000_device *e1000dev;
    struct e1000_tx_desc *txdesc;
    int tx_avl;
    int ret;

    /* Retrieve data structure of e1000 driver */
    e1000dev = (struct e1000_device *)netdev->vendor;

    /* Get available TX buffer length */
    tx_avl = e1000dev->tx_bufsz
        - ((e1000dev->tx_bufsz - e1000dev->tx_head_cache + e1000dev->tx_tail)
           % e1000dev->tx_bufsz);
    if ( tx_avl <= 0 ) {
        return -1;
    }

    /* Check the head of TX ring buffer */
    ret = e1000dev->tx_tail;
    txdesc = (struct e1000_tx_desc *)
        (e1000dev->tx_base
         + (e1000dev->tx_tail % e1000dev->tx_bufsz)
         * sizeof(struct e1000_tx_desc));

    kmemcpy((u8 *)txdesc->address, (u8 *)txpkt, txlen);
    txdesc->length = txlen;
    txdesc->sta = 0;
    txdesc->css = 0;
    txdesc->cso = 0;
    txdesc->special = vlan;
    if ( vlan == 0 ) {
        txdesc->cmd = (1<<3) | (1<<1) | 1;
    } else {
        txdesc->cmd = (1<<3) | (1<<1) | 1 | (1<<6);
    }

    /* Update the tail pointer of the TX buffer */
    e1000dev->tx_tail = (e1000dev->tx_tail + 1) % e1000dev->tx_bufsz;

    return ret;
}
int
e1000_tx_commit(struct netdev *netdev)
{
    struct e1000_device *e1000dev;

    /* Retrieve data structure of e1000 driver */
    e1000dev = (struct e1000_device *)netdev->vendor;

    /* Write to PCI */
    mmio_write32(e1000dev->mmio, E1000_REG_TDT, e1000dev->tx_tail);

    u32 tdh;
    tdh = mmio_read32(e1000dev->mmio, E1000_REG_TDH);
    e1000dev->tx_head_cache = tdh;

    return 0;
}


/*
 * Routing process (RX poll)
 */
int
e1000_routing(struct netdev *netdev, router_rx_cb_t cb)
{
    struct e1000_device *e1000dev;
    struct e1000_rx_desc *rxdesc;
    u32 rdh;
    u32 tdh;
    int rx_que;
    int i;
    u8 *rxpkt;

    /* Retrieve data structure of e1000 driver */
    e1000dev = (struct e1000_device *)netdev->vendor;

#if 0
    kprintf("[%x : %x]\r\n",
            mmio_read32(e1000dev->mmio, E1000_REG_RDH),
            mmio_read32(e1000dev->mmio, E1000_REG_RDT));
#endif

    for ( ;; ) {
        /* Get the head pointers of RX/TX ring buffers */
        rdh = mmio_read32(e1000dev->mmio, E1000_REG_RDH);
        tdh = mmio_read32(e1000dev->mmio, E1000_REG_TDH);

        /* Update the cache */
        e1000dev->rx_head_cache = rdh;
        e1000dev->tx_head_cache = tdh;

        /* Get RX queue length */
        rx_que = (e1000dev->rx_bufsz - e1000dev->rx_tail + rdh)
            % e1000dev->rx_bufsz;

#if 0
        rxdesc = (struct e1000_rx_desc *)
            (e1000dev->rx_base + e1000dev->rx_tail * sizeof(struct e1000_rx_desc));
        kprintf("[%d : %d / %d /// %x | %d]\r\n", rdh, e1000dev->rx_tail, rx_que,
                rxdesc->status, mmio_read32(e1000dev->mmio, E1000_REG_RDH));
#endif

#if 0
        /* Device status */
        kprintf("[%x : %x]\r\n",
                mmio_read32(e1000dev->mmio, E1000_REG_RDH),
                mmio_read32(e1000dev->mmio, E1000_REG_RDT));
#endif

        if ( rx_que <= 0 ) {
            continue;
        }
#if 0
        kprintf("[%x : %x]\r\n",
                mmio_read32(e1000dev->mmio, E1000_REG_RDH),
                mmio_read32(e1000dev->mmio, E1000_REG_RDT));
#endif

        /* Routing */
        for ( i = 0; i < rx_que; i++ ) {
            rxdesc = (struct e1000_rx_desc *)
                (e1000dev->rx_base
                 + ((e1000dev->rx_tail + i) % e1000dev->rx_bufsz)
                 * sizeof(struct e1000_rx_desc));

#if 0
            kprintf("[%x %x %x]\r\n",
                    rxdesc->checksum, rxdesc->status, rxdesc->errors);
#endif
            rxpkt = (u8 *)rxdesc->address;

            if ( rxdesc->status & (1<<3) ) {
                /* VLAN packet (VP) */
                cb(rxpkt, rxdesc->length, rxdesc->special & 0xfff);
            } else {
                cb(rxpkt, rxdesc->length, 0);
            }

            rxdesc->status = 0;
        }

        /* Sync RX */
        e1000dev->rx_tail = (e1000dev->rx_tail + rx_que - 1) % e1000dev->rx_bufsz;
        mmio_write32(e1000dev->mmio, E1000_REG_RDT, e1000dev->rx_tail);
        e1000dev->rx_tail = (e1000dev->rx_tail + 1) % e1000dev->rx_bufsz;

        /* Commit TX queue */
        e1000_tx_commit(netdev);

#if 0
        kprintf("C0 : %x %x %x \r\n",
                mmio_read32(e1000dev->mmio, 0xc0),
                mmio_read32(e1000dev->mmio, 0xc0),
                mmio_read32(e1000dev->mmio, 0xc0));
#endif
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
