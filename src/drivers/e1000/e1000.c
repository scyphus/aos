/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "../pci/pci.h"
#include "../net/netdev.h"
#include "../../kernel/kernel.h"

void pause(void);

#define E1000_REG_CTRL  0x00
#define E1000_REG_STATUS 0x08
#define E1000_REG_EEC   0x10
#define E1000_REG_CTRL_EXT 0x18
#define E1000_REG_EERD  0x14
#define E1000_REG_MDIC  0x20
#define E1000_REG_ICR   0x00c0
#define E1000_REG_ICS   0x00c8
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

    void *rx_mem_base;
    u64 rx_base;
    u32 rx_tail;
    u32 rx_bufsz;
    void *tx_mem_base;
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
struct e1000_device * e1000_init_hw(struct pci_device *);
void e1000_update_hw(void);
struct e1000_device * e1000_init_hw(struct pci_device *);
int e1000_setup_rx_desc(struct e1000_device *);
int e1000_setup_tx_desc(struct e1000_device *);
void e1000_irq_handler(int, void *);
int e1000_recvpkt(u8 *, u32, struct netdev *);
int e1000_sendpkt(const u8 *, u32, struct netdev *);

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
    /* Initialize the driver */
    e1000_update_hw();
}

void
e1000_update_hw(void)
{
    struct pci *pci;
    struct e1000_device *e1000dev;
    struct netdev *netdev;
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
                e1000dev = e1000_init_hw(pci->device);
                netdev = netdev_add_device(e1000dev->macaddr, e1000dev);
                netdev->recvpkt = e1000_recvpkt;
                netdev->sendpkt = e1000_sendpkt;
                idx++;
                break;
            default:
                ;
            }
        }
        pci = pci->next;
    }
}

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
 * IRQ handler
 */
void
e1000_irq_handler(int irq, void *user)
{
    struct e1000_device *dev;
    u16 isr;

    dev = (struct e1000_device *)user;
    /* Read and clear */
    isr = mmio_read32(dev->mmio, E1000_REG_ICR);

    if ( isr & 0x80 ) {
        /* Packet received */
    }
    //kprintf("XXXX %x\r\n", mmio_read32(dev->mmio, E1000_REG_ICR));
}

