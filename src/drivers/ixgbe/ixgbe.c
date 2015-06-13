/*_
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "../pci/pci.h"
#include "../net/netdev.h"
#include "../../kernel/kernel.h"

void pause(void);

#define PKTSZ   4096

#define PCI_VENDOR_INTEL        0x8086


#define IXGBE_X520              0x10fb

#define IXGBE_REG_RAL(n)        0xa200 + 8 * (n)
#define IXGBE_REG_RAH(n)        0xa204 + 8 * (n)
#define IXGBE_REG_CTRL          0x0000
#define IXGBE_REG_CTRL_EXT      0x0018
#define IXGBE_REG_EIMC          0x0888
#define IXGBE_REG_MTA           0x5200  /* x128 */
#define IXGBE_REG_SRRCTL0       0x2100
#define IXGBE_REG_RXDCTL0       0x1028
#define IXGBE_REG_RXCTL         0x3000
#define IXGBE_REG_RSCCTL        0x102c
#define IXGBE_REG_FCTRL         0x5080
#define IXGBE_REG_TXDCTL(n)     (0x6028 + 0x40 * (n))
#define IXGBE_REG_RDBAL(n)       ((n) < 64) \
    ? (0x1000 + 0x40 * (n)) : (0xd000 + 0x40 * ((n) - 64))
#define IXGBE_REG_RDBAH(n)       ((n) < 64) \
    ? (0x1004 + 0x40 * (n)) : (0xd004 + 0x40 * ((n) - 64))
#define IXGBE_REG_RDLEN(n)       ((n) < 64) \
    ? (0x1008 + 0x40 * (n)) : (0xd008 + 0x40 * ((n) - 64))
#define IXGBE_REG_RDH(n)        ((n) < 64) \
    ? (0x1010 + 0x40 * (n)) : (0xd010 + 0x40 * ((n) - 64))
#define IXGBE_REG_RDT(n)        ((n) < 64) \
    ? (0x1018 + 0x40 * (n)) : (0xd018 + 0x40 * ((n) - 64))
#define IXGBE_REG_TDH(n)        (0x6010 + 0x40 * (n))
#define IXGBE_REG_TDT(n)        (0x6018 + 0x40 * (n))
#define IXGBE_REG_TDBAL(n)      (0x6000 + 0x40 * (n))
#define IXGBE_REG_TDBAH(n)      (0x6004 + 0x40 * (n))
#define IXGBE_REG_TDLEN(n)      (0x6008 + 0x40 * (n))
#define IXGBE_REG_TDWBAL(n)     (0x6038 + 0x40 * (n))
#define IXGBE_REG_TDWBAH(n)     (0x603c + 0x40 * (n))
#define IXGBE_REG_DMATXCTL      0x4a80

/* DCA registers */
#define IXGBE_REG_DCA_RXCTRL(n) ((n) < 64) \
    ? (0x100c + 0x40 * (n)) : (0xd00c + 0x40 * ((n) - 64))
#define IXGBE_REG_DCA_TXCTRL(n) (0x600c + 0x40 * (n))
#define IXGBE_REG_DCA_ID        0x11070
#define IXGBE_REG_DCA_CTRL      0x11074

#define IXGBE_CTRL_LRST (1<<3)  /* Link reset */
#define IXGBE_CTRL_PCIE_MASTER_DISABLE  (u32)(1<<2)
#define IXGBE_CTRL_RST  (1<<26)

#define IXGBE_FCTRL_MPE         (1<<8)
#define IXGBE_FCTRL_UPE         (1<<9)
#define IXGBE_FCTRL_BAM         (1<<10)

#define IXGBE_SRRCTL_BSIZE_PKT8K        (8)
#define IXGBE_SRRCTL_BSIZE_PKT16K       (16)
#define IXGBE_SRRCTL_BSIZE_HDR256       (4<<8)
#define IXGBE_SRRCTL_DESCTYPE_LEGACY    (0)

#define IXGBE_RXDCTL_ENABLE     (1<<25)
#define IXGBE_RXDCTL_VME        (1<<30)
#define IXGBE_RXCTL_RXEN        1
#define IXGBE_TXDCTL_ENABLE     (1<<25)
#define IXGBE_DMATXCTL_TE       1
#define IXGBE_DMATXCTL_VT       0x8100

struct ixgbe_tx_desc {
    u64 address;
    u16 length;
    u8 cso;
    u8 cmd;
    u8 sta;
    u8 css;
    u16 special;
} __attribute__ ((packed));

struct ixgbe_rx_desc {
    u64 address;
    u16 length;
    u16 checksum;
    u8 status;
    u8 errors;
    u16 special;
} __attribute__ ((packed));


struct ixgbe_adv_tx_desc_ctx {
    u32 vlan_maclen_iplen;
    u32 fcoef_ipsec_sa_idx;
    u64 other;
} __attribute__ ((packed));

struct ixgbe_adv_tx_desc_data {
    u64 pkt_addr;
    u16 length;
    u8 dtyp_mac;
    u8 dcmd;
    u32 paylen_popts_cc_idx_sta;
} __attribute__ ((packed));

struct ixgbe_adv_rx_desc_read {
    u64 pkt_addr;               /* 0:A0 */
    volatile u64 hdr_addr;      /* 0:DD */
} __attribute__ ((packed));
struct ixgbe_adv_rx_desc_wb {
    /* Write back */
    u32 info0;
    u32 info1;
    u32 staterr;
    u16 length;
    u16 vlan;
} __attribute__ ((packed));
union ixgbe_adv_rx_desc {
    struct ixgbe_adv_rx_desc_read read;
    struct ixgbe_adv_rx_desc_wb wb;
} __attribute__ ((packed));

struct ixgbe_device {
    u64 mmio;

    u64 rx_base;
    u32 rx_tail;
    u32 rx_bufsz;
    u32 rx_divisorm;
    u64 tx_base[8];
    u32 tx_tail[8];
    u32 tx_bufsz[8];
    u32 tx_divisorm[8];

    struct ixgbe_adv_rx_desc_read *rx_read[1];

    u32 *tx_head;

    /* Cache */
    u32 rx_head_cache;
    u32 tx_head_cache[2];

    u8 macaddr[6];

    struct pci_device *pci_device;
};

/* Prototype declarations */
void ixgbe_update_hw(void);
struct ixgbe_device * ixgbe_init_hw(struct pci_device *);
int ixgbe_setup_rx_desc(struct ixgbe_device *);
int ixgbe_setup_tx_desc(struct ixgbe_device *);
int ixgbe_recvpkt(u8 *, u32, struct netdev *);
int ixgbe_sendpkt(const u8 *, u32, struct netdev *);

int ixgbe_routing_test(struct netdev *);

u32 bswap32(u32);

/*
 * Read data from a PCI register by MMIO
 */
static __inline__ volatile u32
mmio_read32(u64 base, u64 offset)
{
    return *(volatile u32 *)(base + offset);
}

/*
 * Write data to a PCI register by MMIO
 */
static __inline__ void
mmio_write32(u64 base, u64 offset, volatile u32 value)
{
    *(volatile u32 *)(base + offset) = value;
}

/*
 * Initialize this driver
 */
void
ixgbe_init(void)
{
    /* Initialize the driver */
    ixgbe_update_hw();
}

/*
 * Update hardware
 */
