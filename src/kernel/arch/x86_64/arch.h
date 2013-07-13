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

#define BOOTINFO_BASE           0x8000
#define TRAMPOLINE_ADDR         0x70000
#define TRAMPOLINE_MAX_SIZE     0x4000
#define GDT_ADDR                0x74000
#define GDT_MAX_SIZE            0x2000
#define IDT_ADDR                0x76000
#define IDT_MAX_SIZE            0x2000

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
#define	P_DATA_SIZE     0x10000
#define P_DATA_BASE     0x1000000
#define P_STACK_GUARD   0x10


/* Assertion: #P * 16 (TSS) + [1 (Null) + 2 (code and data) * 4 (rings)] * 8
              + 18 (gdtr) */
#if (MAX_PROCESSORS * 16 + (1 + 2 * 4) * 8 + 18) >= GDT_MAX_SIZE
#error "The size of the global descriptor table is invalid."
#endif

/* Assertion: #IDT * 16 + 18 (idtr)*/
#if (IDT_NR * 16 + 18) >= IDT_MAX_SIZE
#error "The size of the interrupt descriptor table is invalid."
#endif

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

u64 rdtsc(void);
u64 rdmsr(u64);
void pause(void);
int is_invariant_tsc(void);
int get_cpu_family(void);
int get_cpu_model(void);

int this_cpu(void);

int acpi_load_rsdp(void);

void asm_ioapic_map_intr(u64, u64, u64);

void arch_busy_usleep(u64);

/* spinlock.s */
void spin_lock(int *);
void spin_unlock(int *);

extern u64 acpi_ioapic_base;
extern u64 acpi_pm_tmr_port;


#endif /* _KERNEL_ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
