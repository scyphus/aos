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


#define PCI_VENDOR_INTEL        0x8086

#define I40E_XL710_QDA1         0x1584
#define I40E_XL710_QDA2         0x1583

#define I40E_PFLAN_QALLOC       0x001c0400 /* RO */
#define I40E_GLLAN_TXPRE_QDIS(n)                \
    (0x000e6500 + 0x4 * (n))
#define I40E_QTX_ENA(q)         (0x00100000 + 0x4 * (q))
#define I40E_QTX_CTL(q)         (0x00104000 + 0x4 * (q))
#define I40E_QTX_TAIL(q)        (0x00108000 + 0x4 * (q))

#define I40E_GLHMC_LANTXBASE(n) (0x000c6200 + 0x4 * (n)) /* [0:23] */
#define I40E_GLHMC_LANTXCNT(n)  (0x000c6300 + 0x4 * (n)) /* [0:10] */
#define I40E_GLHMC_LANRXBASE(n) (0x000c6400 + 0x4 * (n)) /* [0:23] */
#define I40E_GLHMC_LANRXCNT(n)  (0x000c6500 + 0x4 * (n)) /* [0:10] */

#define I40E_GLHMC_LANTXOBJSZ   0x000c2004 /* [0:3] RO */
#define I40E_GLHMC_LANRXOBJSZ   0x000c200c /* [0:3] RO */


#define I40E_PFHMC_SDCMD        0x000c0000
#define I40E_PFHMC_SDDATALOW    0x000c0100
#define I40E_PFHMC_SDDATAHIGH   0x000c0200
#define I40E_PFHMC_PDINV        0x000c0300
#define I40E_GLHMC_SDPART(n)    (0x000c0800 + 0x04 * (n)) /* RO */


#define I40E_PRTPM_SAL(n)       (0x001e4440 + 0x20 * (n)) /* RO */
#define I40E_PRTPM_SAH(n)       (0x001e4440 + 0x20 * (n)) /* RO */

//PFHMC_SDCMD
//PFHMC_SDDATALOW
//PFHMC_SDDATAHIGH
//GLHMC_SDPART[n]

struct i40e_tx_desc_data {
    u64 pkt_addr;
    u16 rsv_cmd_dtyp;            /* rsv:2 cmd:10 dtyp:4 */
    u32 txbufsz_offset;         /* Tx buffer size:14 Offset:18*/
    u16 l2tag;
} __attribute__ ((packed));
/* dtyp = 0 ==> 0xF when written back */

struct i40e_tx_desc_ctx {
    u32 tunnel;                 /* rsv:8 Tunneling Parameters:24 */
    u16 l2tag;                  /* L2TAG2  (STag /VEXT) */
    u16 rsv;
    u64 mss_tsolen_cmd_dtyp;
} __attribute__ ((packed));


struct i40e_rx_desc_read {
    volatile u64 pkt_addr;
    volatile u64 hdr_addr;      /* 0:DD */
} __attribute__ ((packed));

struct i40e_rx_desc_wb {
    /* Write back */
    u32 filter_stat;
    u32 l2tag_mirr_fcoe_ctx;
    u64 len_ptype_err_status;
} __attribute__ ((packed));
union i40e_rx_desc {
    struct i40e_rx_desc_read read;
    struct i40e_rx_desc_wb wb;
} __attribute__ ((packed));


struct i40e_device {
    u64 mmio;

    u64 rx_base;
    u32 rx_tail;
    u32 rx_bufsz;
    u32 rx_bufmask;

    u64 tx_base;
    u32 tx_tail;
    u32 tx_bufsz;
    u32 tx_bufmask;

    struct i40e_rx_desc_read *rx_read;

    u8 macaddr[6];

    struct pci_device *pci_device;
};



struct i40e_lan_rxq_ctx {
    /* 0-31 */
    u32 rsv1:11;
    u32 cpuid:8;
    u32 head:13;
    /* 32-95 */
    u64 qlen_l:7;
    u64 base:57;
    /* 96-127 */
    u32 showiv:1;
    u32 rsv2:1;
    u32 hsplit_1:2;
    u32 hsplit_0:4;
    u32 l2tsel:1;
    u32 fcena:1;
    u32 crcstrip:1;
    u32 dsize:1;
    u32 dtype:2;
    u32 hbuff:5;
    u32 dbuff:7;
    u32 qlen_h:6;
    /* 128-191 */
    u64 rsv4l:4;
    u64 rxmax:14;
    u64 rsv3:46;
    /* 192- */
    u64 rsv6:55;
    u64 lrxqtresh:3;
    u64 rsv5:1;
    u64 tphhead:1;
    u64 tphdata:1;
    u64 tphwdesc:1;
    u64 tphrdesc:1;
    u64 rsv4h:1;
} __attribute__ ((packed));