struct e1000_device *
e1000_init_hw(struct pci_device *pcidev)
{
    struct e1000_device *dev;
    u16 m16;
    u32 m32;
    int i;

    dev = kmalloc(sizeof(struct e1000_device));

    /* Assert */
    if ( 0x8086 != pcidev->vendor_id ) {
        kfree(dev);
        return NULL;
    }

    /* Read MMIO */
    dev->mmio = pci_read_mmio(pcidev->bus, pcidev->slot, pcidev->func);
    if ( 0 == dev->mmio ) {
        kfree(dev);
        return NULL;
    }

    /* Initialize */
    mmio_write32(dev->mmio, E1000_REG_IMC, 0xffffffff);
    mmio_write32(dev->mmio, E1000_REG_CTRL,
                 mmio_read32(dev->mmio, E1000_REG_CTRL)
                 | E1000_CTRL_RST);
    arch_busy_usleep(100);
    mmio_write32(dev->mmio, E1000_REG_CTRL,
                 mmio_read32(dev->mmio, E1000_REG_CTRL)
                 | E1000_CTRL_SLU);
    mmio_write32(dev->mmio, E1000_REG_CTRL_EXT,
                 mmio_read32(dev->mmio, E1000_REG_CTRL_EXT)
                 & ~E1000_CTRL_EXT_LINK_MODE_MASK);
#if 0
    mmio_write32(dev->mmio, E1000_REG_TXDCTL,
                 E1000_TXDCTL_GRAN_DESC
                 | (128 << E1000_TXDCTL_HTHRESH_SHIFT)
                 | (8 << E1000_TXDCTL_PTHRESH_SHIFT));
#endif

    switch ( pcidev->device_id ) {
    case E1000_PRO1000MT:
    case E1000_82545EM:
        /* Read MAC address */
        m16 = e1000_eeprom_read_8254x(dev->mmio, 0);
        dev->macaddr[0] = m16 & 0xff;
        dev->macaddr[1] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read_8254x(dev->mmio, 1);
        dev->macaddr[2] = m16 & 0xff;
        dev->macaddr[3] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read_8254x(dev->mmio, 2);
        dev->macaddr[4] = m16 & 0xff;
        dev->macaddr[5] = (m16 >> 8) & 0xff;
        break;

    case E1000_82541PI:
    case E1000_82573L:
        /* Read MAC address */
        m16 = e1000_eeprom_read(dev->mmio, 0);
        dev->macaddr[0] = m16 & 0xff;
        dev->macaddr[1] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read(dev->mmio, 1);
        dev->macaddr[2] = m16 & 0xff;
        dev->macaddr[3] = (m16 >> 8) & 0xff;
        m16 = e1000_eeprom_read(dev->mmio, 2);
        dev->macaddr[4] = m16 & 0xff;
        dev->macaddr[5] = (m16 >> 8) & 0xff;
        break;

    case E1000_82567LM:
    case E1000_82577LM:
    case E1000_82579LM:
        /* Read MAC address */
        m32 = mmio_read32(dev->mmio, E1000_REG_RAL);
        dev->macaddr[0] = m32 & 0xff;
        dev->macaddr[1] = (m32 >> 8) & 0xff;
        dev->macaddr[2] = (m32 >> 16) & 0xff;
        dev->macaddr[3] = (m32 >> 24) & 0xff;
        m32 = mmio_read32(dev->mmio, E1000_REG_RAH);
        dev->macaddr[4] = m32 & 0xff;
        dev->macaddr[5] = (m32 >> 8) & 0xff;
        break;
    }

    /* Link up */
    mmio_write32(dev->mmio, E1000_REG_CTRL,
                 mmio_read32(dev->mmio, E1000_REG_CTRL)
                 | E1000_CTRL_SLU | E1000_CTRL_VME);

    /* Multicast array table */
    for ( i = 0; i < 128; i++ ) {
        mmio_write32(dev->mmio, E1000_REG_MTA + i * 4, 0);
    }

    /* Start TX/RX */
    e1000_setup_rx_desc(dev);
    e1000_setup_tx_desc(dev);

    /* Store the parent device information */
    dev->pci_device = pcidev;

#if 0
    /* Enable interrupt (REG_IMS <- 0x1F6DC, then read REG_ICR ) */
    mmio_write32(dev->mmio, E1000_REG_IMS, 0x908e);
    (void)mmio_read32(dev->mmio, E1000_REG_ICR);
    /* Register IRQ handler */
    register_irq_handler((((pcidev->intr_pin -1) + pcidev->slot) % 4) + 16,
                         &e1000_irq_handler, dev);
#if 0
    kprintf("PCI: %x %x %x %x %x\r\n", pcidev->intr_pin, pcidev->intr_line,
            (((pcidev->intr_pin -1) + pcidev->slot) % 4) + 1,
            mmio_read32(dev->mmio, E1000_REG_IMS),
            mmio_read32(dev->mmio, E1000_REG_ICR));
#endif
    /* http://msdn.microsoft.com/en-us/library/windows/hardware/ff538017(v=vs.85).aspx */
    //mmio_write32(dev->mmio, E1000_REG_ICS, 0x908e);
#endif

#if 0
    for ( i = 0; i < 3000; i++ ) {
        u32 h = mmio_read32(dev->mmio, E1000_REG_ICR);
        if ( h ) {
            kprintf("*** %x\r\n", h);
        }
        arch_busy_usleep(1000);
    }
#endif

    return dev;
}

/*
 * Setup RX descriptor
 */
int
e1000_setup_rx_desc(struct e1000_device *dev)
{
    struct e1000_rx_desc *rxdesc;
    int i;

    dev->rx_tail = 0;
    dev->rx_bufsz = 768;

    /* Cache */
    dev->rx_head_cache = 0;
    dev->tx_head_cache = 0;

    /* ToDo: 16 bytes for alignment */
    dev->rx_mem_base = kmalloc(dev->rx_bufsz * sizeof(struct e1000_rx_desc) + 16);
    if ( 0 == dev->rx_mem_base ) {
        kfree(dev);
        return NULL;
    }
    dev->rx_base = ((u64)dev->rx_mem_base + 0xf) & ~(u64)0xf;
    for ( i = 0; i < dev->rx_bufsz; i++ ) {
        rxdesc = (struct e1000_rx_desc *)(dev->rx_base
                                          + i * sizeof(struct e1000_rx_desc));
        rxdesc->address = (u64)kmalloc(8192 + 16);
        /* FIXME: Memory check */
        rxdesc->checksum = 0;
        rxdesc->status = 0;
        rxdesc->errors = 0;
        rxdesc->special = 0;
    }
    mmio_write32(dev->mmio, E1000_REG_RDBAH, dev->rx_base >> 32);
    mmio_write32(dev->mmio, E1000_REG_RDBAL, dev->rx_base & 0xffffffff);
    mmio_write32(dev->mmio, E1000_REG_RDLEN,
                 dev->rx_bufsz * sizeof(struct e1000_rx_desc));
    mmio_write32(dev->mmio, E1000_REG_RDH, 0);
    /* RDT must be larger than 0 for the initial value to receive the first
       packet but I don't know why */
    mmio_write32(dev->mmio, E1000_REG_RDT, dev->rx_bufsz - 1);
    mmio_write32(dev->mmio, E1000_REG_RCTL,
                 E1000_RCTL_SBP | E1000_RCTL_UPE
                 | E1000_RCTL_MPE | E1000_RCTL_LPE | E1000_RCTL_BAM
                 | E1000_RCTL_BSIZE_8192 | E1000_RCTL_SECRC);
    /* Enable */
    mmio_write32(dev->mmio, E1000_REG_RCTL,
                 mmio_read32(dev->mmio, E1000_REG_RCTL) | E1000_RCTL_EN);

    return 0;
}

