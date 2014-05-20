/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_ARCH_H
#define _KERNEL_ARCH_H

#include <aos/const.h>

#define MAX_PROCESSORS  256
#define IDT_NR          256

#define BOOTINFO_BASE           (u64)0x8000
#define TRAMPOLINE_ADDR         (u64)0x70000
#define TRAMPOLINE_MAX_SIZE     0x4000
#define GDT_ADDR                (u64)0x74000
#define GDT_MAX_SIZE            0x2000
#define IDT_ADDR                (u64)0x76000
#define IDT_MAX_SIZE            0x2000

/* FIXME: ISA memory hole cannot be detect through ACPI */
#define PHYS_MEM_FREE_ADDR      0x02000000

#define GDT_NULL_SEL            (0<<3)
#define GDT_RING0_CODE_SEL      (1<<3)
#define GDT_RING0_DATA_SEL      (2<<3)
#define GDT_RING1_CODE_SEL      (3<<3)
#define GDT_RING1_DATA_SEL      (4<<3)
#define GDT_RING2_CODE_SEL      (5<<3)
#define GDT_RING2_DATA_SEL      (6<<3)
#define GDT_RING3_CODE_SEL      (7<<3)
#define GDT_RING3_DATA_SEL      (8<<3)
#define GDT_TSS_SEL_BASE        (9<<3)

/* Also defined in asmconst.h */
#define	P_DATA_SIZE             0x10000
#define P_DATA_BASE             (u64)0x01000000
#define P_TSS_OFFSET            (0x20 + IDT_NR * 8)
#define P_STACK_GUARD           0x10

#define APIC_BASE               (u64)0xfee00000
#define APIC_SIVR               0x0f0
#define APIC_ICR_LOW            0x300
#define APIC_ICR_HIGH           0x310
#define APIC_LVT_TMR            0x320
#define APIC_TMRDIV             0x3e0
#define APIC_INITTMR            0x380
#define APIC_CURTMR             0x390

#define APIC_LVT_DISABLE        0x10000
#define APIC_LVT_ONESHOT        0x00000000
#define APIC_LVT_PERIODIC       0x00020000
#define APIC_LVT_TSC_DEADLINE   0x00040000

#define APIC_TMRDIV_X1          0xb
#define APIC_TMRDIV_X2          0x0
#define APIC_TMRDIV_X4          0x1
#define APIC_TMRDIV_X8          0x2
#define APIC_TMRDIV_X16         0x3
#define APIC_TMRDIV_X32         0x8
#define APIC_TMRDIV_X64         0x9
#define APIC_TMRDIV_X128        0xa
#define APIC_FREQ_PROBE         100000

#define LAPIC_HZ                100
#define AP_WAIT_USEC            (1000000/LAPIC_HZ/MAX_PROCESSORS)


/*
 * Stack frame for interrupts
 */
struct stackframe64 {
    /* Segment registers */
    u16 gs;
    u16 fs;

    /* Base pointer */
    u64 bp;

    /* Index registers */
    u64 di;
    u64 si;

    /* Generic registers */
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 dx;
    u64 cx;
    u64 bx;
    u64 ax;

    /* Restored by `iretq' instruction */
    u64 ip;             /* Instruction pointer */
    u64 cs;             /* Code segment */
    u64 flags;          /* Flags */
    u64 sp;             /* Stack pointer */
    u64 ss;             /* Stack segment */
} __attribute__ ((packed));

/*
 * TSS
 */
struct tss {
    u32 reserved1;
    u32 rsp0l;
    u32 rsp0h;
    u32 rsp1l;
    u32 rsp1h;
    u32 rsp2l;
    u32 rsp2h;
    u32 reserved2;
    u32 reserved3;
    u32 ist1l;
    u32 ist1h;
    u32 ist2l;
    u32 ist2h;
    u32 ist3l;
    u32 ist3h;
    u32 ist4l;
    u32 ist4h;
    u32 ist5l;
    u32 ist5h;
    u32 ist6l;
    u32 ist6h;
    u32 ist7l;
    u32 ist7h;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u16 iomap;
} __attribute__ ((packed));

/*
 * Task (architecture specific structure)
 */
struct arch_task {
    /* Do not change the first two.  These must be on the top.  See asm.s. */
    /* Restart point */
    struct stackframe64 *rp;
    /* SP0 for tss (fixed value?) */
    u64 sp0;
    /* Stack (kernel and user) */
    void *kstack;
    void *ustack;
    /* Parent structure */
    struct ktask *ktask;
} __attribute__ ((packed));

/*
 * Data space for each processor
 */
struct p_data {
    u32 flags;          /* bit 0: enabled (working); bit 1- reserved */
    u32 cpu_id;
    u64 freq;           /* Frequency */
    u32 reserved[4];
    u64 stats[IDT_NR];  /* Interrupt counter */
    /* P_TSS_OFFSET */
    struct tss tss;
    /* P_CUR_TASK_OFFSET */
    u64 cur_task;
    /* P_NEXT_TASK_OFFSET */
    u64 next_task;
    /* Stack and stack guard follow */
} __attribute__ ((packed));


/* Assertion: #P * 16 (TSS) + [1 (Null) + 2 (code and data) * 4 (rings)] * 8
              + 18 (gdtr) */
#if (MAX_PROCESSORS * 16 + (1 + 2 * 4) * 8 + 18) >= GDT_MAX_SIZE
#error "The size of the global descriptor table is invalid."
#endif

/* Assertion: #IDT * 16 + 18 (idtr)*/
#if (IDT_NR * 16 + 18) >= IDT_MAX_SIZE
#error "The size of the interrupt descriptor table is invalid."
#endif


void intr_gpf(void);

u8 inb(u16);
u16 inw(u16);
u32 inl(u16);
void outb(u16, u8);
void outw(u16, u16);
void outl(u16, u32);
void sfence(void);
void lfence(void);
void mfence(void);

void lidt(void *);
void lgdt(void *, u64);
void ltr(int);
void lldt(int);

u64 rdtsc(void);
u64 rdmsr(u64);
void hlt1(void);
void pause(void);
int is_invariant_tsc(void);
int get_cpu_family(void);
int get_cpu_model(void);

int acpi_load_rsdp(void);

/* Busy sleep */
void arch_busy_usleep(u64);

/* asm.s */
void asm_ioapic_map_intr(u64, u64, u64);
u32 asm_lapic_read(u64);
void asm_lapic_write(u64, u32);
int this_cpu(void);

void halt(void);

u64 bswap64(u64);
void movsb(void *, const void *, u64);
void set_cr3(u64);
u64 rdtsc(void);

void intr_apic_int32(void);
void intr_apic_int33(void);
void intr_apic_loc_tmr(void);
void intr_crash(void);
void intr_apic_spurious(void);

/* spinlock.s */
void spin_lock(volatile int *);
void spin_unlock(volatile int *);

extern u64 acpi_ioapic_base;
extern u64 acpi_pm_tmr_port;
extern u32 acpi_pm1a_ctrl_block;
extern u32 acpi_pm1b_ctrl_block;
extern u16 acpi_slp_typa;
extern u16 acpi_slp_typb;
extern u32 acpi_smi_cmd_port;
extern u8 acpi_enable_val;
extern u8 acpi_cmos_century;


int arch_dbg_printf(const char *fmt, ...);

#endif /* _KERNEL_ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
