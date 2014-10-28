/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../pci/pci.h"
#include "../net/netdev.h"
#include "../../kernel/kernel.h"

void pause(void);

#define PKTSZ   2048

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
    u32 paylen_ports_cc_idx_sta;
} __attribute__ ((packed));

struct ixgbe_adv_rx_desc_read {
    volatile u64 pkt_addr;                 /* 0:A0 */
    volatile u64 hdr_addr;                 /* 0:DD */
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
    u64 tx_base[2];
    u32 tx_tail[2];
    u32 tx_bufsz[2];
    u32 tx_divisorm[2];

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


int ixgbe_routing_test(struct netdev *);



static u16
_ip_checksum(const u8 *data, int len)
{
    /* Compute checksum */
    u16 *tmp;
    u32 cs;
    int i;

    tmp = (u16 *)data;
    cs = 0;
    for ( i = 0; i < len / 2; i++ ) {
        cs += (u32)tmp[i];
        cs = (cs & 0xffff) + (cs >> 16);
    }
    cs = 0xffff - cs;

    return cs;
}

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
    struct ixgbe_device *ixgbedev;
    int idx;

    /* Get the list of PCI devices */
    pci = pci_list();

    /* Search NICs requiring this ixgbe driver */
    idx = 0;
    while ( NULL != pci ) {
        if ( PCI_VENDOR_INTEL == pci->device->vendor_id ) {
            switch ( pci->device->device_id ) {
            case IXGBE_X520:
                ixgbedev = ixgbe_init_hw(pci->device);
                netdev_add_device(ixgbedev->macaddr, ixgbedev);
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
    dev->rx_bufsz = (1<<13);
    dev->rx_divisorm = (1<<13) - 1;
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
        rxdesc->read.hdr_addr = (u64)kmalloc(4096);

        dev->rx_read[0][i].pkt_addr = rxdesc->read.pkt_addr;
        dev->rx_read[0][i].hdr_addr = rxdesc->read.hdr_addr;
    }

    mmio_write32(dev->mmio, IXGBE_REG_RDBAH(0), dev->rx_base >> 32);
    mmio_write32(dev->mmio, IXGBE_REG_RDBAL(0), dev->rx_base & 0xffffffff);
    mmio_write32(dev->mmio, IXGBE_REG_RDLEN(0),
                 dev->rx_bufsz * sizeof(union ixgbe_adv_rx_desc));

    mmio_write32(dev->mmio, IXGBE_REG_SRRCTL0,
                 IXGBE_SRRCTL_BSIZE_PKT16K | IXGBE_SRRCTL_BSIZE_HDR256
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

    dev->tx_tail[0] = 0;
    /* up to 64 K minus 8 */
    dev->tx_bufsz[0] = (1<<14);
    dev->tx_divisorm[0] = (1<<14) - 1;
    /* Cache */
    dev->tx_head_cache[0] = 0;

    /* ToDo: 16 bytes for alignment */
    dev->tx_base[0] = (u64)kmalloc(dev->tx_bufsz[0]
                                   * sizeof(struct ixgbe_adv_tx_desc_data));
    for ( i = 0; i < dev->tx_bufsz[0]; i++ ) {
        txdesc = (struct ixgbe_adv_tx_desc_data *)(dev->tx_base[0] + i
               * sizeof(struct ixgbe_adv_tx_desc_data));
        txdesc->pkt_addr = (u64)kmalloc(PKTSZ);
        txdesc->length = 0;
        txdesc->dtyp_mac = (3 << 4);
        txdesc->dcmd = 0;
        txdesc->paylen_ports_cc_idx_sta = 0;
    }
    mmio_write32(dev->mmio, IXGBE_REG_TDBAH(0), dev->tx_base[0] >> 32);
    mmio_write32(dev->mmio, IXGBE_REG_TDBAL(0), dev->tx_base[0] & 0xffffffff);
    mmio_write32(dev->mmio, IXGBE_REG_TDLEN(0),
                 dev->tx_bufsz[0] * sizeof(struct ixgbe_tx_desc));
    mmio_write32(dev->mmio, IXGBE_REG_TDH(0), 0);
    mmio_write32(dev->mmio, IXGBE_REG_TDT(0), 0);

    /* Write-back */
#if 0
    u32 *tdwba = kmalloc(4096);
    mmio_write32(dev->mmio, IXGBE_REG_TDWBAL(0),
                 ((u64)tdwba & 0xfffffffc) | 1);
    mmio_write32(dev->mmio, IXGBE_REG_TDWBAH(0), ((u64)tdwba) >> 32);
    dev->tx_head = tdwba;
#endif

    /* Enable */
    mmio_write32(dev->mmio, IXGBE_REG_DMATXCTL,
                 IXGBE_DMATXCTL_TE | IXGBE_DMATXCTL_VT);

    /* First */
    mmio_write32(dev->mmio, IXGBE_REG_TXDCTL(0), IXGBE_TXDCTL_ENABLE);
#if 1
    mmio_write32(dev->mmio, IXGBE_REG_TXDCTL(0), IXGBE_TXDCTL_ENABLE
                 | (64<<16) /* WTHRESH */
                 | (16<<8) /* HTHRESH */| (16) /* PTHRESH */);
#endif
    for ( i = 0; i < 10; i++ ) {
        arch_busy_usleep(1);
        m32 = mmio_read32(dev->mmio, IXGBE_REG_TXDCTL(0));
        if ( m32 & IXGBE_TXDCTL_ENABLE ) {
            break;
        }
    }
    if ( !(m32 & IXGBE_TXDCTL_ENABLE) ) {
        kprintf("Error on enable a TX queue\r\n");
    }

    return 0;
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
    int j;
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


/*
 * Get a valid TX buffer
 */
int
ixgbe_tx_buf(struct netdev *netdev, u8 **txpkt, u16 **txlen, u16 vlan)
{
    struct ixgbe_device *ixgbedev;
    struct ixgbe_tx_desc *txdesc;
    int tx_avl;

    /* Retrieve data structure of ixgbe driver */
    ixgbedev = (struct ixgbe_device *)netdev->vendor;

    /* Get available TX buffer length */
    tx_avl = ixgbedev->tx_bufsz[0]
        - ((ixgbedev->tx_bufsz[0] - ixgbedev->tx_head_cache[0] + ixgbedev->tx_tail[0])
           % ixgbedev->tx_bufsz[0]);
    if ( tx_avl <= 0 ) {
        return -1;
    }

    /* Check the head of TX ring buffer */
    txdesc = (struct ixgbe_tx_desc *)
        (ixgbedev->tx_base[0]
         + (ixgbedev->tx_tail[0] % ixgbedev->tx_bufsz[0])
         * sizeof(struct ixgbe_tx_desc));

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
    ixgbedev->tx_tail[0] = (ixgbedev->tx_tail[0] + 1) % ixgbedev->tx_bufsz[0];

    return 0;
}
int
ixgbe_tx_set(struct netdev *netdev, u64 txpkt, u16 txlen, u16 vlan)
{
    struct ixgbe_device *ixgbedev;
    struct ixgbe_tx_desc *txdesc;
    int tx_avl;
    int ret;

    /* Retrieve data structure of ixgbe driver */
    ixgbedev = (struct ixgbe_device *)netdev->vendor;

    /* Get available TX buffer length */
    tx_avl = ixgbedev->tx_bufsz[0]
        - ((ixgbedev->tx_bufsz[0] - ixgbedev->tx_head_cache[0] + ixgbedev->tx_tail[0])
           % ixgbedev->tx_bufsz[0]);
    if ( tx_avl <= 0 ) {
        return -1;
    }

    /* Check the head of TX ring buffer */
    ret = ixgbedev->tx_tail[0];
    txdesc = (struct ixgbe_tx_desc *)
        (ixgbedev->tx_base[0]
         + (ixgbedev->tx_tail[0] % ixgbedev->tx_bufsz[0])
         * sizeof(struct ixgbe_tx_desc));

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
    ixgbedev->tx_tail[0] = (ixgbedev->tx_tail[0] + 1) % ixgbedev->tx_bufsz[0];

    return ret;
}
int
ixgbe_tx_commit(struct netdev *netdev)
{
    struct ixgbe_device *ixgbedev;

    /* Retrieve data structure of ixgbe driver */
    ixgbedev = (struct ixgbe_device *)netdev->vendor;

    /* Write to PCI */
    mmio_write32(ixgbedev->mmio, IXGBE_REG_TDT(0), ixgbedev->tx_tail[0]);

    u32 tdh;
    tdh = mmio_read32(ixgbedev->mmio, IXGBE_REG_TDH(0));
    ixgbedev->tx_head_cache[0] = tdh;

    return 0;
}

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
                txdesc->paylen_ports_cc_idx_sta = (len << 14);
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
            txdesc->paylen_ports_cc_idx_sta = (len << 14);
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

            /* Save Tx */
            u8 *tmp = (u8 *)((u64)txdesc->pkt_addr & 0xfffffffffffffff0ULL);

            txdesc->pkt_addr = (u64)txpkt;
            txdesc->length = rxdesc->wb.length;
            txdesc->dtyp_mac = (3 << 4);
            txdesc->dcmd = (1<<5) | (1<<1) | 1;
            txdesc->paylen_ports_cc_idx_sta = ((u32)rxdesc->wb.length << 14);

            dev1->rx_read[0][dev1->rx_tail].pkt_addr = (u64)tmp;
            rxdesc->read.pkt_addr = dev1->rx_read[0][dev1->rx_tail].pkt_addr;
            rxdesc->read.hdr_addr = dev1->rx_read[0][dev1->rx_tail].hdr_addr;

            /* Reset RX desc */
            //rxdesc->read.pkt_addr = dev1->rx_read[0][dev1->rx_tail].pkt_addr;
            //rxdesc->read.hdr_addr = dev1->rx_read[0][dev1->rx_tail].hdr_addr;

            dev2->tx_tail[0] = (dev2->tx_tail[0] + 1) & dev2->tx_divisorm[0];
            if ( (dev2->tx_tail[0] & 0x7ff) == 0 ) {
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
            //txdesc->paylen_ports_cc_idx_sta = ((u32)rxdesc->wb.length << 14);

            //rxdesc->read.pkt_addr = (u64)txpkt;
            //dev1->rx_read[0][idx].pkt_addr = (u64)rxdesc->read.pkt_addr;
            //rxdesc->read.hdr_addr = dev1->rx_read[0][idx].hdr_addr;

            /* Reset RX desc */
            rxdesc->read.pkt_addr = dev1->rx_read[0][idx].pkt_addr;
            rxdesc->read.hdr_addr = dev1->rx_read[0][idx].hdr_addr;
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
            txdesc->paylen_ports_cc_idx_sta = ((u32)rxdesc->wb.length << 14);

            /* Reset RX desc */
            rxdesc->read.pkt_addr = (u64)tmp;
            dev1->rx_read[0][rxidx].pkt_addr = rxdesc->read.pkt_addr;
            rxdesc->read.hdr_addr = dev1->rx_read[0][rxidx].hdr_addr;
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
            txdesc->paylen_ports_cc_idx_sta = (txdesc->length << 14);

            /* Reset RX desc */
            rxdesc->read.pkt_addr = tmp;
            ixgbedev1->rx_read[0][(ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz].pkt_addr = rxdesc->read.pkt_addr;
            rxdesc->read.hdr_addr = ixgbedev1->rx_read[0][(ixgbedev1->rx_tail + i) % ixgbedev1->rx_bufsz].hdr_addr;
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
