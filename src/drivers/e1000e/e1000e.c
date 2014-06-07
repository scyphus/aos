/*_
 * Copyright 2009 xisa group. All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@xisa.org>
 */

/* $Id$ */

#include <aos/const.h>
#include "../pci/pci.h"
#include "../net/netdev.h"
#include "../../kernel/kernel.h"

#define PCI_VENDOR_INTEL        0x8086

#define E1000E_I217V            0x153b
#define E1000E_I218V            0x1559

#define E1000E_REG_CTRL         0x00000
#define E1000E_REG_STATUS       0x00008
#define E1000E_REG_CTRL_EXT     0x00018
#define E1000E_REG_IMC          0x000d8

#define E1000E_REG_RCTL         0x00100
#define E1000E_REG_RCTL1        0x00104
#define E1000E_REG_RDBAL(n)     (0x02800 + (n) * 0x100)
#define E1000E_REG_RDBAH(n)     (0x02804 + (n) * 0x100)
#define E1000E_REG_RDLEN(n)     (0x02808 + (n) * 0x100)
#define E1000E_REG_RDH(n)       (0x02810 + (n) * 0x100)
#define E1000E_REG_RDT(n)       (0x02818 + (n) * 0x100)
#define E1000E_REG_RXDCTL(n)    (0x02828 + (n) * 0x100)

#define E1000E_REG_TDBAL(n)     (0x03800 + (n) * 0x100)
#define E1000E_REG_TDBAH(n)     (0x03804 + (n) * 0x100)
#define E1000E_REG_TDLEN(n)     (0x03808 + (n) * 0x100)
#define E1000E_REG_TDH(n)       (0x03810 + (n) * 0x100)
#define E1000E_REG_TDT(n)       (0x03818 + (n) * 0x100)
#define E1000E_REG_TXDCTL(n)    (0x03828 + (n) * 0x100)
#define E1000E_REG_TCTL         0x00400

#define E1000E_REG_RFCTL        0x05008
#define E1000E_REG_MTA(n)       (0x05200 + (n) * 4)
#define E1000E_REG_RAL          0x05400
#define E1000E_REG_RAH          0x05404


#define E1000E_CTRL_FD          1       /* Full duplex */
#define E1000E_CTRL_PCIE_MASTER_DISABLE (u32)(1<<2)
#define E1000E_CTRL_SWRST       (1<<26) /* Host software reset */
#define E1000E_CTRL_VME         (1<<30) /* VLAN mode enable */

#define E1000E_RCTL_EN          (1<<1)
#define E1000E_RCTL_SBP         (1<<2)
#define E1000E_RCTL_UPE         (1<<3)
#define E1000E_RCTL_MPE         (1<<4)
#define E1000E_RCTL_LPE         (1<<5)
#define E1000E_RCTL_BAM         (1<<15)
#define E1000E_RCTL_SECRC       (1<<26)
#define E1000E_RCTL_BSIZE_8192  ((2<<16) | (1<<25))


#define E1000E_TCTL_EN          (1<<1)
#define E1000E_TCTL_PSP         (1<<3)

struct e1000e_tx_desc {
    u64 address;
    u16 length;
    u8 cso;
    u8 cmd;
    u8 sta;
    u8 css;
    u16 special;
} __attribute__ ((packed));

struct e1000e_rx_desc {
    u64 address;
    u16 length;
    u16 checksum;
    u8 status;
    u8 errors;
    u16 special;
} __attribute__ ((packed));

struct e1000e_device {
    u64 mmio;

    struct e1000e_rx_desc *rx_desc;
    u32 rx_tail;
    u32 rx_bufsz;
    struct e1000e_tx_desc *tx_desc;
    u32 tx_tail;
    u32 tx_bufsz;

    struct e1000e_rx_desc *rx1_desc;
    u32 rx1_tail;
    u32 rx1_bufsz;

    /* Cache */
    u32 rx_head_cache;
    u32 tx_head_cache;

    u8 macaddr[6];

    struct pci_device *pci_device;
};


