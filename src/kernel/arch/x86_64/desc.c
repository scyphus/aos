/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "desc.h"
#include "arch.h"

#define IDT_PRESENT     0x80
#define IDT_INTGATE     0x0e
struct idt_gate_desc idt[256];
void intr_null(void);
void intr_gp(void);

static void
idt_setup_gate_desc(int nr, u64 base, u16 selector, u8 flags)
{
    struct idtr *idtr = IDT_ADDR + sizeof(struct idt_gate_desc) * IDT_NR;
    struct idt_gate_desc *idt;

    idt = (struct idt_gate_desc *)
        (idtr->base + nr * sizeof(struct idt_gate_desc));

    idt->target_lo = (u16)(base & 0xffff);
    idt->selector = (u16)selector;
    idt->reserved1 = 0;
    idt->flags = flags;// + 0x60;
    idt->target_mid = (u16)((base & 0xffff0000UL) >> 16);
    idt->target_hi = (u16)((base & 0xffffffff00000000UL) >> 32);
    idt->reserved2 = 0;
}
void
idt_setup_intr_gate(int nr, void *target)
{
    idt_setup_gate_desc(nr, (u64)target, GDT_RING0_CODE_SEL,
                        IDT_PRESENT | IDT_INTGATE);
}

void
idt_init(void)
{
    struct idtr *idtr = IDT_ADDR + sizeof(struct idt_gate_desc) * IDT_NR;
    int i;
    u64 sz = IDT_NR * sizeof(struct idt_gate_desc);

    //(u64)kmem_alloc_pages((sz - 1) / PAGESIZE + 1)
    idtr->base = (u64)idt;
    idtr->size = sz - 1;

    for ( i = 0; i < 256; i++ ) {
        idt_setup_intr_gate(i, &intr_null);
    }
}

void
idt_load(void)
{
    struct idtr *idtr = IDT_ADDR + sizeof(struct idt_gate_desc) * IDT_NR;
    //void *x = &idtr;
    //__asm__ __volatile__ ( "lidt _idtr" );
    //__asm__ __volatile__ ( "lidt (%0)" : : "p"(x) );
    lidt(idtr);
}

void
gdt_setup_desc(struct gdt_desc *e, u32 base, u32 limit, u8 type, u8 dpl,
               u8 lbit, u8 dbit, u8 gbit)
{
    limit &= 0xfffff;
    /* 16bit: l=0, d=0, 32bit: l=0, d=1, 64bit: l=1, d=0 */
    e->w0 = limit & 0xffff;
    e->w1 = base & 0xffff;
    e->w2 = ((base>>16) & 0xff) | (type<<8) | (1<<12) | (dpl<<13) | (1<<15);
    e->w3 = ((limit>>16) & 0xf) | (lbit<<5) | (dbit<<6) | (gbit<<7)
        | ((base>>24) & 0xff);
}

void
gdt_setup_desc_tss(struct gdt_desc_tss *e, u64 base, u32 limit, u8 type, u8 dpl,
                   u8 gbit)
{
    limit &= 0xfffff;
    /* gbit => *4KiB */
    e->w0 = limit & 0xffff;
    e->w1 = base & 0xffff;
    e->w2 = ((base>>16) & 0xff) | (type<<8) | (dpl<<13) | (1<<15);
    e->w3 = ((limit>>16) & 0xf) | (gbit<<7) | ((base>>24) & 0xff);
    e->w4 = base>>32;
    e->w5 = 0;
    e->w6 = 0;
    e->w7 = 0;
}

void
gdt_init(void)
{
    int i;
    u64 sz = MAX_PROCESSORS * sizeof(struct gdt_desc_tss)
        + (1 + 2 * 4) * sizeof(struct gdt_desc);
    struct gdt_desc *gdt = GDT_ADDR;

    struct gdtr *gdtr = GDT_ADDR + sz;
    gdtr->base = (u64)gdt;
    gdtr->size = sz - 1;

    /* Null descriptor */
    gdt_setup_desc(&gdt[0], 0, 0, 0, 0, 0, 0, 0);
    /* For each ring */
    gdt_setup_desc(&gdt[1], 0, 0xfffff, 0xa, 0, 1, 0, 1);
    gdt_setup_desc(&gdt[2], 0, 0xfffff, 0x2, 0, 1, 0, 1);
    gdt_setup_desc(&gdt[3], 0, 0xfffff, 0xa, 1, 1, 0, 1);
    gdt_setup_desc(&gdt[4], 0, 0xfffff, 0x2, 1, 1, 0, 1);
    gdt_setup_desc(&gdt[5], 0, 0xfffff, 0xa, 2, 1, 0, 1);
    gdt_setup_desc(&gdt[6], 0, 0xfffff, 0x2, 2, 1, 0, 1);
    gdt_setup_desc(&gdt[7], 0, 0xfffff, 0xa, 3, 1, 0, 1);
    gdt_setup_desc(&gdt[8], 0, 0xfffff, 0x2, 3, 1, 0, 1);

    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        gdt_setup_desc_tss(&gdt[GDT_TSS_SEL_BASE/8+i*2],
                           P_DATA_BASE + i * P_DATA_SIZE,
                           sizeof(struct tss) - 1, 0x9, 0, 1);
    }
}

void
gdt_load(void)
{
    struct gdtr *gdtr = GDT_ADDR + MAX_PROCESSORS * 16 + (1 + 2 * 4) * 8;

    lgdt(gdtr, GDT_RING0_CODE_SEL);
    //__asm__ __volatile__ ( "lgdt (%0)" : : "p"(gdtr) );
}

void
tss_init(void)
{
    struct tss *tss;
    int i;

    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        /* Initialize the TSS for each processor (Local APIC) */
        tss = P_DATA_BASE + i * P_DATA_SIZE;
        tss->reserved1 = 0;
        tss->rsp0l = 0;
        tss->rsp0h = 0;
        tss->rsp1l = 0;
        tss->rsp1h = 0;
        tss->rsp2l = 0;
        tss->rsp2h = 0;
        tss->reserved2 = 0;
        tss->reserved3 = 0;
        tss->ist1l = 0;
        tss->ist1h = 0;
        tss->ist2l = 0;
        tss->ist2h = 0;
        tss->ist3l = 0;
        tss->ist3h = 0;
        tss->ist4l = 0;
        tss->ist4h = 0;
        tss->ist5l = 0;
        tss->ist5h = 0;
        tss->ist6l = 0;
        tss->ist6h = 0;
        tss->ist7l = 0;
        tss->ist7h = 0;
        tss->reserved4 = 0;
        tss->reserved5 = 0;
        tss->reserved6 = 0;
        tss->iomap = 0;
    }
}

void
tr_load(int nr)
{
    int tr;

    tr = GDT_TSS_SEL_BASE + (nr*2) * 8;
    ltr(tr);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
