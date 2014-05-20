/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "../../kernel.h"
#include "arch.h"
#include "desc.h"
#include "acpi.h"
#include "apic.h"
#include "i8254.h"
#include "vga.h"
#include "clock.h"
#include "cmos.h"
#include "bootinfo.h"
#include "memory.h"

//static u64 cpu_freq;
//static u64 fsb_freq;

void apic_test(void);
void search_clock_sources(void);
void vga_init(void);
void pci_init(void);
void netdev_init(void);
void e1000_init(void);
void e1000e_init(void);
void ixgbe_init(void);
void ahci_init(void);

extern void trampoline(void);
extern void trampoline_end(void);

static int dbg_lock;

int
arch_dbg_printf(const char *fmt, ...)
{
    va_list ap;
    u64 clk;

    arch_spin_lock(&dbg_lock);

    va_start(ap, fmt);

    clock_update();
    clk = clock_get();
    kprintf("[%4llu.%.6llu]  ", clk/1000000000, (clk / 1000) % 1000000);
    kvprintf(fmt, ap);

    va_end(ap);

    arch_spin_unlock(&dbg_lock);

    return 0;
}

/*
 * Initialize BSP
 */
void
arch_bsp_init(void)
{
    struct bootinfo *bi;
    struct p_data *pdata;
    u64 tsz;
    u64 i;

    dbg_lock = 0;
    bi = (struct bootinfo *)BOOTINFO_BASE;

    /* Initialize VGA display */
    vga_init();
    //u16 *ptr = (u16 *)0x000b8000;
    //*ptr = (0x07 << 8) | 'a';
    //kprintf("a");
    //kprintf(s, "a", *(u32 *)(0x13600), 15);
    //__asm__ __volatile__ ("1: cli; hlt; jmp 1b");

    /* Find configuration using ACPI */
    acpi_load();
    //__asm__ __volatile__ ( "1:hlt;jmp 1b;" );

    /* Initialize the clock */
    //__asm__ ("mov %%rax,%%dr3" :: "a"(0x1234));
    //u64 *pt = 0x13600 - 0x10;
    //char str[] = "%llx %llx %llx %llx %llx %llx %llx %llx\r\n";
    //kprintf(str, *pt, *(pt+1), *(pt+2), *(pt+3), *(pt+4), *(pt+5), *(pt+6), *(pt+7));
    //halt();
    clock_init();

    /* Print a message */
    kprintf("Welcome to Academic Operating System!\r\n");

    /* Count the number of processors (APIC) */
    /* ToDo */

    /* Stop i8254 timer */
    i8254_stop_timer();

    /* Initialize memory table */
    arch_dbg_printf("Initializing physical memory.\r\n");
    if ( 0 != phys_mem_init(bi) ) {
        panic("Error! Cannot initialize physical memory.\r\n");
    }

    /* For multiprocessors */
#if 0
    if ( 0 != phys_mem_wire((void *)P_DATA_BASE, P_DATA_SIZE*MAX_PROCESSORS) ) {
        panic("Error! Cannot allocate stack for processors.\r\n");
    }
#endif

    arch_dbg_printf("Initializing GDT and IDT.\r\n");

    /* Initialize global descriptor table */
    gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table */
    idt_init();
    idt_load();

    /* Setup interrupt handlers */
    arch_dbg_printf("Setting up interrupt handlers.\r\n");
    idt_setup_intr_gate(IV_TMR, &intr_apic_int32); /* IRQ0 */
    idt_setup_intr_gate(IV_KBD, &intr_apic_int33); /* IRQ1 */
    idt_setup_intr_gate(IV_LOC_TMR, &intr_apic_loc_tmr); /* Local APIC timer */
    idt_setup_intr_gate(0xfe, &intr_crash); /* crash */
    idt_setup_intr_gate(0xff, &intr_apic_spurious); /* Spurious interrupt */

    /* Setup interrupt service routine then initialize I/O APIC */
    ioapic_map_intr(IV_KBD, 1, acpi_ioapic_base); /* IRQ1 */
    ioapic_init();

#if 0
    u64 ptr = 0x80000000;
    *(u64 *)(ptr + 0x00200000) = 0xfe123456;
    kprintf("DEBUG: %.8x %.8x\r\n", *(u64 *)(ptr), *(u64 *)(ptr + 0x00200000));
#endif

    lldt(0);

    /* Initialize TSS and load it */
    arch_dbg_printf("Initializing TSS.\r\n");
    tss_init();
    tr_load(this_cpu());

    /* Enable this processor */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->flags |= 1;

    /* Set general protection fault handler */
    idt_setup_intr_gate(13, &intr_gpf);

    /* Initialize local APIC */
    lapic_init();

    /* Initialize PCI driver */
    arch_dbg_printf("Searching PCI devices.\r\n");
    pci_init();
    arch_dbg_printf("Initializing network devices.\r\n");
    netdev_init();
    e1000_init();
    e1000e_init();
    ixgbe_init();
    arch_dbg_printf("Initializing AHCI.\r\n");
    ahci_init();

    /* Check and copy trampoline */
    tsz = (u64)trampoline_end - (u64)trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        panic("Error! Trampoline size exceeds the maximum allowed size.\r\n");
    }
    for ( i = 0; i < tsz; i++ ) {
        *(u8 *)((u64)TRAMPOLINE_ADDR + i) = *(u8 *)((u64)trampoline + i);
    }

    /* Send INIT IPI */
    arch_dbg_printf("Starting all available application processors.\r\n");
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

    /* ToDo: Synchronize all processors */

    /* Print out the running processors */