void e1000e_update_hw(void);
struct e1000e_device * e1000e_init_hw(struct pci_device *);
int e1000e_setup_rx_desc(struct e1000e_device *);
int e1000e_setup_tx_desc(struct e1000e_device *);
int e1000e_recvpkt(u8 *, u32, struct netdev *);

static __inline__ volatile u32
mmio_read32(u64 base, u64 offset)
{
    return *(volatile u32 *)(base + offset);
}

static __inline__ void
mmio_write32(u64 base, u64 offset, volatile u32 value)
{
    *(volatile u32 *)(base + offset) = value;
}

/*
 * Initialize e1000e driver
 */
void
e1000e_init(void)
{
    /* Initialize the driver */
    e1000e_update_hw();
}

/*
 * Update the list of initialized interfaces
 */
void
e1000e_update_hw(void)
{
    struct pci *pci;
    struct e1000e_device *dev;
    struct netdev *netdev;
    int idx;

    idx = 0;
    pci = pci_list();
    while ( NULL != pci ) {
        if ( PCI_VENDOR_INTEL == pci->device->vendor_id ) {
            switch ( pci->device->device_id ) {
            case E1000E_I217V:
            case E1000E_I218V:
                dev = e1000e_init_hw(pci->device);
                netdev = netdev_add_device(dev->macaddr, dev);
                netdev->recvpkt = e1000e_recvpkt;
                idx++;
                break;
            default:
                ;
            }
        }
        pci = pci->next;
    }
}

/*
 * Initialize an interface
 */
struct e1000e_device *
e1000e_init_hw(struct pci_device *pcidev)
{
    struct e1000e_device *dev;
    int i;
    u32 m32;
    int ret;

    /* Assert */
    if ( PCI_VENDOR_INTEL != pcidev->vendor_id ) {
        return NULL;
    }

    /* Allocate the device structure */
    dev = kmalloc(sizeof(struct e1000e_device));

    /* Read MMIO address (BAR0) */
    dev->mmio = pci_read_mmio(pcidev->bus, pcidev->slot, pcidev->func);
    if ( 0 == dev->mmio ) {
        kfree(dev);
        return NULL;
    }

    /* Reset PCIe master */
    mmio_write32(dev->mmio, E1000E_REG_CTRL,
                 mmio_read32(dev->mmio, E1000E_REG_CTRL)
                 | E1000E_CTRL_PCIE_MASTER_DISABLE);
    arch_busy_usleep(50);
    mmio_write32(dev->mmio, E1000E_REG_CTRL,
                 mmio_read32(dev->mmio, E1000E_REG_CTRL)
                 & (~E1000E_CTRL_PCIE_MASTER_DISABLE));
    arch_busy_usleep(50);

    /* Read MAC address */
    switch ( pcidev->device_id ) {
    case E1000E_I217V:
    case E1000E_I218V:
        /* Read MAC address */
        m32 = mmio_read32(dev->mmio, E1000E_REG_RAL);
        dev->macaddr[0] = m32 & 0xff;
        dev->macaddr[1] = (m32 >> 8) & 0xff;
        dev->macaddr[2] = (m32 >> 16) & 0xff;
        dev->macaddr[3] = (m32 >> 24) & 0xff;
        m32 = mmio_read32(dev->mmio, E1000E_REG_RAH);
        dev->macaddr[4] = m32 & 0xff;
        dev->macaddr[5] = (m32 >> 8) & 0xff;
        break;
    default:
        kfree(dev);
        return NULL;
    }

    /* Initialize */
    mmio_write32(dev->mmio, E1000E_REG_IMC, 0xffffffff);
    mmio_write32(dev->mmio, E1000E_REG_CTRL,
                 mmio_read32(dev->mmio, E1000E_REG_CTRL) | E1000E_CTRL_SWRST);
    for ( i = 0; i < 10; i++ ) {
        arch_busy_usleep(1);
        m32 = mmio_read32(dev->mmio, E1000E_REG_CTRL);
        if ( !(m32 & E1000E_CTRL_SWRST) ) {
            break;
        }
    }
    if ( m32 & E1000E_CTRL_SWRST ) {
        kprintf("Error on reset an interface\r\n");
        kfree(dev);
        return NULL;
    }

#if 0
    mmio_write32(dev->mmio, E1000E_REG_CTRL,
                 mmio_read32(dev->mmio, E1000E_REG_CTRL)
                 | E1000E_CTRL_VME | E1000E_CTRL_FD | (1<<11) | (1<<12) | (2<<8));
#endif

    /* Setup multicast table array */
    for ( i = 0; i < 32; i++ ) {
        mmio_write32(dev->mmio, E1000E_REG_MTA(i), 0);
    }

    /* Setup discriptors */
    ret = e1000e_setup_rx_desc(dev);
    if ( ret < 0 ) {
        kfree(dev);
        return NULL;
    }

    ret = e1000e_setup_tx_desc(dev);
    if ( ret < 0 ) {
        kfree(dev);
        return NULL;
    }

    mmio_write32(dev->mmio, E1000E_REG_TCTL, E1000E_TCTL_EN | E1000E_TCTL_PSP);

#if 1
    mmio_write32(dev->mmio, E1000E_REG_CTRL_EXT,
                 mmio_read32(dev->mmio, E1000E_REG_CTRL_EXT)
                 | (1<<28));
#endif

    return dev;
}

