/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "boot.h"

//eXtensible Host Controller
//0x1e31

//106b:003f

static __inline__ u32
mmio_read32(u64 base, u64 offset)
{
    return *(u32 *)(base + offset);
}
static __inline__ void
mmio_write32(u64 base, u64 offset, u32 value)
{
    *(u32 *)(base + offset) = value;
}

/*
 * Update OHCI hardware
 */
void
ohci_update_hw(void)
{
    struct pci *pci;
    u64 mmio;

    /* Search AHCI controller from PCI */
    pci = pci_list();
    while ( NULL != pci ) {
        if ( pci->device->class == 0x0c  && pci->device->class == 0x03
             && pci->device->class == 0x10 ) {
            /* OHCI */
            mmio = pci_read_mmio(pci->device->bus, pci->device->slot,
                                 pci->device->func);
            kprintf("MMIO: %x\r\n", mmio);

            kprintf("Revision: %x\r\n", mmio_read32(mmio, 0x0));
        }
        pci = pci->next;
    }
}

void
ohci_init(void)
{
    ohci_update_hw();
    //halt64();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
