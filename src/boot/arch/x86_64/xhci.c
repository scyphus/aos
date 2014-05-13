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


/*
 * Update xHCI hardware
 */
void
xhci_update_hw(void)
{
    struct pci *pci;

    /* Search AHCI controller from PCI */
    pci = pci_list();
    while ( NULL != pci ) {
        switch ( pci->device->vendor_id ) {
        case 0x106b:
            switch ( pci->device->device_id ) {
            case 0x003f:
                break;
            }
            break;
        }
        pci = pci->next;
    }
}

void
xhci_init(void)
{
    xhci_update_hw();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