void
ixgbe_update_hw(void)
{
    struct pci *pci;
    struct ixgbe_device *dev;
    struct netdev *netdev;
    int idx;

    /* Get the list of PCI devices */
    pci = pci_list();

    /* Search NICs requiring this ixgbe driver */
    idx = 0;
    while ( NULL != pci ) {
        if ( PCI_VENDOR_INTEL == pci->device->vendor_id ) {
            switch ( pci->device->device_id ) {
            case IXGBE_X520:
                dev = ixgbe_init_hw(pci->device);
                netdev = netdev_add_device(dev->macaddr, dev);
                netdev->recvpkt = ixgbe_recvpkt;
                netdev->sendpkt = ixgbe_sendpkt;
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
 * Initialize hardware
 */
struct ixgbe_device *
ixgbe_init_hw(struct pci_device *pcidev)
{
    struct ixgbe_device *dev;
    u32 m32;
    int i;

    dev = kmalloc(sizeof(struct ixgbe_device));

    /* Assert */
    if ( PCI_VENDOR_INTEL != pcidev->vendor_id ) {
        kfree(dev);
        return NULL;
    }

    /* Read MMIO */
    dev->mmio = pci_read_mmio(pcidev->bus, pcidev->slot, pcidev->func);
    if ( 0 == dev->mmio ) {
        kfree(dev);
        return NULL;
    }

    /* Reset PCIe master */
    mmio_write32(dev->mmio, IXGBE_REG_CTRL,
                 mmio_read32(dev->mmio, IXGBE_REG_CTRL)
                 | IXGBE_CTRL_PCIE_MASTER_DISABLE);
    arch_busy_usleep(50);
    mmio_write32(dev->mmio, IXGBE_REG_CTRL,
                 mmio_read32(dev->mmio, IXGBE_REG_CTRL)
                 & (~IXGBE_CTRL_PCIE_MASTER_DISABLE));
    arch_busy_usleep(50);

    /* Read MAC address */
    switch ( pcidev->device_id ) {
    case IXGBE_X520:
        m32 = mmio_read32(dev->mmio, IXGBE_REG_RAL(0));
        dev->macaddr[0] = m32 & 0xff;
        dev->macaddr[1] = (m32 >> 8) & 0xff;
        dev->macaddr[2] = (m32 >> 16) & 0xff;
        dev->macaddr[3] = (m32 >> 24) & 0xff;
        m32 = mmio_read32(dev->mmio, IXGBE_REG_RAH(0));
        dev->macaddr[4] = m32 & 0xff;
        dev->macaddr[5] = (m32 >> 8) & 0xff;
        break;
    }

    /* Initialize */
    mmio_write32(dev->mmio, IXGBE_REG_EIMC, 0x7fffffff);
    mmio_write32(dev->mmio, IXGBE_REG_CTRL,
                 mmio_read32(dev->mmio, IXGBE_REG_CTRL)
                 | IXGBE_CTRL_RST);
    for ( i = 0; i < 10; i++ ) {
        arch_busy_usleep(1);
        m32 = mmio_read32(dev->mmio, IXGBE_REG_CTRL);
        if ( !(m32 & IXGBE_CTRL_RST) ) {
            break;
        }
    }
    if ( m32 & IXGBE_CTRL_RST ) {
        kprintf("Error on reset\r\n");
    }
    arch_busy_usleep(50);

    mmio_write32(dev->mmio, IXGBE_REG_FCTRL,
                 IXGBE_FCTRL_MPE | IXGBE_FCTRL_UPE | IXGBE_FCTRL_BAM);

    /* Multicast array table */
    for ( i = 0; i < 128; i++ ) {
        mmio_write32(dev->mmio, IXGBE_REG_MTA + i * 4, 0);
    }

    /* Enable interrupt (REG_IMS <- 0x1F6DC, then read REG_ICR ) */
    //mmio_write32(dev->mmio, IXGBE_REG_IMS, 0x1f6dc);
    //mmio_read32(dev->mmio, 0xc0);

    /* Setup TX/RX descriptor */
    ixgbe_setup_rx_desc(dev);
    ixgbe_setup_tx_desc(dev);

    dev->pci_device = pcidev;

    return dev;
}

/*
 * Setup RX descriptor
 */
int
ixgbe_setup_rx_desc(struct ixgbe_device *dev)
{
    union ixgbe_adv_rx_desc *rxdesc;
    int i;
    u32 m32;

    /* Previous tail */
    dev->rx_tail = 0;
    /* up to 64 K minus 8 */
    dev->rx_bufsz = (1<<8);
    dev->rx_divisorm = (1<<8) - 1;
    /* Cache */
    dev->rx_head_cache = 0;

    /* Allocate memory for RX descriptors */
    dev->rx_read[0] = kmalloc(dev->rx_bufsz
                              * sizeof(struct ixgbe_adv_rx_desc_read));
    if ( 0 == dev->rx_read[0] ) {
        kfree(dev);
        return NULL;
    }

    /* ToDo: 16 bytes for alignment */
    dev->rx_base = (u64)kmalloc(dev->rx_bufsz
                                * sizeof(union ixgbe_adv_rx_desc));
    if ( 0 == dev->rx_base ) {
        kfree(dev);
        return NULL;
    }
    for ( i = 0; i < dev->rx_bufsz; i++ ) {
        rxdesc = (union ixgbe_adv_rx_desc *)(dev->rx_base
                                             + i * sizeof(union ixgbe_adv_rx_desc));
        rxdesc->read.pkt_addr = (u64)kmalloc(PKTSZ);
        rxdesc->read.hdr_addr = 0;//(u64)kmalloc(4096);

        dev->rx_read[0][i].pkt_addr = rxdesc->read.pkt_addr;
        dev->rx_read[0][i].hdr_addr = rxdesc->read.hdr_addr;
    }

    mmio_write32(dev->mmio, IXGBE_REG_RDBAH(0), dev->rx_base >> 32);
    mmio_write32(dev->mmio, IXGBE_REG_RDBAL(0), dev->rx_base & 0xffffffff);
    mmio_write32(dev->mmio, IXGBE_REG_RDLEN(0),
                 dev->rx_bufsz * sizeof(union ixgbe_adv_rx_desc));

    mmio_write32(dev->mmio, IXGBE_REG_SRRCTL0,
                 IXGBE_SRRCTL_BSIZE_PKT16K /*| IXGBE_SRRCTL_BSIZE_HDR256*/
                 | /*IXGBE_SRRCTL_DESCTYPE_LEGACY*/(1<<25) | (1<<28));

    /* Support jumbo frame */
#if 0
    mmio_write32(dev->mmio, /*IXGBE_REG_MAXFRS*/0x04268, 0x23f0<<16);
    mmio_write32(dev->mmio, /*HLREG*/0x04240,
                 mmio_read32(dev->mmio, 0x04240) | (1<<2));
#endif

    //RSCCTL
    mmio_write32(dev->mmio, IXGBE_REG_RXDCTL0,
                 IXGBE_RXDCTL_ENABLE | IXGBE_RXDCTL_VME);
    for ( i = 0; i < 10; i++ ) {
        arch_busy_usleep(1);
        m32 = mmio_read32(dev->mmio, IXGBE_REG_RXDCTL0);
        if ( m32 & IXGBE_RXDCTL_ENABLE ) {
            break;
        }
    }
    if ( !(m32 & IXGBE_RXDCTL_ENABLE) ) {
        kprintf("Error on enable an RX queue\r\n");
    }

    mmio_write32(dev->mmio, IXGBE_REG_RDH(0), 0);
    /* RDT must be larger than 0 for the initial value to receive the first
       packet but I don't know why: See 4.6.7 */
    mmio_write32(dev->mmio, IXGBE_REG_RDT(0), dev->rx_bufsz - 1);
    mmio_write32(dev->mmio, IXGBE_REG_RXCTL, IXGBE_RXCTL_RXEN);


    return 0;
}

/*
 * Setup TX descriptor
 */
int
ixgbe_setup_tx_desc(struct ixgbe_device *dev)
{
    struct ixgbe_adv_tx_desc_data *txdesc;
    int i;
    u32 m32;
    int q;

    for ( q = 0; q < 8; q++ ) {
        dev->tx_tail[q] = 0;
        /* up to 64 K minus 8 */
        dev->tx_bufsz[q] = (1<<8);
        dev->tx_divisorm[q] = (1<<8) - 1;
        /* Cache */
        dev->tx_head_cache[q] = 0;

        /* ToDo: 16 bytes for alignment */
        dev->tx_base[q] = (u64)kmalloc(dev->tx_bufsz[q]
                                       * sizeof(struct ixgbe_adv_tx_desc_data));
        for ( i = 0; i < dev->tx_bufsz[q]; i++ ) {
            txdesc = (struct ixgbe_adv_tx_desc_data *)(dev->tx_base[q] + i
                                                       * sizeof(struct ixgbe_adv_tx_desc_data));
            txdesc->pkt_addr = (u64)kmalloc(PKTSZ);
            txdesc->length = 0;
            txdesc->dtyp_mac = (3 << 4);
            txdesc->dcmd = 0;
            txdesc->paylen_popts_cc_idx_sta = 0;
        }
        mmio_write32(dev->mmio, IXGBE_REG_TDBAH(q), dev->tx_base[q] >> 32);
        mmio_write32(dev->mmio, IXGBE_REG_TDBAL(q), dev->tx_base[q] & 0xffffffff);
        mmio_write32(dev->mmio, IXGBE_REG_TDLEN(q),
                     dev->tx_bufsz[q] * sizeof(struct ixgbe_tx_desc));
        mmio_write32(dev->mmio, IXGBE_REG_TDH(q), 0);
        mmio_write32(dev->mmio, IXGBE_REG_TDT(q), 0);

        /* Write-back */
#if 0
        u32 *tdwba = kmalloc(4096);
        mmio_write32(dev->mmio, IXGBE_REG_TDWBAL(q),
                     ((u64)tdwba & 0xfffffffc) | 1);
        mmio_write32(dev->mmio, IXGBE_REG_TDWBAH(q), ((u64)tdwba) >> 32);
        dev->tx_head = tdwba;
#endif

        /* Enable */
        mmio_write32(dev->mmio, IXGBE_REG_DMATXCTL,
        IXGBE_DMATXCTL_TE | IXGBE_DMATXCTL_VT);
    }
    for ( q = 0; q < 8; q++ ) {
        mmio_write32(dev->mmio, IXGBE_REG_TXDCTL(q), IXGBE_TXDCTL_ENABLE);
#if 1
        mmio_write32(dev->mmio, IXGBE_REG_TXDCTL(q), IXGBE_TXDCTL_ENABLE
                     | (64<<16) /* WTHRESH */
                     | (16<<8) /* HTHRESH */| (16) /* PTHRESH */);
#endif
        for ( i = 0; i < 10; i++ ) {
            arch_busy_usleep(1);
            m32 = mmio_read32(dev->mmio, IXGBE_REG_TXDCTL(q));
            if ( m32 & IXGBE_TXDCTL_ENABLE ) {
                break;
            }
        }
        if ( !(m32 & IXGBE_TXDCTL_ENABLE) ) {
            kprintf("Error on enable a TX queue\r\n");
        }
    }

    return 0;
}

int
ixgbe_recvpkt(u8 *pkt, u32 len, struct netdev *netdev)
{
    u32 rdh;
    struct ixgbe_device *dev;
    int rx_que;
    struct ixgbe_rx_desc *rxdesc;
    int ret;

    dev = (struct ixgbe_device *)netdev->vendor;

    rdh = mmio_read32(dev->mmio, IXGBE_REG_RDH(0));
    rx_que = (dev->rx_bufsz - dev->rx_tail + rdh) % dev->rx_bufsz;
    if ( rx_que > 0 ) {
        /* Check the head of RX ring buffer */
        rxdesc = (struct ixgbe_rx_desc *)
            (((u64)dev->rx_base) + (dev->rx_tail % dev->rx_bufsz)
             * sizeof(struct ixgbe_rx_desc));
        ret = len < rxdesc->length ? len : rxdesc->length;
        kmemcpy(pkt, (void *)rxdesc->address, ret);

        rxdesc->checksum = 0;
        rxdesc->status = 0;
        rxdesc->errors = 0;
        rxdesc->special = 0;

        mmio_write32(dev->mmio, IXGBE_REG_RDT(0), dev->rx_tail);
        dev->rx_tail = (dev->rx_tail + 1) % dev->rx_bufsz;

        return ret;
    }

    return -1;
}

int
ixgbe_sendpkt(const u8 *pkt, u32 len, struct netdev *netdev)
{
    u32 tdh;
    struct ixgbe_device *dev;
    int tx_avl;
    struct ixgbe_tx_desc *txdesc;

    dev = (struct ixgbe_device *)netdev->vendor;
    tdh = mmio_read32(dev->mmio, IXGBE_REG_TDH(0));

    tx_avl = dev->tx_bufsz[0] - ((dev->tx_bufsz[0] - tdh + dev->tx_tail[0])
                                 % dev->tx_bufsz[0]);

    if ( tx_avl > 0 ) {
        /* Check the head of TX ring buffer */
        txdesc = (struct ixgbe_tx_desc *)
            (((u64)dev->tx_base[0]) + (dev->tx_tail[0] % dev->tx_bufsz[0])
             * sizeof(struct ixgbe_tx_desc));
        kmemcpy((void *)txdesc->address, pkt, len);

        txdesc->length = len;
        txdesc->sta = 0;
        txdesc->css = 0;
        txdesc->cso = 0;
        txdesc->special = 0;
        txdesc->cmd = (1<<3) | (1<<1) | 1;

        dev->tx_tail[0] = (dev->tx_tail[0] + 1) % dev->tx_bufsz[0];
        mmio_write32(dev->mmio, IXGBE_REG_TDT(0), dev->tx_tail[0]);

        return len;
    }

    return -1;
}













int
ixgbe_check_buffer(struct netdev *netdev)
{
    struct ixgbe_device *ixgbedev;

    ixgbedev = (struct ixgbe_device *)netdev->vendor;
    kprintf("  RDT0 %x\r\n", mmio_read32(ixgbedev->mmio, IXGBE_REG_RDT(0)));
    kprintf("  RDH0 %x\r\n", mmio_read32(ixgbedev->mmio, IXGBE_REG_RDH(0)));
    kprintf("  TDT0 %x\r\n", mmio_read32(ixgbedev->mmio, IXGBE_REG_TDT(0)));
    kprintf("  TDH0 %x\r\n", mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0)));

    return 0;
}


int
ixgbe_routing_test(struct netdev *netdev)
{
    struct ixgbe_device *ixgbedev;
    struct ixgbe_rx_desc *rxdesc;
    struct ixgbe_tx_desc *txdesc;
    u32 rdh;
    u32 tdh;
    int rx_que;
    int tx_avl;
    int nrp;
    int i;
    u8 *rxpkt;
    u8 *txpkt;

    ixgbedev = (struct ixgbe_device *)netdev->vendor;
    for ( ;; ) {
        rdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_RDH(0));
        tdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));

        rx_que = (ixgbedev->rx_bufsz - ixgbedev->rx_tail + rdh)
            % ixgbedev->rx_bufsz;

        tx_avl = ixgbedev->tx_bufsz[0]
            - ((ixgbedev->tx_bufsz[0] - tdh + ixgbedev->tx_tail[0])
               % ixgbedev->tx_bufsz[0]);

        if ( rx_que > tx_avl ) {
            nrp = tx_avl;
        } else {
            nrp = rx_que;
        }
        /* Routing */
        for ( i = 0; i < nrp; i++ ) {
            rxdesc = (struct ixgbe_rx_desc *)
                (ixgbedev->rx_base
                 + ((ixgbedev->rx_tail + i) % ixgbedev->rx_bufsz)
                 * sizeof(struct ixgbe_rx_desc));
            txdesc = (struct ixgbe_tx_desc *)
                (ixgbedev->tx_base[0]
                 + ((ixgbedev->tx_tail[0] + i) % ixgbedev->tx_bufsz[0])
                 * sizeof(struct ixgbe_tx_desc));

            rxpkt = (u8 *)rxdesc->address;
            txpkt = (u8 *)txdesc->address;

            /* Check */
#if 0
            kprintf("XX: %x %x %d.%d.%d.%d\r\n", rxdesc->special,
                    rxdesc->length, rxpkt[30], rxpkt[31], rxpkt[32], rxpkt[33]);
#endif
#if 0
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
#endif
            rxdesc->address = txpkt;
            txdesc->address = rxpkt;
            txdesc->length = rxdesc->length;
            txdesc->sta = 0;
            txdesc->css = 0;
            txdesc->cso = 0;
            txdesc->special = 200;
            txdesc->cmd = (1<<3) | (1<<1) | 1 | (1<<6);
        }

        ixgbedev->rx_tail = (ixgbedev->rx_tail + nrp) & ixgbedev->rx_divisorm;
        ixgbedev->tx_tail[0] = (ixgbedev->tx_tail[0] + nrp) & ixgbedev->tx_divisorm[0];
        mmio_write32(ixgbedev->mmio, IXGBE_REG_RDT(0), ixgbedev->rx_tail);
        mmio_write32(ixgbedev->mmio, IXGBE_REG_TDT(0), ixgbedev->tx_tail[0]);
    }

    return 0;
}