/*
 * Setup RX descriptor
 */
int
e1000e_setup_rx_desc(struct e1000e_device *dev)
{
    struct e1000e_rx_desc *rxdesc;
    int i;

    dev->rx_tail = 0;
    dev->rx_bufsz = 512;

    /* Allocate memory for RX descriptors */
    dev->rx_desc = kmalloc(dev->rx_bufsz * sizeof(struct e1000e_rx_desc));
    if ( NULL == dev->rx_desc ) {
        return -1;
    }

    for ( i = 0; i < dev->rx_bufsz; i++ ) {
        rxdesc = &(dev->rx_desc[i]);
        rxdesc->address = (u64)kmalloc(8192);
        rxdesc->checksum = 0;
        rxdesc->status = 0;
        rxdesc->errors = 0;
        rxdesc->special = 0;
    }

    mmio_write32(dev->mmio, E1000E_REG_RDBAH(0), ((u64)dev->rx_desc) >> 32);
    mmio_write32(dev->mmio, E1000E_REG_RDBAL(0),
                 ((u64)dev->rx_desc) & 0xffffffff);
    mmio_write32(dev->mmio, E1000E_REG_RDLEN(0),
                 dev->rx_bufsz * sizeof(struct e1000e_rx_desc));
    mmio_write32(dev->mmio, E1000E_REG_RDH(0), 0);
    mmio_write32(dev->mmio, E1000E_REG_RDT(0), dev->rx_bufsz - 1);


#if 0
    dev->rx1_tail = 0;
    dev->rx1_bufsz = 512;
    /* Allocate memory for RX descriptors */
    dev->rx1_desc = kmalloc(dev->rx1_bufsz * sizeof(struct e1000e_rx_desc));
    if ( NULL == dev->rx1_desc ) {
        return -1;
    }

    for ( i = 0; i < dev->rx1_bufsz; i++ ) {
        rxdesc = &(dev->rx1_desc[i]);
        rxdesc->address = (u64)kmalloc(8192);
        rxdesc->checksum = 0;
        rxdesc->status = 0;
        rxdesc->errors = 0;
        rxdesc->special = 0;
    }
    mmio_write32(dev->mmio, E1000E_REG_RDBAH(1), ((u64)dev->rx1_desc) >> 32);
    mmio_write32(dev->mmio, E1000E_REG_RDBAL(1),
                 ((u64)dev->rx1_desc) & 0xffffffff);
    mmio_write32(dev->mmio, E1000E_REG_RDLEN(1),
                 dev->rx1_bufsz * sizeof(struct e1000e_rx_desc));
    mmio_write32(dev->mmio, E1000E_REG_RDH(1), 0);
    mmio_write32(dev->mmio, E1000E_REG_RDT(1), dev->rx1_bufsz - 1);
    mmio_write32(dev->mmio, E1000E_REG_RCTL1, E1000E_RCTL_BSIZE_8192);
#endif


#if 0
    mmio_write32(dev->mmio, E1000E_REG_RDH(0), 0);
    mmio_write32(dev->mmio, E1000E_REG_RDT(0), 0x100);
#endif

    mmio_write32(dev->mmio, 0x18,
                 mmio_read32(dev->mmio, E1000E_REG_RFCTL) | (1<<15));

#if 0
    mmio_write32(dev->mmio, E1000E_REG_RFCTL,
                 mmio_read32(dev->mmio, E1000E_REG_RFCTL) | (1<<15));
#endif

    /* Enable */
    mmio_write32(dev->mmio, E1000E_REG_RCTL,
                 mmio_read32(dev->mmio, E1000E_REG_RCTL)
                 | E1000E_RCTL_SBP | E1000E_RCTL_UPE
                 | E1000E_RCTL_MPE | E1000E_RCTL_LPE | E1000E_RCTL_BAM
                 | E1000E_RCTL_BSIZE_8192 | E1000E_RCTL_SECRC | E1000E_RCTL_EN);


#if 0
    arch_busy_usleep(500);
    mmio_write32(dev->mmio, E1000E_REG_RCTL,
                 mmio_read32(dev->mmio, E1000E_REG_RCTL) & (~E1000E_RCTL_EN));
    arch_busy_usleep(500);
#endif
#if 0
    mmio_write32(dev->mmio, E1000E_REG_RCTL,
                 mmio_read32(dev->mmio, E1000E_REG_RCTL) | E1000E_RCTL_EN);
#endif

#if 0
    int addr;
    for ( addr = 0x4080; addr <= 0x40c0; addr += 4 ) {
        kprintf("  %.8x : %.8x\r\n", addr, mmio_read32(dev->mmio, addr));
    }
#endif
#if 0
    kprintf("  %.8x : %.8x\r\n", 0x4074, mmio_read32(dev->mmio, 0x4074));
    kprintf("  %.8x : %.8x\r\n", 0x4088, mmio_read32(dev->mmio, 0x4088));
    kprintf("  %.8x : %.8x\r\n", 0x408c, mmio_read32(dev->mmio, 0x408c));
#endif


    //mmio_write32(dev->mmio, E1000E_REG_RXDCTL(0), 0x01000000 | (4<<16) | (4<<8) | 0x20);
    //mmio_write32(dev->mmio, E1000E_REG_RXDCTL(0), 0);

    return 0;
}

