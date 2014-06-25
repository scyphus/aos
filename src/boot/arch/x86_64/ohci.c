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
 * Update OHCI hardware
 */
void
ohci_update_hw(void)
{
    struct pci *pci;
    u64 mmio;

    /* Search OHCI controller from PCI */
    pci = pci_list();
    while ( NULL != pci ) {
        if ( pci->device->class == 0x0c && pci->device->subclass == 0x03 ) {
            /* USB */
            if ( pci->device->progif == 0x00 ) {
                /* UHCI */
                mmio = pci_read_mmio(pci->device->bus, pci->device->slot,
                                     pci->device->func);
                kprintf("* %x.%x.%x %.4x:%.4x\r\n", pci->device->bus,
                        pci->device->slot, pci->device->func,
                        pci->device->vendor_id, pci->device->device_id);
                kprintf("UHCI MMIO: %x\r\n", mmio);
                kprintf("UHCI Revision: %x\r\n", mmio_read32(mmio, 0x0));
            } else if ( pci->device->progif == 0x10 ) {
                /* OHCI */
                mmio = pci_read_mmio(pci->device->bus, pci->device->slot,
                                     pci->device->func);
                kprintf("* %x.%x.%x %.4x:%.4x\r\n", pci->device->bus,
                        pci->device->slot, pci->device->func,
                        pci->device->vendor_id, pci->device->device_id);
                kprintf("OHCI MMIO: %x\r\n", mmio);
                kprintf("OHCI Revision: %x\r\n", mmio_read32(mmio, 0x0));

                /*
                  Steps
                  - Save HcFmInterval
                  - Reset (maximum 10 microsec)
                  - Restore HcFmInterval
                  - Initialize HCCA block
                  - Set HcHCCA
                  - Enable all interrupts except SOF detect in HcInterruptEnable
                  - Enable all queue in HcControl
                  - HcPeriodicStart to HcFmInterval / 10 * 9
                */
                u32 hcfminterval;
                u32 val;

                /* HcFmInterval 0x34 */
                hcfminterval = mmio_read32(mmio, 0x34);
                kprintf("OHCI HcFmInterval: %x\r\n", hcfminterval);

                /* Reset */
                kprintf("OHCI HcControl: %x\r\n", mmio_read32(mmio, 0x04));
                val = mmio_read32(mmio, 0x08);
                kprintf("OHCI HcCommandStatus: %x\r\n", val);
                mmio_write32(mmio, 0x08, val | 1);
                while ( mmio_read32(mmio, 0x08) & 1 ) {
                    /* Wait until the completion of the reset */
                }

                /* Restore HcFmInterval */
                kprintf("OHCI HcFmInterval: %x\r\n", mmio_read32(mmio, 0x34));
                mmio_write32(mmio, 0x34, hcfminterval);
                kprintf("OHCI HcFmInterval: %x\r\n", mmio_read32(mmio, 0x34));

                kprintf("OHCI HcControl: %x\r\n", mmio_read32(mmio, 0x04));

                /* 4096 is a page size but larger than required size to ensure
                   located on a 256-byte boundary */
                u8 *hcca = bmalloc(4096);
                /* FIXME: Check the boundary and it is in 32-bit address */
                int i;
                for ( i = 0; i < 256; i++ ) {
                    hcca[i] = 0;
                }
                /* HcHCCA */
                mmio_write32(mmio, 0x18, (u32)hcca);
                kprintf("OHCI HcHCCA: %x\r\n", mmio_read32(mmio, 0x18));


            } else if ( pci->device->progif == 0x20 ) {
                /* EHCI */
                mmio = pci_read_mmio(pci->device->bus, pci->device->slot,
                                     pci->device->func);
                kprintf("* %x.%x.%x %.4x:%.4x\r\n", pci->device->bus,
                        pci->device->slot, pci->device->func,
                        pci->device->vendor_id, pci->device->device_id);
                kprintf("EHCI MMIO: %x\r\n", mmio);
                kprintf("EHCI Revision: %x\r\n", mmio_read32(mmio, 0x0));

            } else if ( pci->device->progif == 0xd0 ) {
                /* xHCI */
                mmio = pci_read_mmio(pci->device->bus, pci->device->slot,
                                     pci->device->func);
                kprintf("* %x.%x.%x %.4x:%.4x\r\n", pci->device->bus,
                        pci->device->slot, pci->device->func,
                        pci->device->vendor_id, pci->device->device_id);
                kprintf("XHCI MMIO: %x\r\n", mmio);
                kprintf("XHCI Revision: %x\r\n", mmio_read32(mmio, 0x0));

            }
        }
        pci = pci->next;
    }
}

void
ohci_init(void)
{
    kprintf("Searching OHCI...\r\n");
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
