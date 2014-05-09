/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "boot.h"


#define LH32_TO_PTR(l, h)       (void *)(((u64)(l)) | ((u64)(h)) << 32)

extern struct blkdev_list *blkdev_head;

struct ahci_device {
    u64 abar;
    struct pci_device *pci_device;
};
struct ahci_port {
    struct ahci_device *ahci;
    int port;
};

typedef volatile struct _hba_port {
    u32 clb;            // 0x00, command list base address, 1K-byte aligned
    u32 clbu;           // 0x04, command list base address upper 32 bits
    u32 fb;             // 0x08, FIS base address, 256-byte aligned
    u32 fbu;            // 0x0C, FIS base address upper 32 bits
    u32 is;             // 0x10, interrupt status
    u32 ie;             // 0x14, interrupt enable
    u32 cmd;            // 0x18, command and status
    u32 rsv0;           // 0x1C, Reserved
    u32 tfd;            // 0x20, task file data
    u32 sig;            // 0x24, signature
    u32 ssts;           // 0x28, SATA status (SCR0:SStatus)
    u32 sctl;           // 0x2C, SATA control (SCR2:SControl)
    u32 serr;           // 0x30, SATA error (SCR1:SError)
    u32 sact;           // 0x34, SATA active (SCR3:SActive)
    u32 ci;             // 0x38, command issue
    u32 sntf;           // 0x3C, SATA notification (SCR4:SNotification)
    u32 fbs;            // 0x40, FIS-based switch control
    u32 rsv1[11];       // 0x44 ~ 0x6F, Reserved
    u32 vendor[4];      // 0x70 ~ 0x7F, vendor specific
} __attribute__ ((packed)) hba_port;
typedef volatile struct _hba_mem {
    // 0x00 - 0x2B, Generic Host Control
    u32 cap;            // 0x00, Host capability
    u32 ghc;            // 0x04, Global host control
    u32 is;             // 0x08, Interrupt status
    u32 pi;             // 0x0C, Port implemented
    u32 vs;             // 0x10, Version
    u32 ccc_ctl;        // 0x14, Command completion coalescing control
    u32 ccc_pts;        // 0x18, Command completion coalescing ports
    u32 em_loc;         // 0x1C, Enclosure management location
    u32 em_ctl;         // 0x20, Enclosure management control
    u32 cap2;           // 0x24, Host capabilities extended
    u32 bohc;           // 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    u8 rsv[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    u8 vendor[0x100-0xA0];

    // 0x100 - 0x10FF, Port control registers
    hba_port ports[1];  // 1 ~ 32
} __attribute__ ((packed)) hba_mem;
typedef struct _hba_cmd_header {
    // DW0
    u8 cfl:5;           // Command FIS length in DWORDS, 2 ~ 16
    u8 a:1;             // ATAPI
    u8 w:1;             // Write, 1: H2D, 0: D2H
    u8 p:1;             // Prefetchable

    u8 r:1;             // Reset
    u8 b:1;             // BIST
    u8 c:1;             // Clear busy upon R_OK
    u8 rsv0:1;          // Reserved
    u8 pmp:4;           // Port multiplier port

    u16 prdtl;          // Physical region descriptor table length in entries

    // DW1
    volatile
    u32 prdbc;          // Physical region descriptor byte count transferred

    // DW2, 3
    u32 ctba;           // Command table descriptor base address
    u32 ctbau;          // Command table descriptor base address upper 32 bits

    // DW4 - 7
    u32 rsv1[4];        // Reserved
} hba_cmd_header;