typedef int (*router_rx_cb_t)(const u8 *, u32, int);
int arch_dbg_printf(const char *fmt, ...);



static u32 Ax;
static u32 Ay;
static u32 Az;
static u32 Aw;
static __inline__ u32
xor128(void)
{
    u32 t;

    t = Ax ^ (Ax<<11);
    Ax = Ay;
    Ay = Az;
    Az = Aw;
    return Aw = (Aw ^ (Aw>>19)) ^ (t ^ (t >> 8));
}

int
ixgbe_tx_test(struct netdev *netdev, u8 *pkt, int len, int blksize)
{
    struct ixgbe_device *ixgbedev;
    //struct ixgbe_tx_desc *txdesc;
    struct ixgbe_adv_tx_desc_data *txdesc;
    u32 tdh;
    u32 next_tail;
    //int blksize = 32;
    int i;

    Ax = 123456789;
    Ay = 362436069;
    Az = 521288629;
    Aw = 88675123;

    ixgbedev = (struct ixgbe_device *)netdev->vendor;

    /* Prepare */
    for ( i = 0; i < ixgbedev->tx_bufsz[0]; i++ ) {
        txdesc = (struct ixgbe_adv_tx_desc_data *)(ixgbedev->tx_base[0] + i
                                                   * sizeof(struct ixgbe_adv_tx_desc_data));
        kmemcpy((void *)txdesc->pkt_addr, pkt, len);
    }

    for ( ;; ) {
        tdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));
        //tdh = *(ixgbedev->tx_head);
        next_tail = (ixgbedev->tx_tail[0] + blksize) % ixgbedev->tx_bufsz[0];

        if ( ixgbedev->tx_bufsz[0] -
             ((ixgbedev->tx_bufsz[0] - tdh + ixgbedev->tx_tail[0]) % ixgbedev->tx_bufsz[0])
             > blksize ) {
            /* Not full */
            for ( i = 0; i < blksize; i++ ) {
                txdesc = (struct ixgbe_adv_tx_desc_data *)(ixgbedev->tx_base[0]
                                                  + (ixgbedev->tx_tail[0] + i)
                                                  * sizeof(struct ixgbe_tx_desc));
#if 0
                //for ( j = 0; j < len; j++ ) {
                //    ((u8 *)(txdesc->address))[j] = pkt[j];
                //}
                u32 tmp = xor128();
                //txdesc->pkt_addr = txdesc->pkt_addr & 0xfffffffffffffff0;
                //kmemcpy((void *)((u64)txdesc->pkt_addr+30), &tmp, 4);
                *(u32 *)((u64)txdesc->pkt_addr+30) = tmp;
#endif
                //txdesc->pkt_addr = (u64)pkt;
                txdesc->length = len;
                txdesc->dtyp_mac = (3 << 4);
                txdesc->dcmd = (1<<5) | (1<<1) | 1;
                txdesc->paylen_popts_cc_idx_sta = (len << 14);
            }

            ixgbedev->tx_tail[0] = next_tail;
            mmio_write32(ixgbedev->mmio, IXGBE_REG_TDT(0), ixgbedev->tx_tail[0]);
        }
    }

    return 0;
}

int
ixgbe_tx_test2(struct netdev *netdev, u8 *pkt, int len, int blksize)
{
    struct ixgbe_device *ixgbedev;
    //struct ixgbe_tx_desc *txdesc;
    struct ixgbe_adv_tx_desc_data *txdesc;
    u32 tdh;
    u32 next_tail;
    //int blksize = 32;
    int i;

    Ax = 123456789;
    Ay = 362436069;
    Az = 521288629;
    Aw = 88675123;

    ixgbedev = (struct ixgbe_device *)netdev->vendor;

    /* Prepare */
    for ( i = 0; i < ixgbedev->tx_bufsz[0]; i++ ) {
        txdesc = (struct ixgbe_adv_tx_desc_data *)(ixgbedev->tx_base[0] + i
                                                   * sizeof(struct ixgbe_adv_tx_desc_data));
        kmemcpy((void *)txdesc->pkt_addr, pkt, len);
    }

    for ( ;; ) {
        //tdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));
        tdh = ixgbedev->tx_head_cache[0];
        //tdh = *(ixgbedev->tx_head);
        next_tail = (ixgbedev->tx_tail[0] + blksize) % ixgbedev->tx_bufsz[0];

        if ( ixgbedev->tx_bufsz[0] -
             ((ixgbedev->tx_bufsz[0] - tdh + ixgbedev->tx_tail[0]) % ixgbedev->tx_bufsz[0])
             <= blksize ) {
            tdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));
            ixgbedev->tx_head_cache[0] = tdh;
            if ( ixgbedev->tx_bufsz[0] -
                 ((ixgbedev->tx_bufsz[0] - tdh + ixgbedev->tx_tail[0]) % ixgbedev->tx_bufsz[0])
                 <= blksize ) {
                continue;
            }
        }
        /* Not full */
        for ( i = 0; i < blksize; i++ ) {
            txdesc = (struct ixgbe_adv_tx_desc_data *)(ixgbedev->tx_base[0]
                                                       + (ixgbedev->tx_tail[0] + i)
                                                       * sizeof(struct ixgbe_tx_desc));
#if 0
            //for ( j = 0; j < len; j++ ) {
            //    ((u8 *)(txdesc->address))[j] = pkt[j];
            //}
            u32 tmp = xor128();
            //txdesc->pkt_addr = txdesc->pkt_addr & 0xfffffffffffffff0;
            //kmemcpy((void *)((u64)txdesc->pkt_addr+30), &tmp, 4);
            *(u32 *)((u64)txdesc->pkt_addr+30) = tmp;
#endif

            //txdesc->pkt_addr = (u64)pkt;
            txdesc->length = len;
            txdesc->dtyp_mac = (3 << 4);
            txdesc->dcmd = (1<<5) | (1<<1) | 1;
            txdesc->paylen_popts_cc_idx_sta = (len << 14);
        }

        ixgbedev->tx_tail[0] = next_tail;
        mmio_write32(ixgbedev->mmio, IXGBE_REG_TDT(0), ixgbedev->tx_tail[0]);
    }

    return 0;
}

