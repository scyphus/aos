/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../../kernel.h"
#include "arch.h"
#include "desc.h"
#include "acpi.h"
#include "apic.h"
#include "i8254.h"
#include "vga.h"
#include "clock.h"
#include "cmos.h"
#include "bootinfo.h"
#include "memory.h"

//static u64 cpu_freq;
//static u64 fsb_freq;

void apic_test(void);
void search_clock_sources(void);


extern void trampoline(void);
extern void trampoline_end(void);

static int dbg_lock;

int
dbg_printf(const char *fmt, ...)
{
    va_list ap;
    u64 clk;

    arch_spin_lock(&dbg_lock);

    va_start(ap, fmt);

    clock_update();
    clk = clock_get();
    kprintf("[%4llu.%.6llu]  ", clk/1000000000, (clk / 1000) % 1000000);
    kvprintf(fmt, ap);

    va_end(ap);

    arch_spin_unlock(&dbg_lock);
}


#define E1000_REG_CTRL  0x00
#define E1000_REG_STATUS 0x08
#define E1000_REG_EEC   0x10
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

#define E1000_CTRL_FD   1       /* Full duplex */
#define E1000_CTRL_LRST (1<<3)  /* Link reset */
#define E1000_CTRL_ASDE (1<<5)  /* Auto speed detection enable */
#define E1000_CTRL_SLU  (1<<6)  /* Set linkup */
#define E1000_CTRL_PHY_RST      (1<<31)  /* PHY reset */

#define E1000_CTRL_RST  (1<<26)

#define E1000_TCTL_EN   (1<<1)
#define E1000_TCTL_PSP  (1<<3)  /* pad short packets */

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


u16
e1000_eeprom_read_8254x(u64 mmio, u8 addr)
{
    u16 data;
    u32 tmp;

    /* Start */
    *(u32 *)(mmio + E1000_REG_EERD) = ((u32)addr << 8) | 1;

    /* Until it's done */
    while ( !((tmp = *(u32 *)(mmio + E1000_REG_EERD)) & (1<<4)) ) {
        pause();
    }
    data = (u16)((tmp >> 16) & 0xffff);

    return data;
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

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

u16
pci_read_config(u16 bus, u16 slot, u16 func, u16 offset)
{
    u32 addr;

    addr = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8)
        | ((u32)offset & 0xfc);
    /* Set enable bit */
    addr |= (u32)0x80000000;

    outl(PCI_CONFIG_ADDR, addr);
    return (inl(0xcfc) >> ((offset & 2) * 8)) & 0xffff;
}

void
pci_write_config(u16 bus, u16 slot, u16 func, u16 val)
{
}


u16
pci_check_vendor(u16 bus, u16 slot)
{
    u16 vendor;
    u16 device;

    vendor = pci_read_config(bus, slot, 0, 0);
#if 0
    if ( 0xffff != vendor ) {
        device = pci_read_config(bus, slot, 0, 2);
    }
#endif
    return vendor;
}

u8
pci_get_header_type(u16 bus, u16 slot, u16 func)
{
    return pci_read_config(bus, slot, func, 0x0e) & 0xff;
}

u64
pci_read_mmio(u8 bus, u8 slot, u8 func)
{
    u64 addr;
    u32 bar0;
    u32 bar1;
    u8 type;
    u8 prefetchable;

    bar0 = pci_read_config(bus, slot, func, 0x10);
    bar0 |= (u32)pci_read_config(bus, slot, func, 0x12) << 16;

    type = (bar0 >> 1) & 0x3;
    prefetchable = (bar0 >> 3) & 0x1;
    addr = bar0 & 0xfffffff0;

    if ( 0x00 == type ) {
        /* 32bit */
    } else if ( 0x02 == type ) {
        /* 64bit */
        bar1 = pci_read_config(bus, slot, func, 0x14);
        bar1 |= (u32)pci_read_config(bus, slot, func, 0x16) << 16;
        addr |= ((u64)bar1) << 32;
    } else {
        return 0;
    }

    return addr;
}