/* Host to Device */
typedef struct _fis_reg_h2d {
    // DWORD 0
    u8 fis_type;        // FIS_TYPE_REG_H2D

    u8 pmport:4;        // Port multiplier
    u8 rsv0:3;          // Reserved
    u8 c:1;             // 1: Command, 0: Control

    u8 command;         // Command register
    u8 featurel;        // Feature register, 7:0

    // DWORD 1
    u8 lba0;            // LBA low register, 7:0
    u8 lba1;            // LBA mid register, 15:8
    u8 lba2;            // LBA high register, 23:16
    u8 device;          // Device register

    // DWORD 2
    u8 lba3;            // LBA register, 31:24
    u8 lba4;            // LBA register, 39:32
    u8 lba5;            // LBA register, 47:40
    u8 featureh;        // Feature register, 15:8

    // DWORD 3
    u8 countl;          // Count register, 7:0
    u8 counth;          // Count register, 15:8
    u8 icc;             // Isochronous command completion
    u8 control;         // Control register

    // DWORD 4
    u8 rsv1[4];         // Reserved
} fis_reg_h2d;

typedef struct _hba_prdt_entry {
    u32 dba;            // Data base address
    u32 dbau;           // Data base address upper 32 bits
    u32 rsv0;           // Reserved

    // DW3
    u32 dbc:22;         // Byte count, 4M max
    u32 rsv1:9;         // Reserved
    u32 i:1;            // Interrupt on completion
} hba_prdt_entry;
typedef struct _hba_cmd_tbl {
    // 0x00
    u8 cfis[64];        // Command FIS

    // 0x40
    u8 acmd[16];        // ATAPI command, 12 or 16 bytes

    // 0x50
    u8 rsv[48];         // Reserved

    // 0x80
    hba_prdt_entry prdt_entry[1]; // Physical region descriptor table entries, 0 ~ 65535
} hba_cmd_tbl;

typedef enum {
    FIS_TYPE_REG_H2D    = 0x27, // Register FIS - host to device
    FIS_TYPE_REG_D2H    = 0x34, // Register FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39, // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP  = 0x41, // DMA setup FIS - bidirectional
    FIS_TYPE_DATA       = 0x46, // Data FIS - bidirectional
    FIS_TYPE_BIST       = 0x58, // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP  = 0x5F, // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1, // Set device bits FIS - device to host
} FIS_TYPE;
#define HBA_PxIS_TFES ((u32)1<<30)
#define HBA_PxCMD_CR ((u32)1<<15)
#define HBA_PxCMD_FR ((u32)1<<14)
#define HBA_PxCMD_FRE ((u32)1<<4)
#define HBA_PxCMD_ST ((u32)1)
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

/*
 * Read data from AHCI register
 */
static __inline__ u32
abar_read32(u64 base, u64 offset)
{
    return *(u32 *)(base + offset);
}

/*
 * Write data to AHCI register
 */
static __inline__ void
abar_write32(u64 base, u64 offset, u32 value)
{
    *(u32 *)(base + offset) = value;
}