/*
 * Setup TX descriptor
 */
int
e1000_setup_tx_desc(struct e1000_device *dev)
{
    struct e1000_tx_desc *txdesc;
    int i;

    dev->tx_tail = 0;
    dev->tx_bufsz = 768;

    /* ToDo: 16 bytes for alignment */
    dev->tx_base = (u64)kmalloc(dev->tx_bufsz
                                   * sizeof(struct e1000_tx_desc) + 16);
    for ( i = 0; i < dev->tx_bufsz; i++ ) {
        txdesc = (struct e1000_tx_desc *)(dev->tx_base
                                          + i * sizeof(struct e1000_tx_desc));
        txdesc->address = (u64)kmalloc(8192 + 16);
        txdesc->cmd = 0;
        txdesc->sta = 0;
        txdesc->cso = 0;
        txdesc->css = 0;
        txdesc->special = 0;
    }
    mmio_write32(dev->mmio, E1000_REG_TDBAH, dev->tx_base >> 32);
    mmio_write32(dev->mmio, E1000_REG_TDBAL, dev->tx_base & 0xffffffff);
    mmio_write32(dev->mmio, E1000_REG_TDLEN,
                 dev->tx_bufsz * sizeof(struct e1000_tx_desc));
    mmio_write32(dev->mmio, E1000_REG_TDH, 0);
    mmio_write32(dev->mmio, E1000_REG_TDT, 0);
    mmio_write32(dev->mmio, E1000_REG_TCTL,
                 E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_MULR);

    return 0;
}


int
e1000_recvpkt(u8 *pkt, u32 len, struct netdev *netdev)
{
    u32 rdh;
    struct e1000_device *dev;
    int rx_que;
    struct e1000_rx_desc *rxdesc;
    int ret;

    dev = (struct e1000_device *)netdev->vendor;
    rdh = mmio_read32(dev->mmio, E1000_REG_RDH);

    rx_que = (dev->rx_bufsz - dev->rx_tail + rdh) % dev->rx_bufsz;
    if ( rx_que > 0 ) {
        /* Check the head of RX ring buffer */
        rxdesc = (struct e1000_rx_desc *)
            (dev->rx_base + (dev->rx_tail % dev->rx_bufsz)
             * sizeof(struct e1000_rx_desc));
        ret = len < rxdesc->length ? len : rxdesc->length;
        kmemcpy(pkt, (void *)rxdesc->address, ret);

        mmio_write32(dev->mmio, E1000_REG_RDT, dev->rx_tail);
        dev->rx_tail = (dev->rx_tail + 1) % dev->rx_bufsz;

        return ret;
    }

    return -1;
}

int
e1000_sendpkt(const u8 *pkt, u32 len, struct netdev *netdev)
{
    u32 tdh;
    struct e1000_device *dev;
    int tx_avl;
    struct e1000_tx_desc *txdesc;

    dev = (struct e1000_device *)netdev->vendor;
    tdh = mmio_read32(dev->mmio, E1000_REG_TDH);

    tx_avl = dev->tx_bufsz - ((dev->tx_bufsz - tdh + dev->tx_tail)
                              % dev->tx_bufsz);

    if ( tx_avl > 0 ) {
        /* Check the head of TX ring buffer */
        txdesc = (struct e1000_tx_desc *)
            (dev->tx_base + (dev->tx_tail % dev->tx_bufsz)
             * sizeof(struct e1000_tx_desc));
        kmemcpy((void *)txdesc->address, pkt, len);
        txdesc->length = len;
        txdesc->sta = 0;
        txdesc->css = 0;
        txdesc->cso = 0;
        txdesc->special = 0;
        txdesc->cmd = (1<<3) | (1<<1) | 1;

        dev->tx_tail = (dev->tx_tail + 1) % dev->tx_bufsz;
        mmio_write32(dev->mmio, E1000_REG_TDT, dev->tx_tail);

        return len;
    }

    return -1;
}


typedef int (*router_rx_cb_t)(const u8 *, u32, int);

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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