#if 1
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        pdata = (struct p_data *)((u64)P_DATA_BASE + i * P_DATA_SIZE);
        if ( pdata->flags & 1 ) {
            arch_dbg_printf("Processor #%d is running.\r\n", i);
        }
    }
#endif

    /* Initialize local APIC counter */
    lapic_start_timer(LAPIC_HZ, IV_LOC_TMR);
}

/*
 * Initialize AP
 */
void
arch_ap_init(void)
{
    struct p_data *pdata;

    arch_dbg_printf("Initializing application processor #%d.\r\n", this_cpu());

    /* Load global descriptor table */
    gdt_load();

    /* Load interrupt descriptor table */
    idt_load();

    lldt(0);

    /* Load task register */
    tr_load(this_cpu());

    /* Set a flag to this CPU data area */
    pdata = (struct p_data *)((u64)P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->flags |= 1;

    /* Initialize local APIC */
    lapic_init();

    /* Wait short to distribute APIC interrupts */
    arch_busy_usleep(AP_WAIT_USEC * this_cpu());

    /* Go tick-less */
#if 0
    /* Initialize local APIC counter */
    lapic_start_timer(LAPIC_HZ, IV_LOC_TMR);
#endif

    arch_busy_usleep(1);
}



/*
 * Memory copy
 */
void *
arch_memcpy(void *dst, const void *src, u64 sz)
{
    movsb(dst, src, sz);
    return dst;
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
arch_spin_lock(volatile int *lock)
{
    spin_lock(lock);
}

/*
 * Unlock with spinlock
 */
void
arch_spin_unlock(volatile int *lock)
{
    spin_unlock(lock);
}

/*
 * Halt all processors
 */
void
arch_crash(void)
{
    /* Send IPI and halt self */
    lapic_send_fixed_ipi(0xfe);
    halt();
}

void
arch_poweroff(void)
{
    acpi_poweroff();
}

void
arch_clock_update(void)
{
    clock_update();
}
u64
arch_clock_get(void)
{
    return clock_get();
}


/*
 * Set next task
 */
int
arch_set_next_task(struct ktask *ktask)
{
    struct p_data *pdata;

    pdata = (struct p_data *)(P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->next_task = (u64)ktask->arch;

    return 0;
}

/*
 * Get next task
 */
struct ktask *
arch_get_next_task(void)
{
    struct p_data *pdata;
    struct arch_task *t;

    pdata = (struct p_data *)(P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    t = (struct arch_task *)pdata->next_task;
    if ( NULL != t ) {
        return t->ktask;
    } else {
        /* No running task */
        return NULL;
    }
}

/*
 * Get current task
 */
struct ktask *
arch_get_current_task(void)
{
    struct p_data *pdata;
    struct arch_task *t;

    pdata = (struct p_data *)(P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    t = (struct arch_task *)pdata->cur_task;
    if ( NULL != t ) {
        return t->ktask;
    } else {
        /* No running task */
        return NULL;
    }
}

/*
 * Allocate a task
 */
void *
arch_alloc_task(struct ktask *kt, void (*entry)(struct ktask *), int policy)
{
    struct arch_task *t;
    int cs;
    int ss;
    int flags;

    switch ( policy ) {
    case TASK_POLICY_KERNEL:
        /* Ring 0 for kernel privilege */
        cs = GDT_RING0_CODE_SEL;
        ss = GDT_RING0_DATA_SEL;
        flags = 0x0200;
        break;
    case TASK_POLICY_DRIVER:
        cs = GDT_RING1_CODE_SEL + 1;
        ss = GDT_RING1_DATA_SEL + 1;
        flags = 0x1200;
        break;
    default:
        cs = GDT_RING3_CODE_SEL + 3;
        ss = GDT_RING3_DATA_SEL + 3;
        flags = 0x3200;
    }

    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    t->kstack = kmalloc(TASK_KSTACK_SIZE);
    if ( NULL == t->kstack ) {
        kfree(t);
        return NULL;
    }
    t->ustack = kmalloc(TASK_USTACK_SIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->sp0 = (u64)t->kstack + TASK_KSTACK_SIZE - TASK_STACK_GUARD;

    t->rp = (struct stackframe64 *)(t->sp0 - sizeof(struct stackframe64));
    /* First argument */
    t->rp->di = (u64)kt;
    /* Other registers */
    t->rp->gs = 0;
    t->rp->fs = 0;
    t->rp->ip = (u64)entry;
    t->rp->cs = cs;
    t->rp->flags = flags;  /* IOPL */
    t->rp->sp = (u64)t->ustack + TASK_USTACK_SIZE - TASK_STACK_GUARD;
    t->rp->ss = ss;
    t->ktask = kt;

    return t;
}








u64 asm_popcnt(u64);
int
popcnt(u64 x)
{
    return asm_popcnt(x);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
