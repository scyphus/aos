/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _DRIVERS_PCI_H
#define _DRIVERS_PCI_H

struct pci_device {
    u16 bus;
    u16 slot;
    u16 func;
    u16 vendor_id;
    u16 device_id;
    u8 intr_pin;        /* 0x01: INTA#, 0x02: INTB#, 0x03: INTC#: 0x04: INTD# */
    u8 intr_line;       /* 0xff: no connection */
    u8 class;
    u8 subclass;
    u8 progif;
};
struct pci {
    struct pci_device *device;
    struct pci *next;
};

u16 pci_read_config(u16, u16, u16, u16);
u64 pci_read_mmio(u8, u8, u8);
u32 pci_read_rom_bar(u8, u8, u8);
u8 pci_get_header_type(u16, u16, u16);
struct pci * pci_list(void);

#endif /* _DRIVERS_PCI_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
