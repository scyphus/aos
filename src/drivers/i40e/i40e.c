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

#define I40E_XL710              0x1584

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


struct ixgbe_rx_desc_read {
    volatile u64 pkt_addr;
    volatile u64 hdr_addr;      /* 0:DD */
} __attribute__ ((packed));

struct ixgbe_adv_rx_desc_wb {
    /* Write back */
    u32 filter_stat;
    u32 l2tag_mirr_fcoe_ctx;
    u64 len_ptype_err_status;
} __attribute__ ((packed));


struct i40e_device {
    u64 mmio;

    u8 macaddr[6];

    struct pci_device *pci_device;
};

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
            case I40E_XL710:
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

    kprintf("ROM BAR: %x\r\n",
            pci_read_rom_bar(pcidev->bus, pcidev->slot, pcidev->func));

    #if 0
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
#endif

    dev->pci_device = pcidev;

    i40e_init_fpm(dev);

    return dev;
}


int
i40e_init_fpm(struct i40e_device *dev)
{
    u16 func;
    u32 qalloc;

    /* Get function number to determine the HMC function index */
    func = dev->pci_device->func;

    /* Read PFLAN_QALLOC register to find the base queue index and # of queues
       associated with the PF */
    qalloc = mmio_read32(dev->mmio, I40E_PFLAN_QALLOC);

    kprintf("QALLOC: %x\r\n", qalloc);

    return 0;
}