struct i40e_lan_txq_ctx {
    /* Line 0.0 */
    u32 rsv2:1;
    u32 newctx:1;
    u32 rsv1:17;
    u32 head:13;
    /* Line 0.1 */
    u64 rsv3:3;
    u64 alt_vlan:1;
    u64 fdena:1;
    u64 tsynena:1;
    u64 fcena:1;
    u64 base:57;
    /* Line 0.2 */
    u32 rsv4:24;
    u32 cpuid:8;
    /* Line 1.0 */
    u32 rsv5:19;
    u32 thead_wb:13;
    /* Line 1.1 */
    u32 rsv6:15;
    u32 tphwdesc:1;
    u32 tphrpacket:1;
    u32 tphrdesc:1;
    u32 qlen:13;
    u32 head_wben:1;
    /* Line 1.2 */
    u64 head_wbaddr;
    /* Line 2 */
    u64 rsv7[2];
    /* Line 3 */
    u64 rsv8[2];
    /* Line 4 */
    u64 rsv9[2];
    /* Line 5 */
    u64 rsv10[2];
    /* Line 6 */
    u64 rsv11[2];
    /* Line 7.0 */
    u64 rsv12;
    /* Line 7.1 */
    u64 rsv14:34;
    u64 rdylist:10;
    u64 rsv13:20;
} __attribute__ ((packed));




/* Prototype declarations */
void i40e_update_hw(void);
struct i40e_device * i40e_init_hw(struct pci_device *);
int i40e_setup_rx_desc(struct i40e_device *);
int i40e_setup_tx_desc(struct i40e_device *);
int i40e_init_fpm(struct i40e_device *);

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
i40e_init(void)
{
    /* Initialize the driver */
    i40e_update_hw();
}

/*
 * Update hardware
 */