u64 rdtsc(void);
int
ixgbe_tx_test3(struct netdev *netdev, u8 *pkt, int len, int blksize)
{
    struct ixgbe_device *ixgbedev;
    u64 t0 = 0;
    u64 t1 = 0;
    u64 aa = 0;
    u64 dd = 0;
    u32 tdh = 0;

    aa = 0;
    dd = 0;

    ixgbedev = (struct ixgbe_device *)netdev->vendor;

    aa = dd = 0;
    __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
    t0 = (dd<<32) | aa;
    kprintf("%x\r\n", aa);
    //t0 = rdtsc();
    //tdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));
#if 1
    int i;
    for ( i = 0; i < 1000 * 1000; i++ ) {
        //tdh ^= mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));
        mmio_write32(ixgbedev->mmio, IXGBE_REG_TDT(0), 0);
    }
#endif
    aa = dd = 0;
    __asm__ __volatile__ ("movq $1,%%rcx;mfence;rdpmc" : "=a"(aa), "=d"(dd) );
    t1 = (dd<<32) | aa;
    //t1 = rdtsc();

    kprintf("Performance: %d %.16x %.16x %lld\r\n", tdh, t1, t0, t1 - t0);

#if 0
    t0 = rdtsc();
    mmio_write32(ixgbedev->mmio, IXGBE_REG_TDT(0), 0);
    t1 = rdtsc();
#endif

    return 0;
}

int
ixgbe_tx_test4(struct netdev *netdev, struct netdev *netdev2,
               struct netdev *netdev3, struct netdev *netdev4,
               u8 *pkt, int len, int blksize)
{
    struct ixgbe_device *dev[4];
    //struct ixgbe_tx_desc *txdesc;
    struct ixgbe_adv_tx_desc_data *txdesc;
    u32 tdh;
    u32 next_tail;
    //int blksize = 32;
    int i;
    int j;

    Ax = 123456789;
    Ay = 362436069;
    Az = 521288629;
    Aw = 88675123;

    dev[0] = (struct ixgbe_device *)netdev->vendor;
    dev[1] = (struct ixgbe_device *)netdev2->vendor;
    dev[2] = (struct ixgbe_device *)netdev3->vendor;
    dev[3] = (struct ixgbe_device *)netdev4->vendor;

    /* Prepare */
    for ( i = 0; i < 4; i++ ) {
        for ( j = 0; j < dev[i]->tx_bufsz[0]; j++ ) {
            txdesc = (struct ixgbe_adv_tx_desc_data *)(dev[i]->tx_base[0] + j
                                                       * sizeof(struct ixgbe_adv_tx_desc_data));
            kmemcpy((void *)txdesc->pkt_addr, pkt, len);
        }
    }

    for ( ;; ) {
        for ( i = 0; i < 4; i++ ) {
            tdh = dev[i]->tx_head_cache[0];
            next_tail = (dev[i]->tx_tail[0] + blksize) & dev[i]->tx_divisorm[0];

            if ( dev[i]->tx_bufsz[0] -
                 ((dev[i]->tx_bufsz[0] - tdh + dev[i]->tx_tail[0]) & dev[i]->tx_divisorm[0])
                 < blksize ) {
                tdh = mmio_read32(dev[i]->mmio, IXGBE_REG_TDH(0));
                dev[i]->tx_head_cache[0] = tdh;
                if ( dev[i]->tx_bufsz[0] -
                     ((dev[i]->tx_bufsz[0] - tdh + dev[i]->tx_tail[0]) % dev[i]->tx_bufsz[0])
                     < blksize ) {
                    continue;
                }
            }
            /* Not full */
            for ( j = 0; j < blksize; j++ ) {
                txdesc = (struct ixgbe_adv_tx_desc_data *)(dev[i]->tx_base[0]
                                                           + ((dev[i]->tx_tail[0] + j) & dev[i]->tx_divisorm[0])
                                                           * sizeof(struct ixgbe_tx_desc));
                //txdesc->pkt_addr = (u64)pkt;
                txdesc->length = len;
                txdesc->dtyp_mac = (3 << 4);
                txdesc->dcmd = (1<<5) | (1<<1) | 1;
                txdesc->paylen_popts_cc_idx_sta = (len << 14);
            }

            dev[i]->tx_tail[0] = next_tail;
            mmio_write32(dev[i]->mmio, IXGBE_REG_TDT(0), dev[i]->tx_tail[0]);
        }
    }

    return 0;
}


int
ixgbe_tx_test100g(struct netdev_list *list, u8 *pkt, int len, int blksize)
{
    struct ixgbe_device *dev[10];
    //struct ixgbe_tx_desc *txdesc;
    struct ixgbe_adv_tx_desc_data *txdesc;
    u32 tdh;
    u32 next_tail;
    int i;
    int j;
    struct netdev *netdev;

    for ( i = 0; i < 10; i++ ) {
        netdev = list->netdev;
        dev[i] = (struct ixgbe_device *)netdev->vendor;
        kprintf("XXXX %s %x %x\r\n", netdev->name, dev[i]->pci_device->vendor_id,
                dev[i]->pci_device->device_id);
        list = list->next;
    }

    /* Prepare */
    for ( i = 0; i < 10; i++ ) {
        for ( j = 0; j < dev[i]->tx_bufsz[0]; j++ ) {
            txdesc = (struct ixgbe_adv_tx_desc_data *)(dev[i]->tx_base[0] + j
                                                       * sizeof(struct ixgbe_adv_tx_desc_data));
            kmemcpy((void *)txdesc->pkt_addr, pkt, len);
        }
    }

    for ( ;; ) {
        for ( i = 0; i < 10; i++ ) {
            tdh = dev[i]->tx_head_cache[0];
            next_tail = (dev[i]->tx_tail[0] + blksize) & dev[i]->tx_divisorm[0];

            //kprintf("%d: %x %x\r\n", i, tdh, next_tail);

            if ( dev[i]->tx_bufsz[0] -
                 ((dev[i]->tx_bufsz[0] - tdh + dev[i]->tx_tail[0]) & dev[i]->tx_divisorm[0])
                 < blksize ) {
                tdh = mmio_read32(dev[i]->mmio, IXGBE_REG_TDH(0));
                dev[i]->tx_head_cache[0] = tdh;
                if ( dev[i]->tx_bufsz[0] -
                     ((dev[i]->tx_bufsz[0] - tdh + dev[i]->tx_tail[0]) % dev[i]->tx_bufsz[0])
                     < blksize ) {
                    continue;
                }
            }
            /* Not full */
            for ( j = 0; j < blksize; j++ ) {
                txdesc = (struct ixgbe_adv_tx_desc_data *)(dev[i]->tx_base[0]
                                                           + ((dev[i]->tx_tail[0] + j) & dev[i]->tx_divisorm[0])
                                                           * sizeof(struct ixgbe_tx_desc));
                //txdesc->pkt_addr = (u64)pkt;
                txdesc->length = len;
                txdesc->dtyp_mac = (3 << 4);
                txdesc->dcmd = (1<<5) | (1<<1) | 1;
                txdesc->paylen_popts_cc_idx_sta = (len << 14);
            }

            dev[i]->tx_tail[0] = next_tail;
            mmio_write32(dev[i]->mmio, IXGBE_REG_TDT(0), dev[i]->tx_tail[0]);
        }
    }

    return 0;
}


