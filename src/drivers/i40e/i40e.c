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
#define I40E_QTX_HEAD(q)        (0x000e4000 + 0x4 * (q))
#define I40E_QTX_TAIL(q)        (0x00108000 + 0x4 * (q))

#define I40E_QRX_ENA(q)         (0x00120000 + 0x4 * (q))
#define I40E_QRX_TAIL(q)        (0x00128000 + 0x4 * (q))

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
#define I40E_PRTPM_SAH(n)       (0x001e44c0 + 0x20 * (n)) /* RO */

#define I40E_GLPCI_CNF2         0x000be494
#define I40E_GLPCI_LINKCAP      0x000be4ac
#define I40E_GLSCD_QUANTA       0x000b2080

#define I40E_GLLAN_RCTL_0       0x0012a500

#define I40E_GLPRT_GOTC(n)      (0x00300680 + 0x8 * (n))

#define I40E_PRTGL_SAL          0x001e2120
#define I40E_PRTGL_SAH          0x001e2140

#define I40E_GLGEN_RTRIG        0x000b8190


#define I40E_AQ_LARGE_BUF       512
/* MAX ==> 4096 */

#define I40E_PF_ATQBAL          0x00080000
#define I40E_PF_ATQBAH          0x00080100
#define I40E_PF_ATQLEN          0x00080200      /*enable:1<<31*/
#define I40E_PF_ATQH            0x00080300
#define I40E_PF_ATQT            0x00080400

#define I40E_PF_ARQBAL          0x00080080
#define I40E_PF_ARQBAH          0x00080180
#define I40E_PF_ARQLEN          0x00080280      /*enable:1<<31*/
#define I40E_PF_ARQH            0x00080380
#define I40E_PF_ARQT            0x00080480


//PFHMC_SDCMD
//PFHMC_SDDATALOW
//PFHMC_SDDATAHIGH
//GLHMC_SDPART[n]
//GLTPH_CTRL.Desc_PH
//GLHMC_LANQMAX

// GLSCD_QUANTA: 0x000B2080 /* RW */
// GLNVM_SRCTL
// GLLAN_RCTL_0 0x0012A500


struct i40e_tx_desc_data {
    u64 pkt_addr;
    volatile u16 rsv_cmd_dtyp;            /* rsv:2 cmd:10 dtyp:4 */
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

struct i40e_aq_desc {
    volatile u16 flags;
    u16 opcode;
    u16 len;
    u16 ret;
    u32 cookieh;
    u32 cookiel;
    u32 param0;
    u32 param1;
    u32 addrh;
    u32 addrl;
} __attribute__ ((packed));



struct i40e_device {
    u64 mmio;

    /* Admin queue */
    struct {
        struct i40e_aq_desc *base;
        u32 tail;
        int len;
        u8 *bufset;
    } atq;
    struct {
        struct i40e_aq_desc *base;
        u32 tail;
        int len;
        u8 *bufset;
    } arq;


    u64 rx_base;
    u32 rx_tail;
    u32 rx_bufsz;
    u32 rx_bufmask;

    struct {
        u64 base;
        u32 tail;
        u32 bufsz;
        u32 bufmask;
        u32 head_cache;

        u32 headwb;
    } txq[4];

    u64 tx_base;
    u32 tx_tail;
    u32 tx_bufsz;
    u32 tx_bufmask;
    u32 tx_head_cache;

    struct i40e_rx_desc_read *rx_read;

    u8 macaddr[6];