/*
 * Setup TX descriptor
 */
int
e1000e_setup_tx_desc(struct e1000e_device *dev)
{
    struct e1000e_tx_desc *txdesc;
    int i;

    dev->tx_tail = 0;
    dev->tx_bufsz = 512;

    /* Allocate memory for TX descriptors */
    dev->tx_desc = kmalloc(dev->tx_bufsz * sizeof(struct e1000e_tx_desc));
    if ( NULL == dev->tx_desc ) {
        return -1;
    }

    for ( i = 0; i < dev->tx_bufsz; i++ ) {
        txdesc = &(dev->tx_desc[i]);
        txdesc->address = 0;
        txdesc->cmd = 0;
        txdesc->sta = 0;
        txdesc->cso = 0;
        txdesc->special = 0;
    }

    mmio_write32(dev->mmio, E1000E_REG_TDBAH(0), ((u64)dev->tx_desc) >> 32);
    mmio_write32(dev->mmio, E1000E_REG_TDBAL(0),
                 ((u64)dev->tx_desc) & 0xffffffff);
    mmio_write32(dev->mmio, E1000E_REG_TDLEN(0),
                 dev->tx_bufsz * sizeof(struct e1000e_tx_desc));
    mmio_write32(dev->mmio, E1000E_REG_TDH(0), 0);
    mmio_write32(dev->mmio, E1000E_REG_TDT(0), 0);

    mmio_write32(dev->mmio, E1000E_REG_TXDCTL(0), 0);

    return 0;
}

