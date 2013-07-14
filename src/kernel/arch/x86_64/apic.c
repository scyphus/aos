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
ioapic_map_intr(u64 intvec, u64 tbldst, u64 ioapic_base)
{
    u64 val;

    /*
     * 63:56    destination field
     * 16       interrupt mask (1: masked for edge sensitive)
     * 15       trigger mode (1=level sensitive, 0=edge sensitive)
     * 14       remote IRR (R/O) (1 if local APICs accept the level interrupts)
     * 13       interrupt input pin polarity (0=high active, 1=low active)
     * 12       delivery status (R/O)
     * 11       destination mode (0=physical, 1=logical)
     * 10:8     delivery mode
     *          000 fixed, 001 lowest priority, 010 SMI, 011 reserved
     *          100 NMI, 101 INIT, 110 reserved, 111 ExtINT
     * 7:0      interrupt vector
     */
    val = intvec;

    /* To avoid compiler optimization, call assembler function */
    asm_ioapic_map_intr(val, tbldst, ioapic_base);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
