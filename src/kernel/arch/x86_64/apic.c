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

#define ICR_INIT                0x00000500
#define ICR_STARTUP             0x00000600
#define ICR_LEVEL_ASSERT        0x00004000
#define ICR_DEST_NOSHORTHAND    0x00000000
#define ICR_DEST_SELF           0x00040000
#define ICR_DEST_ALL_INC_SELF   0x00080000
#define ICR_DEST_ALL_EX_SELF    0x000c0000

/*
 * Initialize the local APIC
 */
void
lapic_init(void)
{
    u32 reg;

    /* Enable APIC at spurious interrupt vector register: default vector 0xff */
    reg = asm_lapic_read(APIC_BASE + APIC_SIVR);
    reg |= 0x100;
    asm_lapic_write(APIC_BASE + APIC_SIVR, reg);
}

/*
 * Send INIT IPI
 */
void
lapic_send_init_ipi(void)
{
    u32 icrl;
    u32 icrh;

    icrl = asm_lapic_read(APIC_BASE + APIC_ICR_LOW);
    icrh = asm_lapic_read(APIC_BASE + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_INIT | ICR_DEST_ALL_EX_SELF;
    icrh = icrh & 0x000fffff;

    asm_lapic_write(APIC_BASE + APIC_ICR_LOW, icrl);
    asm_lapic_write(APIC_BASE + APIC_ICR_HIGH, icrh);
}

/*
 * Send Start Up IPI
 */
void
lapic_send_startup_ipi(u8 vector)
{
    u32 icrl;
    u32 icrh;

    icrl = asm_lapic_read(APIC_BASE + APIC_ICR_LOW);
    icrh = asm_lapic_read(APIC_BASE + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_STARTUP | ICR_DEST_ALL_EX_SELF | vector;
    icrh = icrh & 0x000fffff;

    asm_lapic_write(APIC_BASE + APIC_ICR_LOW, icrl);
    asm_lapic_write(APIC_BASE + APIC_ICR_HIGH, icrh);
}


/*
 * Estimate bus frequency using ACPI PM timer
 */
u64
lapic_estimate_freq(void)
{
#if 0
    u32 t1;
    u32 t2;
    //t1 = inl(acpi_pm_tmr_port);
    //t1 = inl(acpi_pm_tmr_port);
#endif
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