void
pci_check_function(u8 bus, u8 slot, u8 func)
{
    u16 vendor;
    u16 device;
    vendor = pci_read_config(bus, slot, func, 0);
    device = pci_read_config(bus, slot, func, 2);
    dbg_printf("    %d.%d.%d vendor:device=%.4x:%.4x\r\n",
               bus, slot, func, vendor, device);

    if ( 0x8086 == vendor && (0x100e == device || 0x100f == device
                              || 0x107c == device
                              || 0x109a == device
                              || 0x10f5 == device
                              || 0x10ea == device) ) {
#if 0
        u32 x = 0;
        int j;
        for ( j = 0; j < 6; j++ ) {
            x = pci_read_config(bus, slot, func, 0x10 + j * 4);
            x |= (u32)pci_read_config(bus, slot, func, 0x12 + j * 4) << 16;
            dbg_printf("      BAR%d %.8x\r\n", j, x);
        }
        //*(u16 *)0xf2525000 = 0xf1;
        //*(u32 *)(mmio + 0x18) |= (u32)(1<<13);//EE_RST
#endif

        //kprintf("CR=[%x]\r\n", pci_read_config(bus, slot, func, 0x4));

        /* Intel PRO/1000 MT Server (82545EM) */

        int i;

        /* Read BAR0 (+BAR1 if 64 bit) */
        u64 mmio = pci_read_mmio(bus, slot, func);
        dbg_printf("      MMIO %.16x\r\n", mmio);

#if 0
        kprintf("COMMAND: %x\r\n", pci_read_config(bus, slot, func, 0x04));
        /* PCI_CR=0x04, PCI_CR_MAST_EN=0x04 */
#endif

        /* Initialize */
#if 1
        *(u32 *)(mmio + E1000_REG_IMC) = 0;
        arch_busy_usleep(100);
        *(u32 *)(mmio + E1000_REG_CTRL) |= E1000_CTRL_RST;
        //| (1<<26); //SWRST
        arch_busy_usleep(100);
        //*(u32 *)(mmio + E1000_REG_CTRL) &= ~(u32)E1000_CTRL_LRST;
        //arch_busy_usleep(100);
        //*(u32 *)(mmio + E1000_REG_CTRL) &= ~(u32)E1000_CTRL_PHY_RST;
        //*(u32 *)(mmio + E1000_REG_CTRL) |= (u32)E1000_CTRL_PHY_RST;
        //arch_busy_usleep(10000);
        //*(u32 *)(mmio + E1000_REG_STATUS) &= ~(u32)(1<<10); // PHYRA
        //arch_busy_usleep(100);
        *(u32 *)(mmio + E1000_REG_CTRL) |= E1000_CTRL_SLU;
        //*(u32 *)(mmio + E1000_REG_CTRL) |= (1<<3) | (1<<18);
        //*(u32 *)(mmio + E1000_REG_CTRL) |= (1<<18);
        //*(u32 *)(mmio + 0x18) &= ~(u32)(1<<12);
        *(u32 *)(mmio + 0x18) &= ~(u32)(3<<22);
        arch_busy_usleep(100);
        kprintf("STATUS: %x\r\n", *(u32 *)(mmio + E1000_REG_STATUS));
        *(u32 *)(mmio + 0x03828) |= (1<<24) | (128 << 8) | (8);
        kprintf("TXDCTL: [%.8x]\r\n", *(u32 *)(mmio + 0x03828));
        kprintf("TXDCTL1: [%.8x]\r\n", *(u32 *)(mmio + 0x03928));
#endif
#if 0
        kprintf("IMS: %x\r\n", *(u32 *)(mmio + E1000_REG_IMS));
        *(u32 *)(mmio + E1000_REG_IMC) = 0;
        arch_busy_usleep(10);
        *(u32 *)(mmio + E1000_REG_CTRL) = E1000_CTRL_RST;
        arch_busy_usleep(10);
        kprintf("IMS: %x\r\n", *(u32 *)(mmio + E1000_REG_IMS));
        *(u32 *)(mmio + E1000_REG_IMC) = 0;
#endif


#if 0
        kprintf("STAT: %x\r\n", *(u32 *)(mmio + E1000_REG_STATUS));

        kprintf("EEC: %x\r\n", *(u32 *)(mmio + E1000_REG_EEC));
        /* EE_REQ=1<<6, EE_GNT=1<<7 ==> 0 to access EEPROM */
#if 0
        *(u32 *)(mmio + E1000_REG_EEC) |= 1<<6;
        arch_busy_usleep(1000);
        kprintf("EEC: %x\r\n", *(u32 *)(mmio + E1000_REG_EEC));
#endif

#endif

#if 0
        kprintf("CTRL: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_CTRL));
        *(u32 *)(mmio + E1000_REG_CTRL) = E1000_CTRL_RST;
        //*(u32 *)(mmio + E1000_REG_CTRL) &= ~(u32)E1000_CTRL_SLU;
        kprintf("CTRL: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_CTRL));
#endif



        /* Setup interrupt line */
#if 0
        u16 r;
        r = pci_read_config(bus, slot, func, 0x3c);
        dbg_printf("      Interrupt PIN:Line %.2x:%.2x\r\n", r >> 8, r & 0xff);
#endif

        //kprintf("CTRL: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_CTRL));

        /* Read MAC address */
        u16 m16;
        u32 m32;
        u8 m[6];
        if ( 0x100e == device || 0x100f == device ) {
            m16 = e1000_eeprom_read_8254x(mmio, 0);
            m[0] = m16 & 0xff;
            m[1] = (m16 >> 8) & 0xff;
            m16 = e1000_eeprom_read_8254x(mmio, 1);
            m[2] = m16 & 0xff;
            m[3] = (m16 >> 8) & 0xff;
            m16 = e1000_eeprom_read_8254x(mmio, 2);
            m[4] = m16 & 0xff;
            m[5] = (m16 >> 8) & 0xff;
        } else if ( 0x107c == device || 0x109a == device ) {
            m16 = e1000_eeprom_read(mmio, 0);
            m[0] = m16 & 0xff;
            m[1] = (m16 >> 8) & 0xff;
            m16 = e1000_eeprom_read(mmio, 1);
            m[2] = m16 & 0xff;
            m[3] = (m16 >> 8) & 0xff;
            m16 = e1000_eeprom_read(mmio, 2);
            m[4] = m16 & 0xff;
            m[5] = (m16 >> 8) & 0xff;
        } else {
            //kprintf("[%x]\r\n", e1000_eeprom_read(mmio, 0x14));
            //*(u32 *)(mmio + 0x00018) |= 1 << 13;

#if 0
            kprintf("CTRL: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_CTRL));
            kprintf("STATUS: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_STATUS));
            *(u32 *)(mmio + E1000_REG_CTRL) |= E1000_CTRL_RST;
            arch_busy_usleep(1);
            //*(u32 *)(mmio + E1000_REG_CTRL) |= E1000_CTRL_ASDE | E1000_CTRL_SLU;
            *(u32 *)(mmio + E1000_REG_CTRL) |= E1000_CTRL_SLU;
            *(u32 *)(mmio + E1000_REG_CTRL) &= ~(u32)E1000_CTRL_LRST;
            *(u32 *)(mmio + E1000_REG_CTRL) &= ~(u32)E1000_CTRL_PHY_RST;
            kprintf("CTRL: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_CTRL));
            kprintf("STATUS: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_STATUS));

            //kprintf("MAC %.4x.%.4x.%.4x\r\n",
            //*(u16 *)0xf8525000, *(u16 *)0xf8525002, *(u16 *)0xf8525004);
            //kprintf("RAH.RAL %.8x.%.8x\r\n", *(u32 *)(mmio + 0x5404),
            //*(u32 *)(mmio + 0x5400));
#endif
            m32 = *(u32 *)(mmio + 0x5400);
            m[0] = m32 & 0xff;
            m[1] = (m32 >> 8) & 0xff;
            m[2] = (m32 >> 16) & 0xff;
            m[3] = (m32 >> 24) & 0xff;
            m32 = *(u32 *)(mmio + 0x5404);
            m[4] = m32 & 0xff;
            m[5] = (m32 >> 8) & 0xff;


#if 0
            m16 = e1000_eeprom_read(mmio, 0);
            m[0] = m16 & 0xff;
            m[1] = (m16 >> 8) & 0xff;
            m16 = e1000_eeprom_read(mmio, 1);
            m[2] = m16 & 0xff;
            m[3] = (m16 >> 8) & 0xff;
            m16 = e1000_eeprom_read(mmio, 2);
            m[4] = m16 & 0xff;
            m[5] = (m16 >> 8) & 0xff;
#endif
        }
        dbg_printf("      MAC %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\r\n", m[0], m[1],
                   m[2], m[3], m[4], m[5]);

        /* Link up */
        //kprintf("[%.8x]", *(u32 *)(mmio + E1000_REG_CTRL));
        *(u32 *)(mmio + E1000_REG_CTRL) |= E1000_CTRL_SLU;
        *(u32 *)(mmio + E1000_REG_CTRL) |= (1<<30); // VME (for 802.1Q)
        //*(u32 *)(mmio + E1000_REG_CTRL) &= ~(u32)E1000_CTRL_SLU;
        //kprintf("[%.8x]", *(u32 *)(mmio + E1000_REG_CTRL));

        /* Multicast array table */
        for ( i = 0; i < 128; i++ ) {
            *(u32 *)(mmio + E1000_REG_MTA + i * 4) = 0;
        }

        /* Enable interrupt (REG_IMS <- 0x1F6DC, then read REG_ICR ) */

        /* Start TX/RX */
        u64 base;
        struct e1000_tx_desc *txdesc;
        struct e1000_rx_desc *rxdesc;
        u32 bufsz = 768;

#if 0
        base = (u64)kmalloc(768 * sizeof(struct e1000_rx_desc) + 16);
        for ( i = 0; i < 768; i++ ) {
            rxdesc = (struct e1000_rx_desc *)
                (base + i * sizeof(struct e1000_rx_desc));
            rxdesc->address = kmalloc(8192 + 16);
            rxdesc->checksum = 0;
            rxdesc->status = 0;
            rxdesc->errors = 0;
            rxdesc->special = 0;
        }
        *(u32 *)(mmio + E1000_REG_RDBAH) = base >> 32;
        *(u32 *)(mmio + E1000_REG_RDBAL) = base & 0xffffffff;
        *(u32 *)(mmio + E1000_REG_RDLEN) = 768 * sizeof(struct e1000_rx_desc);
        *(u32 *)(mmio + E1000_REG_RDH) = 0;
        *(u32 *)(mmio + E1000_REG_RDT) = 0;
        *(u32 *)(mmio + E1000_REG_RCTL) = (1<<1) | (1<<2) | (1<<3) | (1<<4)
            | (1<<5) | (1<<26) | (1<<15) | ((2 << 16) | (1 << 25));
#endif


        /* ToDo: 16 bytes for alignment? */
        base = (u64)kmalloc(bufsz * sizeof(struct e1000_tx_desc) + 16);
        for ( i = 0; i < bufsz; i++ ) {
            txdesc = (struct e1000_tx_desc *)
                (base + i * sizeof(struct e1000_tx_desc));
            txdesc->address = 0;
            txdesc->cmd = 0;
            txdesc->sta = 0;
            txdesc->cso = 0;
            txdesc->css = 0;
            txdesc->special = 0;
        }
        *(u32 *)(mmio + E1000_REG_TDBAH) = base >> 32;
        *(u32 *)(mmio + E1000_REG_TDBAL) = base & 0xffffffff;
        *(u32 *)(mmio + E1000_REG_TDLEN) = bufsz * sizeof(struct e1000_tx_desc);
        *(u32 *)(mmio + E1000_REG_TDH) = 0;
        //*(u32 *)(mmio + E1000_REG_TDT) = bufsz;
        *(u32 *)(mmio + E1000_REG_TDT) = 0;

#if 0
        kprintf("TDBA %.8x %.8x\r\n", *(u32 *)(mmio + E1000_REG_TDBAH),
                *(u32 *)(mmio + E1000_REG_TDBAL));
#endif

        *(u32 *)(mmio + E1000_REG_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP;

        /* Send one packet */
        u8 *pkt = kmalloc(1534) ;
        u64 cnt;
#if 0
        pkt[0] = 0x01;
        pkt[1] = 0x00;
        pkt[2] = 0x5e;
        pkt[3] = 0x00;
        pkt[4] = 0x00;
        pkt[5] = 0x01;
#else
        pkt[0] = 0x00;
        pkt[1] = 0x02;
        pkt[2] = 0xb3;
        pkt[3] = 0x04;
        pkt[4] = 0x05;
        pkt[5] = 0x06;
#endif
        pkt[6] = m[0];
        pkt[7] = m[1];
        pkt[8] = m[2];
        pkt[9] = m[3];
        pkt[10] = m[4];
        pkt[11] = m[5];
        /* IP */
        pkt[12] = 0x08;
        pkt[13] = 0x00;
        /* IP */
        pkt[14] = 0x45;
        pkt[15] = 0x00;
        pkt[16] = 0x00;
        pkt[17] = 0x54;
        /* dst */
        pkt[18] = 0x26;
        pkt[19] = 0x6d;
        pkt[20] = 0x00;
        pkt[21] = 0x00;
        pkt[22] = 0x01;
        pkt[23] = 0x01;
        /* checksum */
        pkt[24] = 0x00;
        pkt[25] = 0x00;
        /* src */
        pkt[26] = 192;
        pkt[27] = 168;
        pkt[28] = 100;
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
        pkt[32] = 100;
        pkt[33] = 1;
#endif
        /* icmp */
        pkt[34] = 0x08;
        pkt[35] = 0x00;
        /* icmp checksum */
        pkt[36] = 0x00;
        pkt[37] = 0x00;
        for ( i = 38; i < 64 + 34; i++ ) {
            pkt[i] = 0;
        }

        kprintf("STATUS: %x\r\n", *(u32 *)(mmio + E1000_REG_STATUS));
        arch_busy_usleep(2000000);
        kprintf("STATUS: %x\r\n", *(u32 *)(mmio + E1000_REG_STATUS));
        kprintf("CTRL_EXT: [%.8x]\r\n", *(u32 *)(mmio + 0x18));
        kprintf("FCT: [%.8x]\r\n", *(u32 *)(mmio + 0x30));
        kprintf("TCTL: [%.8x]\r\n", *(u32 *)(mmio + E1000_REG_TCTL));
        kprintf("RCTL: [%.8x]\r\n", *(u32 *)(mmio + 0x0100));
        kprintf("TIDV: [%.8x]\r\n", *(u32 *)(mmio + 0x03820));
        kprintf("PHY_CTRL: [%.8x]\r\n", *(u32 *)(mmio + 0xf10));
        kprintf("TIPG: [%.8x]\r\n", *(u32 *)(mmio + 0x410));
        kprintf("MDIC: [%.8x]\r\n", *(u32 *)(mmio + 0x20));
        kprintf("PBECCSTS: [%.8x]\r\n", *(u32 *)(mmio + 0x100c));
        kprintf("Packet buffer allocation: [%.8x]\r\n", *(u32 *)(mmio + 0x01000));

        for ( cnt = 0; cnt < 100000000; cnt++ ) {
            txdesc = (struct e1000_tx_desc *)
                (base + (cnt % bufsz) * sizeof(struct e1000_tx_desc));
            txdesc->address = pkt;
            txdesc->length = 26 + 34; // 4B for FCS
            //txdesc->length = 64 + 34;
            //txdesc->address = 0;
            //txdesc->length = 0;
            txdesc->sta = 0;
            txdesc->special = 0;
            txdesc->css = 0;
            txdesc->cso = 0;
            txdesc->cmd = (1<<3) | (1<<1) | 1;
            //txdesc->cmd = (1<<3) | (1<<1) | 1; /* report status | insert FCS | end of packet */
            //txdesc->special = 2000;
#if 1
            if ( 0 == cnt % 2 ) {
                txdesc->cmd = (1<<3);
            } else {
                txdesc->address = 0;
                txdesc->length = 0;
                //txdesc->cmd = (1<<3) | (1<<1) | 1 | (1<<6);
                txdesc->cmd = (1<<3) | (1<<1) | 1;
            }
#else
            txdesc->cmd = (1<<3) | (1<<1) | 1;
#endif

#if 0
            kprintf("CUR: %d [%d:%d:%d] %x/%x\r\n",
                    cnt,
                    *(u32 *)(mmio + E1000_REG_TDH),
                    *(u32 *)(mmio + E1000_REG_TDT),
                    *(u32 *)(mmio + E1000_REG_TDLEN),
                    txdesc->cmd, txdesc->sta);
#endif

            *(u32 *)(mmio + E1000_REG_TDT) = (cnt + 1) % bufsz;

#if 1
            if ( cnt % bufsz ) {
               continue;
            }
#endif

#if 0
            kprintf("STA: %x %x\r\n", txdesc->cmd, txdesc->sta);
            kprintf("CTRL: %x\r\n", *(u32 *)(mmio + E1000_REG_CTRL));
            kprintf("STATUS: %x\r\n", *(u32 *)(mmio + E1000_REG_STATUS));
            kprintf("PBECCSTS: [%.8x]\r\n", *(u32 *)(mmio + 0x100c));
#endif
#if 1
            while( !(txdesc->sta & 0xf) ) {
#if 0
                kprintf("XCUR: [%d:%d] %x\r\n",
                        *(u32 *)(mmio + E1000_REG_TDH),
                        *(u32 *)(mmio + E1000_REG_TDT), txdesc->sta);
#endif
                pause();
            }
#endif
#if 0
            kprintf("STA: %x %x\r\n", txdesc->cmd, txdesc->sta);
            kprintf("A packet was transmitted. [%d:%d]\r\n",
                    *(u32 *)(mmio + E1000_REG_TDH),
                    *(u32 *)(mmio + E1000_REG_TDT));
#endif
            //arch_busy_usleep(1000000);
        }
    }
}

void
pci_check_device(u8 bus, u8 device)
{
    u16 vendor;
    u8 func;
    u8 hdr_type;

    func = 0;
    vendor = pci_read_config(bus, device, func, 0);
    if ( 0xffff == vendor ) {
        return;
    }

    pci_check_function(bus, device, func);
    hdr_type = pci_get_header_type(bus, device, func);

    if ( hdr_type & 0x80 ) {
        /* Multiple functions */
        for ( func = 1; func < 8; func++ ) {
            vendor = pci_read_config(bus, device, func, 0);
            if ( 0xffff != vendor ) {
                pci_check_function(bus, device, func);
            }
         }
    }
}

void
pci_check_bus(u8 bus)
{
    u8 device;

    for ( device = 0; device < 32; device++ ) {
        pci_check_device(bus, device);
    }
}

void
pci_check_all_buses(void)
{
    u8 bus;
    u8 func;
    u8 hdr_type;
    u16 vendor;

    hdr_type = pci_get_header_type(0, 0, 0);
    if ( !(hdr_type & 0x80) ) {
        /* Single PCI host controller */
        for ( bus = 0; bus < 8; bus++ ) {
            pci_check_bus(bus);
        }
    } else {
        /* Multiple PCI host controllers */
        for ( func = 0; func < 8; func++ ) {
            vendor = pci_read_config(0, 0, func, 0);
            if ( 0xffff != vendor ) {
                break;
            }
            bus = func;
            pci_check_bus(bus);
        }
    }
}

void
pci_init(void)
{
    pci_check_all_buses();
#if 0
    /* Configure PCI */
    u32 addr;
    u16 bus = 0;
    u16 slot = 0x11;
    u16 func = 0;
    u16 offset = 0;

    /* Enable bit = 0x80000000 */
    addr = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8)
        | ((u32)offset & 0xfc) | (u32)0x80000000;
    outl(0xcf8, addr);
    u16 tmp = (inl(0xcfc) >> ((offset & 2) * 8)) & 0xffff;
    kprintf("XXX: %.4x\r\n", tmp);

    //0xcf8; /* CONFIG_ADDRESS */
    //0xcfc; /* CONFIG_DATA */
#endif
}


/*
 * Initialize BSP
 */
void
arch_bsp_init(void)
{
    struct bootinfo *bi;
    struct phys_mem *pm;
    struct p_data *pdata;
    u64 tsz;
    u64 i;

    dbg_lock = 0;
    bi = (struct bootinfo *)BOOTINFO_BASE;

    /* Initialize VGA display */
    vga_init();

    /* Find configuration using ACPI */
    acpi_load();
    //__asm__ __volatile__ ( "1:hlt;jmp 1b;" );

    /* Initialize the clock */
    clock_init();

    /* Print a message */
    kprintf("Welcome to Academic Operating System!\r\n");

    /* Count the number of processors (APIC) */
    /* ToDo */

    /* Stop i8254 timer */
    i8254_stop_timer();

    /* Initialize memory table */
    dbg_printf("Initializing physical memory.\r\n");
    if ( 0 != phys_mem_init(bi) ) {
        panic("Error! Cannot initialize physical memory.\r\n");
    }

    /* For multiprocessors */
#if 0
    if ( 0 != phys_mem_wire((void *)P_DATA_BASE, P_DATA_SIZE*MAX_PROCESSORS) ) {
        panic("Error! Cannot allocate stack for processors.\r\n");
    }
#endif

    dbg_printf("Initializing GDT and IDT.\r\n");

    /* Initialize global descriptor table */
    gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table */
    idt_init();
    idt_load();

    /* Setup interrupt handlers */
    dbg_printf("Setting up interrupt handlers.\r\n");
    idt_setup_intr_gate(IV_TMR, &intr_apic_int32); /* IRQ0 */
    idt_setup_intr_gate(IV_KBD, &intr_apic_int33); /* IRQ1 */
    idt_setup_intr_gate(IV_LOC_TMR, &intr_apic_loc_tmr); /* Local APIC timer */
    idt_setup_intr_gate(0xfe, &intr_crash); /* crash */
    idt_setup_intr_gate(0xff, &intr_apic_spurious); /* Spurious interrupt */

    /* Setup interrupt service routine then initialize I/O APIC */
    ioapic_map_intr(IV_KBD, 1, acpi_ioapic_base); /* IRQ1 */
    ioapic_init();


    /* Initialize TSS and load it */
    dbg_printf("Initializing TSS.\r\n");
    tss_init();
    tr_load(this_cpu());

    /* Enable this processor */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->flags |= 1;

    /* Set general protection fault handler */
    idt_setup_intr_gate(13, &intr_gpf);

    /* Initialize local APIC */
    lapic_init();

    /* Initialize PCI driver */
    dbg_printf("Searching PCI devices.\r\n");
    pci_init();


    /* Check and copy trampoline */
    tsz = trampoline_end - trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        panic("Error! Trampoline size exceeds the maximum allowed size.\r\n");
    }
    for ( i = 0; i < tsz; i++ ) {
        *(u8 *)((u64)TRAMPOLINE_ADDR + i) = *(u8 *)((u64)trampoline + i);
    }

    /* Send INIT IPI */
    dbg_printf("Starting all available application processors.\r\n");
    lapic_send_init_ipi();

    /* Wait 10 ms */
    arch_busy_usleep(10000);

    /* Send a Start Up IPI */
    lapic_send_startup_ipi((TRAMPOLINE_ADDR >> 12) & 0xff);

    /* Wait 200 us */
    arch_busy_usleep(200);

    /* Send another Start Up IPI */
    lapic_send_startup_ipi((TRAMPOLINE_ADDR >> 12) & 0xff);

    /* Wait 200 us */
    arch_busy_usleep(200);

    /* ToDo: Synchronize all processors */


    /* Print out the running processors */
#if 0
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        pdata = (struct p_data *)((u64)P_DATA_BASE + i * P_DATA_SIZE);
        if ( pdata->flags & 1 ) {
            dbg_printf("Processor #%d is running.\r\n", i);
        }
    }
#endif

    /* Initialize local APIC counter */
    lapic_start_timer(LAPIC_HZ, IV_LOC_TMR);
}

/*
 * Initialize AP
 */
void
arch_ap_init(void)
{
    struct p_data *pdata;

    dbg_printf("Initializing application processor #%d.\r\n", this_cpu());

    /* Load global descriptor table */
    gdt_load();

    /* Load interrupt descriptor table */
    idt_load();

    /* Load task register */
    tr_load(this_cpu());

    /* Set a flag to this CPU data area */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->flags |= 1;

    /* Initialize local APIC */
    lapic_init();

    /* Wait short to distribute APIC interrupts */
    arch_busy_usleep(AP_WAIT_USEC * this_cpu());

    /* Initialize local APIC counter */
    lapic_start_timer(LAPIC_HZ, IV_LOC_TMR);

    arch_busy_usleep(1);
}






/*
 * Compute TSC frequency
 */
u64
calib_tsc_freq(void)
{
    return 0;
}

u64
get_invariant_tsc_frequency(void)
{
    u64 val;
    int family;
    int model;

    /* Check invariant TSC support first */
    if ( !is_invariant_tsc() ) {
        /* Invariant TSC is not available */
        return 0;
    }

    /* MSR_PLATFORM_INFO */
    val = rdmsr(0xce);
    /* Get maximum non-turbo ratio [15:8] */
    val = (val >> 8) & 0xff;

    family = get_cpu_family();
    model = get_cpu_model();
    if ( 0x06 == family && (0x2a == model || 0x2d == model || 0x3a == model) ) {
        /* SandyBridge (06_2AH, 06_2DH) and IvyBridge (06_3AH) */
        val *= 100000000; /* 100 MHz */
    } else if ( 0x06 == family
                && (0x1a == model || 0x1e == model || 0x1f == model
                    || 0x25 == model || 0x2c == model || 0x2e == model
                    || 0x2f == model) ) {
        /* Nehalem (06_1AH, 06_1EH, 06_1FH, 06_2EH)
           and Westmere (06_2FH, 06_2CH, 06_25H) */
        val *= 133330000; /* 133.33 MHz */
    } else {
        /* Unknown model */
        val = 0;
    }

    return val;
}

/*
 * Search clock sources
 */
void
search_clock_sources(void)
{
    u64 tscfreq;

    /* Get invariant TSC frequency */
    tscfreq = get_invariant_tsc_frequency();
    if ( tscfreq ) {
        kprintf("Freq: %.16x\r\n", tscfreq);
    }

    /* Get ACPI info */
}

/*
 * Put a character to console
 */
void
arch_putc(int c)
{
    vga_putc(c);
}

void
arch_get_tmr(u64 counter, u64 *resolution)
{
}

/*
 * Wait usec microsecond
 */
void
arch_busy_usleep(u64 usec)
{
    acpi_busy_usleep(usec);
}

/*
 * Lock with spinlock
 */
void
arch_spin_lock(int *lock)
{
    spin_lock(lock);
}

/*
 * Unlock with spinlock
 */
void
arch_spin_unlock(int *lock)
{
    spin_unlock(lock);
}

/*
 * Halt all processors
 */
void
arch_crash(void)
{
    /* Send IPI and halt self */
    lapic_send_fixed_ipi(0xfe);
    halt();
}

void
arch_poweroff(void)
{
    acpi_poweroff();
}

void
arch_clock_update(void)
{
    clock_update();
}
u64
arch_clock_get(void)
{
    return clock_get();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
