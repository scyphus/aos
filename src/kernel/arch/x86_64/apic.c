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

#define ICR_FIXED               0x00000000
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
    struct p_data *pdata;

    /* Enable APIC at spurious interrupt vector register: default vector 0xff */
    reg = asm_lapic_read(APIC_BASE + APIC_SIVR);
    reg |= 0x100;
    asm_lapic_write(APIC_BASE + APIC_SIVR, reg);

    /* Set CPU frequency to this CPU data area */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->freq = lapic_estimate_freq();
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

    asm_lapic_write(APIC_BASE + APIC_ICR_HIGH, icrh);
    asm_lapic_write(APIC_BASE + APIC_ICR_LOW, icrl);
}

/*
 * Broadcast fixed IPI
 */
void
lapic_send_fixed_ipi(u8 vector)
{
    u32 icrl;
    u32 icrh;

    icrl = asm_lapic_read(APIC_BASE + APIC_ICR_LOW);
    icrh = asm_lapic_read(APIC_BASE + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_FIXED | ICR_DEST_ALL_EX_SELF | vector;
    icrh = icrh & 0x000fffff;

    asm_lapic_write(APIC_BASE + APIC_ICR_HIGH, icrh);
    asm_lapic_write(APIC_BASE + APIC_ICR_LOW, icrl);
}

/*
 * Broadcast fixed IPI (no shorthand)
 */
void
lapic_send_ns_fixed_ipi(u8 dst, u8 vector)
{
    u32 icrl;
    u32 icrh;

    icrl = asm_lapic_read(APIC_BASE + APIC_ICR_LOW);
    icrh = asm_lapic_read(APIC_BASE + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_FIXED | ICR_DEST_NOSHORTHAND | vector;
    icrh = (icrh & 0x000fffff) | ((u32)dst << 24);
    kprintf("%x %x\r\n", icrh, icrl);

    asm_lapic_write(APIC_BASE + APIC_ICR_HIGH, icrh);
    asm_lapic_write(APIC_BASE + APIC_ICR_LOW, icrl);
}


/*
 * Start local APIC timer
 */
void
lapic_start_timer(u64 freq, u8 vec)
{
    /* Estimate frequency first */
    u64 busfreq;
    struct p_data *pdata;

    /* Get CPU frequency to this CPU data area */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    busfreq = pdata->freq;

    /* Set counter */
    asm_lapic_write(APIC_BASE + APIC_LVT_TMR, APIC_LVT_PERIODIC | (u32)vec);
    asm_lapic_write(APIC_BASE + APIC_TMRDIV, APIC_TMRDIV_X16);
    asm_lapic_write(APIC_BASE + APIC_INITTMR, (busfreq >> 4) / freq);
}

/*
 * Stop APIC timer
 */
void
lapic_stop_timer(void)
{
    /* Disable timer */
    asm_lapic_write(APIC_BASE + APIC_LVT_TMR, APIC_LVT_DISABLE);
}


/*
 * Estimate bus frequency using busy usleep
 */
u64
lapic_estimate_freq(void)
{
    u32 t0;
    u32 t1;
    u32 probe;
    u64 ret;

    /* Set probe timer */
    probe = APIC_FREQ_PROBE;

    /* Disable timer */
    asm_lapic_write(APIC_BASE + APIC_LVT_TMR, APIC_LVT_DISABLE);

    /* Set divide configuration */
    asm_lapic_write(APIC_BASE + APIC_TMRDIV, APIC_TMRDIV_X16);

    /* Vector: lvt[18:17] = 00 : oneshot */
    asm_lapic_write(APIC_BASE + APIC_LVT_TMR, 0x0);

    /* Set initial counter */
    t0 = 0xffffffff;
    asm_lapic_write(APIC_BASE + APIC_INITTMR, t0);

    /* Sleep probing time */
    arch_busy_usleep(probe);

    /* Disable current timer */
    asm_lapic_write(APIC_BASE + APIC_LVT_TMR, APIC_LVT_DISABLE);

    /* Read current timer */
    t1 = asm_lapic_read(APIC_BASE + APIC_CURTMR);

    /* Calculate the APIC bus frequency */
    ret = (u64)(t0 - t1) << 4;
    ret = ret * 1000000 / probe;

    return ret;
}

/*
 * Initialize I/O APIC
 */
void
ioapic_init(void)
{
    /* Disable i8259 PIC */
    outb(0xa1, 0xff);
    outb(0x21, 0xff);

}

/*
 * Set a map entry of interrupt vector
 */
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
