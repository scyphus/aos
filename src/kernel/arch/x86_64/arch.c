/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../../kernel.h"
#include "arch.h"
#include "desc.h"
#include "acpi.h"
#include "apic.h"
#include "i8254.h"
#include "vga.h"
#include "bootinfo.h"

//static u64 cpu_freq;
//static u64 fsb_freq;

void apic_test(void);
void search_clock_sources(void);


void trampoline(void);
void trampoline_end(void);

/*
 * Initialize BSP
 */
void
arch_bsp_init(void)
{
    struct p_data *pdata;
    u64 tsz;
    u64 i;

    /* Initialize VGA display */
    vga_init();

    /* Find configuration using ACPI */
    acpi_load();

    /* Count the number of processors (APIC) */
    /* ToDo */

    /* Stop i8254 timer */
    i8254_stop_timer();


    /* Initialize global descriptor table */
    gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table */
    idt_init();
    idt_load();

    /* Setup interrupt handler */
    idt_setup_intr_gate(32, &intr_apic_int32); /* IRQ0 */
    idt_setup_intr_gate(33, &intr_apic_int33); /* IRQ1 */

    /* Initialize I/O APIC */
    ioapic_init();
    ioapic_map_intr(0x21, 1, acpi_ioapic_base);


    /* Initialize TSS and load it */
    tss_init();
    tr_load(this_cpu());

    /* Enable this processor */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->flags |= 1;

    /* Set general protection fault handler */
    idt_setup_intr_gate(13, &intr_gpf);

    /* Initialize local APIC */
    lapic_init();


    /* Check and copy trampoline */
    tsz = trampoline_end - trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        panic("Error! Trampoline size exceeds the maximum allowed size.\r\n");
    }
    for ( i = 0; i < tsz; i++ ) {
        *(u8 *)((u64)TRAMPOLINE_ADDR + i) = *(u8 *)((u64)trampoline + i);
    }

    /* Send INIT IPI */
    lapic_send_init_ipi();

    /* Wait 10 ms */
    arch_busy_usleep(10000);

    /* Send a Start Up IPI */
    lapic_send_startup_ipi((TRAMPOLINE_ADDR >> 12) & 0xff);

    /* Wait 200 us */
    arch_busy_usleep(200);

    /* Send another Start Up IPI */
    lapic_send_startup_ipi((TRAMPOLINE_ADDR >> 12) & 0xff);

    /* Wait 200 us */
    arch_busy_usleep(200);


    kprintf("-----------------------------------\r\n");

    char *str = "Welcome to AOS!  Now this message is printed by C function.";
    kprintf("%s\r\n", str);

    /* Print out the running processors */
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        pdata = (struct p_data *)((u64)P_DATA_BASE + i * P_DATA_SIZE);
        if ( pdata->flags & 1 ) {
            kprintf("Processor #%d is running.\r\n", i);
        }
    }


    /* Initialize local APIC counter */
    //APIC_INITTMR,0x380
    //APIC_CURTMR,0x390
    //APIC_TMRDIV,0x3e0
    //APIC_LVT_TMR,0x320

    __asm__ __volatile__ ( "movq $0xfee00000,%rdx; movl $0x20020,%eax; movl %eax,0x320(%rdx)" );
    __asm__ __volatile__ ( "movq $0xfee00000,%rdx; movl $03,%eax; movl %eax,0x3e0(%rdx)" );
    //__asm__ __volatile__ ( "movq $0xfee00000,%rdx; movl $0xffffffff,%ebx; movl %ebx,0x380(%rdx)" );
    __asm__ __volatile__ ( "movq $0xfee00000,%rdx; movl $0xffffff,%ebx; movl %ebx,0x380(%rdx)" );
}

/*
 * Initialize AP
 */
void
arch_ap_init(void)
{
    struct p_data *pdata;

    /* Load global descriptor table */
    gdt_load();

    /* Load interrupt descriptor table */
    idt_load();

    /* Load task register */
    tr_load(this_cpu());

    /* Set a flag to this CPU data area */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->flags |= 1;

    /* Initialize local APIC */
    lapic_init();
}


/*
 * Compute TSC frequency
 */
u64
calib_tsc_freq(void)
{
    return 0;
}

u64
get_invariant_tsc_frequency(void)
{
    u64 val;
    int family;
    int model;

    /* Check invariant TSC support first */
    if ( !is_invariant_tsc() ) {
        /* Invariant TSC is not available */
        return 0;
    }

    /* MSR_PLATFORM_INFO */
    val = rdmsr(0xce);
    /* Get maximum non-turbo ratio [15:8] */
    val = (val >> 8) & 0xff;

    family = get_cpu_family();
    model = get_cpu_model();
    if ( 0x06 == family && (0x2a == model || 0x2d == model || 0x3a == model) ) {
        /* SandyBridge (06_2AH, 06_2DH) and IvyBridge (06_3AH) */
        val *= 100000000; /* 100 MHz */
    } else if ( 0x06 == family
                && (0x1a == model || 0x1e == model || 0x1f == model
                    || 0x25 == model || 0x2c == model || 0x2e == model
                    || 0x2f == model) ) {
        /* Nehalem (06_1AH, 06_1EH, 06_1FH, 06_2EH)
           and Westmere (06_2FH, 06_2CH, 06_25H) */
        val *= 133330000; /* 133.33 MHz */
    } else {
        /* Unknown model */
        val = 0;
    }

    return val;
}

/*
 * Search clock sources
 */
void
search_clock_sources(void)
{
    u64 tscfreq;

    /* Get invariant TSC frequency */
    tscfreq = get_invariant_tsc_frequency();
    if ( tscfreq ) {
        kprintf("Freq: %.16x\r\n", tscfreq);
    }

    /* Get ACPI info */
}

/*
 * Put a character to console
 */
void
arch_putc(int c)
{
    vga_putc(c);
}

void
arch_get_tmr(u64 counter, u64 *resolution)
{
}

/*
 * Wait usec microsecond
 */
void
arch_busy_usleep(u64 usec)
{
    acpi_busy_usleep(usec);
}

/*
 * Lock with spinlock
 */
void
arch_spin_lock(int *lock)
{
    spin_lock(lock);
}

/*
 * Unlock with spinlock
 */
void
arch_spin_unlock(int *lock)
{
    spin_unlock(lock);
}

/*
 * Halt processor
 */
void
arch_halt(void)
{
    halt();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