int
ixgbe_tx_100g_m2(struct netdev_list *list, int len, int blksize)
{
    struct ixgbe_device *dev[4];
    struct ixgbe_adv_tx_desc_data *txdesc;
    u32 tdh;
    u32 next_tail;
    int i;
    int j;
    struct netdev *netdev;
    u8 *pkt = kmalloc(9200);

    Ax = 123456789;
    Ay = 362436069;
    Az = 521288629;
    Aw = 88675123;

    for ( i = 0; i < 2; i++ ) {
        netdev = list->netdev;
        dev[i] = (struct ixgbe_device *)netdev->vendor;
        kprintf("XXXX %s %x %x\r\n", netdev->name, dev[i]->pci_device->vendor_id,
                dev[i]->pci_device->device_id);
        list = list->next;
    }

    /* Prepare */
    for ( i = 0; i < 2; i++ ) {
        for ( j = 0; j < dev[i]->tx_bufsz[0]; j++ ) {
            txdesc = (struct ixgbe_adv_tx_desc_data *)(dev[i]->tx_base[0] + j
                                                       * sizeof(struct ixgbe_adv_tx_desc_data));
            pkt[0] = 0x90;
            pkt[1] = 0xe2;
            pkt[2] = 0xba;
            pkt[3] = 0x6a;
            pkt[4] = 0x0c;
            pkt[5] = 0x40;
            pkt[6] = 0x90;
            pkt[7] = 0xe2;
            pkt[8] = 0xba;
            pkt[9] = 0x00;
            pkt[10] = 0x01;
            pkt[11] = 0xff;
            pkt[12] = 0x08;
            pkt[13] = 0x00;
            pkt[14] = 0x45;
            pkt[15] = 0x00;
            pkt[16] = ((len - 14) >> 8) & 0xff;
            pkt[17] = (len - 14) & 0xff;
            pkt[18] = 0x26;
            pkt[19] = 0x6d;
            pkt[20] = 0x00;
            pkt[21] = 0x00;
            pkt[22] = 64;
            pkt[23] = 17;
            pkt[24] = 0x00;
            pkt[25] = 0x00;
            pkt[26] = 203;
            pkt[27] = 178;
            pkt[28] = 135;
            pkt[29] = 8;
            pkt[30] = 10;
            pkt[31] = 0;
            pkt[32] = 1;
            pkt[33] = 1;
            /* UDP */
            pkt[34] = 0xff;
            pkt[35] = 0xff;
            pkt[36] = 0xff;
            pkt[37] = 0xfe;
            pkt[38] = (len - 14 - 20) >> 8;
            pkt[39] = (len - 14 - 20) & 0xff;
            pkt[40] = 0x00;
            pkt[41] = 0x00;
            kmemset(pkt + 42, 0, len - 42);

            kmemcpy((void *)txdesc->pkt_addr, pkt, len);
        }

        struct ixgbe_adv_tx_desc_ctx *ctx;
        ctx = (struct ixgbe_adv_tx_desc_ctx *)(dev[i]->tx_base[0]
                                               + ((dev[i]->tx_tail[0]) & dev[i]->tx_divisorm[0])
                                               * sizeof(struct ixgbe_tx_desc));
        ctx->vlan_maclen_iplen = (14ULL << 9) | 20;
        ctx->other = (1ULL << 29) | (2ULL << 20) | (2ULL << 9);
        dev[i]->tx_tail[0]++;
        mmio_write32(dev[i]->mmio, IXGBE_REG_TDT(0), dev[i]->tx_tail[0]);
    }


    for ( ;; ) {
        for ( i = 0; i < 2; i++ ) {
            tdh = dev[i]->tx_head_cache[0];
            next_tail = (dev[i]->tx_tail[0] + blksize) & dev[i]->tx_divisorm[0];

            if ( dev[i]->tx_bufsz[0] -
                 ((dev[i]->tx_bufsz[0] - tdh + dev[i]->tx_tail[0]) & dev[i]->tx_divisorm[0])
                 < blksize ) {
                tdh = mmio_read32(dev[i]->mmio, IXGBE_REG_TDH(0));
                dev[i]->tx_head_cache[0] = tdh;
                if ( dev[i]->tx_bufsz[0] -
                     ((dev[i]->tx_bufsz[0] - tdh + dev[i]->tx_tail[0]) % dev[i]->tx_bufsz[0])
                     < blksize ) {
                    continue;
                }
            }
            /* Not full */
            for ( j = 0; j < blksize; j++ ) {
                txdesc = (struct ixgbe_adv_tx_desc_data *)(dev[i]->tx_base[0]
                                                           + ((dev[i]->tx_tail[0] + j) & dev[i]->tx_divisorm[0])
                                                           * sizeof(struct ixgbe_tx_desc));
                //u8 *tmppkt;
                u32 r;
                //tmppkt = txdesc->pkt_addr;
                r = xor128();
                *(u32 *)(txdesc->pkt_addr + 30) = r;
                *(u16 *)(txdesc->pkt_addr + 24) = 0;
                //tmppkt[24] = 0x00;
                //tmppkt[25] = 0x00;
                txdesc->length = len;
                txdesc->dtyp_mac = (3 << 4);
                txdesc->dcmd = (1<<5) | (1<<1) | 1;
                txdesc->paylen_popts_cc_idx_sta = ((u64)len << 14)
                    | (1ULL << 8);
            }

            dev[i]->tx_tail[0] = next_tail;
            mmio_write32(dev[i]->mmio, IXGBE_REG_TDT(0), dev[i]->tx_tail[0]);
        }
    }

    return 0;
}

int
ixgbe_forwarding_test_sub(struct netdev *netdev1, struct netdev *netdev2)
{
    struct ixgbe_device *dev1;
    struct ixgbe_device *dev2;

    dev1 = (struct ixgbe_device *)netdev1->vendor;
    dev2 = (struct ixgbe_device *)netdev2->vendor;
    for ( ;; ) {
        dev1->rx_head_cache = mmio_read32(dev1->mmio, IXGBE_REG_RDH(0));
        dev2->tx_head_cache[0] = mmio_read32(dev2->mmio, IXGBE_REG_TDH(0));
        kprintf("** %x %x\r\n", dev1->rx_head_cache, dev2->tx_head_cache[0]);
        mmio_write32(dev1->mmio, IXGBE_REG_RDT(0), (dev1->rx_head_cache - 1) & 0x3fff);
        //mmio_write32(dev2->mmio, IXGBE_REG_TDT(0), dev2->tx_tail[0]);
        arch_busy_usleep(100000);
    }
    return 0;
}

