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
void ahci_init(void);

extern void trampoline(void);
extern void trampoline_end(void);

static int dbg_lock;

int
arch_dbg_printf(const char *fmt, ...)
{
    va_list ap;
    u64 clk;

    clock_update();

    arch_spin_lock(&dbg_lock);

    va_start(ap, fmt);
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
    volatile struct p_data *pdata;
    u64 tsz;
    u64 i;

    dbg_lock = 0;
    bi = (struct bootinfo *)BOOTINFO_BASE;

    /* Initialize VGA display */
    vga_init();

    /* Initialize the clock */
    clock_init();

    /* Print a message */
    kprintf("Welcome to mPIX!\r\n");

    /* Count the number of processors (APIC) */
    /* ToDo */

    /* Stop i8254 timer */
    i8254_stop_timer();

    /* Initialize memory table */
    arch_dbg_printf("Initializing physical memory.\r\n");
    if ( 0 != phys_mem_init(bi) ) {
        panic("Error! Cannot initialize physical memory.\r\n");
    }
    /* Initialize kmem */
    kmem_init();

    /* Find configuration using ACPI */
    acpi_load();

    /* For multiprocessors */
#if 0
    if ( 0 != phys_mem_wire((void *)P_DATA_BASE, P_DATA_SIZE*MAX_PROCESSORS) ) {
        panic("Error! Cannot allocate stack for processors.\r\n");
    }
#endif
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        kmemset((u8 *)((u64)P_DATA_BASE + i * P_DATA_SIZE), 0,
                sizeof(struct p_data));
    }