int
e1000e_recvpkt(u8 *pkt, u32 len, struct netdev *netdev)
{
    u32 rdh;
    struct e1000e_device *dev;
    int rx_que;
    struct e1000e_rx_desc *rxdesc;
    int ret;

    dev = (struct e1000e_device *)netdev->vendor;

#if 0
    u32 tdh;
    u8 txpkt[1024];
    txpkt[0] = 0xff;
    txpkt[1] = 0xff;
    txpkt[2] = 0xff;
    txpkt[3] = 0xff;
    txpkt[4] = 0xff;
    txpkt[5] = 0xff;
    txpkt[6] = dev->macaddr[0];
    txpkt[7] = dev->macaddr[1];
    txpkt[8] = dev->macaddr[2];
    txpkt[9] = dev->macaddr[3];
    txpkt[10] = dev->macaddr[4];
    txpkt[11] = dev->macaddr[5];
    tdh = mmio_read32(dev->mmio, E1000E_REG_TDH(0));
    dev->tx_desc[dev->tx_tail].address = txpkt;
    dev->tx_desc[dev->tx_tail].length = 100;
    dev->tx_desc[dev->tx_tail].sta = 0;
    dev->tx_desc[dev->tx_tail].css = 0;
    dev->tx_desc[dev->tx_tail].cso = 0;
    dev->tx_desc[dev->tx_tail].special = 0;
    dev->tx_desc[dev->tx_tail].cmd = (1<<3) | (1<<1) | 1;
    mmio_write32(dev->mmio, E1000E_REG_TDT(0), dev->tx_tail);
    dev->tx_tail = (dev->tx_tail + 1) % dev->tx_bufsz;
#endif

    rdh = mmio_read32(dev->mmio, E1000E_REG_RDH(0));
#if 0
    kprintf("XXXX %x %x %x / %x %x %x %x %x %x %x %x\r\n",
            mmio_read32(dev->mmio, E1000E_REG_RCTL),
            mmio_read32(dev->mmio, E1000E_REG_RDH(0)),
            mmio_read32(dev->mmio, E1000E_REG_RDT(0)),
            mmio_read32(dev->mmio, E1000E_REG_RDH(1)),
            mmio_read32(dev->mmio, E1000E_REG_RDT(1)),
            mmio_read32(dev->mmio, 0x0400c),
            mmio_read32(dev->mmio, 0x04010),
            mmio_read32(dev->mmio, 0x04048),
            mmio_read32(dev->mmio, 0x0404c),
            mmio_read32(dev->mmio, 0x04050),
            mmio_read32(dev->mmio, 0x0404c));
#endif
#if 0
    int addr;
    for ( addr = 0x4080; addr <= 0x40c0; addr += 4 ) {
        kprintf("  %.8x : %.8x\r\n", addr, mmio_read32(dev->mmio, addr));
    }
#endif

    rx_que = (dev->rx_bufsz - dev->rx_tail + rdh) % dev->rx_bufsz;
    if ( rx_que > 0 ) {
        /* Check the head of RX ring buffer */
        rxdesc = (struct e1000e_rx_desc *)
            (((u64)dev->rx_desc) + (dev->rx_tail % dev->rx_bufsz)
             * sizeof(struct e1000e_rx_desc));
        ret = len < rxdesc->length ? len : rxdesc->length;
        kmemcpy(pkt, (void *)rxdesc->address, ret);

        rxdesc->checksum = 0;
        rxdesc->status = 0;
        rxdesc->errors = 0;
        rxdesc->special = 0;

        mmio_write32(dev->mmio, E1000E_REG_RDT(0), dev->rx_tail);
        dev->rx_tail = (dev->rx_tail + 1) % dev->rx_bufsz;

        return ret;
    }

    return -1;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