int
ixgbe_forwarding_test(struct netdev *netdev1, struct netdev *netdev2)
{
    struct ixgbe_device *dev1;
    struct ixgbe_device *dev2;
    union ixgbe_adv_rx_desc * rxdesc;
    struct ixgbe_adv_tx_desc_data *txdesc;
    u32 rdh;
    u32 tdh;
    u32 rdt;
    u32 tdt;
    int rx_que;
    int tx_avl;
    int nrp;
    int i;
    int cntr = 0;
    u8 *txpkt;

    dev1 = (struct ixgbe_device *)netdev1->vendor;
    dev2 = (struct ixgbe_device *)netdev2->vendor;
    for ( ;; ) {

#if 0
        if ( ((cntr++) & 0x7fffff) == 0 ) {
            kprintf("%.8llx\r\n",
                    mmio_read32(dev1->mmio, 0x03FA0) /* Missed */
                );
            //mmio_read32(dev1->mmio, 0x04000) /* CRC Error */
        }
#endif

        rxdesc = (union ixgbe_adv_rx_desc *)
            (dev1->rx_base + dev1->rx_tail * sizeof(union ixgbe_adv_rx_desc));
        txdesc = (struct ixgbe_adv_tx_desc_data *)
            (dev2->tx_base[0] + dev2->tx_tail[0] * sizeof(struct ixgbe_adv_tx_desc_data));

        if ( rxdesc->read.hdr_addr & 0x1 ) {

            txpkt = (u8 *)dev1->rx_read[0][dev1->rx_tail].pkt_addr;
            *(u32 *)(txpkt + 0) =  0x67664000LLU;
            *(u16 *)(txpkt + 4) =  0x2472LLU;
            /* src */
            *(u16 *)(txpkt + 6) =  *((u16 *)netdev2->macaddr);
            *(u32 *)(txpkt + 8) =  *((u32 *)(netdev2->macaddr + 2));

            /* TTL -= 1 */
            txpkt[22]--;
            /* Compute checksum */
            u16 *ctmp;
            u32 cs;
            txpkt[24] = 0x0;
            txpkt[25] = 0x0;
            ctmp = (u16 *)txpkt;
            cs = 0;
            for ( i = 7; i < 17; i++ ) {
                cs += (u32)ctmp[i];
                cs = (cs & 0xffff) + (cs >> 16);
            }
            cs = 0xffff - cs;
            txpkt[24] = cs & 0xff;
            txpkt[25] = cs >> 8;

            /* Save Tx */
            u8 *tmp = (u8 *)((u64)txdesc->pkt_addr & 0xfffffffffffffff0ULL);

            txdesc->pkt_addr = (u64)txpkt;
            txdesc->length = rxdesc->wb.length;
            txdesc->dtyp_mac = (3 << 4);
            txdesc->dcmd = (1<<5) | (1<<1) | 1;
            txdesc->paylen_popts_cc_idx_sta = ((u32)rxdesc->wb.length << 14);

            dev1->rx_read[0][dev1->rx_tail].pkt_addr = (u64)tmp;
            rxdesc->read.pkt_addr = dev1->rx_read[0][dev1->rx_tail].pkt_addr;
            rxdesc->read.hdr_addr = 0;//dev1->rx_read[0][dev1->rx_tail].hdr_addr;

            /* Reset RX desc */
            //rxdesc->read.pkt_addr = dev1->rx_read[0][dev1->rx_tail].pkt_addr;
            //rxdesc->read.hdr_addr = dev1->rx_read[0][dev1->rx_tail].hdr_addr;

            dev2->tx_tail[0] = (dev2->tx_tail[0] + 1) & dev2->tx_divisorm[0];
            if ( (dev2->tx_tail[0] & 0xfff) == 0 ) {
                mmio_write32(dev2->mmio, IXGBE_REG_TDT(0), dev2->tx_tail[0]);
                mmio_write32(dev1->mmio, IXGBE_REG_RDT(0), dev1->rx_tail);
                //kprintf("%x %x\r\n", dev1->rx_tail, dev2->tx_tail[0]);
            }
            dev1->rx_tail = (dev1->rx_tail + 1) & dev1->rx_divisorm;
        }
        continue;


#if 0
        rdh = dev1->rx_head_cache;
        tdh = dev2->tx_head_cache[0];

        rx_que = (dev1->rx_bufsz - dev1->rx_tail + rdh) & dev1->rx_divisorm;
        tx_avl = dev2->tx_bufsz[0]
            - ((dev2->tx_bufsz[0] - tdh + dev2->tx_tail[0])
               & dev2->tx_divisorm[0])
            - 1;
        //kprintf("%x %x\r\n", rx_que, tx_avl);
#else
        rdh = mmio_read32(dev1->mmio, IXGBE_REG_RDH(0));
        //rx_que = (dev1->rx_bufsz - dev1->rx_tail + rdh) & dev1->rx_divisorm;
        rx_que = (rdh - dev1->rx_tail) & dev1->rx_divisorm;

        tdh = mmio_read32(dev2->mmio, IXGBE_REG_TDH(0));
        tx_avl = (tdh - dev2->tx_tail[0] - 1) & dev2->tx_divisorm[0];
        //tx_avl = dev2->tx_bufsz[0]
        //- ((dev2->tx_bufsz[0] - tdh + dev2->tx_tail[0])
        //& dev2->tx_divisorm[0])
        //- 1;
#endif

        if ( rx_que > tx_avl ) {
            nrp = tx_avl;
        } else {
            nrp = rx_que;
        }
        //if ( nrp > 1024 ) {
        //    nrp = 1024;
        //}
        if ( nrp <= 0 ) {
            continue;
        }

        if ( rx_que > 0x1800 || ((cntr++) & 0x7fff) == 0 ) {
            kprintf("%.4x %.4x %.4x %.8llx\r\n",
                    rx_que, rdh, dev1->rx_tail,
                    mmio_read32(dev1->mmio, 0x03FA0) /* Missed */
                );
            //mmio_read32(dev1->mmio, 0x04000) /* CRC Error */
        }
        for ( i = 0; i < nrp; i++ ) {
            int idx = ((dev1->rx_tail + i) & dev1->rx_divisorm);
            rxdesc = (union ixgbe_adv_rx_desc *)
                (dev1->rx_base + idx * sizeof(union ixgbe_adv_rx_desc));
            txdesc = (struct ixgbe_adv_tx_desc_data *)
                (dev2->tx_base[0] + ((dev2->tx_tail[0] + i) & dev2->tx_divisorm[0])
                 * sizeof(struct ixgbe_adv_tx_desc_data));

            //txpkt = (u8 *)dev1->rx_read[0][idx].pkt_addr;
            //txpkt = (u8 *)(txdesc->pkt_addr & 0xfffffffffffffff0ULL) ;
            //kmemcpy(txpkt, (u8 *)dev1->rx_read[0][idx].pkt_addr, rxdesc->wb.length);

#if 0
            //txpkt = (u8 *)dev1->rx_read[0][idx].pkt_addr;
            *(u32 *)(txpkt + 0) =  0x67664000LLU;
            *(u16 *)(txpkt + 4) =  0x2472LLU;
            /* src */
            *(u16 *)(txpkt + 6) =  *((u16 *)netdev2->macaddr);
            *(u32 *)(txpkt + 8) =  *((u32 *)(netdev2->macaddr + 2));
#endif

            /* Save Tx */
            //u8 *tmp = (u8 *)txdesc->pkt_addr;

            //txdesc->pkt_addr = (u64)txpkt;
            //txdesc->length = rxdesc->wb.length;
            //txdesc->dtyp_mac = (3 << 4);
            //txdesc->dcmd = (1<<5) | (1<<1) | 1;
            //txdesc->paylen_popts_cc_idx_sta = ((u32)rxdesc->wb.length << 14);

            //rxdesc->read.pkt_addr = (u64)txpkt;
            //dev1->rx_read[0][idx].pkt_addr = (u64)rxdesc->read.pkt_addr;
            //rxdesc->read.hdr_addr = dev1->rx_read[0][idx].hdr_addr;

            /* Reset RX desc */
            rxdesc->read.pkt_addr = dev1->rx_read[0][idx].pkt_addr;
            rxdesc->read.hdr_addr = 0;//dev1->rx_read[0][idx].hdr_addr;
            //arch_busy_usleep(1);
        }
        dev1->rx_tail = (dev1->rx_tail + nrp - 1) & dev1->rx_divisorm;
        mmio_write32(dev1->mmio, IXGBE_REG_RDT(0), dev1->rx_tail);
        dev1->rx_tail++;
        //dev2->tx_tail[0] = (dev2->tx_tail[0] + nrp) & dev2->tx_divisorm[0];
        //mmio_write32(dev2->mmio, IXGBE_REG_TDT(0), dev2->tx_tail[0]);
        continue;


        /* Routing */
        for ( i = 0; i < nrp; i++ ) {
            int rxidx;
            int txidx;
            rdt = dev1->rx_tail;
            tdt = dev2->tx_tail[0];
            rxidx = (rdt + i) & dev1->rx_divisorm;
            txidx = (tdt + i) & dev2->tx_divisorm[0];

            rxdesc = (union ixgbe_adv_rx_desc *)
                (dev1->rx_base + rxidx * sizeof(union ixgbe_adv_rx_desc));
            txdesc = (struct ixgbe_adv_tx_desc_data *)
                (dev2->tx_base[0] + txidx * sizeof(struct ixgbe_adv_tx_desc_data));

            txpkt = (u8 *)dev1->rx_read[0][rxidx].pkt_addr;


            //txdesc->vlan_maclen_iplen = 20;
#if 1
            /* dst:  00:40:66:67:72:24  */
            *(u32 *)txpkt =  0x67664000LLU;
            *(u16 *)(txpkt + 4) =  0x2472LLU;
            /* src */
            *(u16 *)(txpkt + 6) =  *((u16 *)netdev2->macaddr);
            *(u32 *)(txpkt + 8) =  *((u32 *)(netdev2->macaddr + 2));
#endif

#if 0
            /* TTL -= 1 */
            txpkt[22]--;
            /* Compute checksum */
            u16 *ctmp;
            u32 cs;
            txpkt[24] = 0x0;
            txpkt[25] = 0x0;
            ctmp = (u16 *)txpkt;
            cs = 0;
            for ( i = 7; i < 17; i++ ) {
                cs += (u32)ctmp[i];
                cs = (cs & 0xffff) + (cs >> 16);
            }
            cs = 0xffff - cs;
            txpkt[24] = cs & 0xff;
            txpkt[25] = cs >> 8;
#endif

            /* Save Tx */
            u8 *tmp = (u8 *)((u64)txdesc->pkt_addr & 0xfffffffffffffff0ULL);

            txdesc->pkt_addr = (u64)txpkt;
            txdesc->length = rxdesc->wb.length;
            txdesc->dtyp_mac = (3 << 4);
            txdesc->dcmd = (1<<5) | (1<<1) | 1;
            txdesc->paylen_popts_cc_idx_sta = ((u32)rxdesc->wb.length << 14);

            /* Reset RX desc */
            rxdesc->read.pkt_addr = (u64)tmp;
            dev1->rx_read[0][rxidx].pkt_addr = rxdesc->read.pkt_addr;
            rxdesc->read.hdr_addr = 0;//dev1->rx_read[0][rxidx].hdr_addr;
#if 0
            if ( 0 == (i & ((1<<12)-1)) ) {
                mmio_write32(dev1->mmio, IXGBE_REG_RDT(0),
                             (rdt + i - 1) & dev1->rx_divisorm);
                mmio_write32(dev2->mmio, IXGBE_REG_TDT(0),
                             (tdt + i) & dev2->tx_divisorm[0]);
            }
#endif
        }
        dev1->rx_tail = (rdt + nrp - 1) & dev1->rx_divisorm;
        mmio_write32(dev1->mmio, IXGBE_REG_RDT(0), dev1->rx_tail);
        dev1->rx_tail++;

        dev2->tx_tail[0] = (tdt + nrp) & dev2->tx_divisorm[0];
        mmio_write32(dev2->mmio, IXGBE_REG_TDT(0), dev2->tx_tail[0]);
    }

#if 0
    for ( ;; ) {
        rdh = mmio_read32(ixgbedev1->mmio, IXGBE_REG_RDH(0));
        tdh = mmio_read32(ixgbedev2->mmio, IXGBE_REG_TDH(0));

        //kprintf("XX: %s : %x %x\r\n", netdev1->name, rdh, tdh);

        /* Update the cache */
        //ixgbedev->rx_head_cache = rdh;
        //ixgbedev->tx_head_cache = tdh;

        rx_que = (ixgbedev1->rx_bufsz - ixgbedev1->rx_tail + rdh)
            % ixgbedev1->rx_bufsz;

        tx_avl = ixgbedev2->tx_bufsz[0]
            - ((ixgbedev2->tx_bufsz[0] - tdh + ixgbedev2->tx_tail[0])
               % ixgbedev2->tx_bufsz[0]);

        if ( rx_que > tx_avl ) {
            nrp = tx_avl;
        } else {
            nrp = rx_que;
        }
#if 0
        if ( nrp > 32 ) {
            nrp = 32;
        }
#endif
        if ( nrp <= 0 ) {
            continue;
        }
        /* Routing */
        for ( i = 0; i < nrp; i++ ) {
            rxdesc = (union ixgbe_adv_rx_desc *)
                (ixgbedev1->rx_base
                 + ((ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz)
                 * sizeof(union ixgbe_adv_rx_desc));
            txdesc = (struct ixgbe_adv_tx_desc_data *)
                (ixgbedev2->tx_base[0]
                 + ((ixgbedev2->tx_tail[0] + i) % ixgbedev2->tx_bufsz[0])
                 * sizeof(struct ixgbe_adv_tx_desc_data));

            //rxpkt = (u8 *)rxdesc->address;
            //rxpkt = (u8 *)rxdesc->pkt_addr;
            //rxpkt = (u8 *)ixgbedev1->rx_read[0][(ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz].pkt_addr;
            //txpkt = (u8 *)txdesc->address;

            txpkt = (u8 *)ixgbedev1->rx_read[0][(ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz].pkt_addr;
#if 0
            kprintf("WB: %x %x %x %x %x %x\r\n",
                    rxpkt,
                    rxdesc->wb.info0, rxdesc->wb.info1,
                    rxdesc->wb.staterr, rxdesc->wb.length, rxdesc->wb.vlan);
#endif
            /* Check */
#if 0
            kprintf("XX: %x %x %d.%d.%d.%d\r\n", rxdesc->special,
                    rxdesc->length, rxpkt[30], rxpkt[31], rxpkt[32], rxpkt[33]);
#endif
#if 0
            kprintf("XX: %x %x %x %x %x %x\r\n", rx_que, tx_avl, rdh,
                    ixgbedev1->rx_tail,
                    mmio_read32(ixgbedev1->mmio, IXGBE_REG_RDT(0)), tdh);
#endif

#if 0
            if ( rxpkt[30] != 10 || rxpkt[31] != 0
                 || rxpkt[32] != 0 || rxpkt[33] != 100 ) {
                txdesc->length = 0;
                txdesc->sta = 0;
                txdesc->special = 0;
                txdesc->css = 0;
                txdesc->cso = 0;
                txdesc->cmd = 0;
                continue;
            }
#endif
#if 1
            /* dst:  00:40:66:67:72:24  */
#if 0
            txpkt[0] = 0x00;
            txpkt[1] = 0x40;
            txpkt[2] = 0x66;
            txpkt[3] = 0x67;
            txpkt[4] = 0x72;
            txpkt[5] = 0x24;
#else
            *(u32 *)txpkt =  0x67664000LLU;
            *(u16 *)(txpkt + 4) =  0x2472LLU;
#endif
            /* src */
#if 0
            txpkt[6] = netdev2->macaddr[0];
            txpkt[7] = netdev2->macaddr[1];
            txpkt[8] = netdev2->macaddr[2];
            txpkt[9] = netdev2->macaddr[3];
            txpkt[10] = netdev2->macaddr[4];
            txpkt[11] = netdev2->macaddr[5];
#else
            *(u32 *)(txpkt + 6) =  ((u32 *)netdev2->macaddr);
            *(u16 *)(txpkt + 10) =  ((u16 *)(netdev2->macaddr + 4));
#endif
            //for ( j = 12; j < rxdesc->wb.length; j++ ) {
            //    txpkt[j] = rxpkt[j];
            //}
#endif

#if 0
#if 0
            u32 addr;
            addr = *(u32 *)(txpkt + 30);
            __asm__ ("bswapl %0" : "=r"(addr): "0"(addr));
            u64 mac = ptcam_lookup(tcam, ((u64)addr) << 32);
#else
            u64 addr;
            addr = (((u64)txpkt[30]<<56)) | (((u64)txpkt[31]<<48))
                | (((u64)txpkt[32]<<40)) | (((u64)txpkt[33]<<32));
            u64 mac = ptcam_lookup(tcam, addr);
#endif
            //kprintf("%llx %llx\r\n", addr, mac);
            u8 *nxmac = (u8 *)&mac;
            *(u32 *)txpkt = *(u32 *)nxmac;
            *(u16 *)(txpkt + 4) = *(u16 *)(nxmac + 4);
            //kmemcpy(txpkt, nxmac, 6);
            *(u32 *)(txpkt + 6) = *(u32 *)netdev2->macaddr;
            *(u16 *)(txpkt + 10) = *(u16 *)(netdev2->macaddr + 4);
            //kmemcpy(txpkt+6, netdev2->macaddr, 6);
#endif

            /* Save Tx */
            u8 *tmp = (u64)txdesc->pkt_addr & 0xfffffffffffffff0ULL;

#if 0
            /* TTL -= 1 */
            txpkt[22]--;
            /* Compute checksum */
            u16 *ctmp;
            u32 cs;
            txpkt[24] = 0x0;
            txpkt[25] = 0x0;
            ctmp = (u16 *)txpkt;
            cs = 0;
            for ( i = 7; i < 17; i++ ) {
                cs += (u32)ctmp[i];
                cs = (cs & 0xffff) + (cs >> 16);
            }
            cs = 0xffff - cs;
            txpkt[24] = cs & 0xff;
            txpkt[25] = cs >> 8;
#endif


            txdesc->pkt_addr = (u64)txpkt;
            txdesc->length = rxdesc->wb.length;
            txdesc->dtyp_mac = (3 << 4);
            txdesc->dcmd = (1<<5) | (1<<1) | 1;
            txdesc->paylen_popts_cc_idx_sta = (txdesc->length << 14);

            /* Reset RX desc */
            rxdesc->read.pkt_addr = tmp;
            ixgbedev1->rx_read[0][(ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz].pkt_addr = rxdesc->read.pkt_addr;
            rxdesc->read.hdr_addr = 0;//ixgbedev1->rx_read[0][(ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz].hdr_addr;
        }

        ixgbedev1->rx_tail = (ixgbedev1->rx_tail + nrp - 1) % ixgbedev1->rx_bufsz;
        ixgbedev2->tx_tail[0] = (ixgbedev2->tx_tail[0] + nrp) % ixgbedev2->tx_bufsz[0];
        mmio_write32(ixgbedev1->mmio, IXGBE_REG_RDT(0), ixgbedev1->rx_tail);
        mmio_write32(ixgbedev2->mmio, IXGBE_REG_TDT(0), ixgbedev2->tx_tail[0]);
        ixgbedev1->rx_tail = (ixgbedev1->rx_tail + 1) % ixgbedev1->rx_bufsz;
    }
#endif

    return 0;
}

















#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

static int
_100g_mgmt(struct ixgbe_device *dev, union ixgbe_adv_rx_desc *rxdesc,
           u8 *pkt, int len, int off)
{
    u8 *ip;
    u8 *udp;
    u8 *data;
    u32 prefix;
    int plen;
    int port;
    u64 mac;
    struct ixgbe_adv_tx_desc_data *txdesc;

    txdesc = (struct ixgbe_adv_tx_desc_data *)
        (dev->tx_base[0] + dev->tx_tail[0]
         * sizeof(struct ixgbe_adv_tx_desc_data));

    ip = pkt + off;
    udp = ip + (int)(ip[0] & 0xf) * 4;
    /* Check port 5000 */
    if ( udp[2] == 0x13 && udp[3] == 0x88 ) {
        data = udp + 8;

        if ( 1 == data[0] ) {
            prefix = ((u32)data[1] << 24) | ((u32)data[2] << 16)
                | ((u32)data[3] << 8) | ((u32)data[4]);
            plen = data[5];
            port = data[6];
            mac = ((u64)data[7] << 40) | ((u64)data[8] << 32)
                | ((u64)data[9] << 24) | ((u64)data[10] << 16)
                | ((u64)data[11] << 8) | ((u64)data[12]);
            kprintf("Inserting %x/%d\r\n", prefix, plen);
            dxr_route_add(dxr, prefix, plen, port + 1);
            kprintf("done\r\n");
        } else if ( 2 == data[0] ) {
            /* Compile FIB */
            kprintf("Compile FIB\r\n");
            dxr_commit(dxr);
        }

        u8 *pkt2 = txdesc->pkt_addr;
        kprintf("done 1.2 %x %x %d\r\n", pkt2, pkt, len);
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
        *(u64 *)(pkt2 + 42) = 0; /* ret */
        kmemset(pkt2 + 50, 0, 8);

        txdesc->length = 60;
        txdesc->dtyp_mac = (3 << 4);
        txdesc->dcmd = (1<<5) | (1<<1) | 1;
        txdesc->paylen_popts_cc_idx_sta = ((u64)60 << 14) | (1ULL << 8);
        dev->tx_tail[0] = (dev->tx_tail[0] + 1) & dev->tx_divisorm[0];
        mmio_write32(dev->mmio, IXGBE_REG_TDT(0), dev->tx_tail[0]);
        kprintf("done 2\r\n");
    }

    return 0;
}

u64 popcnt(u64);
static int
_100g_routing_ipv4(struct ixgbe_device **devs, struct ixgbe_device *dev, int q,
                   union ixgbe_adv_rx_desc *rxdesc, u32 rdt, u8 *pkt, int len,
                   int off)
{
    u32 dst;
    struct ixgbe_adv_tx_desc_data *txdesc;

    if ( unlikely(0x45 != pkt[off]) ) {
        kprintf("FIXME: header size is not normal");
        return -1;
    }

    if ( likely(rxdesc->wb.staterr & (1 << 6)) ) {
        /* Checksum is validated */
        if ( unlikely(rxdesc->wb.staterr & (1ULL << (20 + 11))) ) {
            /* Checksum error */
            kprintf("FIXME: Checksum error\r\n");
            return -1;
        }
    } else {
        kprintf("FIXME: Checksum offloading\r\n");
        return -1;
    }

    /* Get the destination address */
    dst = bswap32(*(u32 *)(pkt + off + 16));
#if 0
    if ( unlikely(0xc0a80004 == dst) ) {
        _100g_mgmt(dev, rxdesc, pkt, len, off);
        return 0;
    }
    u32 idx;
    idx = dxr_lookup(dxr, dst) - 1;
    if ( idx >= 10 ) {
        /* Drop */
        return 0;
    }
#else
    u32 idx;
    idx = dst % 8; //% 10;//popcnt(dst & 0x1ff);
    //idx = (q + 2) % 8;
    //idx = 0;
    //idx = q ^ 1;
    //idx = (q + 4) % 8;
    //idx = q;
#endif

#if 0
    u8 macaddr0[6] = {0x00, 0x01, 0x02, 0x00, 0x00, idx};
    u8 macaddr1[6] = {0x90, 0xe2, 0xba, 0x6a, 0x0c, 0x40};
    kmemcpy(pkt, macaddr0, 6);
    kmemcpy(pkt + 6, macaddr1, 6);

    /* TTL -= 1 */
    pkt[off + 8] -= 1;
    if ( unlikely(0 == pkt[off + 8]) ) {
        /* Time exceed; discards */
        kprintf("FIXME: TTL\r\n");
        return -1;
    }

    /* Reset checksum */
    *(u16 *)(pkt + off + 10) = 0x0;

    txdesc = (struct ixgbe_adv_tx_desc_data *)
        (dev->tx_base[q] + dev->tx_tail[q]
         * sizeof(struct ixgbe_adv_tx_desc_data));

    //kmemcpy(txdesc->pkt_addr, pkt, len);
    dev->rx_read[0][rdt].pkt_addr = txdesc->pkt_addr;
    txdesc->pkt_addr = pkt;
    txdesc->length = len;
    txdesc->dtyp_mac = (3 << 4);
    txdesc->dcmd = (1<<5) | (1<<1) | 1;
    txdesc->paylen_popts_cc_idx_sta = ((u64)len << 14) | (1ULL << 8);
    dev->tx_tail[q] = (dev->tx_tail[q] + 1) & dev->tx_divisorm[q];

    return 0;
#endif

#if 1
#if 1
    u32 tdh;
    tdh = devs[idx]->tx_head_cache[q];
    if ( devs[idx]->tx_bufsz[q] -
         ((devs[idx]->tx_bufsz[q] - tdh + devs[idx]->tx_tail[q])
          & devs[idx]->tx_divisorm[q])
         <= 1 ) {
        tdh = mmio_read32(devs[idx]->mmio, IXGBE_REG_TDH(q));
        devs[idx]->tx_head_cache[q] = tdh;
        if ( devs[idx]->tx_bufsz[q] -
             ((devs[idx]->tx_bufsz[q] - tdh + devs[idx]->tx_tail[q])
              & devs[idx]->tx_divisorm[q])
            <= 1) {
            /* Buffer full */
            return 0;
        }
    }
#endif

    txdesc = (struct ixgbe_adv_tx_desc_data *)
        (devs[idx]->tx_base[q] + devs[idx]->tx_tail[q]
         * sizeof(struct ixgbe_adv_tx_desc_data));

    u8 macaddr0[6] = {0x00, 0x01, 0x02, 0x00, 0x00, idx};
    u8 macaddr1[6] = {0x90, 0xe2, 0xba, 0x6a, 0x0c, 0x40};
    kmemcpy(pkt, macaddr0, 6);
    kmemcpy(pkt + 6, macaddr1, 6);

    /* TTL -= 1 */
    pkt[off + 8] -= 1;
    if ( unlikely(0 == pkt[off + 8]) ) {
        /* Time exceed; discards */
        kprintf("FIXME: TTL\r\n");
        return -1;
    }

    /* Reset checksum */
    *(u16 *)(pkt + off + 10) = 0x0;

    //kmemcpy(txdesc->pkt_addr, pkt, len);
    devs[idx]->rx_read[0][rdt].pkt_addr = txdesc->pkt_addr;
    txdesc->pkt_addr = (u64)pkt;
    txdesc->length = len;
    txdesc->dtyp_mac = (3 << 4);
    txdesc->dcmd = (1<<5) | (1<<1) | 1;
    txdesc->paylen_popts_cc_idx_sta = ((u64)len << 14) | (1ULL << 8);
    devs[idx]->tx_tail[q] = (devs[idx]->tx_tail[q] + 1) & devs[idx]->tx_divisorm[q];
#endif

    return 0;
}


static int
_100g_routing(struct ixgbe_device **devs, struct ixgbe_device *dev, int q)
{
    union ixgbe_adv_rx_desc *rxdesc;
    u8 *pkt;
    u16 etype;
    int rdt;
    int ret;
    int cnt;
    int i;
    /* Destination check */
    u8 macaddr[6] = {0x90, 0xe2, 0xba, 0x6a, 0x0c, 0x40};

    for ( i = 0; i < 32; i++ ) {
        rdt = (dev->rx_tail + i) & dev->rx_divisorm;
        rxdesc = (union ixgbe_adv_rx_desc *)
            (dev->rx_base + rdt * sizeof(union ixgbe_adv_rx_desc));
        if ( !(rxdesc->read.hdr_addr & 0x1) ) {
            /* Buffer is empty */
            break;
        }
        __asm__ ("prefetcht1 (%0)" :: "r"(dev->rx_read[0][rdt].pkt_addr));
    }
    cnt = i;

    if ( cnt > 0 ) {
        for ( i = 0; i < cnt; i++ ) {
            rxdesc = (union ixgbe_adv_rx_desc *)
                (dev->rx_base + dev->rx_tail * sizeof(union ixgbe_adv_rx_desc));
            rdt = dev->rx_tail;
            /* Buffer is not empty */
            pkt = (u8 *)dev->rx_read[0][dev->rx_tail].pkt_addr;
            if ( 0 != kmemcmp(pkt, macaddr, 6) ) {
                /* Drop */
                rxdesc->read.pkt_addr = dev->rx_read[0][rdt].pkt_addr;
                rxdesc->read.hdr_addr = 0;//dev->rx_read[0][rdt].hdr_addr;
                dev->rx_tail = (dev->rx_tail + 1) & dev->rx_divisorm;
                continue;
            }
            /* Ethertype check */
            etype = *(u16 *)(pkt + 12);
            switch ( etype ) {
            case 0x0008:
                /* IPv4: 0x0800 */
                ret = _100g_routing_ipv4(devs, dev, q, rxdesc, rdt, pkt,
                                         rxdesc->wb.length, 14);
                if ( ret < 0 ) {
                }
                break;
            case 0x0608:
                /* ARP: 0x0806 */
                break;
            case 0x0081:
                /* VLAN: 0x8100 */
                break;
            default:
                /* Other */
                ;
            }
            rxdesc->read.pkt_addr = dev->rx_read[0][rdt].pkt_addr;
            rxdesc->read.hdr_addr = 0;//dev->rx_read[0][rdt].hdr_addr;
            dev->rx_tail = (dev->rx_tail + 1) & dev->rx_divisorm;
        }
        mmio_write32(dev->mmio, IXGBE_REG_RDT(0), rdt);
        for ( i = 0; i < 8; i++ ) {
            mmio_write32(devs[i]->mmio, IXGBE_REG_TDT(q), devs[i]->tx_tail[q]);
        }
    }

#if 0
    rdt = -1;
    for ( cnt = 0; cnt < 32; cnt++ ) {
        rxdesc = (union ixgbe_adv_rx_desc *)
            (dev->rx_base + dev->rx_tail * sizeof(union ixgbe_adv_rx_desc));
        if ( !(rxdesc->read.hdr_addr & 0x1) ) {
            /* Buffer is empty */
            break;
        }
        rdt = dev->rx_tail;

        /* Buffer is not empty */
        pkt = (u8 *)dev->rx_read[0][dev->rx_tail].pkt_addr;
        __asm__ ("prefetcht0 (%0)" :: "r"(pkt));

        /* Destination check */
        u8 macaddr[6] = {0x90, 0xe2, 0xba, 0x6a, 0x0c, 0x40};
        if ( 0 != kmemcmp(pkt, macaddr, 6) ) {
            /* Drop */
            rxdesc->read.pkt_addr = dev->rx_read[0][rdt].pkt_addr;
            rxdesc->read.hdr_addr = 0;//dev->rx_read[0][rdt].hdr_addr;
            dev->rx_tail = (dev->rx_tail + 1) & dev->rx_divisorm;
            continue;
        }
        /* Ethertype check */
        etype = *(u16 *)(pkt + 12);
        switch ( etype ) {
        case 0x0008:
            /* IPv4: 0x0800 */
            ret = _100g_routing_ipv4(devs, dev, rxdesc, rdt, pkt,
                                     rxdesc->wb.length, 14);
            if ( ret < 0 ) {
            }
            break;
        case 0x0608:
            /* ARP: 0x0806 */
            break;
        case 0x0081:
            /* VLAN: 0x8100 */
            break;
        default:
            /* Other */
            ;
        }
        rxdesc->read.pkt_addr = dev->rx_read[0][rdt].pkt_addr;
        rxdesc->read.hdr_addr = 0;//dev->rx_read[0][rdt].hdr_addr;
        dev->rx_tail = (dev->rx_tail + 1) & dev->rx_divisorm;
    }

    //rxdesc->read.pkt_addr = dev1->rx_read[0][dev1->rx_tail].pkt_addr;
    //rxdesc->read.hdr_addr = dev1->rx_read[0][dev1->rx_tail].hdr_addr;

    /* Reset RX desc */
    //rxdesc->read.pkt_addr = dev1->rx_read[0][dev1->rx_tail].pkt_addr;
    //rxdesc->read.hdr_addr = dev1->rx_read[0][dev1->rx_tail].hdr_addr;

    if ( rdt >= 0 ) {
        mmio_write32(dev->mmio, IXGBE_REG_RDT(0), rdt);
        //kprintf("%x %x\r\n", dev, dev->tx_tail[0]);
        //mmio_write32(dev->mmio, IXGBE_REG_TDT(q), dev->tx_tail[0]);
    }
    //kprintf("%x %x\r\n", dev, dev->tx_tail[0]);
    for ( i = 0; i < 8; i++ ) {
        mmio_write32(devs[i]->mmio, IXGBE_REG_TDT(q), devs[i]->tx_tail[q]);
    }
    //dev->rx_tail = (dev->rx_tail + 1) & dev->rx_divisorm;
#endif

    return 0;
}


int
ixgbe_100g_routing(struct netdev_list *list, int q)
{
    struct ixgbe_device *dev[10];
    int i;
    struct netdev *netdev;
    int ret;

    /* Prepare 10 ports */
    for ( i = 0; i < 8; i++ ) {
        netdev = list->netdev;
        dev[i] = (struct ixgbe_device *)netdev->vendor;
#if 0
        kprintf("XXXX %s %x %x\r\n", netdev->name, dev[i]->pci_device->vendor_id,
                dev[i]->pci_device->device_id);
#endif
        list = list->next;

        /* Set up context */
        struct ixgbe_adv_tx_desc_ctx *ctx;
        ctx = (struct ixgbe_adv_tx_desc_ctx *)
            (dev[i]->tx_base[q]
             + ((dev[i]->tx_tail[q]) & dev[i]->tx_divisorm[q])
             * sizeof(struct ixgbe_tx_desc));
        ctx->vlan_maclen_iplen = (14ULL << 9) | 20;
        ctx->other = (1ULL << 29) | (2ULL << 20) | (2ULL << 9);
        dev[i]->tx_tail[q] = ((dev[i]->tx_tail[q] + 1) & dev[i]->tx_divisorm[q]);
        mmio_write32(dev[i]->mmio, IXGBE_REG_TDT(q), dev[i]->tx_tail[q]);
    }

    for ( ;; ) {
        ret = _100g_routing(dev, dev[q], q);
#if 0
        for ( i = 0; i < 1; i++ ) {
            /* Poll i-th port */
            ret = _100g_routing(dev, dev[i], 0);
        }
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