/* Start command engine */
void
start_cmd(hba_port *port)
{
    /* Wait until CR (bit15) is cleared */
    while ( port->cmd & HBA_PxCMD_CR ) {
    }

    /* Set FRE (bit4) and ST (bit0) */
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

/* Stop command engine */
void
stop_cmd(hba_port *port)
{
    /* Clear ST (bit0) */
    port->cmd &= ~HBA_PxCMD_ST;

    /* Wait until CR (bit15) is cleared */
    while ( port->cmd & HBA_PxCMD_CR ) {
    }

    /* Clear FRE (bit4) */
    port->cmd &= ~HBA_PxCMD_FRE;

    /* Wait until FR (bit14) is cleared */
    while ( port->cmd & HBA_PxCMD_FR ) {
    }
}

/*
 * Rebase
 */
static void
port_rebase(hba_port *port, int portno)
{
    int i;

    /* Stop the command engine */
    stop_cmd(port);

    // Command list offset: 1K*portno
    // Command list entry size = 32
    // Command list entry maxim count = 32
    // Command list maxim size = 32*32 = 1K per port
    u8 *x;

    x = bmalloc(4096);
    port->clb = (u32)(u64)x;
    port->clbu = (u32)(((u64)x) >> 32);
    for ( i = 0; i < 1024; i++ ) {
        x[i] = 0;
    }

    // FIS offset: 32K+256*portno
    // FIS entry size = 256 bytes per port
    x = bmalloc(4096);
    port->fb = (u32)(u64)x;
    port->fbu = (u32)(((u64)x) >> 32);
    for ( i = 0; i < 256; i++ ) {
        x[i] = 0;
    }

    // Command table offset: 40K + 8K*portno
    // Command table size = 256*32 = 8K per port
    hba_cmd_header *hdr = (hba_cmd_header *)((u64)port->clb);
    for ( i = 0; i < 32; i++ ) {
        hdr[i].prdtl = 8;       // 8 prdt entries per command table
                                // 256 bytes per command table, 64+16+48+16*8
        // Command table offset: 40K + 8K*portno + cmdheader_index*256
        x = bmalloc(4096);
        hdr[i].ctba = (u32)(u64)x;
        hdr[i].ctbau = (u32)(((u64)x) >> 32);
        for ( i = 0; i < 256; i++ ) {
            x[i] = 0;
        }
    }

    start_cmd(port);    // Start command engine
}

/*
 * Find an available command slot
 */
static int
_find_cmdslot(hba_port *port)
{
    u32 slots;
    int i;

    slots = port->sact | port->ci;
    for ( i = 0; i < 32; i++ ) {
        if ( (slots&1) == 0) {
            return i;
        }
        slots >>= 1;
    }

    return -1;
}

/*
 * Read
 */
static int
_read(struct blkdev *blkdev, u64 start, u32 count, u8 *buf)
{
    hba_port *port;
    hba_cmd_header *hdr;
    int slot;
    int i;
    int spin;

    /* Obtain port structure */
    port = (hba_port *)blkdev->parent;

    /* # of retries */
    spin = 0;

    port->is = (u32)-1;

    /* Find an available command slot */
    slot = _find_cmdslot(port);
    if ( -1 == slot ) {
        /* Error */
        return -1;
    }

    hdr = (hba_cmd_header *)(u64)port->clb;
    hdr += slot;
    hdr->cfl = sizeof(fis_reg_h2d) / sizeof(u32); // command size
    hdr->w = 0; // Read from device
    hdr->prdtl = ((count-1) >> 4) + 1; // PRDT entries count

    hba_cmd_tbl *tbl = (hba_cmd_tbl *)(u64)hdr->ctba;
    for ( i = 0; i < sizeof(hba_cmd_tbl)
              + (hdr->prdtl-1) * sizeof(hba_prdt_entry); i ++ ) {
        *(((u8 *)tbl) + i) = 0;
    }

    // 8K bytes (16 sectors) per PRDT
    for ( i = 0; i < hdr->prdtl - 1; i++ ) {
        tbl->prdt_entry[i].dba = (u32)(u64)buf;
        tbl->prdt_entry[i].dbc = 8 * 1024; // 8K bytes
        tbl->prdt_entry[i].i = 1;
        buf += 4 * 1024; // 4K words
        count -= 16; // 16 sectors
    }

    // Last entry
    tbl->prdt_entry[i].dba = (u32)(u64)buf;
    tbl->prdt_entry[i].dbc = count << 9; // 512 bytes per sector
    tbl->prdt_entry[i].i = 1;

    // Setup command
    fis_reg_h2d *fis = (fis_reg_h2d *)(&tbl->cfis);

    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // Command
    fis->command = 0x25; //ATA_CMD_READ_DMA_EX;

    fis->lba0 = (u8)start;
    fis->lba1 = (u8)(start>>8);
    fis->lba2 = (u8)(start>>16);
    fis->device = 1<<6; // LBA mode

    fis->lba3 = (u8)(start>>24);
    fis->lba4 = (u8)(start>>32);
    fis->lba5 = (u8)(start>>40);

    fis->countl = count & 0xff;
    fis->counth = (count >> 8) & 0xff;

    // The below loop waits until the port is no longer busy before issuing a new command
    while ( (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000 ) {
        spin++;
    }
    if ( spin == 1000000 ) {
        //kprintf("Port is hung\r\n");
        return -1;
    }

    port->ci = 1<<slot;	// Issue command

    // Wait for completion
    while ( 1 ) {
        // In some longer duration reads, it may be helpful to spin on the DPS bit
        // in the PxIS port field as well (1 << 5)
        if ( (port->ci & (1<<slot)) == 0 ) {
            break;
        }
        if ( port->is & HBA_PxIS_TFES ) { // Task file error
            //kprintf("Read disk error\r\n");
            return -1;
        }
    }

    // Check again
    if ( port->is & HBA_PxIS_TFES ) {
        return -1;
    }

    return 0;
}

/* Write */
static int
_write(struct blkdev *blkdev, u64 start, u32 count, u8 *buf)
{
    hba_port *port;
    int slot;
    int i;
    int spin;

    spin = 0;

    port = blkdev->parent;

    port->is = (u32)-1;

    slot = _find_cmdslot(port);
    if ( -1 == slot ) {
        /* Error */
        return -1;
    }

    hba_cmd_header *hdr = (hba_cmd_header *)LH32_TO_PTR(port->clb, port->clbu);
    hdr += slot;
    hdr->cfl = sizeof(fis_reg_h2d) / sizeof(u32); // command size
    hdr->w = 0; // Read from device
    hdr->prdtl = ((count-1) >> 4) + 1; // PRDT entries count

    hba_cmd_tbl *tbl = (hba_cmd_tbl *)LH32_TO_PTR(hdr->ctba, hdr->ctbau);
    for ( i = 0; i < sizeof(hba_cmd_tbl)
              + (hdr->prdtl-1) * sizeof(hba_prdt_entry); i ++ ) {
        *(((u8 *)tbl) + i) = 0;
    }

    // 8K bytes (16 sectors) per PRDT
    for ( i = 0; i < hdr->prdtl - 1; i++ ) {
        tbl->prdt_entry[i].dba = (u32)(u64)buf;
        tbl->prdt_entry[i].dbau = (u32)(((u64)buf) >> 32);
        tbl->prdt_entry[i].dbc = 8 * 1024; // 8K bytes
        tbl->prdt_entry[i].i = 1;
        buf += 4 * 1024; // 4K words
        count -= 16; // 16 sectors
    }

    // Last entry
    tbl->prdt_entry[i].dba = (u32)(u64)buf;
    tbl->prdt_entry[i].dbau = (u32)(((u64)buf) >> 32);
    tbl->prdt_entry[i].dbc = count << 9; // 512 bytes per sector
    tbl->prdt_entry[i].i = 1;

    // Setup command
    fis_reg_h2d *fis = (fis_reg_h2d *)(&tbl->cfis);

    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // Command
    fis->command = 0x35; //ATA_CMD_WRITE_DMA_EX;

    fis->lba0 = (u8)start;
    fis->lba1 = (u8)(start>>8);
    fis->lba2 = (u8)(start>>16);
    fis->device = 1<<6; // LBA mode

    fis->lba3 = (u8)(start>>24);
    fis->lba4 = (u8)(start>>32);
    fis->lba5 = (u8)(start>>40);

    fis->countl = count & 0xff;
    fis->counth = (count >> 8) & 0xff;

    // The below loop waits until the port is no longer busy before issuing a new command
    while ( (port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000 ) {
        spin++;
    }
    if ( spin == 1000000 ) {
        //kprintf("Port is hung\r\n");
        return -1;
    }

    port->ci = 1<<slot;	// Issue command
    //kprintf("XXXX %x\r\n", port->ci);

    // Wait for completion
    while ( 1 ) {
        // In some longer duration reads, it may be helpful to spin on the DPS bit
        // in the PxIS port field as well (1 << 5)
        if ( (port->ci & (1<<slot)) == 0 ) {
            break;
        }
        if ( port->is & HBA_PxIS_TFES ) { // Task file error
            //kprintf("Write disk error\r\n");
            return -1;
        }
    }

    // Check again
    if ( port->is & HBA_PxIS_TFES ) {
        //kprintf("Write disk error\r\n");
        return -1;
    }

    return 0;
}

/*
 * Initialize the hardware
 */
struct ahci_device *
ahci_init_hw(struct pci_device *pcidev)
{
    struct ahci_device *dev;
    hba_mem *mem;
    u32 pi;
    u32 ssts;
    u32 sig;
    int i;

    /* Allocate for an AHCI device */
    dev = bmalloc(sizeof(struct ahci_device));
    if ( NULL == dev ) {
        return NULL;
    }

    /* Assert */
    if ( 0x8086 != pcidev->vendor_id ) {
        bfree(dev);
        return NULL;
    }

    /* Read AHCI Base Memory Register (ABAR) */
    dev->abar = pci_read_config(pcidev->bus, pcidev->slot,
                                pcidev->func, 0x10 + 4 * 5);
    dev->abar |= ((u32)pci_read_config(pcidev->bus, pcidev->slot,
                                       pcidev->func, 0x12 + 4 * 5)) << 16;

    /* Implemented port */
    pi = abar_read32(dev->abar, 0x0c);
    for ( i = 0; i < 32; i++ ) {
        if ( pi & 1 ) {
            /* Implemented */
            ssts = abar_read32(dev->abar, 0x100 + 0x80 * i + 0x28);
            sig = abar_read32(dev->abar, 0x100 + 0x80 * i + 0x24);
            /*
             * [11:8]: Interface Power Management
             *      0: Device not present or communication not established
             *      1: Active state
             *      2: Partial power management state
             *      6: Slumber power management state
             *      8: DevSleep power management state
             *
             * [7:4]: Current Interface Speed
             *      0: Device not present or communication not established
             *      1: Gen 1
             *      2: Gen 2
             *      3: Gen 3
             *
             * [3:0]: Device Detection
             *      0: No device detected
             *      1: Device detected but Phy not established
             *      3: Device detected and Phy established
             *      4: Phy in offline mode
             */
            /*
             * Signature
             * ATA: 0x00000101
             * ATAPI: 0xEB140101
             * SEMB: 0xC33C0101 (Enclosure management bridge)
             * PM: 0x96690101 (Port multiplier
             */
            mem = (hba_mem *)dev->abar;
            if ( (ssts & 0xf) == 3 && ((ssts >> 8) & 0xf) == 1
                 && sig == 0x101 ) {

                /* Rebase each port port */
                port_rebase(&(mem->ports[i]), i);
            }
        }
        pi >>= 1;
    }

    return dev;
}

/*
 * Update AHCI hardware
 */
void
ahci_update_hw(void)
{
    struct pci *pci;
    struct ahci_device *dev;
    struct blkdev *blkdev;

    /* Search AHCI controller from PCI */
    pci = pci_list();
    while ( NULL != pci ) {
        if ( 0x8086 == pci->device->vendor_id ) {
            switch ( pci->device->device_id ) {
            case 0x2829:
                /* 82801HBM AHCI Controller */
                dev = ahci_init_hw(pci->device);
                if ( NULL != dev ) {
                    blkdev = bmalloc(sizeof(struct blkdev));
                    blkdev->read = _read;
                    blkdev->write = _write;
                    blkdev->parent = dev;
                }
                break;
            default:
                ;
            }
        }
        pci = pci->next;
    }
}

/*
 * Initialize AHCI driver
 */
void
ahci_init(void)
{
    /* Initialize the driver */
    ahci_update_hw();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
