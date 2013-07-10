/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "arch.h"
#include "apic.h"

extern u64 acpi_ioapic_base;

void
lapic_init(void)
{
}

/*
 * Estimate bus frequency using ACPI PM timer
 */
u64
lapic_estimate_freq(void)
{
    u32 t1;
    u32 t2;


    //t1 = inl(acpi_pm_tmr_port);
    //t1 = inl(acpi_pm_tmr_port);
}

void
ioapic_init(void)
{
    /* Disable i8259 PIC */
    outb(0xa1, 0xff);
    outb(0x21, 0xff);

}

void
ioapic_map_intr(u64 src, u64 dst)
{
    /* To avoid compiler optimization, call assembler function */
    asm_ioapic_map_intr(src, dst, acpi_ioapic_base);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