#if 0
    /* Check CPUID */
    u32 x;
    u32 y;
    asm volatile ("movl $1,%%eax;cpuid" : "=c"(x), "=d"(y));
    kprintf("ECX=%x, EDX=%x\r\n", x, y);
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
    idt_setup_intr_gate(IV_IRQ(0), &intr_apic_int32);
    idt_setup_intr_gate(IV_IRQ(1), &intr_apic_int33);
    idt_setup_intr_gate(IV_IRQ(2), &intr_apic_int34);
    idt_setup_intr_gate(IV_IRQ(3), &intr_apic_int35);
    idt_setup_intr_gate(IV_IRQ(4), &intr_apic_int36);
    idt_setup_intr_gate(IV_IRQ(5), &intr_apic_int37);
    idt_setup_intr_gate(IV_IRQ(6), &intr_apic_int38);
    idt_setup_intr_gate(IV_IRQ(7), &intr_apic_int39);
    idt_setup_intr_gate(IV_IRQ(8), &intr_apic_int40);
    idt_setup_intr_gate(IV_IRQ(9), &intr_apic_int41);
    idt_setup_intr_gate(IV_IRQ(10), &intr_apic_int42);
    idt_setup_intr_gate(IV_IRQ(11), &intr_apic_int43);
    idt_setup_intr_gate(IV_IRQ(12), &intr_apic_int44);
    idt_setup_intr_gate(IV_IRQ(13), &intr_apic_int45);
    idt_setup_intr_gate(IV_IRQ(14), &intr_apic_int46);
    idt_setup_intr_gate(IV_IRQ(15), &intr_apic_int47);
    idt_setup_intr_gate(IV_IRQ(16), &intr_apic_int48);
    idt_setup_intr_gate(IV_IRQ(17), &intr_apic_int49);
    idt_setup_intr_gate(IV_IRQ(18), &intr_apic_int50);
    idt_setup_intr_gate(IV_IRQ(19), &intr_apic_int51);
    idt_setup_intr_gate(IV_IRQ(20), &intr_apic_int52);
    idt_setup_intr_gate(IV_IRQ(21), &intr_apic_int53);
    idt_setup_intr_gate(IV_IRQ(22), &intr_apic_int54);
    idt_setup_intr_gate(IV_IRQ(23), &intr_apic_int55);
    idt_setup_intr_gate(IV_IRQ(24), &intr_apic_int56);
    idt_setup_intr_gate(IV_IRQ(25), &intr_apic_int57);
    idt_setup_intr_gate(IV_IRQ(26), &intr_apic_int58);
    idt_setup_intr_gate(IV_IRQ(27), &intr_apic_int59);
    idt_setup_intr_gate(IV_IRQ(28), &intr_apic_int60);
    idt_setup_intr_gate(IV_IRQ(29), &intr_apic_int61);
    idt_setup_intr_gate(IV_IRQ(30), &intr_apic_int62);
    idt_setup_intr_gate(IV_IRQ(31), &intr_apic_int63);
    idt_setup_intr_gate(IV_IRQ(32), &intr_apic_int64);
    idt_setup_intr_gate(IV_LOC_TMR, &intr_apic_loc_tmr); /* Local APIC timer */
    idt_setup_intr_gate(IV_IPI, &intr_apic_ipi);
    idt_setup_intr_gate(IV_CRASH, &intr_crash); /* crash */
    idt_setup_intr_gate(0xff, &intr_apic_spurious); /* Spurious interrupt */

    /* Setup interrupt service routine then initialize I/O APIC */
    for ( i = 0; i < 32; i++ ) {
        ioapic_map_intr(IV_IRQ(i), i, acpi_ioapic_base); /* IRQn */
    }
    ioapic_map_intr(IV_IRQ(32), 32, acpi_ioapic_base); /* IRQ32 */
    ioapic_init();

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
    /* Set page fault handler */
    idt_setup_intr_gate(14, &intr_pf);

    /* Initialize local APIC */
    lapic_init();

    /* Initialize PCI driver */
    arch_dbg_printf("Searching PCI devices.\r\n");
    pci_init();
    arch_dbg_printf("Initializing network devices.\r\n");
    netdev_init();

    arch_dbg_printf("Initializing AHCI.\r\n");
    ahci_init();

    /* Check and copy trampoline */
    tsz = (u64)trampoline_end - (u64)trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        panic("Error! Trampoline size exceeds the maximum allowed size.\r\n");
    }
    for ( i = 0; i < tsz; i++ ) {
        *(volatile u8 *)((u64)TRAMPOLINE_ADDR + i)
            = *(volatile u8 *)((u64)trampoline + i);
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
    arch_busy_usleep(100000);

#if 0
    /* Performance monitoring registers */
    //IA32_PMCx
    //IA32_PERFEVTSEL0: 0x186
    // LLC cache misses: Event 2E, unmask 41H
    u64 msr = 0x186;
    u64 pmc = (1<<22) | (1<<17) | (1<<16) | (0x41 << 8) | 0x2e;
    //u64 pmc = (1<<22) | (0<<21) | (1<<17) | (1<<16) | (0x00 << 8) | 0x3c;
    u64 zero = 0;
    __asm__ __volatile__ ("wrmsr" :: "a"(pmc), "c"(msr), "d"(zero) );
    msr = 0x187;
    pmc = (1<<22) | (0<<21) | (1<<17) | (1<<16) | (0x00 << 8) | 0x3c;
    //pmc = (1<<22) | (1<<17) | (1<<16) | (0x4f << 8) | 0x2e;
    __asm__ __volatile__ ("wrmsr" :: "a"(pmc), "c"(msr), "d"(zero) );

    //IA32_PERF_GLOBAL_CTRL = 0x38f
    u64 aa = 0;
    u64 dd = 0;
    u64 bb;

    aa = 0x0a;
    __asm__ __volatile__ ("cpuid" : "=a"(aa), "=b"(bb) : "a"(aa) );
    kprintf("CPUID.0AH %x %x\r\n", aa, bb);
    arch_busy_usleep(10);

    msr = 0x38f;
    aa = 1;
    //__asm__ __volatile__ ("wrmsr" : : "c"(msr), "a"(aa), "d"(dd) );
    msr = 0xc1;
    arch_busy_usleep(10);
    __asm__ __volatile__ ("rdmsr" : "=a"(aa), "=d"(dd) : "c"(msr) );
    //aa = 1;
    //__asm__ __volatile__ ("wrmsr" : : "c"(msr), "a"(aa), "d"(dd) );
    kprintf("%x %x\r\n", dd, aa);
    arch_busy_usleep(10);
    //u64 cc = 1;
    //__asm__ __volatile__ ("rdpmc" : "=a"(aa), "=d"(dd)  : "c"(cc) );
    //kprintf("Cycles: %.8x%.8x\r\n", dd, aa);
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
void
arch_spin_lock_intr(volatile int *lock)
{
    spin_lock_intr(lock);
}

/*
 * Unlock with spinlock
 */
void
arch_spin_unlock(volatile int *lock)
{
    spin_unlock(lock);
}
void
arch_spin_unlock_intr(volatile int *lock)
{
    spin_unlock_intr(lock);
}

/*
 * Halt all processors
 */
void
arch_crash(void)
{
    /* Send IPI and halt self */
    lapic_send_fixed_ipi(IV_CRASH);
    halt();
}

u8
arch_inb(u16 a)
{
    return inb(a);
}


void
arch_idle(void)
{
    idle();
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
u64
arch_time(void)
{
    return cmos_rtc_read_datetime(0);
}

/*
 * arch_task_switched
 */
void
arch_task_switched(struct arch_task *cur, struct arch_task *next)
{
    if ( NULL != cur && TASK_STATE_RUNNING == cur->ktask->state ) {
        /* Previous task is changed from running */
        ktask_change_state(cur->ktask, TASK_STATE_READY);
        cur->ktask->scheduled = -1;
    } else if ( NULL != cur ) {
        /* cur->ktask->state == TASK_STATE_BLOCKED */
        cur->ktask->scheduled = -1;
    }
    ktask_change_state(next->ktask, TASK_STATE_RUNNING);
}

/*
 * Set next task
 */
int
arch_set_next_task(struct ktask *ktask)
{
    volatile struct p_data *pdata;

    pdata = (volatile struct p_data *)(P_DATA_BASE + this_cpu() * P_DATA_SIZE);
    pdata->next_task = (u64)ktask->arch;

    return 0;
}
int
arch_set_next_task_other_cpu(struct ktask *ktask, int i)
{
    if ( i < 0 || i >= MAX_PROCESSORS ) {
        return -1;
    }
    volatile struct p_data *pdata;

    pdata = (volatile struct p_data *)(P_DATA_BASE + i * P_DATA_SIZE);
    pdata->next_task = (u64)ktask->arch;

    return 0;
}

/*
 * Get next task
 */
struct ktask *
arch_get_next_task(void)
{
    volatile struct p_data *pdata;
    struct arch_task *t;

    pdata = (volatile struct p_data *)(P_DATA_BASE + this_cpu() * P_DATA_SIZE);
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
    volatile struct p_data *pdata;
    struct arch_task *t;

    pdata = (volatile struct p_data *)(P_DATA_BASE + this_cpu() * P_DATA_SIZE);
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

    /* CPL <= IOPL */
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

void
arch_free_task(void *t)
{
    struct arch_task *at;

    at = (struct arch_task *)t;
    kfree(at->kstack);
    kfree(at->ustack);
}

#if 0
void *
arch_fork(struct ktask *kt, struct proc *proc)
{
    struct arch_task *t;
    int cs;
    int ss;
    int flags;

    /* CPL <= IOPL */
    switch ( proc->ctx.policy ) {
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
    kmemcpy(t->kstack, ((struct arch_task *)(proc->task->arch))->kstack,
            TASK_KSTACK_SIZE);
    t->ustack = kmalloc(TASK_USTACK_SIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    kmemcpy(t->ustack, ((struct arch_task *)(proc->task->arch))->ustack,
            TASK_USTACK_SIZE);
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
#endif

/*
 * Is active processor
 */
int
arch_cpu_active(u16 id)
{
    struct p_data *pdata;

    if ( id >= MAX_PROCESSORS ) {
        return 0;
    }

    pdata = (struct p_data *)(P_DATA_BASE + id * P_DATA_SIZE);
    if ( pdata->flags & 1 ) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * Disable interrupts on the current processor core
 */
void
arch_disable_interrupts(void)
{
    disable_interrupts();
}

/*
 * Enable interrupts on the current processor core
 */
void
arch_enable_interrupts(void)
{
    enable_interrupts();
}

/*
 * Call a system call
 */
void
arch_scall(u64 nr)
{
    syscall(nr);
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
