/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_APIC_H
#define _KERNEL_APIC_H

#include <aos/const.h>

void lapic_init(void);
void lapic_send_init_ipi(void);
void lapic_send_startup_ipi(u8);

void ioapic_init(void);
void ioapic_map_intr(u64, u64, u64);

#endif /* _KERNEL_APIC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