void
i40e_update_hw(void)
{
    struct pci *pci;
    struct i40e_device *dev;
    int idx;

    /* Get the list of PCI devices */
    pci = pci_list();

    /* Search NICs requiring this ixgbe driver */
    idx = 0;
    while ( NULL != pci ) {
        if ( PCI_VENDOR_INTEL == pci->device->vendor_id ) {
            switch ( pci->device->device_id ) {
            case I40E_XL710_QDA1:
            case I40E_XL710_QDA2:
                dev = i40e_init_hw(pci->device);
                netdev_add_device(dev->macaddr, dev);
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
struct i40e_device *
i40e_init_hw(struct pci_device *pcidev)
{
    struct i40e_device *dev;
    u32 m32;
    int i;

    dev = kmalloc(sizeof(struct i40e_device));

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


#if 1
    kprintf("ROM BAR: %x\r\n",
            pci_read_rom_bar(pcidev->bus, pcidev->slot, pcidev->func));
#endif

    /* Read MAC address */
    m32 = mmio_read32(dev->mmio, I40E_PRTPM_SAL(0));
    dev->macaddr[0] = m32 & 0xff;
    dev->macaddr[1] = (m32 >> 8) & 0xff;
    dev->macaddr[2] = (m32 >> 16) & 0xff;
    dev->macaddr[3] = (m32 >> 24) & 0xff;
    m32 = mmio_read32(dev->mmio, I40E_PRTPM_SAH(0));
    dev->macaddr[4] = m32 & 0xff;
    dev->macaddr[5] = (m32 >> 8) & 0xff;

    dev->pci_device = pcidev;

    i40e_init_fpm(dev);

    return dev;
}



int
i40e_init_fpm(struct i40e_device *dev)
{
    u16 func;
    u32 qalloc;
    int i;

    /* Get function number to determine the HMC function index */
    func = dev->pci_device->func;

    /* Read PFLAN_QALLOC register to find the base queue index and # of queues
       associated with the PF */
    qalloc = mmio_read32(dev->mmio, I40E_PFLAN_QALLOC);

    kprintf("QALLOC: %x\r\n", qalloc);

    kprintf("LANTXOBJSZ: %x\r\n", mmio_read32(dev->mmio, I40E_GLHMC_LANTXOBJSZ));
    kprintf("LANRXOBJSZ: %x\r\n", mmio_read32(dev->mmio, I40E_GLHMC_LANRXOBJSZ));

    kprintf("LANTXBASE(0): %x %x\r\n",
            mmio_read32(dev->mmio, I40E_GLHMC_LANTXBASE(0)),
            mmio_read32(dev->mmio, I40E_GLHMC_LANTXCNT(0)));
    kprintf("LANTXBASE(1): %x %x\r\n",
            mmio_read32(dev->mmio, I40E_GLHMC_LANTXBASE(1)),
            mmio_read32(dev->mmio, I40E_GLHMC_LANTXCNT(1)));

    kprintf("LANRXBASE(0): %x %x\r\n",
            mmio_read32(dev->mmio, I40E_GLHMC_LANRXBASE(0)),
            mmio_read32(dev->mmio, I40E_GLHMC_LANRXCNT(0)));
    kprintf("LANRXBASE(1): %x %x\r\n",
            mmio_read32(dev->mmio, I40E_GLHMC_LANRXBASE(1)),
            mmio_read32(dev->mmio, I40E_GLHMC_LANRXCNT(1)));


    /* Tx descriptor */
    struct i40e_tx_desc_data *txdesc;
    dev->tx_tail = 0;
    /* up to 64 K minus 8 */
    dev->tx_bufsz = (1<<13);
    dev->tx_bufmask = (1<<13) - 1;
    /* ToDo: 16 bytes for alignment */
    dev->tx_base = (u64)kmalloc(dev->tx_bufsz * sizeof(struct i40e_tx_desc_data));
    for ( i = 0; i < dev->tx_bufsz; i++ ) {
        txdesc = (struct i40e_tx_desc_data *)(dev->tx_base + i * sizeof(struct i40e_tx_desc_data));
        txdesc->pkt_addr = (u64)kmalloc(4096);
        txdesc->rsv_cmd_dtyp = 0;
        txdesc->txbufsz_offset = 0;
        txdesc->l2tag = 0;
    }


    /* Rx descriptor */
    union i40e_rx_desc *rxdesc;
    /* Previous tail */
    dev->rx_tail = 0;
    /* up to 64 K minus 8 */
    dev->rx_bufsz = (1<<13);
    dev->rx_bufmask = (1<<13) - 1;
    /* Allocate memory for RX descriptors */
    dev->rx_read = kmalloc(dev->rx_bufsz * sizeof(struct i40e_rx_desc_read));
    if ( 0 == dev->rx_read ) {
        kfree(dev);
        return NULL;
    }
    /* ToDo: 16 bytes for alignment */
    dev->rx_base = (u64)kmalloc(dev->rx_bufsz * sizeof(union i40e_rx_desc));
    if ( 0 == dev->rx_base ) {
        kfree(dev);
        return NULL;
    }
    for ( i = 0; i < dev->rx_bufsz; i++ ) {
        rxdesc = (union i40e_rx_desc *)(dev->rx_base + i * sizeof(union i40e_rx_desc));
        rxdesc->read.pkt_addr = (u64)kmalloc(4096);
        rxdesc->read.hdr_addr = (u64)kmalloc(4096);

        dev->rx_read[i].pkt_addr = rxdesc->read.pkt_addr;
        dev->rx_read[i].hdr_addr = rxdesc->read.hdr_addr;
    }



    u8 *hmc;
    u64 hmcint;
    u64 txbase;
    u64 rxbase;
    u32 cnt = 1536;
    u32 txobjsz = mmio_read32(dev->mmio, I40E_GLHMC_LANTXOBJSZ);
    u32 rxobjsz = mmio_read32(dev->mmio, I40E_GLHMC_LANRXOBJSZ);

    hmc = kmalloc(2 * 1024 * 1024);
    kmemset(hmc, 0, 2 * 1024 * 1024);
    hmcint = (u64)hmc;

    txbase = 0;
    /* roundup512((TXBASE * 512) + (TXCNT * 2^TXOBJSZ)) / 512 */
    rxbase = (((txbase * 512) + (1 * (1<<txobjsz))) + 511) / 512;

    struct i40e_lan_txq_ctx *txq_ctx = (struct i40e_lan_txq_ctx *)(hmcint + txbase);
    struct i40e_lan_rxq_ctx *rxq_ctx = (struct i40e_lan_rxq_ctx *)(hmcint + rxbase);

    txq_ctx->head = 0;
    txq_ctx->newctx = 1;
    txq_ctx->base = 0;          /* FIXME */
    txq_ctx->thead_wb = 0;
    txq_ctx->head_wben = 0;
    txq_ctx->qlen = 0;          /* FIXME */
    txq_ctx->head_wbaddr = 0;

    rxq_ctx->head = 0;
    rxq_ctx->base = 0;          /* FIXME */
    rxq_ctx->qlen_l = 0;        /* FIXME */
    rxq_ctx->qlen_h = 0;        /* FIXME */
    rxq_ctx->dbuff = 4096/128;
    rxq_ctx->hbuff = 128/64;
    rxq_ctx->dtype = 0x0;
    rxq_ctx->dsize = 0x0;
    rxq_ctx->crcstrip = 0x0;
    rxq_ctx->rxmax = 4096;

    mmio_write32(dev->mmio, I40E_PFHMC_SDDATALOW,
                 (hmcint & 0xfff00000) | 1 | (1<<1) | (512<<2));
    mmio_write32(dev->mmio, I40E_PFHMC_SDDATAHIGH, hmcint >> 32);
    mmio_write32(dev->mmio, I40E_PFHMC_SDCMD, (1<<31) | 0);


    mmio_write32(dev->mmio, I40E_GLHMC_LANTXBASE(0), 0);
    mmio_write32(dev->mmio, I40E_GLHMC_LANRXBASE(0), rxbase);

    mmio_write32(dev->mmio, I40E_GLHMC_LANTXCNT(0), cnt);
    mmio_write32(dev->mmio, I40E_GLHMC_LANRXCNT(0), cnt);


    kprintf("HMC: %llx %llx %llx %llx\r\n",
            *(u64 *)(hmc + 0), *(u64 *)(hmc + 8),
            *(u64 *)(hmc + 16), *(u64 *)(hmc + 24));

    return 0;
}