    struct pci_device *pci_device;
};



struct i40e_lan_rxq_ctx {
    /* 0-31 */
    u32 head:13;
    u32 cpuid:8;
    u32 rsv1:11;
    /* 32-95 */
    u64 base:57;
    //u64 qlen_l:7;
    u64 qlen:13;
    /* 96-127 */
    //u32 qlen_h:6;
    u32 dbuff:7;
    u32 hbuff:5;
    u32 dtype:2;
    u32 dsize:1;
    u32 crcstrip:1;
    u32 fcena:1;
    u32 l2tsel:1;
    u32 hsplit_0:4;
    u32 hsplit_1:2;
    u32 rsv2:1;
    u32 showiv:1;
    /* 128-191 */
    u64 rsv3:46;
    u64 rxmax:14;
    u64 rsv4l:4;
    /* 192- */
    u64 rsv4h:1;
    u64 tphrdesc:1;
    u64 tphwdesc:1;
    u64 tphdata:1;
    u64 tphhead:1;
    u64 rsv5:1;
    u64 lrxqtresh:3;
    u64 rsv6:55;
} __attribute__ ((packed));

struct i40e_lan_txq_ctx {
    /* Line 0.0 */
    u32 head:13;
    u32 rsv1:17;
    u32 newctx:1;
    u32 rsv2:1;
    /* Line 0.1 */
    u64 base:57;
    u64 fcena:1;
    u64 tsynena:1;
    u64 fdena:1;
    u64 alt_vlan:1;
    u64 rsv3:3;
    /* Line 0.2 */
    u32 cpuid:8;
    u32 rsv4:24;
    /* Line 1.0 */
    u32 thead_wb:13;
    u32 rsv5:19;
    /* Line 1.1 */
    u32 head_wben:1;
    u32 qlen:13;
    u32 tphrdesc:1;
    u32 tphrpacket:1;
    u32 tphwdesc:1;
    u32 rsv6:15;
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
    u64 rsv13:20;
    u64 rdylist:10;
    u64 rsv14:34;
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
static __inline__ volatile u64
mmio_read64(u64 base, u64 offset)
{
    return *(volatile u64 *)(base + offset);
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

#if 0
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
    int j;
    u32 m32;

    /* Get function number to determine the HMC function index */
    func = dev->pci_device->func;

    /* Global/CORE reset */
    //mmio_write32(dev->mmio, I40E_GLGEN_RTRIG, 2);
    //arch_busy_usleep(20000);    /* wait 20ms */

#if 0
    mmio_write32(dev->mmio, I40E_GLSCD_QUANTA, 3);
    arch_busy_usleep(100000);
    kprintf("GLSCD_QUANTA: %x\r\n", mmio_read32(dev->mmio, I40E_GLSCD_QUANTA));
#endif


    /* Initialize the admin queue */
    dev->atq.len = 128;
    dev->atq.base = kmalloc(sizeof(struct i40e_aq_desc) * dev->atq.len);
    dev->atq.tail = 0;
    dev->atq.bufset = kmalloc(4096 * dev->atq.len);
    dev->arq.base = kmalloc(sizeof(struct i40e_aq_desc) * dev->arq.len);
    dev->arq.tail = 0;
    dev->arq.len = 128;
    dev->arq.bufset = kmalloc(4096 * dev->atq.len);
    for ( i = 0; i < dev->arq.len; i++ ) {
        dev->arq.base[i].flags = (1<<9) | (1<<12);
        dev->arq.base[i].opcode = 0;
        dev->arq.base[i].len = 4096;
        dev->arq.base[i].ret = 0;
        dev->arq.base[i].cookieh = 0;
        dev->arq.base[i].cookiel = 0;
        dev->arq.base[i].param0 = 0;
        dev->arq.base[i].param1 = 0;
        dev->arq.base[i].addrh = (u64)(dev->arq.bufset + (i * 4096)) >> 32;
        dev->arq.base[i].addrl = (u64)(dev->arq.bufset + (i * 4096));
    }

    mmio_write32(dev->mmio, I40E_PF_ATQH, 0);
    mmio_write32(dev->mmio, I40E_PF_ATQT, 0);
    mmio_write32(dev->mmio, I40E_PF_ATQBAL, (u64)dev->atq.base);
    mmio_write32(dev->mmio, I40E_PF_ATQBAH, (u64)dev->atq.base >> 32);
    mmio_write32(dev->mmio, I40E_PF_ATQLEN, dev->atq.len | (1<<31));

    mmio_write32(dev->mmio, I40E_PF_ARQH, 0);
    mmio_write32(dev->mmio, I40E_PF_ARQT, 0);
    mmio_write32(dev->mmio, I40E_PF_ARQBAL, (u64)dev->arq.base);
    mmio_write32(dev->mmio, I40E_PF_ARQBAH, (u64)dev->arq.base >> 32);
    mmio_write32(dev->mmio, I40E_PF_ARQLEN, dev->arq.len | (1<<31));

    /* Issue get ver admin command */
    int idx = dev->atq.tail;
    dev->atq.base[idx].flags = 0;
    dev->atq.base[idx].opcode = 0x0001;
    dev->atq.base[idx].len = 0;
    dev->atq.base[idx].ret = 0;
    dev->atq.base[idx].cookieh = 0x1234;
    dev->atq.base[idx].cookiel = 0xabcd;
    dev->atq.base[idx].param0 = 0;
    dev->atq.base[idx].param1 = 0;
    dev->atq.base[idx].addrh = 0;
    dev->atq.base[idx].addrl = 0x00010001;
    dev->atq.tail++;
    mmio_write32(dev->mmio, I40E_PF_ATQT, dev->atq.tail);
    while ( !(dev->atq.base[idx].flags & 0x1) ) {
        arch_busy_usleep(10);
    }
#if 0
    kprintf("ATQH ARQT/ARQH: %x (%x,%x,%x,%x) %x %x\r\n",
            mmio_read32(dev->mmio, I40E_PF_ATQH),
            dev->atq.base[0].flags,
            dev->atq.base[0].param0,
            dev->atq.base[0].param1,
            dev->atq.base[0].addrh,
            mmio_read32(dev->mmio, I40E_PF_ARQT),
            mmio_read32(dev->mmio, I40E_PF_ARQH));
#endif
    idx = dev->atq.tail;
    dev->atq.base[idx].flags = 0;
    dev->atq.base[idx].opcode = 0x0110;
    dev->atq.base[idx].len = 0;
    dev->atq.base[idx].ret = 0;
    dev->atq.base[idx].cookieh = 0x5678;
    dev->atq.base[idx].cookiel = 0xef01;
    dev->atq.base[idx].param0 = 2;
    dev->atq.base[idx].param1 = 0;
    dev->atq.base[idx].addrh = 0;
    dev->atq.base[idx].addrl = 0;
    dev->atq.tail++;
    mmio_write32(dev->mmio, I40E_PF_ATQT, dev->atq.tail);
    while ( !(dev->atq.base[idx].flags & 0x1) ) {
        arch_busy_usleep(10);
    }
#if 0
    kprintf("ATQH ARQT/ARQH: %x (%x,%x,%x,%x) %x %x\r\n",
            mmio_read32(dev->mmio, I40E_PF_ATQH),
            dev->atq.base[1].flags,
            dev->atq.base[1].param0,
            dev->atq.base[1].param1,
            dev->atq.base[1].ret,
            mmio_read32(dev->mmio, I40E_PF_ARQT),
            mmio_read32(dev->mmio, I40E_PF_ARQH));
#endif
#if 1
    /* Set MAC config */
    idx = dev->atq.tail;
    dev->atq.base[idx].flags = 0;
    dev->atq.base[idx].opcode = 0x0603;
    dev->atq.base[idx].len = 0;
    dev->atq.base[idx].ret = 0;
    dev->atq.base[idx].cookieh = 0x9abc;
    dev->atq.base[idx].cookiel = 0xdef0;
    dev->atq.base[idx].param0 = 1518 | (1<<18) | (0<<24);
    dev->atq.base[idx].param1 = 0;
    dev->atq.base[idx].addrh = 0;
    dev->atq.base[idx].addrl = 0;
    dev->atq.tail++;
    mmio_write32(dev->mmio, I40E_PF_ATQT, dev->atq.tail);
    while ( !(dev->atq.base[idx].flags & 0x1) ) {
        arch_busy_usleep(10);
    }
#endif

#if 0
    kprintf("VSI: %x %x %x %x\r\n",
            mmio_read32(dev->mmio, 0x0020C800),
            mmio_read32(dev->mmio, 0x0020C804),
            mmio_read32(dev->mmio, 0x0020C808),
            mmio_read32(dev->mmio, 0x0020C80c));
#endif

    /* Read PFLAN_QALLOC register to find the base queue index and # of queues
       associated with the PF */
    qalloc = mmio_read32(dev->mmio, I40E_PFLAN_QALLOC);

    kprintf("QALLOC: %x\r\n", qalloc);

#if 0
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

    kprintf("GLPCI_CNF2: %x\r\n", mmio_read32(dev->mmio, I40E_GLPCI_CNF2));
    kprintf("GLPCI_LINKCAP: %x\r\n", mmio_read32(dev->mmio, I40E_GLPCI_LINKCAP));
    kprintf("GLSCD_QUANTA: %x\r\n", mmio_read32(dev->mmio, I40E_GLSCD_QUANTA));

    kprintf("PRTGL_SAL/H: %x %x\r\n", mmio_read32(dev->mmio, I40E_PRTGL_SAL),
            mmio_read32(dev->mmio, I40E_PRTGL_SAH));
#endif

    //kprintf("GLLAN_RCTL_0: %x\r\n", mmio_read32(dev->mmio, I40E_GLLAN_RCTL_0));
    //mmio_write32(dev->mmio, I40E_GLLAN_RCTL_0, 1);
    //arch_busy_usleep(10000);
    kprintf("GLLAN_RCTL_0: %x\r\n", mmio_read32(dev->mmio, I40E_GLLAN_RCTL_0));

    /* Tx descriptors */
    struct i40e_tx_desc_data *txdesc;
    for ( i = 0; i < 4; i++ ) {
        dev->txq[i].tail = 0;
        dev->txq[i].head_cache = 0;
        dev->txq[i].headwb = 0;
        /* up to 8 K minus 32 */
        dev->txq[i].bufsz = (1<<10);
        dev->txq[i].bufmask = (1<<10) - 1;
        /* ToDo: 16 bytes for alignment */
        dev->txq[i].base = (u64)kmalloc(dev->txq[i].bufsz * sizeof(struct i40e_tx_desc_data));
        for ( j = 0; j < dev->txq[i].bufsz; j++ ) {
            txdesc = (struct i40e_tx_desc_data *)(dev->txq[i].base + j * sizeof(struct i40e_tx_desc_data));
            txdesc->pkt_addr = (u64)kmalloc(4096);
            txdesc->rsv_cmd_dtyp = 0;
            txdesc->txbufsz_offset = 0;
            txdesc->l2tag = 0;
        }
    }


    /* Rx descriptor */
    union i40e_rx_desc *rxdesc;
    /* Previous tail */
    dev->rx_tail = 0;
    /* up to 64 K minus 8 */
    dev->rx_bufsz = (1<<10);
    dev->rx_bufmask = (1<<10) - 1;
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

    hmc = kmalloc(4 * 1024 * 1024);
    kmemset(hmc, 0, 4 * 1024 * 1024);
    hmcint = (u64)hmc;
    hmcint = (hmcint + 0x1fffff) & (~0x1fffffULL);
    hmc = (u8 *)hmcint;

    txbase = 0;
    /* roundup512((TXBASE * 512) + (TXCNT * 2^TXOBJSZ)) / 512 */
    //rxbase = (((txbase * 512) + (1 * (1<<txobjsz))) + 511) / 512;
    rxbase = (((txbase * 512) + (cnt * (1<<txobjsz))) + 511) / 512;

#if 0
    kprintf("HMC: %llx, Rx: %llx\r\n", hmcint, dev->rx_base);
#endif

    struct i40e_lan_txq_ctx *txq_ctx;
    for ( i = 0; i < 4; i++ ){
        txq_ctx = (struct i40e_lan_txq_ctx *)(hmcint + txbase * 512 + i * 128);
        txq_ctx->head = 0;
        txq_ctx->rsv1 = 0;
        txq_ctx->newctx = 1;
        txq_ctx->base = dev->txq[i].base / 128;
        txq_ctx->thead_wb = 0;
        txq_ctx->head_wben = 1;
        txq_ctx->qlen = dev->txq[i].bufsz;
        txq_ctx->head_wbaddr = (u64)&(dev->txq[i].headwb);
        txq_ctx->tphrdesc = 1;
        txq_ctx->tphrpacket = 1;
        txq_ctx->tphwdesc = 1;
        txq_ctx->rdylist = 0;
    }

    struct i40e_lan_rxq_ctx *rxq_ctx;
    rxq_ctx = (struct i40e_lan_rxq_ctx *)(hmcint + rxbase * 512);
    rxq_ctx->head = 0;
    rxq_ctx->base = dev->rx_base / 128;
    rxq_ctx->qlen = dev->rx_bufsz;
    rxq_ctx->dbuff = 4096/128;
    rxq_ctx->hbuff = 128/64;
    rxq_ctx->dtype = 0x0;
    rxq_ctx->dsize = 0x0;
    rxq_ctx->crcstrip = 0x1;
    rxq_ctx->rxmax = 4096;
    rxq_ctx->tphrdesc = 1;
    rxq_ctx->tphwdesc = 1;
    rxq_ctx->tphdata = 1;
    rxq_ctx->tphhead = 1;

#if 0
    kprintf("HMC: %.8llx %.8llx %.8llx %.8llx\r\n",
            *(u32 *)(hmc + 0), *(u32 *)(hmc + 4),
            *(u32 *)(hmc + 8), *(u32 *)(hmc + 12));
#endif

    mmio_write32(dev->mmio, I40E_PFHMC_SDDATALOW,
                 (hmcint & 0xffe00000) | 1 | (1<<1) | (512<<2));
    mmio_write32(dev->mmio, I40E_PFHMC_SDDATAHIGH, hmcint >> 32);
    mmio_write32(dev->mmio, I40E_PFHMC_SDCMD, (1<<31) | 0);


    mmio_write32(dev->mmio, I40E_GLHMC_LANTXBASE(0), txbase);
    mmio_write32(dev->mmio, I40E_GLHMC_LANRXBASE(0), rxbase);

    mmio_write32(dev->mmio, I40E_GLHMC_LANTXCNT(0), cnt);
    mmio_write32(dev->mmio, I40E_GLHMC_LANRXCNT(0), cnt);

#if 0
    kprintf("HMC: %.8llx %.8llx %.8llx %.8llx\r\n",
            *(u32 *)(hmc + 0), *(u32 *)(hmc + 4),
            *(u32 *)(hmc + 8), *(u32 *)(hmc + 12));
#endif

    for ( i = 0; i < 4; i++ ) {
        /* Clear QDIS flag */
        mmio_write32(dev->mmio, I40E_GLLAN_TXPRE_QDIS(0), (1<<31) | i);

        /* PF Queue */
        mmio_write32(dev->mmio, I40E_QTX_CTL(i), 2);

        mmio_write32(dev->mmio, I40E_QTX_HEAD(i), 0);
        mmio_write32(dev->mmio, I40E_QTX_TAIL(i), 0);

        /* Enable */
        mmio_write32(dev->mmio, I40E_QTX_ENA(i), 1);
        for ( j = 0; j < 10; j++ ) {
            arch_busy_usleep(1);
            m32 = mmio_read32(dev->mmio, I40E_QTX_ENA(i));
            if ( 5 == (m32 & 5) ) {
                break;
            }
        }
        if ( 5 != (m32 & 5) ) {
            kprintf("Error on enable a TX queue (%d)\r\n", i);
            return -1;
        }
    }

    /* Invalidate */
    //mmio_write32(dev->mmio, I40E_PFHMC_PDINV, 0);


    /* Enable Rx */
    mmio_write32(dev->mmio, I40E_QRX_TAIL(0), dev->rx_bufsz - 1);

    mmio_write32(dev->mmio, I40E_QRX_ENA(0), 1);
    for ( i = 0; i < 10; i++ ) {
        arch_busy_usleep(1);
        m32 = mmio_read32(dev->mmio, I40E_QRX_ENA(0));
        if ( 5 == (m32 & 5) ) {
            break;
        }
    }
    if ( 5 != (m32 & 5) ) {
        kprintf("Error on enable a RX queue\r\n");
        return -1;
    }

    //kprintf("GLLAN_RCTL_0: %x\r\n", mmio_read32(dev->mmio, I40E_GLLAN_RCTL_0));
    //mmio_write32(dev->mmio, I40E_GLLAN_RCTL_0, 1);
    //arch_busy_usleep(10000);
    //kprintf("GLLAN_RCTL_0: %x\r\n", mmio_read32(dev->mmio, I40E_GLLAN_RCTL_0));

    return 0;
}

int
i40e_tx_test(struct netdev *netdev, u8 *pkt, int len, int blksize)
{
    struct i40e_device *dev;
    struct i40e_tx_desc_data *txdesc;
    int i;

    blksize--;

    dev = (struct i40e_device *)netdev->vendor;

    /* Prepare */
    for ( i = 0; i < dev->tx_bufsz; i++ ) {
        txdesc = (struct i40e_tx_desc_data *)
            (dev->tx_base + i * sizeof(struct i40e_tx_desc_data));
        kmemcpy((void *)txdesc->pkt_addr, pkt, len);
        txdesc->rsv_cmd_dtyp = 0xf;
    }

    int cnt = 0;
    for ( ;; ) {
        txdesc = (struct i40e_tx_desc_data *)
            (dev->tx_base + dev->tx_tail * sizeof(struct i40e_tx_desc_data));

        if ( 0xf == (txdesc->rsv_cmd_dtyp & 0xf) )  {
            /* Write back ok */
            txdesc->l2tag = 0;
            txdesc->txbufsz_offset = (len << 18) | 14/2;
            txdesc->rsv_cmd_dtyp = 0 | (((1) | (1<<1)) << 4);

            cnt += 1;
            dev->tx_tail = (dev->tx_tail + 1) & dev->tx_bufmask;

            if ( 0 == (cnt & blksize) ) {
                cnt = 0;
                mmio_write32(dev->mmio, I40E_QTX_TAIL(0), dev->tx_tail);
            }
        } else if ( cnt ) {
            cnt = 0;
            mmio_write32(dev->mmio, I40E_QTX_TAIL(0), dev->tx_tail);
        }
    }

    return 0;
}

int
i40e_tx_test2(struct netdev *netdev, u8 *pkt, int len, int blksize,
              int frm, int to)
{
    struct i40e_device *dev;
    struct i40e_tx_desc_data *txdesc;
    int i;
    int j;
    u32 tdh;
    u32 next_tail;

    dev = (struct i40e_device *)netdev->vendor;

    /* Prepare */
    for ( i = frm; i < to; i++ ) {
        for ( j = 0; j < dev->txq[i].bufsz; j++ ) {
            txdesc = (struct i40e_tx_desc_data *)
                (dev->txq[i].base + j * sizeof(struct i40e_tx_desc_data));
            kmemcpy((void *)txdesc->pkt_addr, pkt, len);
        }
    }

    for ( ;; ) {

        for ( i = frm; i < to; i++ ) {
            tdh = dev->txq[i].head_cache;
            next_tail = (dev->txq[i].tail + blksize) & dev->txq[i].bufmask;
            if ( dev->txq[i].bufsz -
                 (((dev->txq[i].bufsz - tdh + dev->txq[i].tail) & dev->txq[i].bufmask))
                 < blksize ) {
                tdh = mmio_read32(dev->mmio, I40E_QTX_HEAD(i));
                dev->txq[i].head_cache = tdh;
                if ( dev->txq[i].bufsz -
                     (((dev->txq[i].bufsz - tdh + dev->txq[i].tail) & dev->txq[i].bufmask))
                     < blksize ) {
                    /* Still full */
                    kprintf("Full: %x %x\r\n", tdh, dev->txq[i].tail);
                    continue;
                }
            }
            /* Not full */
            for ( j = 0; j < blksize; j++ ) {
                txdesc = (struct i40e_tx_desc_data *)
                    (dev->txq[i].base
                     + ((dev->txq[i].tail + j) & dev->txq[i].bufmask)
                     * sizeof(struct i40e_tx_desc_data));
                txdesc->l2tag = 0;
                txdesc->txbufsz_offset = (len << 18) | 14/2;
                txdesc->rsv_cmd_dtyp = 0 | (((1)/* | (1<<1)*/) << 4);
            }
            dev->txq[i].tail = next_tail;
            mmio_write32(dev->mmio, I40E_QTX_TAIL(i), dev->txq[i].tail);

            /* Packets Transmitted [64 Bytes] Count Low GLPRT_PTC64L:
               0x003006A0 */
        }
    }

    return 0;
}

int
i40e_tx_test3(struct netdev *netdev, u8 *pkt, int len, int blksize,
              int frm, int to)
{
    struct i40e_device *dev;
    struct i40e_tx_desc_data *txdesc;
    int i;
    int j;
    u32 tdh;
    u32 next_tail;


    dev = (struct i40e_device *)netdev->vendor;
    //kprintf("GLLAN_RCTL_0: %x\r\n", mmio_read32(dev->mmio, I40E_GLLAN_RCTL_0));

    /* Prepare */
    for ( i = frm; i < to; i++ ) {
        for ( j = 0; j < dev->txq[i].bufsz; j++ ) {
            txdesc = (struct i40e_tx_desc_data *)
                (dev->txq[i].base + j * sizeof(struct i40e_tx_desc_data));
            kmemcpy((void *)txdesc->pkt_addr, pkt, len);
        }
    }

    for ( ;; ) {

        for ( i = frm; i < to; i++ ) {

            tdh = dev->txq[i].headwb;

            u32 avl;
            avl = (dev->txq[i].bufsz - dev->txq[i].tail + tdh - 1)
                & dev->txq[i].bufmask;

            if ( avl < blksize ) {
                //kprintf("Full: %x %x\r\n", tdh, dev->txq[i].tail);
                continue;
            }
            next_tail = (dev->txq[i].tail + blksize) & dev->txq[i].bufmask;

            /* Not full */
            for ( j = 0; j < blksize; j++ ) {
                txdesc = (struct i40e_tx_desc_data *)
                    (dev->txq[i].base
                     + ((dev->txq[i].tail + j) & dev->txq[i].bufmask)
                     * sizeof(struct i40e_tx_desc_data));
                txdesc->l2tag = 0;
                txdesc->txbufsz_offset = (len << 18) | 14/2/* | ((20/4)<<7)*/;
                txdesc->rsv_cmd_dtyp = 0 | (((1) /*| (1<<1)*/ /*| (1<<2)*/
                                             /*| (2<<5)*//*IPv4 w/o cso*/) << 4);
                //txdesc->rsv_cmd_dtyp |= (1<<1)<<4;
            }
            txdesc->rsv_cmd_dtyp |= (1<<1)<<4;
            dev->txq[i].tail = next_tail;
            mmio_write32(dev->mmio, I40E_QTX_TAIL(i), dev->txq[i].tail);

            /* Packets Transmitted [64 Bytes] Count Low GLPRT_PTC64L:
               0x003006A0 */
        }
    }

    return 0;
}

int
i40e_forwarding_test(struct netdev *netdev1, struct netdev *netdev2)
{
    struct i40e_device *dev1;
    struct i40e_device *dev2;
    union i40e_rx_desc * rxdesc;
    struct i40e_tx_desc_data *txdesc;
    u8 *txpkt;

    dev1 = (struct i40e_device *)netdev1->vendor;
    dev2 = (struct i40e_device *)netdev2->vendor;
    static int cnt = 0;
    for ( ;; ) {

        rxdesc = (union i40e_rx_desc *)
            (dev1->rx_base + dev1->rx_tail * sizeof(union i40e_rx_desc));
        txdesc = (struct i40e_tx_desc_data *)
            (dev2->txq[0].base + dev2->txq[0].tail * sizeof(struct i40e_tx_desc_data));

#if 0
        kprintf("%llx %x %x %x\r\n", *(volatile u64 *)((u64)rxdesc + 8),
                *(u32 *)0x14e30000,
                mmio_read32(dev1->mmio, 0x00300480),
                mmio_read32(dev1->mmio, 0x00300000));
#endif
        if ( rxdesc->read.hdr_addr & 0x1 ) {

            txpkt = (u8 *)dev1->rx_read[dev1->rx_tail].pkt_addr;
            *(u32 *)(txpkt + 0) =  0x67664000LLU;
            *(u16 *)(txpkt + 4) =  0x2472LLU;
            /* src */
            *(u16 *)(txpkt + 6) =  *((u16 *)netdev2->macaddr);
            *(u32 *)(txpkt + 8) =  *((u32 *)(netdev2->macaddr + 2));

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
            txdesc->l2tag = 0;
            txdesc->txbufsz_offset
                = (((rxdesc->wb.len_ptype_err_status >> 38) & 0x3ffffffULL) << 18)
                | 14/2;
            txdesc->rsv_cmd_dtyp = 0 | (((1) | (1<<1)) << 4);

            dev1->rx_read[dev1->rx_tail].pkt_addr = (u64)tmp;
            rxdesc->read.pkt_addr = dev1->rx_read[dev1->rx_tail].pkt_addr;
            rxdesc->read.hdr_addr = dev1->rx_read[dev1->rx_tail].hdr_addr;

            dev2->txq[0].tail = (dev2->txq[0].tail + 1) & dev2->txq[0].bufmask;
            if ( 0 == ((++cnt) & 0xff) ) {
                mmio_write32(dev2->mmio, I40E_QTX_TAIL(0), dev2->txq[0].tail);
                mmio_write32(dev1->mmio, I40E_QRX_TAIL(0), dev1->rx_tail);
            }
            dev1->rx_tail = (dev1->rx_tail + 1) & dev1->rx_bufmask;

#if 0
            if ( 0 == (cnt & 0xffffff) ) {
                kprintf("%.16llx %.16llx\r\n",
                        mmio_read32(dev1->mmio, 0x0036e400),
                        mmio_read32(dev1->mmio, 0x00300480));
                //kprintf("%.16llx %.16llx\r\n", cnt, mmio_read32(dev1->mmio, 0x00300480));
                //kprintf("%llx\r\n", mmio_read32(dev1->mmio, 0x00310000));
            }
#endif

        }
    }

    return 0;
}


void
_tohex(u32 x, u8 *buf)
{
    int c;
    int i;

    for ( i = 0; i < 8; i++ ) {
        c = (x>>(28 - 4 * i)) & 0xf;
        if ( c < 10 ) {
            c += '0';
        } else {
            c += 'a' - 10;
        }
        buf[i] = c;
    }
    buf[i] = 0;
}

void
i40e_test(struct netdev *netdev, struct tcp_session *sess)
{
    struct i40e_device *dev;

    dev = (struct i40e_device *)netdev->vendor;


    kmemset(dev->atq.bufset + (3 * 4096), 0, 4096);
    kmemset(dev->atq.bufset + (3 * 4096), 1, 1);

#if 0
    /* Get switch config */
    dev->atq.base[3].flags = (1<<9) | (1<<12);
    dev->atq.base[3].opcode = 0x0200;
    dev->atq.base[3].len = 4096;
    dev->atq.base[3].ret = 0;
    dev->atq.base[3].cookieh = 0xabcd;
    dev->atq.base[3].cookiel = 0xefab;
    dev->atq.base[3].param0 = 0;
    dev->atq.base[3].param1 = 0;
#else
    /* Get VSI parameters */
    dev->atq.base[3].flags = /*(1<<9) |*/ (1<<12);
    dev->atq.base[3].opcode = 0x0212;
    dev->atq.base[3].len = 0x80;
    dev->atq.base[3].ret = 0;
    dev->atq.base[3].cookieh = 0xabcd;
    dev->atq.base[3].cookiel = 0xefab;
    dev->atq.base[3].param0 = 0x0206;
    dev->atq.base[3].param1 = 0;
#endif
    dev->atq.base[3].addrh = (u64)(dev->atq.bufset + (3 * 4096)) >> 32;
    dev->atq.base[3].addrl = (u64)(dev->atq.bufset + (3 * 4096));
    mmio_write32(dev->mmio, I40E_PF_ATQT, 4);
    while ( !(dev->atq.base[3].flags & 0x1) ) {
        arch_busy_usleep(10);
    }

    if ( sess ) {
        sess->send(sess, "***\n", 4);
        u8 tmp[16];
        _tohex(dev->atq.base[3].ret, tmp);
        tmp[8] = '\n';
        sess->send(sess, tmp, 9);
        sess->send(sess, "***\n", 4);
        _tohex(dev->atq.base[3].param0, tmp);
        tmp[8] = '\n';
        sess->send(sess, tmp, 9);
        sess->send(sess, "***\n", 4);
        _tohex(dev->atq.base[3].param1, tmp);
        tmp[8] = '\n';
        sess->send(sess, tmp, 9);
        sess->send(sess, "***\n", 4);
        _tohex(mmio_read32(dev->mmio, I40E_PF_ARQH), tmp);
        tmp[8] = '\n';
        sess->send(sess, tmp, 9);
        sess->send(sess, "***\n", 4);

        u32 *x = (u32 *)(dev->atq.bufset + (3 * 4096));
        u32 y;
        u8 buf[4096];
        int i;
        for ( i = 0; i < 32; i++ ) {
            y = *(u32 *)(x + i);
            _tohex(y, buf + (i * 9));
            if ( 3 == (i % 4) ) {
                buf[(i + 1) * 9 - 1] = '\n';
            } else {
                buf[(i + 1) * 9 - 1] = ' ';
            }
        }
        sess->send(sess, buf, 32 * 9);
    }

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
